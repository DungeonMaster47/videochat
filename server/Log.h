#pragma once

#include <mutex>
#include <fstream>
#include <ctime> 
#include "server_utils.h"

class Log
{
private:
	Log() {}
	Log(const Log&) {};
	Log& operator=(Log&) {};

	void getLog();

	static Log* p_instance;
	static std::mutex m_;
	std::ofstream f;
public:
	static Log* getInstance();
	template<class T>
	std::ostream& operator<<(const T& x)
	{
		std::lock_guard<std::mutex> lock(m_);
		if (!f.is_open())
		{
			getLog();
		}
		f << x;
		return f;
	}
};
