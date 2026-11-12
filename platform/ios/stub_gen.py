"""Stand-in for the recompiled code (build/gen), for a compile check of the
port without the ROM: every function the runtime names, empty, and an empty
table. The app it makes builds and links but cannot run the game."""
import os
import re
import sys

root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, "build", "gen")
names = set()
for folder in ("src/runtime", "tools"):
    for f in os.listdir(os.path.join(root, folder)):
        if f.endswith((".cpp", ".h", ".py")):
            names |= set(re.findall(r"\bfn_[0-9A-F]{8}\b", open(os.path.join(root, folder, f), errors="ignore").read()))
os.makedirs(out, exist_ok=True)
with open(os.path.join(out, "fns.h"), "w") as fo:
    fo.write('#pragma once\n#include "ppc_rt.h"\n\n')
    for n in sorted(names):
        fo.write(f"void {n}(Ctx& c);\n")
    fo.write("\nstruct FnEntry { uint32_t addr; void (*fn)(Ctx&); };\n")
    fo.write("extern const FnEntry g_fn_table[];\nextern const int g_fn_count;\n")
with open(os.path.join(out, "table.cpp"), "w") as fo:
    fo.write('#include "fns.h"\n\n')
    for n in sorted(names):
        fo.write(f"void {n}(Ctx&) {{}}\n")
    fo.write("const FnEntry g_fn_table[] = {{0, nullptr}};\nconst int g_fn_count = 0;\n")
with open(os.path.join(out, "files.cmake"), "w") as fo:
    fo.write("set(GEN_SOURCES\n    ${GEN_DIR}/table.cpp\n)\n")
print(f"stand-in code: {len(names)} functions in {out}")
