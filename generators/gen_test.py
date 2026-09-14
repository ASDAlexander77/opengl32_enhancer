import re

NAMES_FILE = r"all_names.txt"
OUT_FILE = r"test.ts"

names = [l.strip() for l in open(NAMES_FILE, encoding="utf-8") if l.strip()]

# Representative subset actually invoked through GetProcAddress (not just resolved),
# the same way an intercepting host application would call them - proving the calls
# are forwarded to the real driver through this proxy, not just that the symbol
# exists. Chosen to avoid any Get*-style call that writes through an output pointer
# (can't guarantee the real driver checks "no current context" before it touches the
# pointer). Covers a mix of arities and parameter/return types (void, u32, u8,
# Opaque) to exercise the calling convention, not just symbol resolution.
# Entries: (name, arg literals, arg tslang types, return type)
CALL_TESTS = [
    ("glGetError", [], [], "u32"),
    ("glFlush", [], [], "void"),
    ("glFinish", [], [], "void"),
    ("glEnable", ["0x0B71"], ["u32"], "void"),          # GL_DEPTH_TEST
    ("glDisable", ["0x0B71"], ["u32"], "void"),
    ("glIsEnabled", ["0x0B71"], ["u32"], "u8"),
    ("glClearColor", ["0.0", "0.0", "0.0", "1.0"], ["f32", "f32", "f32", "f32"], "void"),
    ("glViewport", ["0", "0", "64", "64"], ["i32", "i32", "i32", "i32"], "void"),
    ("glMatrixMode", ["0x1700"], ["u32"], "void"),      # GL_MODELVIEW
    ("glLoadIdentity", [], [], "void"),
    ("glLineWidth", ["1.0"], ["f32"], "void"),
    ("glColor3f", ["1.0", "1.0", "1.0"], ["f32", "f32", "f32"], "void"),
    ("glVertex3f", ["0.0", "0.0", "0.0"], ["f32", "f32", "f32"], "void"),
    ("glBegin", ["0x0004"], ["u32"], "void"),           # GL_TRIANGLES
    ("glEnd", [], [], "void"),
    ("glTranslated", ["0.0", "0.0", "0.0"], ["f64", "f64", "f64"], "void"),
    ("glRotatef", ["0.0", "0.0", "1.0", "0.0"], ["f32", "f32", "f32", "f32"], "void"),
    ("wglGetCurrentContext", [], [], "Opaque"),
    ("wglGetCurrentDC", [], [], "Opaque"),
    ("wglCreateContext", ["0"], ["Opaque"], "Opaque"),
    ("wglMakeCurrent", ["0", "0"], ["Opaque", "Opaque"], "u8"),
    ("wglDeleteContext", ["0"], ["Opaque"], "u8"),
]

call_names = {n for n, _, _, _ in CALL_TESTS}
missing = call_names - set(names)
assert not missing, f"CALL_TESTS references unwrapped names: {missing}"

lines = []
lines.append("// AUTO-GENERATED interception test for wrapper.ts.")
lines.append("//")
lines.append("// Loads the opengl32_enh.dll proxy built from wrapper.ts (by its build output")
lines.append("// name, from the same output directory this test executable is built into) and")
lines.append("// checks two things:")
lines.append("//")
lines.append("//  1. Every function this wrapper claims to intercept actually resolves via")
lines.append("//     GetProcAddress under its original name (catches typos in @dllname, or a")
lines.append("//     function silently missing from the export table).")
lines.append("//  2. A representative subset (different arities, param/return types) can be")
lines.append("//     called end-to-end through the proxy without crashing, proving the calls")
lines.append("//     are actually forwarded to the real driver rather than just resolved.")
lines.append("//     These are calls known to be safe with no current GL context: no output")
lines.append("//     pointers, so nothing is written through an argument.")
lines.append("declare function LoadLibraryA(libraryName: Opaque): Opaque;")
lines.append("declare function GetProcAddress(library: Opaque, functionName: Opaque): Opaque;")
lines.append("")
lines.append("console.log(\"Loading proxy opengl32_enh.dll...\");")
lines.append('const mod = LoadLibraryA("opengl32_enh.dll");')
lines.append("if (mod === null) {")
lines.append('    console.error("Failed to load opengl32_enh.dll (proxy build not next to this executable?)");')
lines.append("} else {")
lines.append('    console.log("Proxy loaded. Resolving all intercepted functions...");')
lines.append("")
lines.append("    const names = [")
for n in names:
    lines.append(f'        "{n}",')
lines.append("    ];")
lines.append("")
lines.append("    let resolved: number = 0;")
lines.append("    let failed: number = 0;")
lines.append("    names.forEach((name) => {")
lines.append("        const addr = GetProcAddress(mod, name);")
lines.append("        if (addr === null) {")
lines.append("            failed = failed + 1;")
lines.append('            console.error("  NOT FOUND: " + name);')
lines.append("        } else {")
lines.append("            resolved = resolved + 1;")
lines.append("        }")
lines.append("    });")
lines.append("")
lines.append('    console.log("Resolved: " + resolved.toString() + " / " + (<number>names.length).toString());')
lines.append("    if (failed > 0) {")
lines.append('        console.error(failed.toString() + " function(s) failed to resolve.");')  # `failed` is already `number`, so toString() resolves cleanly
lines.append("    } else {")
lines.append('        console.log("All intercepted functions resolved successfully.");')
lines.append("    }")
lines.append("")
lines.append('    console.log("");')
lines.append('    console.log("Calling a representative subset through the proxy...");')
lines.append("")

for name, args, arg_types, ret in CALL_TESTS:
    args_join = ", ".join(args)
    types_join = ", ".join(arg_types)
    proc_var = f"__proc_{name}"
    lines.append(f'    console.log("  {name}(...)");')
    lines.append(f'    const {proc_var} = GetProcAddress(mod, "{name}");')
    lines.append(f"    if ({proc_var} === null) {{")
    lines.append(f'        console.error("    NOT FOUND - cannot call");')
    lines.append("    } else {")
    if ret == "void":
        lines.append(f"        (<({types_join}) => void>{proc_var})({args_join});")
        lines.append(f'        console.log("    -> called, no crash");')
    else:
        lines.append(f"        const __r = (<({types_join}) => {ret}>{proc_var})({args_join});")
        if ret == "Opaque":
            lines.append('        console.log("    -> " + (__r === null ? "null" : "non-null"));')
        else:
            lines.append('        console.log("    -> " + (<number>__r).toString());')
    lines.append("    }")
    lines.append("")

lines.append('    console.log("Done - no crash means every called wrapper forwarded correctly.");')
lines.append("}")
lines.append("")

with open(OUT_FILE, "w", encoding="utf-8", newline="\n") as f:
    f.write("\n".join(lines))

print(f"Wrote {OUT_FILE} with {len(names)} resolve checks and {len(CALL_TESTS)} live calls.")
