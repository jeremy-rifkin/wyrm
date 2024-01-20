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

static void
print_no_sanitize_attr_value (FILE *file, tree value)
{
  unsigned int flags = tree_to_uhwi (value);
  bool first = true;
  for (int i = 0; sanitizer_opts[i].name != NULL; ++i)
    {
      if ((sanitizer_opts[i].flag & flags) == sanitizer_opts[i].flag)
    {
      if (!first)
        fprintf (file, " | ");
      fprintf (file, "%s", sanitizer_opts[i].name);
      first = false;
    }
    }
}

static void
dump_default_def (FILE *file, tree def, int spc, dump_flags_t flags)
{
  for (int i = 0; i < spc; ++i)
    fprintf (file, " ");
  dump_ssaname_info_to_file (file, def, spc);

  print_generic_expr (file, TREE_TYPE (def), flags);
  fprintf (file, " ");
  print_generic_expr (file, def, flags);
  fprintf (file, " = ");
  print_generic_expr (file, SSA_NAME_VAR (def), flags);
  fprintf (file, ";\n");
}

void
custom_dump_function_to_file (tree fndecl, FILE *file, dump_flags_t flags)
{
  tree arg, var, old_current_fndecl = current_function_decl;
  struct function *dsf;
  bool ignore_topmost_bind = false, any_var = false;
  basic_block bb;
  tree chain;
  bool tmclone = (TREE_CODE (fndecl) == FUNCTION_DECL
          && decl_is_tm_clone (fndecl));
  struct function *fun = DECL_STRUCT_FUNCTION (fndecl);

  tree fntype = TREE_TYPE (fndecl);
  tree attrs[] = { DECL_ATTRIBUTES (fndecl), TYPE_ATTRIBUTES (fntype) };

  for (int i = 0; i != 2; ++i)
    {
      if (!attrs[i])
    continue;

      fprintf (file, "__attribute__((");

      bool first = true;
      tree chain;
      for (chain = attrs[i]; chain; first = false, chain = TREE_CHAIN (chain))
    {
      if (!first)
        fprintf (file, ", ");

      tree name = get_attribute_name (chain);
      print_generic_expr (file, name, dump_flags);
      if (TREE_VALUE (chain) != NULL_TREE)
        {
          fprintf (file, " (");

          if (strstr (IDENTIFIER_POINTER (name), "no_sanitize"))
        print_no_sanitize_attr_value (file, TREE_VALUE (chain));
          else
        print_generic_expr (file, TREE_VALUE (chain), dump_flags);
          fprintf (file, ")");
        }
    }

      fprintf (file, "))\n");
    }

  current_function_decl = fndecl;
  if (flags & TDF_GIMPLE)
    {
      static bool hotness_bb_param_printed = false;
    //   if (profile_info != NULL
    //   && !hotness_bb_param_printed)
    // {
    //   hotness_bb_param_printed = true;
    //   fprintf (file,
    //        "/* --param=gimple-fe-computed-hot-bb-threshold=%" PRId64
    //        " */\n", get_hot_bb_threshold ());
    // }

      print_generic_expr (file, TREE_TYPE (TREE_TYPE (fndecl)),
              dump_flags | TDF_SLIM);
      fprintf (file, " __GIMPLE (%s",
           (fun->curr_properties & PROP_ssa) ? "ssa"
           : (fun->curr_properties & PROP_cfg) ? "cfg"
           : "");

      if (fun && fun->cfg)
    {
      basic_block bb = ENTRY_BLOCK_PTR_FOR_FN (fun);
      if (bb->count.initialized_p ())
        fprintf (file, ",%s(%" PRIu64 ")",
             profile_quality_as_string (bb->count.quality ()),
             bb->count.value ());
      if (dump_flags & TDF_UID)
        fprintf (file, ")\n%sD_%u (", function_name (fun),
             DECL_UID (fndecl));
      else
        fprintf (file, ")\n%s (", function_name (fun));
    }
    }
  else
    {
      print_generic_expr (file, TREE_TYPE (fntype), dump_flags);
      if (dump_flags & TDF_UID)
    fprintf (file, " %sD.%u %s(", function_name (fun), DECL_UID (fndecl),
         tmclone ? "[tm-clone] " : "");
      else
    fprintf (file, " %s %s(", function_name (fun),
         tmclone ? "[tm-clone] " : "");
    }

  arg = DECL_ARGUMENTS (fndecl);
  while (arg)
    {
      print_generic_expr (file, TREE_TYPE (arg), dump_flags);
      fprintf (file, " ");
      print_generic_expr (file, arg, dump_flags);
      if (DECL_CHAIN (arg))
    fprintf (file, ", ");
      arg = DECL_CHAIN (arg);
    }
  fprintf (file, ")\n");

  dsf = DECL_STRUCT_FUNCTION (fndecl);
  if (dsf && (flags & TDF_EH))
    dump_eh_tree (file, dsf);

  if (flags & TDF_RAW && !gimple_has_body_p (fndecl))
    {
      dump_node (fndecl, TDF_SLIM | flags, file);
      current_function_decl = old_current_fndecl;
      return;
    }

  /* When GIMPLE is lowered, the variables are no longer available in
     BIND_EXPRs, so display them separately.  */
  if (fun && fun->decl == fndecl && (fun->curr_properties & PROP_gimple_lcf))
    {
      unsigned ix;
      ignore_topmost_bind = true;

      fprintf (file, "{\n");
      if (gimple_in_ssa_p (fun)
      && (flags & TDF_ALIAS))
    {
      for (arg = DECL_ARGUMENTS (fndecl); arg != NULL;
           arg = DECL_CHAIN (arg))
        {
          tree def = ssa_default_def (fun, arg);
          if (def)
        dump_default_def (file, def, 2, flags);
        }

      tree res = DECL_RESULT (fun->decl);
      if (res != NULL_TREE
          && DECL_BY_REFERENCE (res))
        {
          tree def = ssa_default_def (fun, res);
          if (def)
        dump_default_def (file, def, 2, flags);
        }

      tree static_chain = fun->static_chain_decl;
      if (static_chain != NULL_TREE)
        {
          tree def = ssa_default_def (fun, static_chain);
          if (def)
        dump_default_def (file, def, 2, flags);
        }
    }

      if (!vec_safe_is_empty (fun->local_decls))
    FOR_EACH_LOCAL_DECL (fun, ix, var)
      {
        print_generic_decl (file, var, flags);
        fprintf (file, "\n");

        any_var = true;
      }

      tree name;

      if (gimple_in_ssa_p (fun))
    FOR_EACH_SSA_NAME (ix, name, fun)
      {
        if (!SSA_NAME_VAR (name)
        /* SSA name with decls without a name still get
           dumped as _N, list those explicitely as well even
           though we've dumped the decl declaration as D.xxx
           above.  */
        || !SSA_NAME_IDENTIFIER (name))
          {
        fprintf (file, "  ");
        print_generic_expr (file, TREE_TYPE (name), flags);
        fprintf (file, " ");
        print_generic_expr (file, name, flags);
        fprintf (file, ";\n");

        any_var = true;
          }
      }
    }

  if (fun && fun->decl == fndecl
      && fun->cfg
      && basic_block_info_for_fn (fun))
    {
      /* If the CFG has been built, emit a CFG-based dump.  */
      if (!ignore_topmost_bind)
    fprintf (file, "{\n");

      if (any_var && n_basic_blocks_for_fn (fun))
    fprintf (file, "\n");

      FOR_EACH_BB_FN (bb, fun)
    dump_bb (file, bb, 2, flags);

      fprintf (file, "}\n");
    }
  else if (fun && (fun->curr_properties & PROP_gimple_any))
    {
      /* The function is now in GIMPLE form but the CFG has not been
     built yet.  Emit the single sequence of GIMPLE statements
     that make up its body.  */
      gimple_seq body = gimple_body (fndecl);

      if (gimple_seq_first_stmt (body)
      && gimple_seq_first_stmt (body) == gimple_seq_last_stmt (body)
      && gimple_code (gimple_seq_first_stmt (body)) == GIMPLE_BIND)
    print_gimple_seq (file, body, 0, flags);
      else
    {
      if (!ignore_topmost_bind)
        fprintf (file, "{\n");

      if (any_var)
        fprintf (file, "\n");

      print_gimple_seq (file, body, 2, flags);
      fprintf (file, "}\n");
    }
    }
  else
    {
      int indent;

      /* Make a tree based dump.  */
      chain = DECL_SAVED_TREE (fndecl);
      if (chain && TREE_CODE (chain) == BIND_EXPR)
    {
      if (ignore_topmost_bind)
        {
          chain = BIND_EXPR_BODY (chain);
          indent = 2;
        }
      else
        indent = 0;
    }
      else
    {
      if (!ignore_topmost_bind)
        {
          fprintf (file, "{\n");
          /* No topmost bind, pretend it's ignored for later.  */
          ignore_topmost_bind = true;
        }
      indent = 2;
    }

      if (any_var)
    fprintf (file, "\n");

      print_generic_stmt_indented (file, chain, flags, indent);
      if (ignore_topmost_bind)
    fprintf (file, "}\n");
    }

  if (flags & TDF_ENUMERATE_LOCALS)
    dump_enumerated_decls (file, flags);
  fprintf (file, "\n\n");

  current_function_decl = old_current_fndecl;
}

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
