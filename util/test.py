import os
import subprocess
import sys
import time

def test(root_dir):
    start_time = time.time()

    # Execute tests
    for dirpath, dirnames, filenames in os.walk(root_dir):
        for filename in filenames:
            test_file(os.path.join(os.getcwd(), dirpath, filename))

    end_time = time.time()
    total_time = end_time - start_time
    print(f"{round(total_time, 3)} s")

def test_file(filepath):
    print(filepath)
    expected_output = parse_test_file(filepath)
    actual_output = run_test_file(filepath)
    print("expect: ", expected_output)
    print("actual: ", actual_output)
    assert diff_outputs(expected_output, actual_output)

def parse_test_file(filepath):
    output_list = []
    with open(filepath, 'r') as fobj:
        at_output = False
        for line in fobj.readlines():
            print(line, end ="")
            if at_output:
                output_list.append(line[3:])
                continue
            if line == "// === OUTPUT ===\n":
                at_output = True

    print(output_list)
    return "".join(output_list)

def run_test_file(filepath):
    args = ("./nqq", filepath)
    popen = subprocess.Popen(args, stdout=subprocess.PIPE)
    popen.wait()
    return popen.stdout.read()

def diff_outputs(a, b):
    if a == b:
        return True
    return False

if __name__ == "__main__":
    root_dir = "test"
    test(root_dir)

# args = ("bin/bar", "-c", "somefile.xml", "-d", "text.txt", "-r", "aString", "-f", "anotherString")
# #Or just:
# #args = "bin/bar -c somefile.xml -d text.txt -r aString -f anotherString".split()
# popen = subprocess.Popen(args, stdout=subprocess.PIPE)
# popen.wait()
# output = popen.stdout.read()
# print output