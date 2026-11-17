# Veux torch strength (Indian variant target)

This connects Android's `CameraManager.turnOnTorchWithStrengthLevel()` API to
the rear CCI flash through Lunaris's camera provider extension. Camera ID `0`
advertises 44 levels, with default and maximum both 44. These are LM36011
register codes, not percentages. Code 44 (`0x2c`) is the reported stock HAL
torch setting; this implementation adjusts downward without increasing it.

The phone has not been available for testing. The common Veux device tree
includes Indian POCO/Redmi models; the hardware assumption still needs checking
on the intended handset. Only the rear flash node is opted in. The kernel
rejects writes if the HAL selects a CCI slave other than `0x64`, or if the
flash is not already running in torch mode. Other flash ICs need their own
verified register mapping before enabling strength control.

## Integrate all three parts

Base revisions used:

| Component | Repository / branch | Base commit |
| --- | --- | --- |
| Device | `hsslfe/frost_device_xiaomi_veux`, `16.2` | `b33752878de8a508cd09f662ff021321792b55e1` |
| Kernel | `hsslfe/Frost_kernel_xiaomi_sm6375`, `main` | `01c3670e53716c5a8ae9c484c68decf13487e245` |
| Framework | `hsslfe/luna_frameworks_av`, `16.2` (matches Lunaris) | `ce0c8f71f46b5fa91ef152252706dfcbcc55ffb5` |

Use the `codex` branch from all three repositories in the ROM manifest:

- `hsslfe/frost_device_xiaomi_veux` at `device/xiaomi/veux`
- `hsslfe/Frost_kernel_xiaomi_sm6375` at `kernel/xiaomi/sm6375`
- `hsslfe/luna_frameworks_av` at `frameworks/av`

The framework fork's `codex` branch already contains the required framework
change. From the Android source root, verify that it is present:

```sh
bash device/xiaomi/veux/tools/apply-torch-strength.sh --check "$PWD"
```

If using the unmodified Lunaris framework instead, apply the bundled fallback
with `bash device/xiaomi/veux/tools/apply-torch-strength.sh "$PWD"`.
The script preserves local edits, checks the entire patch before applying it,
and detects an already applied patch. It does not reset or commit anything.
Keep the framework change in the ROM manifest/fork so repo sync retains it.
No `frameworks/base`, camera blob, property,
overlay or VINTF change is required. The supplied Lunaris 16.2 SystemUI already
has `FlashlightDialogDelegate` and enables its slider when the reported maximum
level is greater than 1. Its controller uses the same CameraManager strength API.

The build setting is `libcameraservice.ext_lib`, matching the supplied Lunaris
branch. It links `//device/xiaomi/veux:libcameraservice_torch_veux` into the
system camera service. This is a system static library, not a vendor HAL.

After selecting your usual Veux lunch target:

```sh
m libcameraservice selinux_policy bootimage dtboimage
```

Then perform your normal complete ROM build. Device permissions and policy
must be shipped with the framework and kernel; flashing just a new kernel
does not enable the Android feature.

## Behavior and permissions

The HAL continues to own power, mode, CCI initialization and flash capture.
The new attribute only reads/writes the torch current under `flash_mutex`.
It creates no persistent override and never rewrites the HAL's register lists.
Off reads return Android's default level; the next normal enable starts at
the default. Calling normal enable on an already lit torch preserves its level.

Only `cameraserver` receives DAC write access and an SELinux allow rule for the
new attribute. Apps, SystemUI and `system_server` need no direct sysfs access.
SELinux enforcement and neverallow checks remain enabled. Ueventd handles
attribute creation/rebind; the boot action also sets ownership to
`cameraserver:cameraserver` and mode `0660`.

The framework checks HAL and sysfs errors, holds the provider for the strength
path, scopes metadata and control to camera `0`, and publishes synthesized
characteristic keys. It requests HAL off if applying brightness fails and
returns the error. An off rollback failure is logged and the provider remains
held. Existing extensions retain their original hooks through weak defaults.

## Verification

Run host checks with Python 3, GCC and G++:

```sh
python3 device/xiaomi/veux/camera/tests/run-host-tests.py \
  --framework frameworks/av --kernel kernel/xiaomi/sm6375
```

The tests compile the real extension, its weak hook compatibility, and the real
driver/manager methods extracted into host ABI shims. AddressSanitizer and
UndefinedBehaviorSanitizer check boundary values, interrupted/short IO, missing
nodes, permission and CCI errors, wrong slave IDs, inactive torch sessions,
locking, HAL failures, failed rollback, metadata and off/on resets. These shims
do not replace Android or kernel builds.

Completed in the implementation workspace:

- Host tests above passed.
- `cam_flash_dev.o` compiled against the real `veux_defconfig` kernel headers
  using AArch64 GCC 14.2. The ROM's production Clang/LTO build was not run.
- `vendor/xiaomi/veux.dtb` compiled with the kernel's DTC.
- Patch whitespace and framework apply/reapply/conflict checks passed.

A full ROM/SELinux binary build, Camera CTS and phone tests have not been run.

Before release, test with SELinux enforcing and a Camera2 test app holding
camera permission:

1. Check camera `0` reports `FLASH_INFO_STRENGTH_MAXIMUM_LEVEL == 44` and
   `FLASH_INFO_STRENGTH_DEFAULT_LEVEL == 44`, including in `getKeys()`.
   The front camera must not acquire these 44-level capabilities.
2. Request levels 1, 22 and 44. Confirm the light actually changes and
   `getTorchStrengthLevel("0")` returns the requested level.
3. Turn off with `setTorchMode("0", false)`. The getter must return 44.
   Turn on normally; it must use 44. Repeating ordinary on after selecting
   22 must preserve 22. Check torch and strength callbacks.
4. Verify levels 0 and 45 raise `IllegalArgumentException` without lighting
   the torch. Repeat short off/on cycles and open/close MIUI Camera and GCam.
5. Verify photo flash, video torch, camera takeover and torch recovery still
   work. Inspect logs for `avc: denied`, CCI timeouts and `VeuxTorchStrength`.
6. On userdebug builds, inspect the attribute's owner/mode/SELinux label:
   `cameraserver:cameraserver`, `0660`, `vendor_sysfs_torch_strength`.

Register/path research references:

- [Veux flash session fixes](https://github.com/acerhizmq/kernel_xiaomi_veux/commit/6d91504dbb2f31f2834581b9bfd1b7450aba316f)
- [Veux extension and reported stock setting](https://github.com/acerhizmq/device_xiaomi_veux/commit/7cee00b9becd0ff3c3436b744df228d8c9850f81)

This implementation avoids the power changes and persistent overrides in older
experiments and retains the existing camera blobs and camera app setup.
