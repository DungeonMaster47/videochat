#pragma once
#include <Windows.h>
#include <ShlObj.h>

constexpr WCHAR DIR_NAME[] = L"\\videochat_server\\";
constexpr WCHAR LOG_NAME[] = L"log.txt";

BOOL DirectoryExists(LPCTSTR szPath);

void GetAppDir(PWSTR dirpath);