import os
import subprocess
import sys
import time

def test(root_dir):
    start_time = time.time()

    # Execute tests
    for dirpath, dirnames, filenames in os.walk(root_dir):
        for filename in filenames:
            if filename[0] == "_":
                continue
            test_file(os.path.join(os.getcwd(), dirpath, filename))

    end_time = time.time()
    total_time = end_time - start_time
    print(f"{round(total_time, 3)} s")

def test_file(filepath):
    print(filepath) # TODO remove
    expected_output = parse_test_file(filepath)
    actual_output = run_test_file(filepath)
    if not diff_outputs(expected_output, actual_output):
        print("FAILURE")

def parse_test_file(filepath):
    output_list = []
    with open(filepath, 'r') as fobj:
        at_output = False
        for line in fobj.readlines():
            if line == "/* === START OUTPUT ===\n":
                at_output = True
                continue
            elif line == "=== STOP OUTPUT === */\n" or line == "=== STOP OUTPUT === */":
                break
            elif at_output:
                output_list.append(line)

    return "".join(output_list)

def run_test_file(filepath):
    args = ("./nqq", filepath)
    popen = subprocess.Popen(args, stdout=subprocess.PIPE)
    popen.wait()
    return popen.stdout.read().decode("utf-8")

def diff_outputs(a, b):
    if a == b:
        return True
    return False

if __name__ == "__main__":
    root_dir = "test"
    test(root_dir)