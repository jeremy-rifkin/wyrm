import subprocess
import os
import sys
from enum import Enum

class Status(Enum):
    PASS = 1
    FAIL = 2
    UNSUPPORTED = 3

ALIVE_PATH = "/home/rifkin/thirdparty/alive2/build/alive-tv"
# CLANG = "/usr/bin/clang++-15"
CLANG = "/usr/bin/clang++-17"

def test_alive(test_file):
    # test_file.c ---transpiler--> x.ll -\
    # test_file.c -----clang-----> y.ll   ----> alive
    print(f"{os.path.basename(test_file)}")
    p = subprocess.Popen(
        [
            "g++",
            "-O3",
            test_file,
            "-fplugin=./libplugin.so"
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )
    stdout, stderr = p.communicate()
    stdout, stderr = stdout.decode("utf-8"), stderr.decode("utf-8")
    # print(stdout, stderr)
    if "TRANSPILED SUCCESSFULLY" in stdout:
        p = subprocess.Popen(
            [
                CLANG,
                test_file,
                "-Og",
                "-S",
                "-emit-llvm",
                "-o",
                "y.ll"
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        stdout, stderr = p.communicate()
        stdout, stderr = stdout.decode("utf-8"), stderr.decode("utf-8")
        if p.returncode == 0:
            # Remove `target triple`
            with open("y.ll", "r") as f:
                y = "\n".join([line for line in f if not line.startswith("target triple = ")])
            with open("y.ll", "w") as f:
                f.write(y)
            p = subprocess.Popen(
                [
                    ALIVE_PATH,
                    "--bidirectional",
                    "x.ll",
                    "y.ll"
                ],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE
            )
            stdout, stderr = p.communicate()
            stdout, stderr = stdout.decode("utf-8"), stderr.decode("utf-8")
            if "Transformation seems to be correct!" in stdout and "Transformation doesn't verify!" not in stdout and not "ERROR: Couldn't prove the correctness of the transformation" in stdout and not "ERROR: Could not translate" in stdout:
                # print(stdout, stderr)
                # with open("x.ll", "r") as x:
                #     print("Transpiled code:")
                #     print(x.read())
                # with open("y.ll", "r") as y:
                #     print("Clang code:")
                #     print(y.read().replace("\n\n", "\n"))
                print(f"{os.path.basename(test_file)} - Pass")
                return Status.PASS
            else:
                print(stdout, stderr)
                # sys.exit(1)
                with open("x.ll", "r") as x:
                    print("Transpiled code:")
                    print(x.read())
                with open("y.ll", "r") as y:
                    print("Clang code:")
                    print(y.read().replace("\n\n", "\n"))
                print(f"{os.path.basename(test_file)} - Fail")
                sys.exit(1)
                return Status.FAIL
    else:
        print(f"{os.path.basename(test_file)} - Unsupported")
        # print(stdout, stderr)
        return Status.UNSUPPORTED

def test_output(test_file):
    # test_file.c --transpiler--> x.ll -----\
    #                             main.cpp   --clang--> a.out
    print(f"{os.path.basename(test_file)}")
    p = subprocess.Popen(
        [
            "g++",
            "-O3",
            test_file,
            "-fplugin=./libplugin.so"
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )
    stdout, stderr = p.communicate()
    stdout, stderr = stdout.decode("utf-8"), stderr.decode("utf-8")
    with open(test_file, "r") as f:
        lines = [line for line in f]
        decls = [line[len("// DECL: "):].strip() for line in lines if line.startswith("// DECL: ")]
        test_lines = [line[len("// TEST: "):].strip() for line in lines if line.startswith("// TEST: ")]
    # print(stdout, stderr)
    if "TRANSPILED SUCCESSFULLY" in stdout:
        with open("main.cpp", "w") as f:
            f.write(
                "\n".join(
                    [
                        "#include <assert.hpp>",
                        *decls,
                        "int main() {",
                        *map(lambda line: "    " + line, test_lines),
                        "}",
                        ""
                    ]
                )
            )
        p = subprocess.Popen(
            [
                CLANG,
                "main.cpp",
                "-std=c++17",
                "x.ll",
                "-I_deps/assert-src/include",
                "-L_deps/assert-build/",
                "-lassert",
                "-g"
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        stdout, stderr = p.communicate()
        stdout, stderr = stdout.decode("utf-8"), stderr.decode("utf-8")
        if p.returncode == 0:
            p = subprocess.Popen(
                [
                    "./a.out"
                ],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                env={**os.environ.copy(), **{"LD_LIBRARY_PATH": "_deps/assert-build/"}}
            )
            stdout, stderr = p.communicate()
            stdout, stderr = stdout.decode("utf-8"), stderr.decode("utf-8")
            if p.returncode == 0:
                return Status.PASS
            else:
                print(stdout, stderr)
                return Status.FAIL
        else:
            print(stdout, stderr)
            return Status.FAIL
    else:
        print(f"{os.path.basename(test_file)} - Unsupported")
        # print(stdout, stderr)
        return Status.UNSUPPORTED

pass_count, fail_count, unsupported_count = 0, 0, 0

def alive_tests():
    global pass_count, fail_count, unsupported_count
    tests = []
    tests_path = os.path.join(os.path.dirname(os.path.realpath(__file__)), "alive-tests")
    for dirpath, dnames, fnames in os.walk(tests_path):
        for f in fnames:
            abspath = os.path.join(dirpath, f)
            tests.append(abspath)
    tests = sorted(tests, key=lambda path: int(os.path.basename(path).split("_")[0]))
    for file in tests:
        # match test(file):
        #     case Status.PASS:
        #         pass_count += 1
        #     case Status.FAIL:
        #         fail_count += 1
        #     case Status.UNSUPPORTED:
        #         unsupported_count += 1
        r = test_alive(file)
        if r == Status.PASS:
            pass_count += 1
        elif r == Status.FAIL:
            fail_count += 1
        elif r == Status.UNSUPPORTED:
            unsupported_count += 1

def output_tests():
    global pass_count, fail_count, unsupported_count
    tests = []
    tests_path = os.path.join(os.path.dirname(os.path.realpath(__file__)), "output-tests")
    for dirpath, dnames, fnames in os.walk(tests_path):
        for f in fnames:
            abspath = os.path.join(dirpath, f)
            tests.append(abspath)
    tests = sorted(tests, key=lambda path: int(os.path.basename(path).split("_")[0]))
    for file in tests:
        # match test(file):
        #     case Status.PASS:
        #         pass_count += 1
        #     case Status.FAIL:
        #         fail_count += 1
        #     case Status.UNSUPPORTED:
        #         unsupported_count += 1
        r = test_output(file)
        if r == Status.PASS:
            pass_count += 1
        elif r == Status.FAIL:
            fail_count += 1
        elif r == Status.UNSUPPORTED:
            unsupported_count += 1

def main():
    alive_tests()
    output_tests()

    print(f"Passed: {pass_count}")
    print(f"Failed: {fail_count}")
    print(f"Unsupported: {unsupported_count}")

main()
