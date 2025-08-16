#include <Windows.h>
#include <string>
#pragma comment( linker, "/subsystem:windows /entry:mainCRTStartup" )

HWND FindDesktopIconWindow() {
	HWND hDesktop = FindWindow(L"Progman", L"Program Manager");
	if (!hDesktop) return nullptr;

	DWORD_PTR result = 0;
	SendMessageTimeout(hDesktop, 0x052C, 0, 0, SMTO_NORMAL, 1000, &result);

	HWND hWorkerW = nullptr;
	while ((hWorkerW = FindWindowEx(nullptr, hWorkerW, L"WorkerW", nullptr))) {
		HWND hShellView = FindWindowEx(hWorkerW, nullptr, L"SHELLDLL_DefView", nullptr);
		if (hShellView) {
			HWND hListView = FindWindowEx(hShellView, nullptr, L"SysListView32", L"FolderView");
			if (hListView) return hListView;
		}
	}
	return nullptr;
}

bool AreDesktopIconsVisible() {
	HWND hDesktopIcon = FindDesktopIconWindow();
	if (hDesktopIcon) {
		return IsWindowVisible(hDesktopIcon) != 0;
	}
	return false;
}

bool ToggleDesktopIcons(bool show) {
	HWND hDesktopIcon = FindDesktopIconWindow();
	if (hDesktopIcon) {
		ShowWindow(hDesktopIcon, show ? SW_SHOW : SW_HIDE);
		return true;
	}
	return false;
}

bool ToggleDesktopIcons() {
	bool currentState = AreDesktopIconsVisible();
	return ToggleDesktopIcons(!currentState);
}

int main(int argc, char* argv[]) {
	if (argc == 2)
	{
		if (std::string(argv[1]) == "true")
			ToggleDesktopIcons(true);
		else if (std::string(argv[1]) == "false")
			ToggleDesktopIcons(false);
	}
	else if (argc == 1)
		ToggleDesktopIcons();
	return 0;
}