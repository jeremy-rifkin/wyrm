# Wyrm

![](./res/wyrm.png)

Wyrm is a GCC GIMPLE to LLVM IR transpiler. I built this as a research project.

Big note: This is an experimental project in an experimental state 🙂

### <img align="left" src="https://github.com/jeremy-rifkin/wyrm/raw/main/res/ce.svg" height="30" /> [Try it on Compiler Explorer](https://godbolt.org/z/sxboYrjcE)

Simple usage:
```bash
mkdir transpiler/build
cd transpiler/build
# Any gcc is fine, it just has to match what you use later
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=On -DCMAKE_C_COMPILER=gcc-11 -DCMAKE_CXX_COMPILER=g++-11
ninja
g++-11 ../alive-tests/2_basic_integer_arithmetic.cpp -fplugin=./libplugin.so
# or on a gimple file:
#   gcc-11 -fgimple ../alive-tests/2_basic_integer_arithmetic.cpp -fplugin=./libplugin.so
# A bunch of junk will be printed to stdout and llvm ir will be written to x.ll
```

Example:
```c
int __GIMPLE(ssa)
square(int num) {
  int D_2744;
  int _2;

__BB(2):
  _2 = num_1(D) * num_1(D);
  goto __BB3;

__BB(3):
L0:
  return _2;
}
```
```llvm
define noundef i32 @square(i32 noundef %z0) {
    %z1 = or i32 %z0, 0
    br label %bb2
bb2:
    %z2 = mul nsw i32 %z1, %z1
    br label %bb3
bb3:
    ret i32 %z2
}
```

Some cool things that can be done with a transpiler like this:

- **Alive2-like bounded verification for GCC**: [Alive2][alive] is a bounded verification tool for LLVM IR that's useful
  for verifying transformations and compiler optimizations. This transpiler can allow similar verification for GIMPLE by
  first transpiling to llvm.
  - I didn't know this when I started my project (the repository hadn't been published yet) but Krister Walfridsson has
    been working on a [verification tool][smtgcc] for gcc as well. Maybe this transpiler could be used to help verify
    that verifier.
- **Bounded Verification of C/C++ compilation between GCC and Clang**: This transpiler, along with a tool like alive2,
  could be used to do bounded validations of compilations performed by GCC and Clang, with the possibility of
  discovering bugs and non-conformances.
- **Mixing Compilations**: There is potential to run code through both gcc and clang optimization passes or take
  advantage of LLVM's tooling for a project that relies on gcc.

I have some primitive wrappers for these but haven't committed them here yet.

The transpiler currently handles most GIMPLE types, instructions, and operators but some things aren't implemented yet.
Unsupported features will result in an assertion failure currently.

## Differences between GIMPLE and LLVM IR

Through this project I studied semantic and design differences between GIMPLE and LLVM IR and since I haven't seen these
written down anywhere so I'm documenting here. All of these are to the best of my understanding:

- LLVM has the notion of `undef` and `poison` values in its IR as a means of deferring UB while GCC has no such notion.
- **Types:** LLVM IR uses types such as `i32`, `{i32, f32}`, etc. GIMPLE uses C types, `long long int`,
  `struct S { ... }`, etc.
- **Signedness:** LLVM IR does not have signed or unsigned integers as part of its type system. Instead signed behavior
  and semantics surrounding sign manifest in the instructions integers are used in, e.g.. `sdiv` vs `udiv`, signed vs
  unsigned division respectively, and `add` vs `add nsw`, add vs add with no signed wrap.
- **SSA Variables:** Both LLVM IR and GIMPLE are scopeless. LLVM implicitly declares variables as they are used by
  instructions, e.g. `%add = add i32 %a, %b`, while GIMPLE creates a set of variable declarations at the top of a given
  GIMPLE function.
- **Implicit Conversion:** Because GIMPLE types variable declarations separately, conversions can be done implicitly or
  C-cast style with a NOP assignment in GIMPLE, as opposed to LLVM IR where conversions are explicit with instructions
  such as `sext`, `zext`, `trunc`, etc. Additionally, GIMPLE permits implicit casting in returns where LLVM IR does not.
- **Block Terminators:** Every LLVM IR block has an explicit terminator instruction which transfers control flow to
  another block, returns from a function, or performs some other action. In GIMPLE, control flow can fallthrough from
  one basic block to another.
- **Memory SSA:** GIMPLE has two forms of SSA: Register and memory. Register SSA is the standard SSA almost every major
  compiler uses, memory SSA is used to represent the state of memory and virtual memory phis are used to preserve
  constraints for code motion optimization. I don't fully understand the need or motivation and it seems for the purpose
  of transpiling I don't need to worry about them.
- **Attributes Constraining Values:** Because LLVM allows for `undef` values there is a `noundef` attribute that must be
  used on parameters and return types as appropriate to give the optimizer proper information. This is relevant for
  trying to use Alive2 on transpiled GIMPLE programs that otherwise would not see `undef` inputs or results.
- **Immediate Operands:** LLVM IR allows for immediate operands in instructions, e.g. `add i32 %num, 2`. GIMPLE does not
  and a separate SSA name be created to hold the constant.
- **Copies:** Unlike GIMPLE, LLVM IR does not allow simply assignments of SSA names, e.g. `%c = i32 %a` or `%c = i32 5`.
  Instead, for transpiling, something like `%c = add i32 %a, 0` must be used.
- **The Calling Convention:** LLVM lowers to the calling convention before its IR while GCC only lowers to the calling
  convention when lowering to RTL. A full transpilation would implementing target calling conventions.

The last of these, calling conventions, is one of the bigger roadblocks for a full transpilation. I haven't explored
implementing target calling conventions yet.


[alive]: https://alive2.llvm.org/ce/
[smtgcc]: https://github.com/kristerw/smtgcc
