#ifndef BIMPLE_H
#define BIMPLE_H

#include <string>
#include <utility>
#include <vector>
#include <memory>
#include <sstream>
#include <optional>

#ifndef ASSERT_USE_MAGIC_ENUM
#define ASSERT_USE_MAGIC_ENUM
#endif
#include <assert.hpp>
#include <fmt/core.h>

#include "utils.h"

namespace bimple {
    enum class type_tag {
        integer,
        void_type,
        boolean,
        bitint,
        real,
        fixed_point,
        complex,
        enumeral,
        // opaque,
        pointer,
        // refernce,
        function,
        // method,
        // array,
        union_type,
        struct_type // "Record type" in gcc
    };

    template<typename Derived, typename Base>
    Derived* downcast(Base* base) {
        if(base->tag == Derived::struct_tag()) {
            return static_cast<Derived*>(base);
        } else {
            return nullptr;
        }
    }

    template<typename Derived, typename Base>
    const Derived* downcast(const Base* base) {
        if(base->tag == Derived::struct_tag()) {
            return static_cast<const Derived*>(base);
        } else {
            return nullptr;
        }
    }

    template<typename Derived, typename Base>
    Derived* downcast(std::unique_ptr<Base>& base) {
        if(base->tag == Derived::struct_tag()) {
            return static_cast<Derived*>(base.get());
        } else {
            return nullptr;
        }
    }

    template<typename Derived, typename Base>
    const Derived* downcast(const std::unique_ptr<Base>& base) {
        if(base->tag == Derived::struct_tag()) {
            return static_cast<const Derived*>(base.get());
        } else {
            return nullptr;
        }
    }

    struct type {
        type_tag tag;
        std::size_t size;
        // Size comes from TYPE_SIZE which is in bits. Assuming 8 bits per byte.
        type(type_tag tag, std::size_t size) : tag(tag), size(size / 8) {}
        bool operator==(const type& other) const;
        virtual std::string to_string() const = 0;
        friend std::ostream& operator<<(std::ostream& s, const type& t) {
            s<<t.to_string();
            return s;
        }
    };

    struct integer : public type {
        unsigned bits;
        bool is_unsigned;
        integer(unsigned bits, bool is_unsigned, std::size_t size) :
            type(struct_tag(), size),
            bits(bits),
            is_unsigned(is_unsigned) {}
        std::string to_string() const override {
            return fmt::format("{}int{}", is_unsigned ? "u" : "", bits);
        }
        bool operator==(const integer& other) const {
            return bits == other.bits && is_unsigned == other.is_unsigned;
        }
        static constexpr type_tag struct_tag() {
            return type_tag::integer;
        }
    };

    struct void_type : public type {
        void_type() : type(struct_tag(), 0) {}
        std::string to_string() const override {
            return "void";
        }
        bool operator==(const void_type& other) const {
            return true;
        }
        static constexpr type_tag struct_tag() {
            return type_tag::void_type;
        }
    };

    struct boolean : public type {
        boolean(std::size_t size) : type(struct_tag(), size) {}
        std::string to_string() const override {
            return "bool";
        }
        bool operator==(const boolean& other) const {
            return true;
        }
        static constexpr type_tag struct_tag() {
            return type_tag::boolean;
        }
    };

    struct pointer : public type {
        std::unique_ptr<type> target_type;
        pointer(std::unique_ptr<type>&& target_type, std::size_t size) :
            type(struct_tag(), size),
            target_type(std::move(target_type)) {}
        std::string to_string() const override {
            return fmt::format("{}*", target_type->to_string());
        }
        bool operator==(const pointer& other) const {
            return *target_type == *other.target_type;
        }
        static constexpr type_tag struct_tag() {
            return type_tag::pointer;
        }
    };

    struct real : public type {
        unsigned bits;
        real(unsigned bits, std::size_t size) : type(struct_tag(), size), bits(bits) {}
        std::string to_string() const override {
            return fmt::format("f{}", bits);
        }
        bool operator==(const real& other) const {
            return true;
        }
        static constexpr type_tag struct_tag() {
            return type_tag::real;
        }
    };

    struct function_type : public type {
        std::unique_ptr<type> return_type;
        std::vector<std::unique_ptr<type>> args;
        function_type(
            std::unique_ptr<type>&& return_type,
            std::vector<std::unique_ptr<type>>&& args
        ) : type(struct_tag(), 0), return_type(std::move(return_type)), args(std::move(args)) {}
        std::string to_string() const override {
            return fmt::format(
                "(({}) -> {})",
                format_list(
                    args,
                    [] (const std::unique_ptr<type>& arg) {
                        return arg->to_string();
                    }
                ),
                return_type->to_string()
            );
        }
        bool operator==(const function_type& other) const {
            return true;
        }
        static constexpr type_tag struct_tag() {
            return type_tag::function;
        }
    };

    inline bool type::operator==(const type& other) const {
        if(tag != other.tag) {
            return false;
        }
        if(auto* self = downcast<integer>(this)) {
            return *self == *downcast<integer>(&other);
        } else if(auto* self = downcast<void_type>(this)) {
            return *self == *downcast<void_type>(&other);
        } else if(auto* self = downcast<boolean>(this)) {
            return *self == *downcast<boolean>(&other);
        } else if(auto* self = downcast<pointer>(this)) {
            return *self == *downcast<pointer>(&other);
        } else if(auto* self = downcast<real>(this)) {
            return *self == *downcast<real>(&other);
        } else if(auto* self = downcast<function_type>(this)) {
            return *self == *downcast<function_type>(&other);
        } else {
            VERIFY(false, "Unhandled type", other.tag);
            __builtin_unreachable();
        }
    }

    enum class atom_tag {
        variable,
        addr_expr,
        mem_ref,
        integer_constant,
        real_constant
    };

    struct atom {
        atom_tag tag;
        std::unique_ptr<bimple::type> type;
        atom(atom_tag tag, std::unique_ptr<bimple::type>&& type) :
            tag(tag),
            type(std::move(type)) {}
        virtual std::string to_string(bool types = false) const = 0;
    };

    struct variable : public atom {
        std::string name;
        variable() : atom(struct_tag(), nullptr) {}
        variable(std::string&& name, std::unique_ptr<bimple::type>&& type) :
            atom(struct_tag(), std::move(type)),
            name(name) {}
        std::string to_string(bool types = false) const {
            if(types) {
                return fmt::format("{} [{}]", name, type->to_string());
            } else {
                return name;
            }
        }
        static constexpr atom_tag struct_tag() {
            return atom_tag::variable;
        }
    };

    struct addr_expr : public atom {
        std::string name;
        addr_expr() : atom(struct_tag(), nullptr) {}
        addr_expr(std::string&& name, std::unique_ptr<bimple::type>&& type) :
            atom(struct_tag(), std::move(type)),
            name(name) {}
        std::string to_string(bool types = false) const {
            if(types) {
                return fmt::format("@{} [{}]", name, type->to_string());
            } else {
                return name;
            }
        }
        static constexpr atom_tag struct_tag() {
            return atom_tag::addr_expr;
        }
    };

    struct mem_ref : public atom {
        std::unique_ptr<atom> base;
        std::unique_ptr<atom> offset;
        mem_ref() : atom(struct_tag(), nullptr) {}
        mem_ref(
            std::unique_ptr<atom>&& base,
            std::unique_ptr<atom>&& offset,
            std::unique_ptr<bimple::type>&& type
        ) :
            atom(struct_tag(), std::move(type)),
            base(std::move(base)),
            offset(std::move(offset)) {}

        std::string to_string(bool types = false) const {
            if(types) {
                return fmt::format("*({} + {}) [{}]", base->to_string(), offset->to_string(), type->to_string());
            } else {
                return fmt::format("*({} + {})", base->to_string(), offset->to_string());
            }
        }
        static constexpr atom_tag struct_tag() {
            return atom_tag::mem_ref;
        }
    };

    struct integer_constant : public atom {
        int value;
        integer_constant() : atom(struct_tag(), nullptr) {}
        integer_constant(int value, std::unique_ptr<bimple::type>&& type) :
            atom(struct_tag(), std::move(type)),
            value(value) {}

        std::string to_string(bool types = false) const {
            if(types) {
                return fmt::format("{} [{}]", value, type->to_string());
            } else {
                return fmt::format("{}", value);
            }
        }
        static constexpr atom_tag struct_tag() {
            return atom_tag::integer_constant;
        }
    };

    struct real_constant : public atom {
        std::string value;
        real_constant() : atom(struct_tag(), nullptr) {}
        real_constant(std::string&& value, std::unique_ptr<bimple::type>&& type) :
            atom(struct_tag(), std::move(type)),
            value(value) {}

        std::string to_string(bool types = false) const {
            if(types) {
                return fmt::format("{} [{}]", value, type->to_string());
            } else {
                return value;
            }
        }
        static constexpr atom_tag struct_tag() {
            return atom_tag::real_constant;
        }
    };

    struct phi {
        variable result;
        std::vector<std::pair<int, variable>> values;

        std::string to_string(bool types = false) const {
            return fmt::format(
                "{} = phi({})",
                result.to_string(),
                format_list(
                    values,
                    [] (const std::pair<int, bimple::variable>& pair) {
                        return fmt::format("{} {}", pair.first, pair.second.to_string());
                    }
                )
            );
        }
    };

    enum class statement_tag {
        binary_assignment,
        unary_assignment,
        call,
        function_return,
        cond
    };

    struct statement {
        statement_tag tag;
        statement(statement_tag tag) : tag(tag) {}
        virtual std::string to_string(bool types = false) const = 0;
    };

    struct assignment : public statement {
        std::unique_ptr<atom> lhs;
    protected:
        assignment(statement_tag tag, std::unique_ptr<atom>&& lhs) :
            statement(tag),
            lhs(std::move(lhs)) {}
    };

    enum class operators {
        mul,
        add,
        pointer_add,
        sub,
        trunc_div,
        trunc_mod,
        rdiv,
        bit_and,
        bit_or,
        bit_xor,
        lshift,
        rshift,
        logical_and,
        logical_or,
        assign,
        lt,
        gt,
        lteq,
        gteq,
        eq,
        neq,
        mem_ref,
        bit_not,
        neg,
    };

    inline std::string to_string(operators op) {
        using enum operators;
        switch(op) {
            case assign:
                return "";
            case mul:
                return "*";
            case trunc_div:
            case rdiv:
                return "/";
            case trunc_mod:
                return "%";
            case bit_and:
                return "&";
            case bit_or:
                return "|";
            case bit_xor:
                return "^";
            case lshift:
                return "<<";
            case rshift:
                return ">>";
            case logical_and:
                return "&&";
            case logical_or:
                return "||";
            case add:
            case pointer_add:
                return "+";
            case sub:
                return "-";
            case lt:
                return "<";
            case gt:
                return ">";
            case lteq:
                return "<=";
            case gteq:
                return ">=";
            case eq:
                return "==";
            case neq:
                return "!=";
            case mem_ref:
                return "[mem_ref]";
            case bit_not:
                return "~";
            case neg:
                return "-";
            default:
                VERIFY(false, "Unhandled operator", op);
                __builtin_unreachable();
        }
    }

    struct binary_assignment : public assignment {
        std::unique_ptr<atom> rhs1;
        std::unique_ptr<atom> rhs2;
        operators op;
        binary_assignment(
            std::unique_ptr<atom>&& lhs,
            std::unique_ptr<atom>&& rhs1,
            std::unique_ptr<atom>&& rhs2,
            operators op
        ) :
            assignment(struct_tag(), std::move(lhs)),
            rhs1(std::move(rhs1)),
            rhs2(std::move(rhs2)),
            op(op) {};

        std::string to_string(bool types = false) const override {
            return fmt::format(
                "{} = {} {} {}",
                lhs->to_string(types),
                rhs1->to_string(types),
                bimple::to_string(op),
                rhs2->to_string(types)
            );
        }
        static constexpr statement_tag struct_tag() {
            return statement_tag::binary_assignment;
        }
    };

    struct unary_assignment : public assignment {
        std::unique_ptr<atom> rhs;
        operators op;
        unary_assignment(
            std::unique_ptr<atom>&& lhs,
            std::unique_ptr<atom>&& rhs,
            operators op
        ) :
            assignment(struct_tag(), std::move(lhs)),
            rhs(std::move(rhs)),
            op(op) {};

        std::string to_string(bool types = false) const override {
            return fmt::format(
                "{} = {}{}",
                lhs->to_string(types),
                bimple::to_string(op),
                rhs->to_string(types)
            );
        }
        static constexpr statement_tag struct_tag() {
            return statement_tag::unary_assignment;
        }
    };

    struct call : public statement {
        std::unique_ptr<atom> fn;
        std::unique_ptr<atom> lhs;
        std::vector<std::unique_ptr<atom>> args;

        call(
            std::unique_ptr<atom>&& fn,
            std::unique_ptr<atom>&& lhs,
            std::vector<std::unique_ptr<atom>>&& args
        ) :
            statement(struct_tag()),
            fn(std::move(fn)),
            lhs(std::move(lhs)),
            args(std::move(args)) {}

        std::string to_string(bool types = false) const override {
            std::ostringstream s;
            s<<lhs->to_string(types)<<" = "<<fn->to_string(types)<<"(";
            bool is_first = true;
            for(const auto& arg : args) {
                if(!is_first) {
                    s<<", ";
                }
                is_first = false;
                s<<arg->to_string();
            }
            s<<")";
            return std::move(s).str();
        }
        static constexpr statement_tag struct_tag() {
            return statement_tag::call;
        }
    };

    struct function_return : public statement {
        std::optional<variable> value;
        function_return() : statement(struct_tag()) {};
        function_return(variable&& value) :
                statement(struct_tag()),
                value(std::move(value)) {}

        std::string to_string(bool types = false) const override {
            if(value) {
                return fmt::format("return {}", value->to_string());
            } else {
                return "return";
            }
        }
        static constexpr statement_tag struct_tag() {
            return statement_tag::function_return;
        }
    };

    struct cond : public statement {
        std::unique_ptr<atom> lhs;
        std::unique_ptr<atom> rhs;
        operators op;
        cond(
            std::unique_ptr<atom>&& lhs,
            std::unique_ptr<atom>&& rhs,
            operators op
        ) :
            statement(struct_tag()),
            lhs(std::move(lhs)),
            rhs(std::move(rhs)),
            op(op) {}

        std::string to_string(bool types = false) const override {
            return fmt::format(
                "conditional branch {} {} {}",
                lhs->to_string(),
                bimple::to_string(op),
                rhs->to_string()
            );
        }
        static constexpr statement_tag struct_tag() {
            return statement_tag::cond;
        }
    };

    struct basic_block {
        int index;
        std::vector<phi> phis;
        std::vector<std::unique_ptr<statement>> statements;
        // for fallthrough, successors.size() will be 1
        // for cond, successors[0] is true branch and successors[1] is false branch
        std::vector<int> successors;

        std::string to_string(bool types = false) const {
            std::ostringstream s;
            for(const auto& phi : phis) {
                s<<"    "<<phi.to_string(types)<<"\n";
            }
            for(const auto& statement : statements) {
                s<<"    "<<statement->to_string(types);
                if(&statement != &statements.back()) {
                    s<<"\n";
                }
            }
            return std::move(s).str();
        }
    };

    struct function {
        std::string identifier;
        std::vector<std::pair<std::string, std::unique_ptr<type>>> args;
        std::unique_ptr<type> return_type;
        // every bb's index in this vector should match it's index member
        std::vector<basic_block> basic_blocks;
        std::vector<int> topological;

        std::string to_string(bool types = false) const {
            std::ostringstream s;
            s<<"fn "<<identifier<<"("<<format_list(
                args,
                [] (const std::pair<std::string, std::unique_ptr<bimple::type>>& pair) {
                    return fmt::format("{}: {}", pair.first, pair.second->to_string());
                }
            );
            s<<"): "<<return_type->to_string()<<" {\n";
            for(const auto& bb : basic_blocks) {
                s<<bb.index<<":\n"<<bb.to_string(types)<<"\n";
            }
            s<<"}";
            return std::move(s).str();
        }
    };
}

#endif
