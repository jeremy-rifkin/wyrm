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

#include "bimple.h"
#include "utils.h"
#include "llvm_codegen.h"

using namespace std::string_literals;

class llvm_codegen::impl {
    // bimple name -> llvm ir name
    std::unordered_map<std::string, std::string> name_map;
    // temporaries used for conditional results
    std::unordered_map<const bimple::cond*, std::string> cond_temps;
    unsigned llvmir_id = 0;
public:
    std::string generate_type(const std::unique_ptr<bimple::type>& type) {
        // std::cout<<*type<<std::endl;
        if(auto* ptr = bimple::downcast<bimple::integer>(type)) {
            return fmt::format("i{}", ptr->bits);
        } else if(auto* ptr = bimple::downcast<bimple::real>(type)) {
            // TODO bfloat, ppc_fp128
            switch(ptr->bits) {
                case 16: return "half";
                case 32: return "float";
                case 64: return "double";
                case 80: return "x86_fp80";
                case 128: return "fp128";
                default:
                    VERIFY(false, ptr->bits);
                    __builtin_unreachable();
            }
        } else if(auto* ptr = bimple::downcast<bimple::void_type>(type)) {
            return "void";
        } else if(auto* ptr = bimple::downcast<bimple::pointer>(type)) {
            return "ptr";
        } else {
            VERIFY(false, "Unhandled type", type->tag);
            __builtin_unreachable();
        }
    }

    std::string generate_atom(const std::unique_ptr<bimple::atom>& atom) {
        if(auto* ptr = bimple::downcast<bimple::variable>(atom)) {
            return llvm_name(ptr->name);
        } else if(auto* ptr = bimple::downcast<bimple::addr_expr>(atom)) {
            return fmt::format("@{}", ptr->name);
        } else if(auto* ptr = bimple::downcast<bimple::integer_constant>(atom)) {
            return std::to_string(ptr->value);
        } else if(auto* ptr = bimple::downcast<bimple::real_constant>(atom)) {
            return ptr->to_string(); // FIXME
        } else {
            VERIFY(false, "Unhandled atom", atom->tag);
            __builtin_unreachable();
        }
    }

    std::string generate_basic_assign(const bimple::unary_assignment* assignment) {
        ASSERT(assignment->op == bimple::operators::assign);
        // handle stores
        if(auto* l_mem = bimple::downcast<bimple::mem_ref>(assignment->lhs)) {
            auto scaled = new_temp();
            auto tmp = new_temp();
            return join(
                {
                    fmt::format(
                        "{} = udiv i64 {}, {}",
                        scaled,
                        // generate_type(memref->offset->type),
                        generate_atom(l_mem->offset),
                        assignment->lhs->type->size
                    ),
                    fmt::format(
                        "{} = getelementptr inbounds {}, ptr {}, {} {}",
                        tmp,
                        generate_type(assignment->rhs->type),
                        generate_atom(l_mem->base),
                        "i64", // generate_type(l_mem->offset->type),
                        scaled // generate_atom(l_mem->offset)
                    ),
                    fmt::format(
                        "store {} {}, ptr {}",
                        generate_type(assignment->rhs->type),
                        generate_atom(assignment->rhs),
                        tmp
                    )
                },
                '\n'
            );
        }
        auto lhs = generate_atom(assignment->lhs);
        auto rhs = generate_atom(assignment->rhs);
        // handle basic integer copy or conversion
        auto* l_int = bimple::downcast<bimple::integer>(assignment->lhs->type);
        auto* r_int = bimple::downcast<bimple::integer>(assignment->rhs->type);
        if(l_int && r_int) {
            if(l_int->bits == r_int->bits) { // note: sign check not needed as llvm ints don't have signs
                // raw copy
                return fmt::format(
                    "{} = or {} {}, 0",
                    lhs,
                    generate_type(assignment->rhs->type),
                    rhs
                );
            } else if(l_int->bits < r_int->bits) {
                return fmt::format(
                    "{} = trunc {} {} to {}",
                    lhs,
                    generate_type(assignment->rhs->type),
                    rhs,
                    generate_type(assignment->lhs->type)
                );
            } else if(l_int->bits > r_int->bits) {
                return fmt::format(
                    "{} = {} {} {} to {}",
                    lhs,
                    VERIFY(bimple::downcast<bimple::integer>(assignment->rhs->type))->is_unsigned ? "zext" : "sext",
                    generate_type(assignment->rhs->type),
                    rhs,
                    generate_type(assignment->lhs->type)
                );
            }
            VERIFY(false, "Unhandled bits for int conversion", l_int->bits, r_int->bits);
            __builtin_unreachable();
        }
        // handle basic pointer copy
        auto* l_ptr = bimple::downcast<bimple::pointer>(assignment->lhs->type);
        auto* r_ptr = bimple::downcast<bimple::pointer>(assignment->rhs->type);
        if(l_ptr && r_ptr) {
            return fmt::format(
                "{} = bitcast ptr {} to ptr",
                lhs,
                rhs
            );
        }
        // handle basic integer copy or conversion
        auto* l_real = bimple::downcast<bimple::real>(assignment->lhs->type);
        auto* r_real = bimple::downcast<bimple::real>(assignment->rhs->type);
        if(l_real && r_real) {
            if(l_real->bits == r_real->bits) { // note: sign check not needed as llvm ints don't have signs
                // raw copy
                return fmt::format(
                    "{0} = bitcast {1} {2} to {1}",
                    lhs,
                    generate_type(assignment->rhs->type),
                    rhs
                );
            } else if(l_real->bits < r_real->bits) {
                return fmt::format(
                    "{} = fptrunc {} {} to {}",
                    lhs,
                    generate_type(assignment->rhs->type),
                    rhs,
                    generate_type(assignment->lhs->type)
                );
            } else if(l_real->bits > r_real->bits) {
                return fmt::format(
                    "{} = fpext {} {} to {}",
                    lhs,
                    generate_type(assignment->rhs->type),
                    rhs,
                    generate_type(assignment->lhs->type)
                );
            }
            VERIFY(false, "Unhandled bits for real conversion", l_real->bits, r_real->bits);
            __builtin_unreachable();
        }
        // handle integer -> real
        if(l_real && r_int) {
            return fmt::format(
                "{} = {}itofp {} {} to {}",
                lhs,
                r_int->is_unsigned ? 'u' : 's',
                generate_type(assignment->rhs->type),
                rhs,
                generate_type(assignment->lhs->type)
            );
        }
        // handle real -> integer
        if(l_int && r_real) {
            return fmt::format(
                "{} = fpto{}i {} {} to {}",
                lhs,
                l_int->is_unsigned ? 'u' : 's',
                generate_type(assignment->rhs->type),
                rhs,
                generate_type(assignment->lhs->type)
            );
        }
        VERIFY(false, "Unhandled types for basic assign", *assignment->lhs->type, *assignment->rhs->type);
        __builtin_unreachable();
    }

    std::string generate_unary_assignment(const bimple::unary_assignment* assignment) {
        switch(assignment->op) {
            case bimple::operators::assign:
                return generate_basic_assign(assignment);
            case bimple::operators::mem_ref:
                {
                    auto* memref = VERIFY(bimple::downcast<bimple::mem_ref>(assignment->rhs));
                    // return fmt::format(
                    //     "{} = load {}, ptr {}",
                    //     generate_atom(assignment->lhs),
                    //     generate_type(assignment->lhs->type),
                    //     generate_atom(assignment->rhs)
                    // );
                    auto scaled = new_temp();
                    auto tmp = new_temp();
                    return join(
                        {
                            fmt::format(
                                "{} = udiv i64 {}, {}",
                                scaled,
                                // generate_type(memref->offset->type),
                                generate_atom(memref->offset),
                                assignment->lhs->type->size
                            ),
                            fmt::format(
                                "{} = getelementptr inbounds {}, ptr {}, {} {}",
                                tmp,
                                generate_type(assignment->lhs->type),
                                generate_atom(memref->base),
                                "i64", // generate_type(memref->offset->type),
                                scaled // generate_atom(memref->offset)
                            ),
                            fmt::format(
                                "{} = load {}, ptr {}",
                                generate_atom(assignment->lhs),
                                generate_type(assignment->lhs->type),
                                tmp
                            )
                        },
                        '\n'
                    );
                }
            case bimple::operators::bit_not:
                // llvm doesn't have a bitwise not
                ASSERT(bimple::downcast<bimple::integer>(assignment->rhs->type));
                ASSERT(*assignment->lhs->type == *assignment->rhs->type);
                return fmt::format(
                    "{} = xor {} {}, -1",
                    generate_atom(assignment->lhs),
                    generate_type(assignment->rhs->type),
                    generate_atom(assignment->rhs)
                );
            case bimple::operators::neg:
                // llvm doesn't have a negation
                ASSERT(*assignment->lhs->type == *assignment->rhs->type);
                if(bimple::downcast<bimple::integer>(assignment->rhs->type)) {
                    return fmt::format(
                        "{} = sub{} {} 0, {}",
                        generate_atom(assignment->lhs),
                        nsw(assignment->rhs->type),
                        generate_type(assignment->rhs->type),
                        generate_atom(assignment->rhs)
                    );
                } else if(bimple::downcast<bimple::real>(assignment->rhs->type)) {
                    return fmt::format(
                        "{} = fneg {} {}",
                        generate_atom(assignment->lhs),
                        generate_type(assignment->rhs->type),
                        generate_atom(assignment->rhs)
                    );
                } else {
                    ASSERT(false, "Unhandled type for unary negation", assignment->rhs->type->tag);
                }
                ASSERT(bimple::downcast<bimple::integer>(assignment->rhs->type));
            default:
                VERIFY(false, "Unhandled unary operator", assignment->op);
                __builtin_unreachable();
        }
    }

    const char* nsw(const std::unique_ptr<bimple::type>& type) {
        return VERIFY(bimple::downcast<bimple::integer>(type))->is_unsigned ? "" : " nsw";
    }

    std::string generate_llvm_op(bimple::operators op, const std::unique_ptr<bimple::type>& type) {
        if(auto* int_type = bimple::downcast<bimple::integer>(type)) {
            switch(op) {
                case bimple::operators::mul:
                    return "mul"s + nsw(type);
                case bimple::operators::add:
                    return "add"s + nsw(type);
                case bimple::operators::sub:
                    return "sub"s + nsw(type);
                case bimple::operators::trunc_div:
                    return int_type->is_unsigned ? "udiv" : "sdiv";
                case bimple::operators::trunc_mod:
                    return int_type->is_unsigned ? "urem" : "srem";
                case bimple::operators::bit_and:
                    return "and";
                case bimple::operators::bit_or:
                    return "or";
                case bimple::operators::bit_xor:
                    return "xor";
                case bimple::operators::lshift:
                    return "shl";
                case bimple::operators::rshift:
                    return int_type->is_unsigned ? "lshr" : "ashr";
                default:
                    VERIFY(false, "Unhandled integer arithmetic operator", op);
                    __builtin_unreachable();
            }
        } else if(auto* real = bimple::downcast<bimple::real>(type)) {
            switch(op) {
                case bimple::operators::add:
                    return "fadd";
                case bimple::operators::sub:
                    return "fsub";
                case bimple::operators::mul:
                    return "fmul";
                case bimple::operators::rdiv:
                    return "fdiv";
                default:
                    VERIFY(false, "Unhandled floating point arithmetic operator", op);
                    __builtin_unreachable();
            }
        } else {
            VERIFY(false, "Unhandled type", *type, op);
            __builtin_unreachable();
        }
    }

    std::string generate_arithmetic_assignment(const bimple::binary_assignment* assignment) {
        ASSERT(*assignment->rhs1->type == *assignment->rhs2->type);
        // simple arithmetic (add, mul, div, ...)
        auto lhs = generate_atom(assignment->lhs);
        auto rhs1 = generate_atom(assignment->rhs1);
        auto rhs2 = generate_atom(assignment->rhs2);
        return fmt::format(
            "{} = {} {} {}, {}",
            lhs,
            generate_llvm_op(assignment->op, assignment->rhs1->type),
            generate_type(assignment->rhs1->type),
            rhs1,
            rhs2
        );
    }

    std::string generate_boolean_assignment(const bimple::binary_assignment* assignment) {
        ASSERT(*assignment->rhs1->type == *assignment->rhs2->type);
        ASSERT(assignment->rhs1->type->tag == bimple::type_tag::integer || assignment->rhs1->type->tag == bimple::type_tag::real);
        auto lhs_type = ASSERT(bimple::downcast<bimple::integer>(assignment->lhs->type));
        // simple arithmetic (add, mul, div, ...)
        if(lhs_type->bits > 1) {
            auto tmp = new_temp();
            auto lhs = generate_atom(assignment->lhs);
            auto rhs1 = generate_atom(assignment->rhs1);
            auto rhs2 = generate_atom(assignment->rhs2);
            return join(
                {
                    fmt::format(
                        "{} = {} {} {} {}, {}",
                        tmp,
                        assignment->rhs1->type->tag == bimple::type_tag::integer ? "icmp" : "fcmp",
                        generate_llvm_boolean_op(assignment->op, assignment->rhs1->type),
                        generate_type(assignment->rhs2->type),
                        rhs1,
                        rhs2
                    ),
                    fmt::format(
                        "{} = zext i1 {} to {}",
                        lhs,
                        tmp,
                        generate_type(assignment->lhs->type)
                    )
                },
                '\n'
            );
        } else {
            ASSERT(lhs_type->bits == 1);
            auto lhs = generate_atom(assignment->lhs);
            auto rhs1 = generate_atom(assignment->rhs1);
            auto rhs2 = generate_atom(assignment->rhs2);
            return fmt::format(
                "{} = {} {} {} {}, {}",
                lhs,
                assignment->rhs1->type->tag == bimple::type_tag::integer ? "icmp" : "fcmp",
                generate_llvm_boolean_op(assignment->op, assignment->rhs1->type),
                generate_type(assignment->rhs2->type),
                rhs1,
                rhs2
            );
        }
    }

    std::string generate_llvm_pointer_op(bimple::operators op, const std::unique_ptr<bimple::type>& type) {
        //if(auto* int_type = bimple::downcast<bimple::integer>(type)) {
            switch(op) {
                case bimple::operators::pointer_add:
                    return "add";
                default:
                    VERIFY(false, "Unhandled operator", op);
                    __builtin_unreachable();
            }
        //} else {
        //    VERIFY(false, "Unhandled type", *type);
        //    __builtin_unreachable();
        //}
    }

    std::string generate_pointer_arithmetic_assignment(const bimple::binary_assignment* assignment) {
        // ASSERT(*assignment->rhs1->type == *assignment->rhs2->type);
        auto tmp1 = new_temp();
        auto tmp2 = new_temp();
        auto lhs = generate_atom(assignment->lhs);
        auto rhs1 = generate_atom(assignment->rhs1);
        auto rhs2 = generate_atom(assignment->rhs2);
        // TODO: Don't hardcode i64
        // TODO: Assuming rhs1 is the ptr
        VERIFY(assignment->op == bimple::operators::pointer_add);
        auto scaled = new_temp();
        return join(
            {
                fmt::format(
                    "{} = udiv {} {}, {}",
                    scaled,
                    generate_type(assignment->rhs2->type),
                    rhs2,
                    VERIFY(bimple::downcast<bimple::pointer>(assignment->rhs1->type))->target_type->size
                ),
                fmt::format(
                    "{} = getelementptr inbounds {}, ptr {}, {} {}",
                    lhs,
                    generate_type(VERIFY(bimple::downcast<bimple::pointer>(assignment->rhs1->type))->target_type), //generate_type(assignment->rhs1->type),
                    rhs1,
                    generate_type(assignment->rhs2->type),
                    scaled
                )
            },
            '\n'
        );
    }

    std::string generate_binary_assignment(const bimple::binary_assignment* assignment) {
        switch(assignment->op) {
            case bimple::operators::mul:
            case bimple::operators::add:
            case bimple::operators::sub:
            case bimple::operators::trunc_div:
            case bimple::operators::trunc_mod:
            case bimple::operators::rdiv:
            case bimple::operators::bit_and:
            case bimple::operators::bit_or:
            case bimple::operators::bit_xor:
            case bimple::operators::lshift:
            case bimple::operators::rshift:
                return generate_arithmetic_assignment(assignment);
            case bimple::operators::pointer_add:
                return generate_pointer_arithmetic_assignment(assignment);
            case bimple::operators::lt:
            case bimple::operators::gt:
            case bimple::operators::lteq:
            case bimple::operators::gteq:
            case bimple::operators::eq:
            case bimple::operators::neq:
                return generate_boolean_assignment(assignment);
            default:
                VERIFY(false, "Unhandled binary operator", assignment->op);
                __builtin_unreachable();
        }
    }

    std::string generate_llvm_boolean_op(bimple::operators op, const std::unique_ptr<bimple::type>& type) {
        if(auto* int_type = bimple::downcast<bimple::integer>(type)) {
            switch(op) {
                case bimple::operators::lt:
                    return fmt::format("{}lt", int_type->is_unsigned ? 'u' : 's');
                case bimple::operators::gt:
                    return fmt::format("{}gt", int_type->is_unsigned ? 'u' : 's');
                case bimple::operators::lteq:
                    return fmt::format("{}le", int_type->is_unsigned ? 'u' : 's');
                case bimple::operators::gteq:
                    return fmt::format("{}ge", int_type->is_unsigned ? 'u' : 's');
                case bimple::operators::eq:
                    return "eq";
                case bimple::operators::neq:
                    return "ne";
                default:
                    VERIFY(false, "Unhandled comparison operator", op);
                    __builtin_unreachable();
            }
        } else if(auto* real_type = bimple::downcast<bimple::real>(type)) {
            switch(op) {
                case bimple::operators::lt:
                    return "olt";
                case bimple::operators::gt:
                    return "ogt";
                case bimple::operators::lteq:
                    return "ole";
                case bimple::operators::gteq:
                    return "oge";
                case bimple::operators::eq:
                    return "oeq";
                case bimple::operators::neq:
                    return "une";
                default:
                    VERIFY(false, "Unhandled comparison operator", op);
                    __builtin_unreachable();
            }
        } else {
            VERIFY(false, "Unhandled type", *type);
            __builtin_unreachable();
        }
    }

    std::string generate_cond(const bimple::cond* cond) {
        ASSERT(!cond_temps.contains(cond));
        auto tmp = new_temp();
        cond_temps.insert({cond, tmp});
        auto lhs = generate_atom(cond->lhs);
        auto rhs = generate_atom(cond->rhs);
        ASSERT(*cond->lhs->type == *cond->rhs->type);
        if(cond->lhs->type->tag == bimple::type_tag::integer) {
            return fmt::format(
                "{} = icmp {} {} {}, {}",
                tmp,
                generate_llvm_boolean_op(cond->op, cond->lhs->type),
                generate_type(cond->rhs->type),
                lhs,
                rhs
            );
        } else if(cond->lhs->type->tag == bimple::type_tag::real) {
            return fmt::format(
                "{} = fcmp {} {} {}, {}",
                tmp,
                generate_llvm_boolean_op(cond->op, cond->lhs->type),
                generate_type(cond->rhs->type),
                lhs,
                rhs
            );
        } else {
            VERIFY(false, "Unhandled type for conditional", cond->lhs->type->tag);
            __builtin_unreachable();
        }
    }

    std::string generate_statement(const std::unique_ptr<bimple::statement>& statement) {
        if(auto* ptr = bimple::downcast<bimple::unary_assignment>(statement)) {
            return generate_unary_assignment(ptr);
        } else if(auto* ptr = bimple::downcast<bimple::binary_assignment>(statement)) {
            return generate_binary_assignment(ptr);
        } else if(auto* ptr = bimple::downcast<bimple::cond>(statement)) {
            return generate_cond(ptr);
        } else if(auto* ptr = bimple::downcast<bimple::function_return>(statement)) {
            if(ptr->value) {
                return fmt::format(
                    "ret {} {}",
                    generate_type(ptr->value->type),
                    llvm_name(ptr->value->name)
                );
            } else {
                return "ret void";
            }
        } else if(auto* ptr = bimple::downcast<bimple::call>(statement)) {
            // FIXME libassert issue due to a pragma when this was inlined...?
            auto* fnptr = VERIFY(bimple::downcast<bimple::pointer>(ptr->fn->type));
            return fmt::format(
                "{} = call noundef {} {}({})",
                generate_atom(ptr->lhs),
                generate_type(
                    VERIFY(
                        bimple::downcast<bimple::function_type>(
                            fnptr->target_type
                        )
                    )->return_type
                ),
                generate_atom(ptr->fn),
                format_list(
                    ptr->args,
                    [this] (const std::unique_ptr<bimple::atom>& arg) {
                        return fmt::format("{} noundef {}", generate_type(arg->type), generate_atom(arg));
                    }
                )
            );
        } else {
            VERIFY(false, "Unhandled statement", statement->tag);
            __builtin_unreachable();
        }
    }

    std::string generate_phi(const bimple::phi& phi) {
        return fmt::format(
            "{} = phi {} {}",
            llvm_name(phi.result.name),
            generate_type(phi.result.type),
            format_list(
                phi.values,
                [this, &phi] (const std::pair<int, bimple::variable>& pair) {
                    const auto& [src, var] = pair;
                    ASSERT(*phi.result.type == *var.type);
                    return fmt::format(
                        "[ {}, %{} ]",
                        llvm_name(var.name),
                        llvm_bb(src)
                    );
                }
            )
        );
    }

    std::string generate_terminator(const bimple::basic_block& bb) {
        if(bb.successors.size() == 1) {
            // Assuming fallthrough TODO
            return fmt::format("br label %{}", llvm_bb(bb.successors[0]));
        } else if(bb.successors.size() == 2) {
            // assuming branch TODO
            auto* cond = VERIFY(bimple::downcast<bimple::cond>(bb.statements.back()));
            return fmt::format(
                "br i1 {}, label %{}, label %{}",
                cond_temps.at(cond),
                llvm_bb(bb.successors[0]),
                llvm_bb(bb.successors[1])
            );
        } else {
            VERIFY(false, bb.index, bb.successors.size());
            __builtin_unreachable();
        }
    }

    // std::vector<int> topological_sort(const std::vector<bimple::basic_block>& bbs) {
    //     std::vector<int> order;
    //     std::vector<bool> visited(bbs.size(), false);
    //     // dfs
    //     for(const auto& bb : bbs) {
    //         ASSERT(bb.index == &bb - &bbs.front());
    //         if(!visited[bb.index]) {
    //             std::stack<int> stack;
    //             stack.push(bb.index);
    //             while(!stack.empty()) {
    //                 auto node = stack.top();
    //                 // fmt::print("node {} : {}\n", order);
    //                 stack.pop();
    //                 if(!visited[node]) {
    //                     order.push_back(node);
    //                     visited[node] = true;
    //                     for(const auto& tgt : bbs[node].successors) {
    //                         stack.push(tgt);
    //                     }
    //                 }
    //             }
    //         }
    //     }
    //     // std::reverse(order.begin(), order.end());
    //     // fmt::print("->->->->->->->-> {}\n", order);
    //     return order;
    // }

    std::string generate(const bimple::function& fn) {
        std::string code;
        code += fmt::format("define noundef {} @{}(", generate_type(fn.return_type), fn.identifier);
        // generate function arguments
        for(const auto& arg : fn.args) {
            const auto& [name, type] = arg;
            code += fmt::format("{} noundef {}", generate_type(type), llvm_name(name));
            if(&arg != &fn.args.back()) code += fmt::format(", ");
        }
        code += fmt::format(") {{\n");
        // function body
        // for(const auto& index : topological_sort(fn.basic_blocks)) {
        // for(const auto& index : fn.topological) {
        for(const auto& bb : fn.basic_blocks) {
            if(bb.index == 1) continue; // TODO
            code += fmt::format("{}:\n", llvm_bb(bb.index));
            for(const auto& phi : bb.phis) {
                code += indent(generate_phi(phi), 4, ' ') + "\n";
            }
            for(const auto& statement : bb.statements) {
                code += indent(generate_statement(statement), 4, ' ') + "\n";
            }
            // Handle terminator
            if(bb.successors[0] == 1) continue; // TODO: Hacky
            code += indent(generate_terminator(bb), 4, ' ') + "\n";
        }
        code += fmt::format("}}\n");
        return code;
    }
private:
    // Note: Including %
    std::string llvm_name(const std::string& bimple_name) {
        if(name_map.contains(bimple_name)) {
            return fmt::format("%z{}", name_map.at(bimple_name));
        } else {
            return fmt::format("%z{}", name_map.insert({bimple_name, std::to_string(llvmir_id++)}).first->second);
        }
    }

    // Note: No %
    std::string llvm_bb(int bb_index) {
        return fmt::format("bb{}", bb_index);
    }

    std::string new_temp() {
        return fmt::format("%t{}", llvmir_id++);
    }
};

llvm_codegen::llvm_codegen() : pimpl(std::make_unique<impl>()) {}
llvm_codegen::~llvm_codegen() = default;

std::string llvm_codegen::generate(const bimple::function& fn) {
    return pimpl->generate(fn);
}
