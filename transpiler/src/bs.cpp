#include <gcc-plugin.h>
#include <tree-pass.h>
#include <context.h>
#include <tree.h>
#include <tree-cfg.h>
#include <gimple.h>
#include <cgraph.h>
#include <stringpool.h>
#include <attribs.h>
#include <value-range.h>
#include <opts.h>
#include <except.h>
#include <gimple-ssa.h>
#include <tree-dfa.h>
#include <tree-ssanames.h>
#include <gimple-iterator.h>
#include <gimple-pretty-print.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#ifndef ASSERT_USE_MAGIC_ENUM
#define ASSERT_USE_MAGIC_ENUM
#endif
#include <assert.hpp>

#include "bs.h"

struct precision_data_pair {
    int32_t precision;
    bool data; // negative, signaling, ...
    bool operator==(const precision_data_pair& other) const = default;
};

template<>
struct std::hash<precision_data_pair> {
    std::size_t operator()(const precision_data_pair& pair) const noexcept {
        return uint64_t(pair.precision) | (uint64_t(pair.data) << 32);
    }
};

std::string stringify_real_cst(const_tree node) {
    // https://github.com/gcc-mirror/gcc/blob/5d2a360f0a541646abb11efdbabc33c6a04de7ee/gcc/print-tree.cc#L60
    ASSERT(!TREE_OVERFLOW(node));
    REAL_VALUE_TYPE d = TREE_REAL_CST(node);
    if(REAL_VALUE_ISINF(d)) {
        static std::unordered_map<precision_data_pair, std::string_view> map = {
            {{16, false}, "0xH7C00"},
            {{16, true}, "0xHFC00"},
            {{32, false}, "0x7FF0000000000000"},
            {{32, true}, "0xFFF0000000000000"},
            {{64, false}, "0x7FF0000000000000"},
            {{64, true}, "0xFFF0000000000000"},
            {{80, false}, "0xK7FFF8000000000000000"},
            {{80, true}, "0xKFFFF8000000000000000"},
            {{128, false}, "0xL00000000000000007FFF000000000000"},
            {{128, true}, "0xL0000000000000000FFFF000000000000"},
        };
        int precision = TYPE_PRECISION(TREE_TYPE(node));
        ASSERT(map.contains({precision, REAL_VALUE_NEGATIVE(d)}), precision, REAL_VALUE_NEGATIVE(d));
        return std::string(map.at({precision, REAL_VALUE_NEGATIVE(d)}));
    }
    if(REAL_VALUE_ISNAN(d)) {
        static std::unordered_map<precision_data_pair, std::string_view> map = {
            {{16, false}, "0xH7E00"},
            {{16, true}, "0xH7F00"},
            {{32, false}, "0x7FF8000000000000"},
            {{32, true}, "0x7FF4000000000000"},
            {{64, false}, "0x7FF8000000000000"},
            {{64, true}, "0x7FFC000000000000"},
            {{80, false}, "0xK7FFFC000000000000000"},
            {{80, true}, "0xK7FFFE000000000000000"},
            {{128, false}, "0xL00000000000000007FFF800000000000"},
            {{128, true}, "0xL00000000000000007FFFC00000000000"},
        };
        int precision = TYPE_PRECISION(TREE_TYPE(node));
        ASSERT(map.contains({precision, !!d.signalling}), precision, !!d.signalling);
        return std::string(map.at({precision, !!d.signalling}));
    }
    char string[64];
    // real_to_hexadecimal(string, &d, sizeof(string), 0, 1);
    real_to_decimal(string, &d, sizeof(string), 0, 1);
    return string;
}
