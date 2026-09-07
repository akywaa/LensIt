#include "LensIt.h"
#include <shlobj.h>
#include <shlwapi.h>
#include <knownfolders.h>

#pragma comment(lib, "shlwapi.lib")

bool IsAutoStartEnabled() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t buf[MAX_PATH];
        DWORD bufSize = sizeof(buf);
        LONG result = RegQueryValueExW(hKey, L"LensIt", NULL, NULL, (LPBYTE)buf, &bufSize);
        RegCloseKey(hKey);
        return (result == ERROR_SUCCESS);
    }
    return false;
}

bool SetAutoStart(bool enable) {
    HKEY hKey;
    const wchar_t* runKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    if (RegOpenKeyExW(HKEY_CURRENT_USER, runKey, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(NULL, exePath, MAX_PATH);
            std::wstring cmd = L"\"" + std::wstring(exePath) + L"\"";
            RegSetValueExW(hKey, L"LensIt", 0, REG_SZ, (const BYTE*)cmd.c_str(), (DWORD)((cmd.length() + 1) * sizeof(wchar_t)));
        }
        else {
            RegDeleteValueW(hKey, L"LensIt");
        }
        RegCloseKey(hKey);
        return true;
    }
    return false;
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
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&psl);
    if (SUCCEEDED(hr)) {
        psl->SetPath(exePath);

        wchar_t dirPath[MAX_PATH];
        wcscpy_s(dirPath, exePath);
        PathRemoveFileSpecW(dirPath);
        psl->SetWorkingDirectory(dirPath);
        psl->SetIconLocation(exePath, 0);

        IPersistFile* ppf = nullptr;
        hr = psl->QueryInterface(IID_IPersistFile, (void**)&ppf);
        if (SUCCEEDED(hr)) {
            ppf->Save(shortcutPath.c_str(), TRUE);
            ppf->Release();
        }
        psl->Release();
        return SUCCEEDED(hr);
    }
    return false;
}

bool SelfInstallIfNeeded() {
    PWSTR localAppData = NULL;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &localAppData))) {
        return false;
    }

    std::wstring targetDir = std::wstring(localAppData) + L"\\LensIt";
    std::wstring targetExe = targetDir + L"\\LensIt.exe";
    CoTaskMemFree(localAppData);

    wchar_t currentExe[MAX_PATH];
    GetModuleFileNameW(NULL, currentExe, MAX_PATH);

    if (_wcsicmp(currentExe, targetExe.c_str()) == 0) {
        return false;
    }

    CreateDirectoryW(targetDir.c_str(), NULL);
    if (CopyFileW(currentExe, targetExe.c_str(), FALSE)) {
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        if (CreateProcessW(targetExe.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, targetDir.c_str(), &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            ExitProcess(0);
        }
    }
    return false;
}