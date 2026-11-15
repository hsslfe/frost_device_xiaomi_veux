// SPDX-License-Identifier: Apache-2.0
#include "common/CameraProviderExtension.h"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <unistd.h>

static bool available = true;
static int openError, readError, writeError, opens, closes, writes;
static bool interruptOpen, interruptRead, interruptWrite, shortWrite;
static std::string value = "44\n";

extern "C" int __wrap_access(const char* path, int flags) {
    assert(std::strstr(path, "camera-flash@0/torch_strength"));
    assert(flags == (R_OK | W_OK));
    errno = ENOENT;
    return available ? 0 : -1;
}
extern "C" int __wrap_open(const char* path, int flags, ...) {
    assert(std::strstr(path, "camera-flash@0/torch_strength"));
    assert(flags & O_CLOEXEC);
    if (interruptOpen) { interruptOpen = false; errno = EINTR; return -1; }
    if (openError) { errno = openError; return -1; }
    ++opens;
    return 123;
}
extern "C" int __wrap_close(int fd) {
    assert(fd == 123);
    ++closes;
    return 0;
}
extern "C" ssize_t __wrap_read(int fd, void* buf, size_t count) {
    assert(fd == 123);
    if (interruptRead) { interruptRead = false; errno = EINTR; return -1; }
    if (readError) { errno = readError; return -1; }
    count = count < value.size() ? count : value.size();
    std::memcpy(buf, value.data(), count);
    return count;
}
extern "C" ssize_t __wrap_write(int fd, const void* buf, size_t count) {
    assert(fd == 123);
    if (interruptWrite) { interruptWrite = false; errno = EINTR; return -1; }
    if (writeError) { errno = writeError; return -1; }
    ++writes;
    if (shortWrite) return count - 1;
    value.assign(static_cast<const char*>(buf), count);
    return count;
}

int main() {
    assert(supportsTorchStrengthControlExt());
    assert(supportsTorchStrengthControlForCameraExt("0"));
    assert(!supportsTorchStrengthControlForCameraExt("1"));
    assert(!supportsTorchStrengthControlForCameraExt("2"));
    assert(!supportsTorchStrengthControlForCameraExt(nullptr));
    available = false;
    assert(!supportsTorchStrengthControlExt());
    assert(!supportsTorchStrengthControlForCameraExt("0"));
    available = true;

    assert(getTorchDefaultStrengthLevelExt() == 44);
    assert(getTorchMaxStrengthLevelExt() == 44);
    for (int level : {1, 22, 44}) {
        assert(setTorchStrengthLevelCheckedExt(level, true) == 0);
        assert(getTorchStrengthLevelExt() == level);
    }
    int before = opens;
    for (int level : {-1, 0, 45, 127}) {
        assert(setTorchStrengthLevelCheckedExt(level, true) == -EINVAL);
    }
    assert(setTorchStrengthLevelCheckedExt(44, false) == 0);
    assert(opens == before); // Turning off never reopens CCI/sysfs.

    openError = EACCES;
    assert(setTorchStrengthLevelCheckedExt(10, true) == -EACCES);
    assert(getTorchStrengthLevelExt() == -EACCES);
    openError = 0;
    writeError = EBUSY;
    assert(setTorchStrengthLevelCheckedExt(10, true) == -EBUSY);
    writeError = EIO;
    assert(setTorchStrengthLevelCheckedExt(10, true) == -EIO);
    writeError = 0;
    readError = EIO;
    assert(getTorchStrengthLevelExt() == -EIO);
    readError = 0;
    shortWrite = true;
    assert(setTorchStrengthLevelCheckedExt(10, true) == -EIO);
    shortWrite = false;
    interruptOpen = interruptWrite = true;
    assert(setTorchStrengthLevelCheckedExt(10, true) == 0);
    interruptOpen = interruptRead = true;
    assert(getTorchStrengthLevelExt() == 10);

    for (const char* bad : {"", "0\n", "45\n", "-1\n", "garbage", "1x", "2\n3",
            "999999999999999999999999999999999999"}) {
        value = bad;
        assert(getTorchStrengthLevelExt() == -EIO);
    }
    assert(opens == closes);
    assert(writes > 0);
}
