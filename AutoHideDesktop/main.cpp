#include <windows.h>
#include <shellapi.h>
#include <vector>
#include <atomic>
#include "resource.h"

#pragma comment(linker, "/subsystem:windows /entry:WinMainCRTStartup")

// ==== 全局变量 ====
HINSTANCE g_hInst;
NOTIFYICONDATA nid = {};
std::atomic<bool> g_running(true);
HWND g_hwndTray = nullptr;

enum State { UNKNOWN, MOVING, STOPPED };
volatile State g_previousState = UNKNOWN;

std::atomic<bool> g_inDesktop(false);
std::atomic<DWORD64> g_lastMoveTime(0);
std::atomic<bool> g_paused(false);

UINT m_uMsgTaskbarRestart = 0;
// ==== 工具函数 ====
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

bool SetDesktopIconsVisibility(bool show) {
    HWND hDesktopIcon = FindDesktopIconWindow();
    if (hDesktopIcon) {
        ShowWindow(hDesktopIcon, show ? SW_SHOW : SW_HIDE);
        return true;
    }
    return false;
}

void FindTaskbarWindows(std::vector<HWND>& taskbars) {
    HWND hTaskbar = nullptr;
    while ((hTaskbar = FindWindowEx(nullptr, hTaskbar, L"Shell_TrayWnd", nullptr)) != nullptr) {
        taskbars.push_back(hTaskbar);
    }
}

void SetTaskbarVisibility(bool show) {
    std::vector<HWND> taskbars;
    FindTaskbarWindows(taskbars);

    for (HWND hTaskbar : taskbars) {
        BOOL isVisible = IsWindowVisible(hTaskbar);

        if (show) {
            if (!isVisible) {
                ShowWindow(hTaskbar, SW_SHOW);
                SetWindowPos(hTaskbar, HWND_TOPMOST, 0, 0, 0, 0,
                    SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
                SetWindowPos(hTaskbar, HWND_NOTOPMOST, 0, 0, 0, 0,
                    SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
                SetForegroundWindow(hTaskbar);
            }
        }
        else {
            if (isVisible) {
                ShowWindow(hTaskbar, SW_HIDE);
                SetWindowPos(hTaskbar, HWND_BOTTOM, 0, 0, 0, 0,
                    SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
            }
        }
    }
}


bool IsCursorOnDesktop() {
    POINT pt;
    if (!GetCursorPos(&pt)) return false;

    HWND hwnd = WindowFromPoint(pt);
    if (!hwnd) return false;

    HWND hParent = hwnd;
    while (GetParent(hParent)) {
        hParent = GetParent(hParent);
    }

    char className[256] = { 0 };
    GetClassNameA(hParent, className, sizeof(className));

    return strcmp(className, "WorkerW") == 0 ||
        strcmp(className, "Progman") == 0;
}

// ==== 鼠标钩子 ====
LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && (wParam == WM_MOUSEMOVE || wParam == WM_NCMOUSEMOVE)) {
        bool onDesktop = IsCursorOnDesktop();
        DWORD64 currentTime = GetTickCount64();

        if (onDesktop) {
            g_lastMoveTime.store(currentTime);
            g_inDesktop.store(true);
        }
        else {
            g_inDesktop.store(false);
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

// ==== 监控线程 ====
DWORD WINAPI MonitorThread(LPVOID) {
    static bool isStopMove = true;

    while (g_running.load()) {
        Sleep(100);
        if (g_paused.load())
        {
            continue;
        }
        else {
            if (g_inDesktop.load()) {
                DWORD64 currentTime = GetTickCount64();
                bool isMoving = (currentTime - g_lastMoveTime.load() <= 1000);

                State newState = isMoving ? MOVING : STOPPED;
                if (newState != g_previousState) {
                    g_previousState = newState;

                    if (newState == MOVING) {
                        if (isStopMove) {
                            SetDesktopIconsVisibility(true);
                            SetTaskbarVisibility(true);
                            isStopMove = false;
                        }
                    }
                    else {
                        SetDesktopIconsVisibility(false);
                        SetTaskbarVisibility(false);
                        isStopMove = true;
                    }
                }
            }
            else if (g_previousState != UNKNOWN) {
                g_previousState = UNKNOWN;
            }
        }
    }
    return 0;
}

// ==== 托盘消息处理 ====
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_PAUSE 1001
#define ID_TRAY_EXIT  1002

void ShowTrayMenu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    bool paused = g_paused.load();
    InsertMenu(hMenu, -1, MF_BYPOSITION, ID_TRAY_PAUSE, paused ? L"恢复" : L"暂停");
    InsertMenu(hMenu, -1, MF_BYPOSITION, ID_TRAY_EXIT, L"退出");

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == m_uMsgTaskbarRestart)
    {
		Shell_NotifyIcon(NIM_ADD, &nid);//如果资源管理器崩溃了，重新添加托盘图标
        SetTaskbarVisibility(true);
    }
    switch (msg) {
    case WM_CREATE:
        m_uMsgTaskbarRestart = RegisterWindowMessage(L"TaskbarCreated");//注册资源管理器崩溃消息
        break;
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            ShowTrayMenu(hwnd);
		}
		else if (lParam == WM_LBUTTONDBLCLK) {
            g_paused.store(!g_paused.load());
		}
        break;
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_TRAY_PAUSE) {
            g_paused.store(!g_paused.load());
        }
        else if (LOWORD(wParam) == ID_TRAY_EXIT) {
            // 退出前恢复
            SetDesktopIconsVisibility(true);
            SetTaskbarVisibility(true);
            PostQuitMessage(0);
        }
        break;
    case WM_DESTROY:
        Shell_NotifyIcon(NIM_DELETE, &nid);
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ==== 主函数 ====
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    HANDLE hObject = CreateMutex(NULL, FALSE, L"AutoHideDesktop");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(hObject);
        return 1;
    }
    g_hInst = hInstance;

    // 注册窗口类
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"AutoHideDesktop";
    RegisterClass(&wc);

    // 创建隐藏窗口
    g_hwndTray = CreateWindow(L"AutoHideDesktop", L"", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, hInstance, NULL);

    // 添加托盘图标
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_hwndTray;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
    lstrcpy(nid.szTip, L"AutoHideDesktop");
    Shell_NotifyIcon(NIM_ADD, &nid);

    // 设置鼠标钩子
    HHOOK hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, hInstance, 0);

    // 创建监控线程
    HANDLE hThread = CreateThread(NULL, 0, MonitorThread, NULL, 0, NULL);

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 清理
    g_running.store(false);
    if (hMouseHook) UnhookWindowsHookEx(hMouseHook);
    if (hThread) {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
    }

    return 0;
}

