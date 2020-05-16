MAX = 500

with open("global.nqq", 'w') as f:
    print("Building globals")
    # Code section
    for i in range(MAX):
        f.write(f"let _{i} = {i};\n")
        f.write(f"print _{i}; // expect: {i}\n")
        f.write(f"_{i} = {i + 1};\n")
        f.write(f"print _{i}; // expect: {i + 1}\n")
    f.write("\n")

with open("local.nqq", 'w') as f:
    print("Building locals")
    # Code section
    f.write("{\n")
    for i in range(MAX):
        f.write(f"    let _{i} = {i};\n")
        f.write(f"    print _{i}; // expect: {i}\n")
        f.write(f"    _{i} = {i + 1};\n")
        f.write(f"    print _{i}; // expect: {i + 1}\n")
    f.write("}")
    f.write("\n")
