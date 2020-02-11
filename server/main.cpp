#include <Windows.h>
#include <vector>
#include <thread>
#include <fstream>
#include "tinyxml/tinyxml.h"
#include "tinyxml/tinystr.h"
#include "Log.h"
#include "..\videochat\message_codes.h"
#pragma comment(lib,"ws2_32.lib")

constexpr int	DEFAULT_PORT = 7778;
constexpr size_t  BUFSIZE = 320 * 240 * 24;
constexpr WCHAR CONFIG_NAME[] = L"config.xml";
WCHAR SERVICE_NAME[] = L"videochat_server";

SERVICE_STATUS        g_ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE                g_ServiceStopEvent = INVALID_HANDLE_VALUE;

void ReceiveAndSend(SOCKET sender, SOCKET receiver)
{
	char* buf = new char[BUFSIZE];
	buf[0] = message_codes::READY;
	send(receiver, buf, 1, 0);
	while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0)
	{
		int len = recv(sender, buf, BUFSIZE, 0);
		if(len == 0 || len == SOCKET_ERROR)
		{
			buf[0] = message_codes::DISCONNECTED;
			send(receiver, buf, 1, 0);
			break;
		}
		if (send(receiver, buf, len, 0) == SOCKET_ERROR)
			break;
	}
	delete[] buf;
}

int GetPort()
{
	WCHAR filepath[MAX_PATH];
	CHAR filepathA[MAX_PATH];
	GetAppDir(filepath);
	lstrcatW(filepath, CONFIG_NAME);
	WideCharToMultiByte(CP_ACP, 0, filepath, -1, filepathA, MAX_PATH, NULL, NULL);
	TiXmlDocument config;
	config.LoadFile(filepathA);
	const char* port = config.FirstChildElement("chat_server")->FirstChildElement("port")->GetText();
	if(config.Error())
		return DEFAULT_PORT;
	return atoi(port);
}

DWORD WINAPI ServiceWorkerThread(LPVOID lpParam)
{
	SOCKET listen_socket = socket(AF_INET, SOCK_STREAM, NULL);
	if (SOCKET_ERROR == listen_socket)
		return WSAGetLastError();

	SOCKADDR_IN addr;
	int addrl = sizeof(addr);
	addr.sin_addr.S_un.S_addr = INADDR_ANY;
	addr.sin_port = htons(GetPort());
	addr.sin_family = AF_INET;

	if (SOCKET_ERROR == bind(listen_socket, (struct sockaddr*)&addr, sizeof(addr))) {
		return WSAGetLastError();
	}

	if (SOCKET_ERROR == listen(listen_socket, SOMAXCONN)) {
		return WSAGetLastError();
	}

	u_long mode = 1;

	if (SOCKET_ERROR == ioctlsocket(listen_socket, FIONBIO, &mode)) {
		return WSAGetLastError();
	}

	mode = 0;

	SOCKET client_sockets[2] = { INVALID_SOCKET, INVALID_SOCKET };
	std::thread client_thread;

	int i = 0;

	while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0)
	{
		if (i < 2)
		{
			sockaddr_in client_addr;
			int addr_size = sizeof(client_addr);
			client_sockets[i] = accept(listen_socket, (sockaddr*)&client_addr, &addr_size);
			if (client_sockets[i] != SOCKET_ERROR)
			{
				*Log::getInstance() << "user connected: " << inet_ntoa(client_addr.sin_addr) << std::endl;
				ioctlsocket(client_sockets[i], FIONBIO, &mode);
				char msg = message_codes::WAIT;
				send(client_sockets[i], &msg, 1, 0);
				++i;
				if (i == 2)
				{
					client_thread = std::thread(ReceiveAndSend, client_sockets[1], client_sockets[0]);
					ReceiveAndSend(client_sockets[0], client_sockets[1]);
					closesocket(client_sockets[0]);
					closesocket(client_sockets[1]);
					client_thread.join();
					i = 0;
				}
			}
		}
	}

	closesocket(listen_socket);
	return ERROR_SUCCESS;
}

VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode)
{
	switch (CtrlCode)
	{
	case SERVICE_CONTROL_STOP:
	{

		if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
			break;

		g_ServiceStatus.dwControlsAccepted = 0;
		g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
		g_ServiceStatus.dwWin32ExitCode = 0;
		g_ServiceStatus.dwCheckPoint = 4;

		SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

		SetEvent(g_ServiceStopEvent);

		break;
	}
	default:
		break;
	}
}

VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv)
{
	g_StatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);

	if (g_StatusHandle == NULL)
	{
		return;
	}

	ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
	g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	g_ServiceStatus.dwControlsAccepted = 0;
	g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
	g_ServiceStatus.dwWin32ExitCode = 0;
	g_ServiceStatus.dwServiceSpecificExitCode = 0;
	g_ServiceStatus.dwCheckPoint = 0;

	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
		return;

	g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (g_ServiceStopEvent == NULL)
	{
		g_ServiceStatus.dwControlsAccepted = 0;
		g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		g_ServiceStatus.dwWin32ExitCode = GetLastError();
		g_ServiceStatus.dwCheckPoint = 1;

		return;
	}

	g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
	g_ServiceStatus.dwWin32ExitCode = 0;
	g_ServiceStatus.dwCheckPoint = 0;

	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
		return;

	HANDLE hThread = CreateThread(NULL, 0, ServiceWorkerThread, NULL, 0, NULL);

	WaitForSingleObject(hThread, INFINITE);

	CloseHandle(g_ServiceStopEvent);

	g_ServiceStatus.dwControlsAccepted = 0;
	g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
	g_ServiceStatus.dwWin32ExitCode = 0;
	g_ServiceStatus.dwCheckPoint = 3;

	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
		return;

	return;
}

int main()
{
	WSADATA wsa;
	if (WSAStartup(0x0101, &wsa))
		return WSAGetLastError();

	SERVICE_TABLE_ENTRY ServiceTable[1];
	ServiceTable[0].lpServiceName = SERVICE_NAME;
	ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)ServiceMain;

	if (StartServiceCtrlDispatcher(ServiceTable) == FALSE)
	{
		return GetLastError();
	}

	return 0;
}