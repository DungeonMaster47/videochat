#include <Windows.h>
#include <vector>
#include <thread>
#include "..\videochat\message_codes.h"
#pragma comment(lib,"ws2_32.lib")

constexpr auto BUFSIZE = 320 * 240 * 24;

void receive_and_send(SOCKET sender, SOCKET receiver)
{
	char* buf = new char[BUFSIZE];
	while (true)
		send(receiver, buf, recv(sender, buf, BUFSIZE, 0), 0);
	delete[] buf;
}

int main()
{
	WSADATA wsa;
	if (WSAStartup(0x0101, &wsa))
		PostQuitMessage(0);

	SOCKET sock = socket(AF_INET, SOCK_STREAM, NULL);
	if (sock == SOCKET_ERROR)
		PostQuitMessage(0);

	SOCKADDR_IN addr;
	int addrl = sizeof(addr);
	addr.sin_addr.S_un.S_addr = INADDR_ANY;
	addr.sin_port = htons(7779);
	addr.sin_family = AF_INET;

	if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
		PostQuitMessage(0);
	}

	if (listen(sock, SOMAXCONN) == SOCKET_ERROR) {
		PostQuitMessage(0);
	}

	int client_sock1 = accept(sock, NULL, NULL);

	if (client_sock1 == INVALID_SOCKET) {
		PostQuitMessage(0);
	}

	int client_sock2 = accept(sock, NULL, NULL);

	if (client_sock2 == INVALID_SOCKET) {
		PostQuitMessage(0);
	}

	std::thread t1 = std::thread(receive_and_send, client_sock1, client_sock2);
	std::thread t2 = std::thread(receive_and_send, client_sock2, client_sock1);
	char ready = message_codes::READY;
	send(client_sock1, &ready, 1, 0);
	send(client_sock2, &ready, 1, 0);
	t1.join();
	t2.join();
	/*buf[0] = message_codes::READY;
	send(client_sock1, (char*)buf, 1, 0);
	cv::Mat img;*/
	/*while (true)
	{
		int result = recv(client_sock1, (char*)buf, 1024*25, 0);
		switch (buf[result - 1])
		{
		case message_codes::TEXT:
			buf[result - 1] = 0;
			printf((char*)buf);
			break;
		case message_codes::FRAME:
			std::vector<uchar> vec(buf, buf + result);
			img = cv::imdecode(vec, cv::ImreadModes::IMREAD_UNCHANGED);
			cv::imshow("ass", img);
			cv::waitKey(33);
		}
	}*/
	closesocket(client_sock1);
	closesocket(client_sock2);
	closesocket(sock);
	return 0;
}
