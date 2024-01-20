// gcc plugin stuff
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
#include <plugin-version.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <new>
#include <stack>
#include <string_view>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifndef ASSERT_USE_MAGIC_ENUM
#define ASSERT_USE_MAGIC_ENUM
#endif
#include <assert.hpp>
#include <fmt/core.h>
#include <fmt/ranges.h>

#include "bs.h"
#include "bimple.h"
#include "utils.h"
#include "simple_gimple_to_bimple_converter.h"
#include "llvm_codegen.h"

using namespace std::string_literals;

// assertion for gcc
int plugin_is_GPL_compatible;

// learned how to create a pass from https://stackoverflow.com/questions/25626124/how-to-register-a-gimple-pass
static const struct pass_data llvm_transpilation_pass_data = {
                .type                   = GIMPLE_PASS,
                .name                   = "llvm_transpilation",
                .optinfo_flags          = OPTGROUP_NONE,
                // .has_gate               = false,
                // .has_execute            = true,
                .tv_id                  = TV_NONE,
                .properties_required    = PROP_gimple_any,
                .properties_provided    = 0,
                .properties_destroyed   = 0,
                .todo_flags_start       = 0,
                .todo_flags_finish      = 0
};

class llvm_transpilation_pass : public gimple_opt_pass {
public:
    llvm_transpilation_pass(gcc::context* ctx) : gimple_opt_pass(llvm_transpilation_pass_data, ctx) {}
    // put the function you want to execute when the pass is executed
    // unsigned int execute() {return 0;}
    unsigned int execute(function* fun) {
        ASSERT(!fun->static_chain_decl);
        // std::vector<int> postorder(last_basic_block_for_fn(fun));
        printf("============= Execute function =============\n");
        print_current_pass(stdout);
        printf("%s:\n", get_name(fun->decl));
        // print_gimple_seq(stdout, fun->gimple_body, 4, TDF_ALL_VALUES);
        // debug_gimple_seq(fun->gimple_body);
        //debug_bb(fun->cfg->x_entry_block_ptr);
        // printf("============= ================ =============\n");
        // debug_function(fun->decl, TDF_ALL_VALUES);
        // printf("============= ================ =============\n");
        // custom_dump_function_to_file(fun->decl, stdout, TDF_ALL_VALUES);
        dump_function_to_file(fun->decl, stdout, TDF_ALL_VALUES);
        // int nof_blocks = post_order_compute(postorder.data(), true, true);
        // for(int i = nof_blocks - 1; i >= 0; i--) {
        //     basic_block gcc_bb = (*fun->cfg->x_basic_block_info)[postorder[i]];
        //     //gccbb2bb[gcc_bb] = func->build_bb();
        //     debug_bb(gcc_bb);
        // }

        printf("Converting:\n");

        bimple::function function = simple_gimple_to_bimple_converter().generate_function(fun);
        std::cout<<function.to_string(true)<<std::endl;
        std::string x = llvm_codegen{}.generate(function);
        std::cout<<x;
        std::ofstream f("x.ll", std::ios_base::app);
        f<<x;
        f.close();
        printf("TRANSPILED SUCCESSFULLY\n");

        printf("============================================\n");
        return 0;
    }
};

int plugin_init(struct plugin_name_args* plugin_info, struct plugin_gcc_version* version) {
    // We check the current gcc loading this plugin against the gcc we used to
    // created this plugin
    if(!plugin_default_version_check (version, &gcc_version)) {
        std::cerr << "This GCC plugin is for version " << GCCPLUGIN_VERSION_MAJOR << "." << GCCPLUGIN_VERSION_MINOR << "\n";
        return 1;
    }

    // Let's print all the information given to this plugin!

    std::cerr << "Plugin info\n";
    std::cerr << "===========\n\n";
    std::cerr << "Base name: " << plugin_info->base_name << "\n";
    std::cerr << "Full name: " << plugin_info->full_name << "\n";
    std::cerr << "Number of arguments of this plugin:" << plugin_info->argc << "\n";

    for(int i = 0; i < plugin_info->argc; i++) {
        std::cerr << "Argument " << i << ": Key: " << plugin_info->argv[i].key << ". Value: " << plugin_info->argv[i].value << "\n";
    }

    std::cerr << "\n";
    std::cerr << "Version info\n";
    std::cerr << "============\n\n";
    std::cerr << "Base version: " << version->basever << "\n";
    std::cerr << "Date stamp: " << version->datestamp << "\n";
    std::cerr << "Dev phase: " << version->devphase << "\n";
    std::cerr << "Revision: " << version->devphase << "\n";
    std::cerr << "Configuration arguments: " << version->configuration_arguments << "\n";
    std::cerr << "\n";

    std::cerr << "Plugin successfully initialized\n";

    const char* const plugin_name = plugin_info->base_name;

    struct register_pass_info llvm_transpilation_info;
    llvm_transpilation_info.pass                         = new llvm_transpilation_pass(g);
    llvm_transpilation_info.reference_pass_name          = "ssa" ;
    llvm_transpilation_info.ref_pass_instance_number     = 1;
    llvm_transpilation_info.pos_op                       = PASS_POS_INSERT_AFTER;
    register_callback(
        plugin_name,
        PLUGIN_PASS_MANAGER_SETUP,
        NULL,
        &llvm_transpilation_info
    );

    std::remove("x.ll");

    return 0;
}
