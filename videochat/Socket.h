#pragma once
#include <Windows.h>
#include <mutex>
#pragma comment(lib,"ws2_32.lib")
class Socket
{
private:
	Socket(const Socket& s) = default;
	Socket& operator=(const Socket& s) = default;

	SOCKET sock = INVALID_SOCKET;
	bool open = false;
	bool connected = false;
	std::mutex send_m;
	std::mutex recv_m;
public:
	bool is_open();
	bool is_connected();
	bool create();
	bool connect(const char* ip, const char* port);
	bool bind(const char* port);
	bool bind(int port);
	bool listen();
	Socket accept(sockaddr* addr, int* addrlen);
	bool set_blocking_mode();
	bool set_non_blocking_mode();
	bool close();
	int send(const char* buf, size_t len);
	int recv(char* buf, size_t len);

	Socket(const SOCKET s = INVALID_SOCKET);
	Socket(Socket&& s);
	Socket& operator=(Socket&& s);
	~Socket();
};

