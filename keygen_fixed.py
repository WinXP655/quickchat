import sys
import re

def validate_hex_key(hex_str):
    hex_str = hex_str.replace(" ", ""). replace("\n", "")

    if not re.fullmatch(r"^[0-9a-fA-F]+$", hex_str):
        return None, "Invalid characters. Use only 0-9 and a-f/A-F."

    if len(hex_str) % 2 != 0:
        return None, "Odd numbers of characters. Each byte must be represented by 2 hex digits."

    try:
        key_bytes = bytes.fromhex(hex_str)
    except ValueError as e:
        return None, f"Conversion error: {e}"

    return key_bytes, None

def write_key_file(filename, key_bytes):
    key_len = len(key_bytes)
    hex_str = "".join(f"{b:02x}" for b in key_bytes)
    c_array = ", ".join(f"0x{b:02x}" for b in key_bytes)

    with open(filename, "w") as f:
        f.write("// Auto-generated XOR key by keygen_fixed. Do not commit this file!\n")
        f.write("// Run keygen.py to regenerate it.\n\n")
        f.write("#ifndef KEY_H\n")
        f.write("#define KEY_H\n\n")
        f.write(f"#define KEY_LEN {key_len}\n")
        f.write("\n")
        f.write(f"static const unsigned char key[{key_len}] = {{\n {c_array}\n}};\n\n")
        f.write("#endif // KEY_H\n")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python keygen_fixed.py <hex key>")
        print("Example: python keygen_fixed.py 00112233445566778899aabbccddeeff")
        sys.exit(1)

    user_hex = sys.argv[1]
    key_bytes, error = validate_hex_key(user_hex)

    if error is not None:
        print(error)

    write_key_file("key.h", key_bytes)
    print("Done.")