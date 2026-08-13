#!/usr/bin/env python3
"""Regenerate .vscode/c_cpp_properties.json from build/compile_commands.json.

Why this exists: the Pico SDK passes most of its headers via `-isystem`, and
`pico/platform.h` is architecture-split (host / rp2040 / rp2350). A recursive
`**` include glob can't disambiguate the three copies, so header IntelliSense
(headers aren't in compile_commands.json) fails with
"cannot open source file pico/platform.h". This script pulls the EXACT include
dirs the compiler uses (both -I and -isystem) from the compile DB, so IntelliSense
resolves headers the same way the real build does.

Run this after any CMake reconfigure that changes include dirs, then reload the
VS Code window. Requires build/compile_commands.json to exist (configure first).
"""
import json, re, os

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
db_path = os.path.join(root, "build", "compile_commands.json")
db = json.load(open(db_path))

def is_main(e):
    f = e["file"].replace("\\", "/")
    return f.endswith("src/main.cpp") and "weather/src" in f

cmd = next(e["command"] for e in db if is_main(e)).replace("\\", "/")

dirs = re.findall(r"(?:-I|-isystem)\s*([A-Za-z]:/\S+)", cmd)
out, seen = [], set()
for d in dirs:
    d = d.rstrip("/")
    d = d.replace(root.replace("\\", "/"), "${workspaceFolder}")
    d = d.replace("C:/Users/Jared", "${userHome}")  # adjust if your home dir differs
    if d not in seen:
        seen.add(d)
        out.append(d)

config = {
    "configurations": [{
        "name": "Pico",
        "compileCommands": "${workspaceFolder}/build/compile_commands.json",
        "includePath": out,
        # Applied to headers (which aren't in the compile DB) so IntelliSense
        # enables the same features as the build — e.g. __fp16 via the format flag.
        "compilerArgs": ["-mfp16-format=ieee"],
        "forcedInclude": [
            "${workspaceFolder}/build/generated/pico_base/pico/config_autogen.h"
        ],
        "defines": [
            "PICO_RP2350=1",
            "PICO_BOARD=\"pico2_w\"",
            "ARM_MATH_CM33",
            "__ARM_FEATURE_DSP=1"
        ],
        "compilerPath": "${userHome}/.pico-sdk/toolchain/14_2_Rel1/bin/arm-none-eabi-gcc.exe",
        "cStandard": "c11",
        "cppStandard": "c++20",
        "intelliSenseMode": "linux-gcc-arm"
    }],
    "version": 4
}

dest = os.path.join(root, ".vscode", "c_cpp_properties.json")
with open(dest, "w", newline="\n") as f:
    json.dump(config, f, indent=4)
    f.write("\n")

print(f"wrote {dest} with {len(out)} include dirs")
