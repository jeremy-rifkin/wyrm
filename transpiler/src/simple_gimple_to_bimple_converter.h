#ifndef SIMPLE_GIMPLE_TO_BIMPLE_CONVERTER
#define SIMPLE_GIMPLE_TO_BIMPLE_CONVERTER

#include <tree.h>

#include <memory>

#include "bimple.h"

class simple_gimple_to_bimple_converter {
    class impl;
    std::unique_ptr<impl> pimpl;
public:
    simple_gimple_to_bimple_converter();
    ~simple_gimple_to_bimple_converter();
    bimple::function generate_function(function* fun);
};

#endif
