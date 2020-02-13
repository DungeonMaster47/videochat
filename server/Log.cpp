#include "Log.h"

Log* Log::getInstance()
{
	if (p_instance == nullptr) {
		std::lock_guard<std::mutex> lock(m_);
		if (p_instance == nullptr) {
			p_instance = new Log();
		}
	}
	return p_instance;
}

void Log::getLog()
{
	WCHAR dirpath[MAX_PATH];
	GetAppDir(dirpath);
	WCHAR filepath[MAX_PATH];

	lstrcpyW(filepath, dirpath);
	lstrcatW(filepath, LOG_NAME);
	if (!DirectoryExists(dirpath))
		CreateDirectoryW(dirpath, NULL);
	f.open(filepath, std::ios::app);
}

std::mutex Log::m_;
Log* Log::p_instance = nullptr;