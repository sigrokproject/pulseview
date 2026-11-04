#!/usr/bin/env python3
"""Optional Linux regression; reuse a built pulseview-test (Unix Makefiles)."""
from pathlib import Path
import hashlib
import json
import os
import shlex
import subprocess
import sys

if len(sys.argv) != 3 or sys.platform != "linux":
    raise SystemExit("Usage on Linux: check-store-thread.py BUILD_DIR NEW_OUTPUT_DIR")

root = Path(__file__).resolve().parents[1]
build = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
out.mkdir()  # Keep every log and result; never overwrite an earlier run.
cwd = build / "test"
target = cwd / "CMakeFiles/pulseview-test.dir"
flags = (target / "flags.make").read_text().splitlines()
link = shlex.split((target / "link.txt").read_text())
compile_command = [link[0]]
for key in ["CXX_DEFINES", "CXX_INCLUDES", "CXX_FLAGS"]:
    compile_command += shlex.split(next(
        row.split(" = ", 1)[1] for row in flags if row.startswith(key + " = ")))
compile_command += ["-c", str(root / "contrib/check-store-thread.cpp"),
                    "-o", str(out / "check.o")]

command = []
objects = {}
for arg in link:
    if arg.startswith("-Wl,--dependency-file="):
        continue
    if arg.endswith(".o"):
        if "/__/pv/" not in arg and "mocs_compilation.cpp.o" not in arg:
            continue
        arg = str((cwd / arg).resolve())
        objects[arg] = hashlib.sha256(Path(arg).read_bytes()).hexdigest()
    command.append(arg)
command.insert(1, str(out / "check.o"))
command[command.index("-o") + 1] = str(out / "check-store-thread")
command.append("-ldl")
proof = {"compile": compile_command, "link": command, "objects": objects}
(out / "build.json").write_text(json.dumps(proof, indent=2) + "\n")
for label, args in [("compile", compile_command), ("link", command)]:
    with (out / (label + ".log")).open("wb") as log:
        subprocess.run(args, cwd=cwd, stdout=log, stderr=subprocess.STDOUT, check=True)
env = os.environ.copy()
env["QT_QPA_PLATFORM"] = "offscreen"
with (out / "stdout").open("wb") as stdout, (out / "stderr").open("wb") as stderr:
    result = subprocess.run([str(out / "check-store-thread"), str(out / "results")],
                            env=env, stdout=stdout, stderr=stderr, timeout=90)
(out / "result.json").write_text(json.dumps({"exit": result.returncode}) + "\n")
print((out / "stdout").read_text(), end="")
raise SystemExit(result.returncode)
