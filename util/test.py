import os
import subprocess
import sys
import time

# TODO fix bug where a test fails and test runner hangs waiting on process
# TODO add support for error templating
# TODO fix bug where errors show up before output diff
# TODO add a whole boat load more tests

class TestRunner():
    """
    TestRunner handles finding all of the test suites, executing them, and
    displaying/summarizing their results.
    """
    def __init__(self, test_dir, verbose):
        self.test_dir = test_dir
        self.verbose = verbose
        self.suites = {}

    def run(self):
        self._test()
        self._summarize()


    def _test(self):
        for dirpath, dirnames, filenames in os.walk(self.test_dir):
            # Is not a leaf of the directory tree
            if len(filenames) == 0:
                continue

            # Run the test suite and record the summary to display later
            suite = TestSuite(dirpath, filenames)
            suite.run()
            self.suites[dirpath] = suite

            if self.verbose:
                for out in suite.all_outputs:
                    print(out, end='')
            else:
                for out in suite.failure_outputs:
                    print(out,end='')

    def _summarize(self):
        total_failures = 0
        total_tests = 0
        total_time = 0.0
        print("=== SUMMARY ===")
        for k, v in self.suites.items():
            total_failures += v.failure_count
            total_tests += v.test_count
            total_time += v.run_time
            suite_summary = []
            if v.failure_count == 0:
                suite_summary.append(ColorPrinter.okgreen("PASS"))
            else:
                suite_summary.append(ColorPrinter.fail("FAIL"))
            suite_summary.append(k)
            suite_summary.append(f"({v.run_time:2.3f} s)")

            print("  ".join(suite_summary))

        print()
        runner_summary = []
        if total_failures == 0:
            runner_summary.append(ColorPrinter.okgreen("PASS"))
        else:
            runner_summary.append(ColorPrinter.fail("FAIL"))
        runner_summary.append(f"{total_tests - total_failures} of {total_tests} tests passed")
        runner_summary.append(f"({total_time:2.3f} s)")

        print("  ".join(runner_summary))

class TestSuite():
    """
    TestSuite encompasses the logic to test the multiple test files found in a
    test suite (a leaf directory). It generates the ouput of the tests but
    does not put it to stdout, it just stores it.
    """
    def __init__(self, dirpath, filenames):
        self._dirpath = dirpath
        self._filenames = filenames
        self.all_outputs = []
        self.failure_outputs = []
        self.run_time = 0.0
        self.test_count = 0
        self.failure_count = 0

    def run(self):
        start_time = time.time()

        for filename in self._filenames:
            self._test(filename)

        end_time = time.time()
        self.run_time = end_time - start_time

    def _test(self, filename):
        if self._file_excludable(filename):
            return
        self.test_count += 1
        test = Test(os.path.join(self._dirpath, filename))
        test.execute()
        if test.failed:
            self.failure_outputs.append(test.output)
            self.failure_count += 1
        self.all_outputs.append(test.output)

    def _file_excludable(self, filename):
        if filename.startswith("_"):
            return True
        return False

class Test():
    """
    Test parses, executes, and evaluates the success of a test given by a
    filename.
    """
    def __init__(self, filename):
        self.filename = filename
        self.failed = True
        self.output = ""
        self.expected = ""
        self.actual = ""

    def execute(self):
        self.parse_expected()
        self.get_actual()
        self.diff()

    def parse_expected(self):
        output_list = []
        with open(self.filename, 'r') as f:
            at_output = False
            for line in f.readlines():
                if line == "/* === START OUTPUT ===\n":
                    at_output = True
                    continue
                elif line in ["=== STOP OUTPUT === */\n", "=== STOP OUTPUT === */"]:
                    break
                elif at_output:
                    output_list.append(line)

        self.expected = "".join(output_list)

    def get_actual(self):
        args = ("./nqq", self.filename)
        popen = subprocess.Popen(args, stdout=subprocess.PIPE)
        popen.wait()
        self.actual = popen.stdout.read().decode("utf-8")

    def diff(self):
        if self.actual == self.expected:
            self.failed = False
            self.build_pass_output()
        else:
            self.failed = True
            self.build_fail_output()

    def build_pass_output(self):
        self.output = f"{ColorPrinter.okgreen('PASS')}  {self.filename}\n"

    def build_fail_output(self):
        temp_output = []
        temp_output.append(f"{ColorPrinter.fail('FAIL')}  {self.filename}\n")
        temp_output.append(ColorPrinter.warning("<<<<<<< Expected Output\n"))
        temp_output.append(ColorPrinter.warning(self.expected))
        temp_output.append(ColorPrinter.warning("=======\n"))
        temp_output.append(ColorPrinter.warning(self.actual))
        temp_output.append(ColorPrinter.warning(">>>>>>> Actual Output\n"))

        self.output = ColorPrinter.warning("".join(temp_output))

class ColorPrinter():
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

    @classmethod
    def fail(cls, s):
        return cls.FAIL + s + cls.ENDC

    @classmethod
    def warning(cls, s):
        return cls.WARNING + s + cls.ENDC

    @classmethod
    def okgreen(cls, s):
        return cls.OKGREEN + s + cls.ENDC

if __name__ == "__main__":
    # Check that current working directory is correct
    if os.path.basename(os.getcwd()) != "nqq":
        print("End to end tests must be run from root directory.")
        exit()

    verbose = False
    if len(sys.argv) == 2 and sys.argv[1] in ['-v', '--verbose']:
        verbose = True

    test_dir = os.path.join('test', 'e2e')

    runner = TestRunner(test_dir, verbose)
    runner.run()