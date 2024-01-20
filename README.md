# Wyrm

![](./res/wyrm.png)

Wyrm is a GCC GIMPLE to LLVM IR transpiler. I built this as a research project hoping to learn more about the design
decisions made by the two compiler projects and explore how complicated the transpilation process would be between the
two IRs.

Simple usage:
```bash
mkdir transpiler/build
cd transpiler/build
# Any gcc is fine, it just has to match what you use later
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=On -DCMAKE_C_COMPILER=gcc-11 -DCMAKE_CXX_COMPILER=g++-11
ninja
g++-11 ../alive-tests/2_basic_integer_arithmetic.cpp -fplugin=./libplugin.so
```
