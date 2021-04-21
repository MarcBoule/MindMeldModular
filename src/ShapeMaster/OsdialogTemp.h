// temporary stuff until Rack V2 is released
// required for windows only, issue #16 in osdialog

// WHEN THIS FILE GETS REMOVED, remove the following lines in the plugin's makefile
// L3: include $(RACK_DIR)/arch.mk
// L14: ifdef ARCH_WIN
// L15: 	LDFLAGS += -lcomdlg32
// L16: endif

#ifdef ARCH_WIN 

#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>


static char* wchar_to_utf8(const wchar_t* s) {
	if (!s)
		return NULL;
	int len = WideCharToMultiByte(CP_UTF8, 0, s, -1, NULL, 0, NULL, NULL);
	if (!len)
		return NULL;
	char* r = (char*)malloc(len);
	WideCharToMultiByte(CP_UTF8, 0, s, -1, r, len, NULL, NULL);
	return r;
}

static wchar_t* utf8_to_wchar(const char* s) {
	if (!s)
		return NULL;
	int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
	if (!len)
		return NULL;
	wchar_t* r = (wchar_t*)malloc(len * sizeof(wchar_t));
	MultiByteToWideChar(CP_UTF8, 0, s, -1, r, len);
	return r;
}

static INT CALLBACK browseCallbackProc(HWND hWnd, UINT Msg, long long int wParam, LPARAM lParam) {
	if (Msg == BFFM_INITIALIZED) {
		SendMessageW(hWnd, BFFM_SETEXPANDED, 1, lParam);
	}
	return 0;
}

char* osdialog_file_win(osdialog_file_action action, const char* path, const char* filename, osdialog_filters* filters) {
	if (action == OSDIALOG_OPEN_DIR) {
		// open directory dialog
		BROWSEINFOW bInfo;
		ZeroMemory(&bInfo, sizeof(bInfo));
		bInfo.hwndOwner = GetActiveWindow();
		bInfo.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;

		// path
		wchar_t initialDir[MAX_PATH] = L"";
		if (path) {
			// We need to convert the path to a canonical absolute path with GetFullPathNameW()
			wchar_t* pathW = utf8_to_wchar(path);
			GetFullPathNameW(pathW, MAX_PATH, initialDir, NULL);
			free(pathW);
			bInfo.lpfn = browseCallbackProc;
			bInfo.lParam = (LPARAM) initialDir;
		}

		PIDLIST_ABSOLUTE lpItem = SHBrowseForFolderW(&bInfo);
		if (!lpItem) {
			return NULL;
		}
		wchar_t szDir[MAX_PATH] = L"";
		SHGetPathFromIDListW(lpItem, szDir);
		return wchar_to_utf8(szDir);
	}
	else {
		// open or save file dialog
		OPENFILENAMEW ofn;
		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = GetActiveWindow();
		ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		// filename
		wchar_t strFile[MAX_PATH] = L"";
		if (filename) {
			wchar_t* filenameW = utf8_to_wchar(filename);
			snwprintf(strFile, MAX_PATH, L"%S", filenameW);
			free(filenameW);
		}
		ofn.lpstrFile = strFile;
		ofn.nMaxFile = MAX_PATH;

		// path
		wchar_t strInitialDir[MAX_PATH] = L"";
		if (path) {
			// We need to convert the path to a canonical absolute path with GetFullPathNameW()
			wchar_t* pathW = utf8_to_wchar(path);
			GetFullPathNameW(pathW, MAX_PATH, strInitialDir, NULL);
			free(pathW);
			ofn.lpstrInitialDir = strInitialDir;
		}

		// filters
		wchar_t* strFilter = NULL;
		if (filters) {
			char fBuf[4096];
			int fLen = 0;

			for (; filters; filters = filters->next) {
				fLen += snprintf(fBuf + fLen, sizeof(fBuf) - fLen, "%s", filters->name);
				fBuf[fLen++] = '\0';
				for (osdialog_filter_patterns* patterns = filters->patterns; patterns; patterns = patterns->next) {
					fLen += snprintf(fBuf + fLen, sizeof(fBuf) - fLen, "*.%s", patterns->pattern);
					if (patterns->next)
						fLen += snprintf(fBuf + fLen, sizeof(fBuf) - fLen, ";");
				}
				fBuf[fLen++] = '\0';
			}
			fBuf[fLen++] = '\0';

			// Don't use utf8_to_wchar() because this is not a NULL-terminated string.
			strFilter = (wchar_t*)malloc(fLen * sizeof(wchar_t));
			MultiByteToWideChar(CP_UTF8, 0, fBuf, fLen, strFilter, fLen);
			ofn.lpstrFilter = strFilter;
			ofn.nFilterIndex = 1;
		}

		BOOL success;
		if (action == OSDIALOG_OPEN) {
			success = GetOpenFileNameW(&ofn);
		}
		else {
			success = GetSaveFileNameW(&ofn);
		}

		// Clean up
		if (strFilter) {
			free(strFilter);
		}
		if (!success) {
			return NULL;
		}
		return wchar_to_utf8(strFile);
	}
}

#endif