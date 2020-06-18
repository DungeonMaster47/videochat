#include <Windows.h>
#include <opencv2/opencv.hpp>
#include <thread>
#include <atomic>
#include <string>
#include "message_codes.h"
#include "Socket.h"
#include "AudioPlayer.h"
#include "AudioRecorder.h"

#pragma comment(lib,"ws2_32.lib")

#define ID_SEND_BUTTON 1
#define ID_SEND_TEXTBOX 2
#define ID_CHAT_LIST 3
#define ID_IP_TEXTBOX 4
#define ID_PORT_TEXTBOX 5
#define ID_CONNECT_BUTTON 6
#define ID_CAM_BUTTON 7
#define ID_MIC_BUTTON 8

namespace main_window {
	constexpr char APP_NAME[] = "videochat";
	constexpr unsigned WIDTH = 640;
	constexpr unsigned HEIGHT = 480;
}
namespace frame {
	constexpr unsigned WIDTH = 320;
	constexpr unsigned HEIGHT = 240;
}
constexpr unsigned FRAMERATE = 60;
constexpr unsigned MILLISECONDS_IN_SECOND = 1000;
constexpr unsigned FRAME_DELAY = MILLISECONDS_IN_SECOND/FRAMERATE;
constexpr size_t BUFSIZE = 320 * 240 * 24;

struct MainWindow
{
	HWND hWnd;
	HWND hSendButton;
	HWND hConnectButton;
	HWND hCamButton;
	HWND hMicButton;
	HWND hSendTextbox;
	HWND hIPTextbox;
	HWND hPortTextbox;
	HWND hChatList;
	std::atomic<bool> isConnected;
	std::atomic<bool> isRecording;
	std::thread recorder;
	std::thread receiver;
	Socket sock;
	AudioRecorder audioRecorder;
	AudioPlayer audioPlayer;
};

void DrawBitmap(HDC hDC, const int x, const int y, HBITMAP hBitmap)
{
	HBITMAP hOldbm;
	HDC hMemDC;
	BITMAP bm;
	POINT  ptSize, ptOrg;

	hMemDC = CreateCompatibleDC(hDC);

	hOldbm = (HBITMAP)SelectObject(hMemDC, hBitmap);

	if (hOldbm)
	{
		SetMapMode(hMemDC, GetMapMode(hDC));

		GetObject(hBitmap, sizeof(BITMAP), (LPSTR)&bm);

		ptSize.x = bm.bmWidth;   
		ptSize.y = bm.bmHeight;  

		DPtoLP(hDC, &ptSize, 1);

		ptOrg.x = 0;
		ptOrg.y = 0;

		DPtoLP(hMemDC, &ptOrg, 1);

		BitBlt(hDC, x, y, ptSize.x, ptSize.y,
			hMemDC, ptOrg.x, ptOrg.y, SRCCOPY);

		SelectObject(hMemDC, hOldbm);
	}

	DeleteDC(hMemDC);
}

bool AddStringToListBox(HWND hListbox, const char* str)
{
	std::string s(str);
	size_t offset = 0;
	offset = s.find('\n', offset);
	if (std::string::npos == offset)
	{
		if (LB_ERR == SendMessage(hListbox, LB_ADDSTRING, NULL, (LPARAM)str))
			return false;
	}
	else
	{
		size_t offset_old = 0;
		while (std::string::npos != offset)
		{
			s[offset] = 0;
			++offset;
			if (LB_ERR == SendMessage(hListbox, LB_ADDSTRING, NULL, (LPARAM)s.c_str()+offset_old))
				return false;
			offset_old = offset;
			offset = s.find('\n', offset);
		}
		if (LB_ERR == SendMessage(hListbox, LB_ADDSTRING, NULL, (LPARAM)s.c_str() + offset_old))
			return false;
	}

	size_t lbn = SendMessage(hListbox, LB_GETCOUNT, NULL, NULL);	
	if(LB_ERR == lbn)
		return false;
	if(LB_ERR == SendMessage(hListbox, LB_SETTOPINDEX, (WPARAM)lbn - 1, 0))
		return false;
	return true;
}

void Record(HWND hWnd, Socket& sock, std::atomic<bool>& isRecording)
{

	cv::VideoCapture vCap(cv::CAP_ANY);
	if (!vCap.isOpened())
	{
		MessageBox(hWnd, "Can't initialize camera.", "Error", MB_OK | MB_ICONERROR);
		vCap.release();
		isRecording = false;
		return;
	}
	cv::Mat img = cv::Mat::zeros(cv::Size(frame::WIDTH, frame::HEIGHT), CV_8UC3);
	DWORD* pixels = new DWORD[frame::WIDTH * frame::HEIGHT];
	while (vCap.read(img) && isRecording.load())
	{
		cv::resize(img, img, cv::Size(frame::WIDTH, frame::HEIGHT));

		std::vector<uchar> buf;
		cv::imencode(".jpg", img, buf);

		buf.push_back(message_codes::FRAME);
		
		sock.send((const char*)buf.data(), buf.size());

		for (size_t i = 0; i < frame::WIDTH * frame::HEIGHT; ++i)
			pixels[i] = *((DWORD*)&img.data[i*3]);

		HBITMAP hBitmap = CreateBitmap(frame::WIDTH, frame::HEIGHT, 1, 32, pixels);

		HDC hDC = GetDC(hWnd);
		DrawBitmap(hDC, 0, 0, hBitmap);
		ReleaseDC(hWnd, hDC);
		DeleteObject(hBitmap);

		cv::waitKey(FRAME_DELAY);
	}
	isRecording = false;

	img = cv::Mat::zeros(cv::Size(frame::WIDTH, frame::HEIGHT), CV_8UC3);
	std::vector<uchar> buf;
	cv::imencode(".jpg", img, buf);

	buf.push_back(message_codes::FRAME);

	sock.send((const char*)buf.data(), (int)buf.size());

	for (size_t i = 0; i < frame::WIDTH * frame::HEIGHT; ++i)
		pixels[i] = 0;
	
	HBITMAP hBitmap = CreateBitmap(frame::WIDTH, frame::HEIGHT, 1, 32, pixels);

	HDC hDC = GetDC(hWnd);
	DrawBitmap(hDC, 0, 0, hBitmap);
	ReleaseDC(hWnd, hDC);
	DeleteObject(hBitmap);

	delete[] pixels;
	vCap.release();
}

void Receive(MainWindow& self)
{
	DWORD* pixels = new DWORD[frame::WIDTH * frame::HEIGHT];
	std::vector<uchar> buf;
	cv::Mat img;
	buf.reserve(BUFSIZE);
	while (true)
	{
		buf.resize(BUFSIZE);
		int recieveSize = self.sock.recv((char*)buf.data(), (int)buf.capacity());

		if (!self.sock.is_connected())
		{
			if(!self.sock.close())
				break;
		
			AddStringToListBox(self.hChatList, "Service: server disconnected");

			self.isConnected = false;
			break;
		}

		if (recieveSize <= 0)
			continue;

		buf.resize(recieveSize);

		switch (buf.back())
		{
		case message_codes::WAIT:
		{
			AddStringToListBox(self.hChatList, "Service: waiting for another user to connect");
			break;
		}
		case message_codes::READY:
		{
			AddStringToListBox(self.hChatList, "Service: user connected");
			break;
		}
		case message_codes::DISCONNECTED:
		{
			AddStringToListBox(self.hChatList, "Service: user disconnected");
			break;
		}
		case message_codes::TEXT:
		{
			buf.back() = 0;
			AddStringToListBox(self.hChatList, (std::string("Someone: ")+(const char*)buf.data()).data());
			break;
		}
		case message_codes::FRAME:
		{
			buf.pop_back();

			img = cv::imdecode(buf, cv::ImreadModes::IMREAD_UNCHANGED);

			if (NULL == img.data)
				break;

			for (size_t i = 0; i < frame::WIDTH * frame::HEIGHT; ++i)
				pixels[i] = *((DWORD*)&img.data[i * 3]);

			HBITMAP hBitmap = CreateBitmap(frame::WIDTH, frame::HEIGHT, 1, 32, pixels);

			HDC hDC = GetDC(self.hWnd);
			DrawBitmap(hDC, 0, frame::HEIGHT, hBitmap);

			ReleaseDC(self.hWnd, hDC);
			DeleteObject(hBitmap);
			break;
		}
		case message_codes::SOUND:
		{
			std::vector<short> samples;
			samples.reserve(buf.size()/2+1);
			samples.resize(buf.size()/2);
			if (samples.data() == nullptr)
				break;
			memcpy(samples.data(), buf.data(), buf.size());
			self.audioPlayer.addSamples(samples);
			break;
		}
		}

	}

	for (size_t i = 0; i < frame::WIDTH * frame::HEIGHT; ++i)
		pixels[i] = 0;

	HBITMAP hBitmap = CreateBitmap(frame::WIDTH, frame::HEIGHT, 1, 32, pixels);
	
	HDC hDC = GetDC(self.hWnd);
	DrawBitmap(hDC, 0, frame::HEIGHT, hBitmap);
	ReleaseDC(self.hWnd, hDC);
	DeleteObject(hBitmap);

	delete[] pixels;
}

bool SendText(Socket& sock, char* text)
{
	size_t len = strlen(text);
	text[len] = message_codes::TEXT;
	bool result = !(sock.send(text, (int)len + 1) == SOCKET_ERROR);
	text[len] = 0;
	return result;
}

LRESULT CALLBACK WndProc(_In_ HWND hWnd, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	static MainWindow self;

	if (WM_CREATE == uMsg)
	{
		WSADATA wsa;
		if (WSAStartup(MAKEWORD(2, 2), &wsa))
		{
			MessageBox(hWnd, "Error initializing windows sockets", "Error", MB_OK);
			PostQuitMessage(0);
		}
		
		self.hWnd = hWnd;
		self.hSendButton = CreateWindow("button", "Send", WS_CHILD | BS_PUSHBUTTON | WS_VISIBLE, main_window::WIDTH-80, main_window::HEIGHT-30*2, 80, 30, hWnd, (HMENU)ID_SEND_BUTTON, 0, NULL);
		self.hCamButton = CreateWindow("button", "Cam on", WS_CHILD | BS_PUSHBUTTON | WS_VISIBLE, main_window::WIDTH-80, main_window::HEIGHT-30*3, 80, 30, hWnd, (HMENU)ID_CAM_BUTTON, 0, NULL);
		self.hMicButton = CreateWindow("button", "Mic on", WS_CHILD | BS_PUSHBUTTON | WS_VISIBLE, main_window::WIDTH - 80, main_window::HEIGHT - 30 * 4, 80, 30, hWnd, (HMENU)ID_MIC_BUTTON, 0, NULL);
		self.hConnectButton = CreateWindow("button", "Connect", WS_CHILD | BS_PUSHBUTTON | WS_VISIBLE, main_window::WIDTH-80, main_window::HEIGHT-30, 80, 30, hWnd, (HMENU)ID_CONNECT_BUTTON, 0, NULL);
		self.hSendTextbox = CreateWindow("edit", NULL, WS_BORDER | WS_VISIBLE | WS_CHILD | ES_LEFT | ES_MULTILINE, frame::WIDTH, main_window::HEIGHT-30*4, frame::WIDTH-80, 30*3, hWnd, (HMENU)ID_SEND_TEXTBOX, 0, NULL);
		self.hIPTextbox = CreateWindow("edit", NULL, WS_BORDER | WS_VISIBLE | WS_CHILD | ES_LEFT , frame::WIDTH+30, main_window::HEIGHT-30, frame::WIDTH-200, 30, hWnd, (HMENU)ID_IP_TEXTBOX, 0, NULL);
		self.hPortTextbox = CreateWindow("edit", NULL, WS_BORDER | WS_VISIBLE | WS_CHILD | ES_LEFT , frame::WIDTH+180, main_window::HEIGHT-30, frame::WIDTH-260, 30, hWnd, (HMENU)ID_PORT_TEXTBOX, 0, NULL);
		self.hChatList = CreateWindow("listbox", NULL, WS_BORDER | WS_VISIBLE | WS_CHILD | LBS_HASSTRINGS | WS_VSCROLL, frame::WIDTH, 0, main_window::WIDTH-frame::WIDTH, main_window::HEIGHT-30*4, hWnd, (HMENU)ID_CHAT_LIST, 0, NULL);
		self.receiver = std::thread();
		self.recorder = std::thread();
		self.isConnected = false;
		self.isRecording = false;
		try
		{
			self.audioPlayer.start();
		}
		catch(std::exception e)
		{
			MessageBox(hWnd, e.what(), main_window::APP_NAME, MB_OK | MB_ICONERROR);
		}
		Socket& sock = self.sock;
		self.audioRecorder.setProcessingCallback([&sock](std::vector<short> samples)
		{
			if (samples.size() <= 0)
				return;
			std::vector<char> byteSamples;
			byteSamples.resize(samples.size() * sizeof(short));
			memcpy(byteSamples.data(), samples.data(), byteSamples.size());
			byteSamples.push_back(message_codes::SOUND);
			sock.send((byteSamples.data()), byteSamples.size());
		});


		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	switch (uMsg)
	{
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hDC = BeginPaint(hWnd, &ps);
		TextOut(hDC, frame::WIDTH + 10, main_window::HEIGHT - 30, "IP:", 3);
		TextOut(hDC, frame::WIDTH + 150, main_window::HEIGHT - 30, "Port:", 5);
		DWORD* pixels = new DWORD[frame::WIDTH * frame::HEIGHT];

		for (size_t i = 0; i < frame::WIDTH * frame::HEIGHT; ++i)
			pixels[i] = 0;

		HBITMAP hBitmap = CreateBitmap(frame::WIDTH, frame::HEIGHT, 1, 32, pixels);

		DrawBitmap(hDC, 0, 0, hBitmap);
		DrawBitmap(hDC, 0, frame::HEIGHT, hBitmap);
		DeleteObject(hBitmap);
		delete[] pixels;

		EndPaint(hWnd, &ps);
		break;
	}
	case WM_CLOSE:
	{
		if (MessageBox(hWnd, "Are you sure you want to quit?", main_window::APP_NAME, MB_YESNO | MB_ICONQUESTION) == IDYES)
		{
			DestroyWindow(hWnd);
		}
		return 0;
	}
	case WM_DESTROY:
	{
		self.audioRecorder.stop();
		self.audioPlayer.stop();
		self.sock.close();
		self.isRecording = false;
		if (self.receiver.joinable())
			self.receiver.join();
		if (self.recorder.joinable())
			self.recorder.join();
		WSACleanup();
		PostQuitMessage(0);
		return 0;
	}
	case WM_COMMAND:
	{
		if (NULL == LOWORD(wParam))
			return 0;
		if (BN_CLICKED == HIWORD(wParam))
		{
			switch (LOWORD(wParam))
			{
			case ID_SEND_BUTTON:
			{
				char message[512] = {};
				for (int i = 0; i < SendMessage(self.hSendTextbox, EM_GETLINECOUNT, 0, 0); ++i)
				{
					char line[128] = {};
					*(WORD*)line = 128;
					line[SendMessage(self.hSendTextbox, EM_GETLINE, i, LPARAM(line))] = 0;
					if (strlen(line) == 0)
						continue;
					if(i != 0)
						message[strlen(message)] = '\n';
					strcat_s(message, line);
				}
				if (!SendText(self.sock, message))
				{
					MessageBox(hWnd, "Error sending message", "Error", MB_OK);
				}
				char text[512] = "You: ";
				strcat_s(text, message);
				AddStringToListBox(self.hChatList, text);
				SetWindowText(self.hSendTextbox, 0);
				break;
			}
			case ID_CONNECT_BUTTON:
			{
				if (self.isConnected.load())
				{
					MessageBox(hWnd, "Already connected", "Error", MB_OK);
					break;
				}
				char ip[32];
				*(WORD*)ip = 32;
				ip[SendMessage(self.hIPTextbox, EM_GETLINE, NULL, LPARAM(ip))] = 0;
				char port[16];
				*(WORD*)port = 16;
				port[SendMessage(self.hPortTextbox, EM_GETLINE, NULL, LPARAM(port))] = 0;
				if (!self.sock.connect(ip, port))
					MessageBox(hWnd, "Error connecting to server", "Error", MB_OK);
				else
				{
					self.isConnected = true;
					if (self.receiver.joinable())
						self.receiver.join();
					self.receiver = std::thread(Receive, std::ref(self));
				}
				break;
			}
			case ID_CAM_BUTTON:
			{
				if (!self.isConnected)
					break;

				if (self.isRecording.load())
				{
					self.isRecording = false;
					self.recorder.join();
					SendMessage(self.hCamButton, WM_SETTEXT, NULL, (LPARAM)"Cam on");
				}
				else
				{
					if (self.recorder.joinable())
						self.recorder.join();
					self.isRecording = true;
					self.recorder = std::thread(Record, hWnd, std::ref(self.sock), std::ref(self.isRecording));
					SendMessage(self.hCamButton, WM_SETTEXT, NULL, (LPARAM)"Cam off");
				}
				break;
			}
			case ID_MIC_BUTTON:
			{
				if (!self.isConnected)
					break;

				if (self.audioRecorder.isRecording())
				{
					self.audioRecorder.stop();
					SendMessage(self.hMicButton, WM_SETTEXT, NULL, (LPARAM)"Mic on");
				}
				else
				{
					try
					{
						self.audioRecorder.start();
						SendMessage(self.hMicButton, WM_SETTEXT, NULL, (LPARAM)"Mic off");
					}
					catch(std::exception e)
					{
						MessageBox(hWnd, e.what(), main_window::APP_NAME, MB_OK | MB_ICONERROR);
					}
				}
				break;
			}
			}
		}
	}
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);

}

int CALLBACK WinMain(_In_ HINSTANCE hInstance, _In_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
	WNDCLASSEX wcex;

	ZeroMemory(&wcex, sizeof(wcex));

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = NULL;
	wcex.lpfnWndProc = WndProc;
	wcex.hInstance = hInstance;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_APPWORKSPACE);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = main_window::APP_NAME;
	if (!RegisterClassEx(&wcex))
	{
		MessageBox(NULL,
			"Call to RegisterClass failed!",
			"Error",
			NULL);

		return 1;
	}

	HWND hWnd = CreateWindowEx(WS_EX_APPWINDOW, main_window::APP_NAME, main_window::APP_NAME,
		WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME, NULL, NULL, main_window::WIDTH,
		main_window::HEIGHT, NULL, NULL, hInstance, NULL);
	
	RECT r;
	GetClientRect(hWnd, &r);

	SetWindowPos(hWnd, NULL, 0, 0, main_window::WIDTH * 2 - r.right, main_window::HEIGHT * 2 - r.bottom, SWP_NOREPOSITION);

	SetWindowLong(hWnd, GWL_STYLE, GetWindowLong(hWnd, GWL_STYLE)&~WS_MAXIMIZEBOX);

	ShowWindow(hWnd, SW_SHOW);

	if (!hWnd)
	{
		MessageBox(NULL,
			"CreateWindow failed!",
			"Error",
			NULL);

		return 1;
	}

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return (int)msg.wParam;
}
