#include "server_utils.h"

BOOL DirectoryExists(LPCTSTR szPath)
{
	DWORD dwAttrib = GetFileAttributes(szPath);

	return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
		(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

void GetAppDir(PWSTR dirpath)
{
	PWSTR user_dir = NULL;
	SHGetKnownFolderPath(FOLDERID_ProgramFiles, 0, NULL, &user_dir);
	lstrcpyW(dirpath, user_dir);
	CoTaskMemFree(user_dir);
	lstrcatW(dirpath, DIR_NAME);
}