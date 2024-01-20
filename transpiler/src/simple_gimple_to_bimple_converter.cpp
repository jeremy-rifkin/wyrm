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

using namespace std::string_literals;

// done for magic enum reasons
// copied from https://chromium.googlesource.com/chromiumos/third_party/gcc/+/refs/heads/factory-rambi-6420.B/gcc/gimple.h#71
enum class gimple_rhs_class_class {
    GIMPLE_INVALID_RHS,	/* The expression cannot be used on the RHS.  */
    GIMPLE_TERNARY_RHS,	/* The expression is a ternary operation.  */
    GIMPLE_BINARY_RHS,	/* The expression is a binary operation.  */
    GIMPLE_UNARY_RHS,	/* The expression is a unary operation.  */
    GIMPLE_SINGLE_RHS	/* The expression is a single object (an SSA
                            name, a _DECL, a _REF, etc.  */
};

// done for magic enum reasons
enum class gimple_code_class {
#define DEFGSCODE(SYM, STRING, STRUCT)	SYM,
#include "gimple.def"
#undef DEFGSCODE
    LAST_AND_UNUSED_GIMPLE_CODE
};

class simple_gimple_to_bimple_converter::impl {
public:
    std::size_t type_size(tree type) {
        return TREE_INT_CST_LOW(TYPE_SIZE(type));
    }

    std::unique_ptr<bimple::type> generate_type(tree type) {
        switch(TREE_CODE(type)) {
            case VOID_TYPE:
                return std::make_unique<bimple::void_type>();
            case INTEGER_TYPE:
                return std::make_unique<bimple::integer>(unsigned(TYPE_PRECISION(type)), !!TYPE_UNSIGNED(type), type_size(type));
            case BOOLEAN_TYPE:
                // TODO
                return std::make_unique<bimple::integer>(unsigned(TYPE_PRECISION(type)), !!TYPE_UNSIGNED(type), type_size(type));
                // return std::make_unique<bimple::boolean>(type_size(type));
            case REAL_TYPE:
                return std::make_unique<bimple::real>(unsigned(TYPE_PRECISION(type)), type_size(type));
            case POINTER_TYPE:
                return std::make_unique<bimple::pointer>(generate_type(TREE_TYPE(type)), type_size(type));
            case FUNCTION_TYPE:
                {
                    auto return_type = generate_type(TREE_TYPE(type));
                    std::vector<std::unique_ptr<bimple::type>> args;
                    tree arg = DECL_ARGUMENTS(type);
                    while(arg) {
                        args.push_back(generate_type(TREE_TYPE(arg)));
                        arg = DECL_CHAIN(arg);
                    }
                    return std::make_unique<bimple::function_type>(
                        std::move(return_type),
                        std::move(args)
                    );
                }
            default:
                VERIFY(
                    false,
                    "Unhandled node",
                    get_tree_code_name(TREE_CODE(type))
                );
                __builtin_unreachable();
        }
    }

    std::string get_identifier_value(tree node) {
        switch(TREE_CODE(node)) {
            case IDENTIFIER_NODE:
                return gcc_str(DECL_NAME(node)->identifier.id.str);
            case SSA_NAME:
                if(SSA_NAME_IDENTIFIER(node) != NULL_TREE) {
                    // auto y = SSA_NAME_VERSION(node);
                    // auto x = IDENTIFIER_POINTER(SSA_NAME_VAR(node));
                    // auto z = SSA_NAME_VAR(node);
                    return IDENTIFIER_POINTER(SSA_NAME_IDENTIFIER(node)) + "_"s + std::to_string(SSA_NAME_VERSION(node));
                } else {
                    return "_" + std::to_string(SSA_NAME_VERSION(node));
                }
            default:
                VERIFY(
                    false,
                    "Unhandled node",
                    get_tree_code_name(TREE_CODE(node))
                );
                __builtin_unreachable();
        }
    }

    std::string get_referenced_value(tree node) {
        switch(TREE_CODE(node)) {
            case FUNCTION_DECL:
                // TODO Probably wrong
                return gcc_str(DECL_ASSEMBLER_NAME(node)->identifier.id.str);
            default:
                VERIFY(
                    false,
                    "Unhandled node",
                    get_tree_code_name(TREE_CODE(node))
                );
                __builtin_unreachable();
        }
    }

    std::unique_ptr<bimple::atom> generate_atom(tree node) {
        switch(TREE_CODE(node)) {
            case IDENTIFIER_NODE:
            case SSA_NAME:
                return std::make_unique<bimple::variable>(
                    get_identifier_value(node),
                    generate_type(TREE_TYPE(node))
                );
            case INTEGER_CST:
                return std::make_unique<bimple::integer_constant>(
                    TREE_INT_CST_LOW(node),
                    generate_type(TREE_TYPE(node))
                );
            case REAL_CST:
                return std::make_unique<bimple::real_constant>(
                    stringify_real_cst(node),
                    generate_type(TREE_TYPE(node))
                );
            case MEM_REF:
                return std::make_unique<bimple::mem_ref>(
                    generate_atom(TREE_OPERAND(node, 0)),
                    generate_atom(TREE_OPERAND(node, 1)),
                    generate_type(TREE_TYPE(node))
                );
            case ADDR_EXPR:
                // seems to be used for referencing storage or global items, e.g. function names
                return std::make_unique<bimple::addr_expr>(
                    get_referenced_value(TREE_OPERAND(node, 0)),
                    generate_type(TREE_TYPE(node))
                );
            default:
                VERIFY(
                    false,
                    "Unhandled node",
                    get_tree_code_name(TREE_CODE(node))
                );
                __builtin_unreachable();
        }
    }

    std::unique_ptr<bimple::binary_assignment> generate_binary_assignment(gassign* statement) {
        tree_code code = gimple_assign_rhs_code(statement);
        tree lhs = gimple_assign_lhs(statement);
        tree rhs1 = gimple_assign_rhs1(statement);
        tree rhs2 = gimple_assign_rhs2(statement);
        bimple::operators op;
        switch(code) {
            case PLUS_EXPR:
                op = bimple::operators::add;
                break;
            case MINUS_EXPR:
                op = bimple::operators::sub;
                break;
            case MULT_EXPR:
                op = bimple::operators::mul;
                break;
            case POINTER_PLUS_EXPR:
                op = bimple::operators::pointer_add;
                break;
            case TRUNC_DIV_EXPR:
                op = bimple::operators::trunc_div;
                break;
            case TRUNC_MOD_EXPR:
                op = bimple::operators::trunc_mod;
                break;
            case RDIV_EXPR:
                op = bimple::operators::rdiv;
                break;
            case BIT_AND_EXPR:
                op = bimple::operators::bit_and;
                break;
            case BIT_IOR_EXPR:
                op = bimple::operators::bit_or;
                break;
            case BIT_XOR_EXPR:
                op = bimple::operators::bit_xor;
                break;
            case LSHIFT_EXPR:
                op = bimple::operators::lshift;
                break;
            case RSHIFT_EXPR:
                op = bimple::operators::rshift;
                break;
            case LT_EXPR:
                op = bimple::operators::lt;
                break;
            case GT_EXPR:
                op = bimple::operators::gt;
                break;
            case LE_EXPR:
                op = bimple::operators::lteq;
                break;
            case GE_EXPR:
                op = bimple::operators::gteq;
                break;
            case EQ_EXPR:
                op = bimple::operators::eq;
                break;
            case NE_EXPR:
                op = bimple::operators::neq;
                break;
            default:
                VERIFY(
                    false,
                    "Unhandled binary assignment operator",
                    get_tree_code_name(code)
                );
                __builtin_unreachable();
        }
        return std::make_unique<bimple::binary_assignment>(
            generate_atom(lhs),
            generate_atom(rhs1),
            generate_atom(rhs2),
            op
        );
    }

    std::unique_ptr<bimple::unary_assignment> generate_unary_assignment(gassign* statement) {
        tree_code code = gimple_assign_rhs_code(statement);
        tree lhs = gimple_assign_lhs(statement);
        tree rhs = gimple_assign_rhs1(statement);
        bimple::operators op;
        switch(code) {
            case INTEGER_CST:
            case REAL_CST:
            case NOP_EXPR:
            case SSA_NAME:
            case FLOAT_EXPR: // used for int -> real
            case FIX_TRUNC_EXPR: // used for real -> int
                op = bimple::operators::assign;
                break;
            case MEM_REF:
                op = bimple::operators::mem_ref;
                break;
            case BIT_NOT_EXPR:
                op = bimple::operators::bit_not;
                break;
            case NEGATE_EXPR:
                op = bimple::operators::neg;
                break;
            default:
                VERIFY(
                    false,
                    "Unhandled unary assignment operator",
                    get_tree_code_name(code)
                );
                __builtin_unreachable();
        }
        return std::make_unique<bimple::unary_assignment>(
            generate_atom(lhs),
            generate_atom(rhs),
            op
        );
    }

    std::unique_ptr<bimple::assignment> generate_assignment(gassign* statement) {
        tree_code code = gimple_assign_rhs_code(statement);
        switch(get_gimple_rhs_class(code)) {
            case GIMPLE_BINARY_RHS:
                return generate_binary_assignment(statement);
            case GIMPLE_UNARY_RHS:
            case GIMPLE_SINGLE_RHS:
                return generate_unary_assignment(statement);
            default:
                VERIFY(
                    false,
                    "Unhandled gimple assignment operator",
                    static_cast<gimple_rhs_class_class>(get_gimple_rhs_class(code)),
                    get_tree_code_name(code)
                );
                __builtin_unreachable();
        }
    }

    std::unique_ptr<bimple::call> generate_call(gcall* statement) {
        tree lhs = gimple_call_lhs(statement);
        tree fun = gimple_call_fn(statement);
        // VERIFY(
        //     false,
        //     "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
        //     get_tree_code_name(TREE_CODE(fun))
        // );
        std::vector<std::unique_ptr<bimple::atom>> args;
        for(int i = 0; i < gimple_call_num_args(statement); i++){
            tree arg = gimple_call_arg(statement, i);
            args.push_back(generate_atom(arg));
        }
        return std::make_unique<bimple::call>(
            generate_atom(fun),
            generate_atom(lhs),
            std::move(args)
        );
    }

    std::unique_ptr<bimple::function_return> generate_return(greturn* statement) {
        tree value = gimple_return_retval(statement);
        if(value == NULL_TREE) {
            return std::make_unique<bimple::function_return>();
        } else {
            return std::make_unique<bimple::function_return>(
                bimple::variable {get_identifier_value(value), generate_type(TREE_TYPE(value))}
            );
        }
    }

    std::unique_ptr<bimple::cond> generate_cond(gcond* statement) {
        tree_code code = gimple_cond_code(statement);
        tree lhs = gimple_cond_lhs(statement);
        tree rhs = gimple_cond_rhs(statement);
        bimple::operators op;
        switch(code) {
            case LT_EXPR:
                op = bimple::operators::lt;
                break;
            case GT_EXPR:
                op = bimple::operators::gt;
                break;
            case LE_EXPR:
                op = bimple::operators::lteq;
                break;
            case GE_EXPR:
                op = bimple::operators::gteq;
                break;
            case EQ_EXPR:
                op = bimple::operators::eq;
                break;
            case NE_EXPR:
                op = bimple::operators::neq;
                break;
            default:
                VERIFY(
                    false,
                    "Unhandled gimple condition operator",
                    static_cast<gimple_rhs_class_class>(get_gimple_rhs_class(code)),
                    get_tree_code_name(code)
                );
                __builtin_unreachable();
        }
        return std::make_unique<bimple::cond>(
            generate_atom(lhs),
            generate_atom(rhs),
            op
        );
    }

    std::unique_ptr<bimple::statement> generate_statement(gimple* statement) {
        switch(gimple_code(statement)) {
            case GIMPLE_ASSIGN:
                return generate_assignment(reinterpret_cast<gassign*>(statement));
            case GIMPLE_CALL:
                return generate_call(reinterpret_cast<gcall*>(statement));
            case GIMPLE_RETURN:
                return generate_return(reinterpret_cast<greturn*>(statement));
            case GIMPLE_COND:
                return generate_cond(reinterpret_cast<gcond*>(statement));
            case GIMPLE_PREDICT: // nothing for now
            case GIMPLE_LABEL:
            case GIMPLE_NOP:
                return {};
            default:
                VERIFY(
                    false,
                    "Unhandled gimple statement",
                    static_cast<gimple_code_class>(gimple_code(statement))
                );
                __builtin_unreachable();
        }
    }

    bimple::basic_block generate_bb(basic_block bb) {
        std::cout<<bb->index<<std::endl;
        bimple::basic_block bbb;
        bbb.index = bb->index;
        // Handle phi nodes
        for(gphi_iterator it = gsi_start_phis(bb); !gsi_end_p(it); gsi_next(&it)) {
            gphi* phi = it.phi();
            tree result = gimple_phi_result(phi);
            // virtual memory phi
            if(TREE_CODE(TREE_TYPE(result)) == VOID_TYPE) {
                continue;
            }
            bimple::phi bimple_phi;
            bimple_phi.result = {get_identifier_value(result), generate_type(TREE_TYPE(result))};
            for (unsigned i = 0; i < gimple_phi_num_args(phi); i++) {
                basic_block src = gimple_phi_arg_edge(phi, i)->src;
                tree def = gimple_phi_arg_def(phi, i);
                bimple_phi.values.push_back({src->index, bimple::variable{get_identifier_value(def), generate_type(TREE_TYPE(def))}});
            }
            bbb.phis.push_back(std::move(bimple_phi));
        }
        // Handle statements
        for(gimple_stmt_iterator it = gsi_start_bb(bb); !gsi_end_p(it); gsi_next(&it)) {
            auto statement = generate_statement(gsi_stmt(it));
            if(statement) {
                bbb.statements.push_back(std::move(statement));
            }
        }
        // Handle edges
        if(!bbb.statements.empty() && bbb.statements.back()->tag == bimple::statement_tag::cond) {
            // https://github.com/gcc-mirror/gcc/blob/5d2a360f0a541646abb11efdbabc33c6a04de7ee/gcc/tree-cfg.cc#L9511
            edge e = EDGE_SUCC(bb, 0);
            if(e->flags & EDGE_TRUE_VALUE) {
                VERIFY(EDGE_SUCC(bb, 1)->flags & EDGE_FALSE_VALUE);
                bbb.successors = {e->dest->index, EDGE_SUCC(bb, 1)->dest->index};
            } else if(e->flags & EDGE_FALSE_VALUE) {
                VERIFY(EDGE_SUCC(bb, 1)->flags & EDGE_TRUE_VALUE);
                bbb.successors = {EDGE_SUCC(bb, 1)->dest->index, e->dest->index};
            } else {
                VERIFY(false, "", e->flags);
            }
        } /*else if(auto cond = dynamic_cast<bimple::cond*>(bbb.statements.back().get())) {

        }*/ else {
            edge e;
            edge_iterator ei;
            FOR_EACH_EDGE(e, ei, bb->succs) {
                bbb.successors.push_back(e->dest->index);
            }
        }
        return bbb;
    }

    bimple::function generate_function(function* fun) {
        bimple::function function;
        function.identifier = gcc_str(DECL_ASSEMBLER_NAME(fun->decl)->identifier.id.str);
        function.return_type = generate_type(TREE_TYPE(TREE_TYPE(fun->decl)));
        tree arg = DECL_ARGUMENTS(fun->decl);
        while(arg) {
            function.args.push_back(
                std::make_pair(
                    gcc_str(DECL_NAME(arg)->identifier.id.str),
                    generate_type(TREE_TYPE(arg))
                )
            );
            arg = DECL_CHAIN(arg);
        }
        // handle entry block
        auto entry_bb = generate_bb(ENTRY_BLOCK_PTR_FOR_FN(fun));
        // generate copies from args to initial ssa definitions
        for(arg = DECL_ARGUMENTS(fun->decl); arg != NULL; arg = DECL_CHAIN(arg)) {
            tree def = ssa_default_def(fun, arg);
            if(def) {
                entry_bb.statements.push_back(
                    std::make_unique<bimple::unary_assignment>(
                        generate_atom(def),
                        std::make_unique<bimple::variable>(
                            IDENTIFIER_POINTER(SSA_NAME_IDENTIFIER(def)),
                            generate_type(TREE_TYPE(def)) // TODO
                        ),
                        bimple::operators::assign
                    )
                );
            }
        }
        function.basic_blocks.push_back(std::move(entry_bb));
        ASSERT(function.basic_blocks.back().index == function.basic_blocks.size() - 1);
        // handle exit block
        function.basic_blocks.push_back(generate_bb(EXIT_BLOCK_PTR_FOR_FN(fun)));
        ASSERT(function.basic_blocks.back().index == function.basic_blocks.size() - 1);
        // generate basic blocks
        basic_block bb;
        FOR_EACH_BB_FN(bb, fun) {
            function.basic_blocks.push_back(generate_bb(bb));
            ASSERT(function.basic_blocks.back().index == function.basic_blocks.size() - 1);
        }
        // topological order while we're here
        std::vector<int> postorder(last_basic_block_for_fn(fun));
        int nof_blocks = post_order_compute(postorder.data(), true, true);
        // for(int i = nof_blocks - 1; i >= 0; i--) {
        //     // basic_block gcc_bb = (*fun->cfg->x_basic_block_info)[postorder[i]];
        //     //gccbb2bb[gcc_bb] = func->build_bb();
        //     // debug_bb(gcc_bb);
        // }
        std::reverse(postorder.begin(), postorder.end());
        function.topological = std::move(postorder);
        return function;
    };

    // because gcc can't be normal in any regard
    const char* gcc_str(const unsigned char* gcc_str) {
        return reinterpret_cast<const char*>(gcc_str);
    }
};

simple_gimple_to_bimple_converter::simple_gimple_to_bimple_converter() : pimpl(std::make_unique<impl>()) {}

simple_gimple_to_bimple_converter::~simple_gimple_to_bimple_converter() = default;

bimple::function simple_gimple_to_bimple_converter::generate_function(function* fun) {
    return pimpl->generate_function(fun);
}
