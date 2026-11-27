#include "NativeTheme.h"

#include <FL/Fl_Window.H>

#ifdef _WIN32

#include <FL/x.H>
#include <windows.h>

#ifdef _MSC_VER
#pragma comment(lib, "advapi32.lib")
#endif

namespace NativeTheme {

bool system_prefers_dark() {
    HKEY key = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0,
                      KEY_READ, &key) != ERROR_SUCCESS)
        return false;
    DWORD value = 1, size = sizeof(value), type = 0;
    const LONG rc = RegQueryValueExA(key, "AppsUseLightTheme", nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size);
    RegCloseKey(key);
    return rc == ERROR_SUCCESS && type == REG_DWORD && value == 0;   // 0 = apps use the dark theme
}

namespace {

typedef HRESULT(WINAPI* DwmSetWindowAttributeFn)(HWND, DWORD, LPCVOID, DWORD);

// dwmapi.dll is loaded on demand, so older Windows versions still start fine.
bool set_dark_title_bar(HWND hwnd, bool dark) {
    static HMODULE dwm = LoadLibraryA("dwmapi.dll");
    static DwmSetWindowAttributeFn set_attribute =
        dwm ? reinterpret_cast<DwmSetWindowAttributeFn>(reinterpret_cast<void*>(GetProcAddress(dwm, "DwmSetWindowAttribute")))
            : nullptr;
    if (!set_attribute) return false;
    const BOOL value = dark ? TRUE : FALSE;
    // 20 = DWMWA_USE_IMMERSIVE_DARK_MODE (Windows 10 build 18985 and later, Windows 11);
    // 19 is the same switch in the earlier Windows 10 2004 builds.
    if (SUCCEEDED(set_attribute(hwnd, 20, &value, sizeof(value)))) return true;
    return SUCCEEDED(set_attribute(hwnd, 19, &value, sizeof(value)));
}

}  // namespace

bool preference_changed() {
    static int last = -1;
    const int now = system_prefers_dark() ? 1 : 0;
    if (now == last) return false;
    last = now;
    return true;
}

void apply(Fl_Window* window) {
    if (!window || !window->shown()) return;
    HWND hwnd = fl_xid(window);
    if (hwnd) set_dark_title_bar(hwnd, system_prefers_dark());
}

}  // namespace NativeTheme

#else  // not Windows: nothing to do

namespace NativeTheme {
bool system_prefers_dark() { return false; }
bool preference_changed() { return false; }
void apply(Fl_Window*) {}
}  // namespace NativeTheme

#endif
