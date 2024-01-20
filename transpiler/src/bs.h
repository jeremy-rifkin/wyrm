#ifndef BS_H
#define BS_H

// warning: #include order is important because gcc didn't setup the headers right
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
#include <dumpfile.h>

#include <stdio.h>
#include <string>

void custom_dump_function_to_file(tree fndecl, FILE *file, dump_flags_t flags);

std::string stringify_real_cst(const_tree node);

#endif
