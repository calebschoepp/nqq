def header():
    return "/* === START OUTPUT ===\n"

def footer():
    return "=== STOP OUTPUT === */\n"

MAX = 500

with open("global.nqq", 'w') as f:
    print("Building globals")
    # Code section
    for i in range(MAX):
        f.write(f"var _{i} = {i};\n")
        f.write(f"print _{i};\n")
        f.write(f"_{i} = {i + 1};\n")
        f.write(f"print _{i};\n")
    f.write("\n")
    
    # Output section
    f.write(header())
    for i in range(MAX):
        f.write(f"{i}\n")
        f.write(f"{i + 1}\n")
    f.write(footer())

with open("local.nqq", 'w') as f:
    print("Building locals")
    # Code section
    f.write("{\n")
    for i in range(MAX):
        f.write(f"    var _{i} = {i};\n")
        f.write(f"    print _{i};\n")
        f.write(f"    _{i} = {i + 1};\n")
        f.write(f"    print _{i};\n")
    f.write("}")
    f.write("\n")
    
    # Output section
    f.write(header())
    for i in range(MAX):
        f.write(f"{i}\n")
        f.write(f"{i + 1}\n")
    f.write(footer())
