import random
import sys

KEY_LEN = 32

def generate_key(length=KEY_LEN):
    return [random.randint(0x00, 0xFF) for _ in range(length)]

def write_key_file(filename="key.h", key=None):
    if key is None:
        key = generate_key()

    with open(filename, "w") as f:
        f.write("// Auto-generated XOR key. Do not commit this file!\n")
        f.write("// Run keygen.py to regenerate it.\n\n")
        f.write("#ifndef KEY_H\n")
        f.write("#define KEY_H\n\n")
        f.write("#define KEY_LEN {}\n".format(len(key)))
        f.write("\n")
        f.write("static const unsigned char key[{}] = {{\n    ".format(len(key)))
        f.write(", ".join(f"0x{b:02x}" for b in key))
        f.write("\n};\n\n")
        f.write("#endif // KEY_H\n")

if __name__ == "__main__":
    length = KEY_LEN
    if len(sys.argv) > 1:
        try:
            length = int(sys.argv[1])
        except ValueError:
            print("Invalid length, using default 32.")

    print(f"Generating XOR key (length: {length})...")
    key = generate_key(length)
    write_key_file("key.h", key)

    hex_str = "".join(f"{b:02x}" for b in key)
    print(f"Key: {hex_str}")

    c_array = ", ".join(f"0x{b:02x}" for b in key)
    print(f"C array: {c_array}")