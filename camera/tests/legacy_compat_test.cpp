// SPDX-License-Identifier: Apache-2.0
#include "common/CameraProviderExtension.h"
#include <cassert>

static int32_t value;
static bool on;
// Model an existing device that implements only the original extension API.
bool supportsTorchStrengthControlExt() { return true; }
void setTorchStrengthLevelExt(int32_t level, bool enabled) { value = level; on = enabled; }

int main() {
    assert(supportsTorchStrengthControlForCameraExt("legacy-camera"));
    assert(setTorchStrengthLevelCheckedExt(22, true) == 0 && value == 22 && on);
    assert(setTorchStrengthLevelCheckedExt(1, false) == 0 && value == 1 && !on);
}
