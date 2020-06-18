#include "Socket.h"

Socket::Socket(Socket&& s)
{
	sock = s.sock;
	connected = s.connected;
	open = s.open;

	s.sock = INVALID_SOCKET;
	s.connected = false;
	s.open = false;
}

Socket::Socket(const SOCKET s)
{
	if (INVALID_SOCKET == s)
	{
		open = false;
		connected = false;
	}
	else
	{
		sock = s;
		open = true;
		connected = true;
	}
}

Socket& Socket::operator=(Socket && s)
{
	close();
	sock = s.sock;
	connected = s.connected;
	open = s.open;

	s.sock = INVALID_SOCKET;
	s.connected = false;
	s.open = false;

	return *this;
}

Socket::~Socket()
{
	close();
}

bool Socket::is_open()
{
	return open;
}

bool Socket::is_connected()
{
	return connected;
}

bool Socket::create()
{
	if (open)
		return open;

	sock = socket(AF_INET, SOCK_STREAM, NULL);
	if (SOCKET_ERROR == sock)
		return false;

	return open = true;
}

bool Socket::connect(const char * ip, const char * port)
{
	if (!create())
		return false;

	SOCKADDR_IN addr;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_port = htons(atoi(port));

	if (SOCKET_ERROR == ::connect(sock, (sockaddr*)&addr, sizeof(addr)))
		return false;

	return connected = true;
}

bool Socket::bind(const char * port)
{
	if (!create())
		return false;

	SOCKADDR_IN addr;
	int addrl = sizeof(addr);
	addr.sin_addr.S_un.S_addr = INADDR_ANY;
	addr.sin_port = htons(atoi(port));
	addr.sin_family = AF_INET;

	if (SOCKET_ERROR == ::bind(sock, (struct sockaddr*)&addr, sizeof(addr)))
		return false;

	return connected = true;
}

bool Socket::bind(int port)
{
	if (!create())
		return false;

	SOCKADDR_IN addr;
	int addrl = sizeof(addr);
	addr.sin_addr.S_un.S_addr = INADDR_ANY;
	addr.sin_port = htons(port);
	addr.sin_family = AF_INET;

	if (SOCKET_ERROR == ::bind(sock, (struct sockaddr*)&addr, sizeof(addr)))
		return false;

	return connected = true;
}

bool Socket::listen()
{
	if (!connected)
		return false;
	if (SOCKET_ERROR == ::listen(sock, SOMAXCONN))
		return false;

	return true;
}

Socket Socket::accept(sockaddr* addr, int* addrlen)
{
	SOCKET s = ::accept(sock, addr, addrlen);
	return Socket(s);
}

bool Socket::set_blocking_mode()
{
	u_long mode = 0;

	if (SOCKET_ERROR == ioctlsocket(sock, FIONBIO, &mode)) {
		return false;
	}
	return true;
}

bool Socket::set_non_blocking_mode()
{
	u_long mode = 1;

	if (SOCKET_ERROR == ioctlsocket(sock, FIONBIO, &mode)) {
		return false;
	}
	return true;
}

bool Socket::close()
{
	open = false;
	connected = false;
	if(SOCKET_ERROR == closesocket(sock))
		return false;
	return true;
}

int Socket::send(const char * buf, int len)
{
	std::lock_guard<std::mutex> lock(send_m);

	if (len <= 0)
		return 0;

	if (!connected || !open)
		return SOCKET_ERROR;
	int result = ::send(sock, reinterpret_cast<const char*>(&len), sizeof(int), 0);
	if (SOCKET_ERROR == result)
		connected = false;
	result = ::send(sock, buf, len, 0);
	if (SOCKET_ERROR == result)
		connected = false;

	return result;
}

int Socket::recv(char * buf, int len)
{
	std::lock_guard<std::mutex> lock(recv_m);

	if (!connected || !open)
		return SOCKET_ERROR;
	int packetSize = 0;
	int bytesRead = ::recv(sock, reinterpret_cast<char*>(&packetSize), sizeof(int), 0);
	if (0 == bytesRead || SOCKET_ERROR == bytesRead)
	{
		connected = false;
		return SOCKET_ERROR;
	}

	if (bytesRead != sizeof(int))
		return SOCKET_ERROR;

	if (packetSize <= 0)
	{
		return SOCKET_ERROR;
	}

	if (packetSize > len)
	{
		char* newBuf = new char[packetSize];
		bytesRead = ::recv(sock, newBuf, packetSize, 0);
		delete[] newBuf;
		return SOCKET_ERROR;
	}

	bytesRead = ::recv(sock, buf, packetSize, 0);

	if (0 == bytesRead || SOCKET_ERROR == bytesRead)
	{
		connected = false;
		return SOCKET_ERROR;
	}

	return packetSize;
}
