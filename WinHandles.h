#pragma once

#include <windows.h>
#include <utility>

template <class Handle, class Deleter>
class HandleGuard {
public:
    HandleGuard() = default;
    explicit HandleGuard(Handle handle) noexcept : handle_(handle) {}
    ~HandleGuard() { reset(); }

    HandleGuard(HandleGuard&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    HandleGuard& operator=(HandleGuard&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    HandleGuard(const HandleGuard&) = delete;
    HandleGuard& operator=(const HandleGuard&) = delete;

    Handle get() const noexcept { return handle_; }
    explicit operator bool() const noexcept { return handle_ != nullptr; }

    Handle release() noexcept {
        Handle released = handle_;
        handle_ = nullptr;
        return released;
    }

    void reset(Handle handle = nullptr) {
        if (handle_) Deleter{}(handle_);
        handle_ = handle;
    }

private:
    Handle handle_ = nullptr;
};

struct DeleteDc {
    void operator()(HDC dc) const noexcept { DeleteDC(dc); }
};

struct DeleteGdiObject {
    void operator()(HGDIOBJ object) const noexcept { DeleteObject(object); }
};

struct UnhookHook {
    void operator()(HHOOK hook) const noexcept { UnhookWindowsHookEx(hook); }
};

struct DestroyWindowIcon {
    void operator()(HICON icon) const noexcept { DestroyIcon(icon); }
};

using UniqueDC = HandleGuard<HDC, DeleteDc>;
using UniqueBitmap = HandleGuard<HBITMAP, DeleteGdiObject>;
using UniqueHook = HandleGuard<HHOOK, UnhookHook>;
using UniqueIcon = HandleGuard<HICON, DestroyWindowIcon>;

class ScopedScreenDC {
public:
    ScopedScreenDC() noexcept : dc_(GetDC(nullptr)) {}
    explicit ScopedScreenDC(HWND owner) noexcept : owner_(owner), dc_(GetDC(owner)) {}
    ~ScopedScreenDC() {
        if (dc_) ReleaseDC(owner_, dc_);
    }

    ScopedScreenDC(ScopedScreenDC&& other) noexcept : owner_(other.owner_), dc_(other.dc_) {
        other.dc_ = nullptr;
    }

    ScopedScreenDC(const ScopedScreenDC&) = delete;
    ScopedScreenDC& operator=(const ScopedScreenDC&) = delete;

    HDC get() const noexcept { return dc_; }
    explicit operator bool() const noexcept { return dc_ != nullptr; }

private:
    HWND owner_ = nullptr;
    HDC dc_ = nullptr;
};

class ScopedMemoryDC {
public:
    explicit ScopedMemoryDC(HDC reference) noexcept : dc_(CreateCompatibleDC(reference)) {}
    ~ScopedMemoryDC() {
        if (dc_) DeleteDC(dc_);
    }

    ScopedMemoryDC(ScopedMemoryDC&& other) noexcept : dc_(other.dc_) {
        other.dc_ = nullptr;
    }

    ScopedMemoryDC(const ScopedMemoryDC&) = delete;
    ScopedMemoryDC& operator=(const ScopedMemoryDC&) = delete;

    HDC get() const noexcept { return dc_; }
    explicit operator bool() const noexcept { return dc_ != nullptr; }

private:
    HDC dc_ = nullptr;
};

class ScopedSelectedObject {
public:
    ScopedSelectedObject(HDC dc, HGDIOBJ object) noexcept
        : dc_(dc), previous_(SelectObject(dc, object)) {}

    ~ScopedSelectedObject() { restore(); }

    ScopedSelectedObject(const ScopedSelectedObject&) = delete;
    ScopedSelectedObject& operator=(const ScopedSelectedObject&) = delete;

    void restore() noexcept {
        if (previous_ && previous_ != HGDI_ERROR) {
            SelectObject(dc_, previous_);
            previous_ = nullptr;
        }
    }

    HGDIOBJ previous() const noexcept { return previous_; }

private:
    HDC dc_ = nullptr;
    HGDIOBJ previous_ = nullptr;
};

class ScopedClipboard {
public:
    explicit ScopedClipboard(HWND owner) {
        for (int attempt = 0; attempt < 5 && !opened_; ++attempt) {
            opened_ = (OpenClipboard(owner) != FALSE);
            if (!opened_) Sleep(10);
        }
    }

    ~ScopedClipboard() {
        if (opened_) CloseClipboard();
    }

    ScopedClipboard(const ScopedClipboard&) = delete;
    ScopedClipboard& operator=(const ScopedClipboard&) = delete;

    bool ok() const noexcept { return opened_; }

private:
    bool opened_ = false;
};

class ScopedRegistryKey {
public:
    ScopedRegistryKey(HKEY root, LPCWSTR subKey, REGSAM access) noexcept {
        if (RegOpenKeyExW(root, subKey, 0, access, &key_) != ERROR_SUCCESS) key_ = nullptr;
    }

    ~ScopedRegistryKey() {
        if (key_) RegCloseKey(key_);
    }

    ScopedRegistryKey(const ScopedRegistryKey&) = delete;
    ScopedRegistryKey& operator=(const ScopedRegistryKey&) = delete;

    HKEY get() const noexcept { return key_; }
    explicit operator bool() const noexcept { return key_ != nullptr; }

private:
    HKEY key_ = nullptr;
};
