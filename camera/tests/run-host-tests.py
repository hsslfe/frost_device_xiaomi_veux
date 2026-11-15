#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Compile the real extension and extracted driver/control functions with host shims.

This checks control flow and error handling, not the Android/kernel ABI or hardware.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile


def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--framework", type=Path, required=True)
    parser.add_argument("--kernel", type=Path, required=True)
    args = parser.parse_args()
    tests = Path(__file__).resolve().parent
    service = args.framework.resolve() / "services/camera/libcameraservice"
    kernel = args.kernel.resolve() / "techpack/camera/drivers/cam_sensor_module/cam_flash"
    common = ["-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined", "-g"]
    with tempfile.TemporaryDirectory(prefix="veux-torch-") as directory:
        out = Path(directory)
        (out / "log").mkdir()
        (out / "log/log.h").write_text(
            '#include <cstdio>\n#define ALOGE(...) do { std::fprintf(stderr, __VA_ARGS__); '
            'std::fputc(\'\\n\', stderr); } while (false)\n')
        extension = out / "extension_test"
        subprocess.run(["g++", "-std=c++17", *common, "-I" + str(out),
                        "-I" + str(service), '-D__unused=__attribute__((unused))',
                        str(tests.parent / "CameraProviderExtension.cpp"),
                        str(service / "common/CameraProviderExtension.cpp"),
                        str(tests / "extension_test.cpp"),
                        "-Wl,--wrap=open,--wrap=read,--wrap=write,--wrap=close,--wrap=access",
                        "-o", str(extension)], check=True)
        subprocess.run([str(extension)], check=True)
        print("PASS: real extension, weak symbol overrides, syscall errors and level bounds")

        legacy = out / "legacy_compat_test"
        subprocess.run(["g++", "-std=c++17", *common, "-I" + str(service),
                        '-D__unused=__attribute__((unused))',
                        str(service / "common/CameraProviderExtension.cpp"),
                        str(tests / "legacy_compat_test.cpp"), "-o", str(legacy)], check=True)
        subprocess.run([str(legacy)], check=True)
        print("PASS: original extension API remains compatible through the new weak hooks")

        source = (kernel / "cam_flash_dev.c").read_text()
        start = source.index("#define VEUX_TORCH_SID")
        end = source.index("static DEVICE_ATTR", start)
        template = (tests / "kernel_test.c.in").read_text()
        (out / "kernel_test.c").write_text(
            template.replace("// FUNCTIONS_UNDER_TEST", source[start:end]))
        binary = out / "kernel_test"
        subprocess.run(["gcc", "-std=gnu11", *common, "-Wno-unused-parameter",
                        str(out / "kernel_test.c"), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
        print("PASS: real kernel handlers, mutex coverage, active-mode/SID guards and CCI errors")

        source = (service / "common/CameraProviderManager.cpp").read_text()
        names = ["status_t CameraProviderManager::getTorchStrengthLevel(",
                 "status_t CameraProviderManager::turnOnTorchWithStrengthLevel(",
                 "bool CameraProviderManager::shouldSkipTorchStrengthUpdate(",
                 "status_t CameraProviderManager::setTorchMode(",
                 "status_t CameraProviderManager::setTorchModeLocked(",
                 "status_t CameraProviderManager::ProviderInfo::DeviceInfo3::fixupTorchStrengthTags("]
        template = (tests / "manager_test.cpp.in").read_text()
        (out / "manager_test.cpp").write_text(template.replace(
            "// FUNCTIONS_UNDER_TEST", "\n\n".join(function(source, name) for name in names)))
        binary = out / "manager_test"
        subprocess.run(["g++", "-std=c++17", *common, "-pthread",
                        str(out / "manager_test.cpp"), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
        print("PASS: real manager methods, HAL/sysfs error propagation, resets and metadata keys")


if __name__ == "__main__":
    main()
