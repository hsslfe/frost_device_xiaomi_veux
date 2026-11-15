// SPDX-License-Identifier: Apache-2.0

#define LOG_TAG "VeuxTorchStrength"

#include "common/CameraProviderExtension.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <log/log.h>
#include <unistd.h>

namespace {
constexpr char kStrengthPath[] =
        "/sys/devices/platform/soc/5c1b000.qcom,cci0/"
        "5c1b000.qcom,cci0:qcom,camera-flash@0/torch_strength";
// Match the kernel's LM36011 register range, capped at the stock HAL current.
constexpr int32_t kDefaultStrength = 44;
constexpr int32_t kMaxStrength = 44;

int openStrength(int flags) {
    int fd;
    do {
        fd = open(kStrengthPath, flags | O_CLOEXEC);
    } while (fd < 0 && errno == EINTR);
    return fd;
}
}  // namespace

bool supportsTorchStrengthControlExt() {
    // An older kernel or missing permissions must not advertise this feature.
    return access(kStrengthPath, R_OK | W_OK) == 0;
}

bool supportsTorchStrengthControlForCameraExt(const char* cameraId) {
    return cameraId != nullptr && std::strcmp(cameraId, "0") == 0 &&
            supportsTorchStrengthControlExt();
}

int32_t getTorchDefaultStrengthLevelExt() {
    return kDefaultStrength;
}

int32_t getTorchMaxStrengthLevelExt() {
    return kMaxStrength;
}

int32_t getTorchStrengthLevelExt() {
    int fd = openStrength(O_RDONLY);
    if (fd < 0) return -errno;

    char buf[32] = {};
    ssize_t count;
    do {
        count = read(fd, buf, sizeof(buf) - 1);
    } while (count < 0 && errno == EINTR);
    int error = errno;
    close(fd);
    if (count < 0) return -error;
    if (count == 0 || count == static_cast<ssize_t>(sizeof(buf) - 1)) return -EIO;

    char* end;
    errno = 0;
    long strength = std::strtol(buf, &end, 10);
    if (errno != 0 || end == buf || strength < 1 || strength > kMaxStrength ||
            (*end != '\0' && std::strcmp(end, "\n") != 0)) {
        return -EIO;
    }
    return static_cast<int32_t>(strength);
}

int32_t setTorchStrengthLevelCheckedExt(int32_t strength, bool enabled) {
    // The HAL has already switched off. No sysfs write or power change is needed.
    if (!enabled) return 0;
    if (strength < 1 || strength > kMaxStrength) return -EINVAL;

    int fd = openStrength(O_WRONLY);
    if (fd < 0) return -errno;

    char buf[16];
    int size = std::snprintf(buf, sizeof(buf), "%d\n", strength);
    ssize_t count;
    do {
        count = write(fd, buf, size);
    } while (count < 0 && errno == EINTR);
    int error = errno;
    close(fd);
    if (count < 0) {
        ALOGE("Torch strength %d failed: %s", strength, std::strerror(error));
        return -error;
    }
    return count == size ? 0 : -EIO;
}

// Keep the original hook for source compatibility with existing callers.
void setTorchStrengthLevelExt(int32_t strength, bool enabled) {
    int32_t result = setTorchStrengthLevelCheckedExt(strength, enabled);
    if (result < 0) ALOGE("Torch strength update failed: %d", result);
}
