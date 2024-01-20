#ifndef LLVM_CODEGEN
#define LLVM_CODEGEN

#include <memory>
#include <string>

#include "bimple.h"

class llvm_codegen {
    class impl;
    std::unique_ptr<impl> pimpl;
public:
    llvm_codegen();
    ~llvm_codegen();
    std::string generate(const bimple::function& fn);
};

#endif
