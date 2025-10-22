#!/usr/bin/env python3
import sys
import glob
import os
from PIL import Image

def bmp_to_c_array(filename):
    img = Image.open(filename).convert("RGBA")
    data = img.tobytes()
    array_name = os.path.splitext(os.path.basename(filename))[0]
    lines = []
    for i in range(0, len(data), 12):
        chunk = ", ".join(f"0x{b:02X}" for b in data[i:i + 12])
        lines.append("    " + chunk + ",")
    array_content = "\n".join(lines)
    return (
        f"// Auto-generated from {filename}\n"
        f"#include <switch.h>\n\n"
        f"const u8 {array_name}[] = {{\n"
        f"{array_content}\n"
        f"}};\n"
        f"const unsigned int {array_name}_size = sizeof({array_name});\n"
    )


def convert_all_bmps(pattern):
    for filepath in glob.glob(pattern):
        print(f"File found: {filepath}\n")
        if not filepath.lower().endswith(".bmp"):
            continue
        c_filename = os.path.splitext(filepath)[0] + ".inc"
        with open(c_filename, "w") as f:
            f.write(bmp_to_c_array(filepath))
        print(f"→ {c_filename} generated.")


def main():
    import sys
    if len(sys.argv) < 2:
        print("Usage: python bmp2c.py <pattern>")
    else:
        convert_all_bmps(sys.argv[1])

if __name__ == "__main__":
    main()