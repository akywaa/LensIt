#include "LensIt.h"
#include "WinHandles.h"

#include <shlobj.h>
#include <knownfolders.h>
#include <filesystem>

bool IsAutoStartEnabled() {
    ScopedRegistryKey key(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", KEY_READ);
    if (!key) return false;

    wchar_t buf[MAX_PATH];
    DWORD bufSize = sizeof(buf);
    const LONG result = RegQueryValueExW(key.get(), L"LensIt", NULL, NULL, reinterpret_cast<LPBYTE>(buf), &bufSize);
    return (result == ERROR_SUCCESS);
}

bool SetAutoStart(bool enable) {
    ScopedRegistryKey key(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", KEY_SET_VALUE);
    if (!key) return false;

    if (enable) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        std::wstring cmd = L"\"" + std::wstring(exePath) + L"\"";
        RegSetValueExW(key.get(), L"LensIt", 0, REG_SZ,
            reinterpret_cast<const BYTE*>(cmd.c_str()),
            static_cast<DWORD>((cmd.length() + 1) * sizeof(wchar_t)));
    }
    else {
        RegDeleteValueW(key.get(), L"LensIt");
    }
    return true;
}

bool CreateDesktopShortcut() {
    PWSTR desktopPath = NULL;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, NULL, &desktopPath))) {
        return false;
    }

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    std::wstring shortcutPath = std::wstring(desktopPath) + L"\\LensIt.lnk";
    CoTaskMemFree(desktopPath);

    IShellLinkW* psl = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void**>(&psl));
    if (SUCCEEDED(hr)) {
        psl->SetPath(exePath);
        psl->SetWorkingDirectory(std::filesystem::path(exePath).parent_path().c_str());
        psl->SetIconLocation(exePath, 0);

        IPersistFile* ppf = nullptr;
        hr = psl->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&ppf));
        if (SUCCEEDED(hr)) {
            ppf->Save(shortcutPath.c_str(), TRUE);
            ppf->Release();
        }
        psl->Release();
        return SUCCEEDED(hr);
    }
    return false;
}
