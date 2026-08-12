#!/usr/bin/env python3
"""Compile and run host tests against exact platform-neutral source seams."""

from pathlib import Path
import os
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = ROOT / ".host-build"

COMMON_INCLUDES = [
    "SOZO-Common/test",
    "SOZO-Common/lib/SozoBusCore/src",
    "SOZO-Common/lib/SozoDomain/src",
    "SOZO-Common/lib/SozoLightingCore/src",
    "SOZO-Common/lib/SozoNodeProtocol/src",
    "SOZO-Common/lib/SozoSceneCore/src",
    "SOZO-Common/lib/SozoVersion/src",
    "SOZO-Common/lib/SpatialLightCore/src",
]

COMMON_SOURCES = [
    "SOZO-Common/lib/SozoBusCore/src/SozoBus.cpp",
    "SOZO-Common/lib/SozoDomain/src/SozoDomain.cpp",
    "SOZO-Common/lib/SozoNodeProtocol/src/SozoNodeMessages.cpp",
    "SOZO-Common/lib/SozoNodeProtocol/src/SozoNodeProtocol.cpp",
    "SOZO-Common/lib/SozoSceneCore/src/LightNodeRuntime.cpp",
    "SOZO-Common/lib/SozoSceneCore/src/LightingScene.cpp",
    "SOZO-Common/lib/SpatialLightCore/src/SpatialLightCore.cpp",
]

TARGETS = {
    "node-name-policy": {
        "includes": ["SOZO-ESP32-S3/lib/SozoSettings/src"],
        "sources": [
            "SOZO-ESP32-S3/test/test_node_name_policy/test_main.cpp",
            "SOZO-ESP32-S3/lib/SozoSettings/src/NodeNamePolicy.cpp",
        ],
    },
    "gateway-node": {
        "includes": [
            "SOZO-ESP32-S3/lib/SozoNode/src",
            "SOZO-ESP32-S3/lib/SozoTransport/src",
        ],
        "sources": [
            "SOZO-ESP32-S3/test/test_sozo_node/test_main.cpp",
            "SOZO-ESP32-S3/lib/SozoNode/src/NodeCoordinator.cpp",
            "SOZO-ESP32-S3/lib/SozoNode/src/NodeFirmwareTransfer.cpp",
            "SOZO-ESP32-S3/lib/SozoNode/src/NodeFleetCoordinator.cpp",
            "SOZO-ESP32-S3/lib/SozoNode/src/NodeRegistry.cpp",
            "SOZO-ESP32-S3/lib/SozoNode/src/SceneMessageMapper.cpp",
            "SOZO-ESP32-S3/lib/SozoTransport/src/BleLinkStateMachine.cpp",
            "SOZO-ESP32-S3/lib/SozoTransport/src/BleOperationSupervisor.cpp",
            "SOZO-ESP32-S3/lib/SozoTransport/src/BleOutboundMailbox.cpp",
        ],
    },
    "c3-node": {
        "includes": ["SOZO-ESP32-C3/lib/SozoC3Node/src"],
        "sources": [
            "SOZO-ESP32-C3/test/test_node_runtime/test_main.cpp",
            "SOZO-ESP32-C3/lib/SozoC3Node/src/C3NodeApplication.cpp",
            "SOZO-ESP32-C3/lib/SozoC3Node/src/NodeSceneRuntime.cpp",
            "SOZO-ESP32-C3/lib/SozoC3Node/src/NodeFirmwareReceiver.cpp",
            "SOZO-ESP32-C3/lib/SozoC3Node/src/PairingWindow.cpp",
        ],
    },
}


def main() -> int:
    if len(sys.argv) != 2 or sys.argv[1] not in TARGETS:
        choices = ", ".join(sorted(TARGETS))
        print(f"usage: {Path(sys.argv[0]).name} <{choices}>", file=sys.stderr)
        return 2

    target_name = sys.argv[1]
    target = TARGETS[target_name]
    compiler = os.environ.get("CXX", "g++")
    if shutil.which(compiler) is None:
        print(f"C++ compiler not found: {compiler}", file=sys.stderr)
        return 2

    BUILD_DIR.mkdir(exist_ok=True)
    executable = BUILD_DIR / (target_name + (".exe" if os.name == "nt" else ""))
    includes = [*COMMON_INCLUDES, *target["includes"]]
    sources = [*COMMON_SOURCES, *target["sources"]]
    command = [
        compiler,
        "-std=c++17",
        "-Wall",
        "-Wextra",
        "-pedantic",
        *(f"-I{ROOT / path}" for path in includes),
        *(str(ROOT / path) for path in sources),
        "-o",
        str(executable),
    ]

    print(f"Compiling {target_name} native test...")
    subprocess.run(command, cwd=ROOT, check=True)
    print(f"Running {target_name} native test...")
    subprocess.run([str(executable)], cwd=ROOT, check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
