#ifndef MSMS_FILE_IDENTITY_H
#define MSMS_FILE_IDENTITY_H

#include <cerrno>
#include <cstdint>

#include <wx/string.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace msms {

// Snapshot of a destination path's underlying file identity. A successful
// snapshot can represent either an existing file or a confirmed absence.
// Identity intentionally excludes size and timestamps: editing the contents
// of the same file in place does not change which file the user consented to
// replace. stat()/GetFileInformationByHandle follow symlinks to their target.
struct FileIdentity {
    bool valid{false};
    bool exists{false};
    std::uint64_t device{0};    // st_dev or dwVolumeSerialNumber
    std::uint64_t fileIndex{0}; // st_ino or nFileIndex

    bool operator==(const FileIdentity& other) const noexcept {
        if (!valid || !other.valid || exists != other.exists) {
            return false;
        }
        return !exists || (device == other.device && fileIndex == other.fileIndex);
    }

    bool operator!=(const FileIdentity& other) const noexcept {
        return !(*this == other);
    }
};

inline FileIdentity CaptureFileIdentity(const wxString& path) {
    FileIdentity identity;
#ifdef _WIN32
    const wchar_t* widePath = path.wc_str();
    HANDLE handle = ::CreateFileW(
        widePath, FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION info{};
        if (::GetFileInformationByHandle(handle, &info)) {
            identity.valid = true;
            identity.exists = true;
            identity.device = static_cast<std::uint64_t>(info.dwVolumeSerialNumber);
            identity.fileIndex =
                (static_cast<std::uint64_t>(info.nFileIndexHigh) << 32) |
                static_cast<std::uint64_t>(info.nFileIndexLow);
        }
        ::CloseHandle(handle);
        return identity;
    }

    const DWORD error = ::GetLastError();
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
        identity.valid = true;
    }
#else
    const wxScopedCharBuffer utf8Path = path.ToUTF8();
    if (!utf8Path.data()) {
        return identity;
    }
    struct stat info {};
    if (::stat(utf8Path.data(), &info) == 0) {
        identity.valid = true;
        identity.exists = true;
        identity.device = static_cast<std::uint64_t>(info.st_dev);
        identity.fileIndex = static_cast<std::uint64_t>(info.st_ino);
        return identity;
    }

    if (errno == ENOENT || errno == ENOTDIR) {
        identity.valid = true;
    }
#endif
    // For access errors or other failures the identity is unknown, not absent.
    // Callers must fail closed instead of bypassing an overwrite warning.
    return identity;
}

} // namespace msms

#endif // MSMS_FILE_IDENTITY_H
