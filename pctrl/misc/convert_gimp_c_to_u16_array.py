#!/usr/bin/env python3
"""
Convert a GIMP-exported C image (RGB565) to a clean uint16_t array.
Handles multiline string concatenation correctly.
"""

import re
import sys
import os

def main():
    if len(sys.argv) != 3:
        print("Usage: python3 gimp_export_rgb565_to_uint16.py input.c output.h")
        sys.exit(1)

    input_path = sys.argv[1]
    output_path = sys.argv[2]

    with open(input_path, 'r') as f:
        content = f.read()

    # Extract header (width, height, bytes_per_pixel)
    header_match = re.search(r'\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,', content)
    if not header_match:
        sys.exit("❌ Impossible de trouver width, height, bytes_per_pixel")

    width, height, bpp = map(int, header_match.groups())
    print(f"📏 Dimensions: {width}×{height}, {bpp} bytes per pixel")

    if bpp != 2:
        print("⚠️ Attention: bytes_per_pixel != 2 (RGB565 attendu)")

    # Find all quoted pixel_data string fragments after the header
    m = re.search(r'pixel_data.*?=\s*(.+)\s*};', content, re.S)
    if not m:
        sys.exit("❌ Impossible de localiser la section pixel_data")

    pixel_section = m.group(1)

    # Collect all quoted substrings between the braces
    fragments = re.findall(r'"([^"]*)"', pixel_section)
    if not fragments:
        sys.exit("❌ Aucun fragment de données trouvé")

    print(f"📦 {len(fragments)} fragments de chaîne détectés")

    # Concatenate and decode all escaped sequences into bytes
    concatenated = ''.join(fragments)
    pixel_bytes = concatenated.encode('utf-8').decode('unicode_escape').encode('latin1')

    expected_size = width * height * bpp
    print(f"📊 Données lues : {len(pixel_bytes)} octets (attendu {expected_size})")
    if len(pixel_bytes) < expected_size:
        print("⚠️ Les données semblent tronquées (fichier incomplet ?).")

    pixels = []
    for i in range(0, len(pixel_bytes), 2):
        val = pixel_bytes[i] | (pixel_bytes[i + 1] << 8)
        pixels.append(val)

    var_name = os.path.splitext(os.path.basename(output_path))[0]

    with open(output_path, 'w') as f:
        f.write('#include <stdint.h>\n\n')
        f.write(f'// Image converted from {os.path.basename(input_path)}\n')
        f.write(f'// Size: {width}x{height}, RGB565\n\n')
        f.write(f'const uint16_t {var_name}[{width * height}] = {{\n')

        for i, val in enumerate(pixels):
            f.write(f'0x{val:04X}, ')
            if (i + 1) % 12 == 0:
                f.write('\n')
        f.write('\n};\n')
        f.write(f'\n// Width: {width}\n// Height: {height}\n')

    print(f"✅ Conversion terminée → {output_path}")

if __name__ == "__main__":
    main()