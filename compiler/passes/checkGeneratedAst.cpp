/*
 * Copyright 2020-2025 Hewlett Packard Enterprise Development LP
 * Copyright 2004-2019 Cray Inc.
 * Other additional copyright holders may be indicated within.
 *
 * The entirety of this work is licensed under the Apache License,
 * Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// checkGeneratedAst.cpp

// this file contains AST checks that occur after converting
// from uAST to AST.

#include "passes.h"

#include "astutil.h"
#include "DeferStmt.h"
#include "expr.h"
#include "stmt.h"
#include "stlUtil.h"
#include "stringutil.h"

#include "global-ast-vecs.h"

static void checkNamedArguments(CallExpr* call);
static void checkManagedClassKinds(CallExpr* call);
static void checkExplicitDeinitCalls(CallExpr* call);
// static void checkPrivateDecls(DefExpr* def);
static void checkParsedVar(VarSymbol* var);
static void checkFunction(FnSymbol* fn);
static void checkExportedNames();
static void includedStrictNames(ModuleSymbol* mod);
static void checkModule(ModuleSymbol* mod);
static void setupForCheckExplicitDeinitCalls();
static void checkOperator(FnSymbol* fn);
static void checkUseStmt(UseStmt* use);

void
checkGeneratedAst() {
  setupForCheckExplicitDeinitCalls();

  forv_Vec(CallExpr, call, gCallExprs) {
    checkManagedClassKinds(call);
    checkNamedArguments(call);
    checkExplicitDeinitCalls(call);
  }

  forv_Vec(DefExpr, def, gDefExprs) {
    //
    // This test checks to see if query domains (e.g., '[?D]') are
    // used in places other than formal argument type specifiers.
    //
    if (!isFnSymbol(def->parentSymbol)) {
      if (CallExpr* type = toCallExpr(def->exprType)) {
        if (type->isNamed("chpl__buildArrayRuntimeType")) {
          if (CallExpr* domainExpr = toCallExpr(type->get(1))) {
            DefExpr* queryDomain = toDefExpr(domainExpr->get(1));
            if (queryDomain) {
              USR_FATAL_CONT(queryDomain,
                             "Domain query expressions may currently only be used in formal argument types");
            }
          }
        }
      }
    }

    // checkPrivateDecls(def);
  }

  forv_Vec(VarSymbol, var, gVarSymbols) {
    checkParsedVar(var);
  }

  forv_Vec(FnSymbol, fn, gFnSymbols) {
    checkFunction(fn);
    checkOperator(fn);
  }

  forv_Vec(ModuleSymbol, mod, gModuleSymbols) {
    includedStrictNames(mod);
    checkModule(mod);
  }

  checkExportedNames();

  checkDefersAfterParsing();

  forv_Vec(UseStmt, use, gUseStmts) {
    checkUseStmt(use);
  }
}


static void
checkNamedArguments(CallExpr* call) {
  Vec<const char*> names;

  for_actuals(expr, call) {
    if (NamedExpr* named = toNamedExpr(expr)) {
      forv_Vec(const char, name, names) {
        if (!strcmp(name, named->name))
          USR_FATAL_CONT(named,
                         "The named argument '%s' is used more "
                         "than once in the same function call.",
                         name);
      }

      names.add(named->name);
    }
  }
}

static const char* getClassKindSpecifier(CallExpr* call) {
  if (call->isPrimitive(PRIM_TO_UNMANAGED_CLASS) ||
      call->isPrimitive(PRIM_TO_UNMANAGED_CLASS_CHECKED) ||
      call->isNamed("_to_unmanaged"))
    return "unmanaged";
  if (call->isPrimitive(PRIM_TO_BORROWED_CLASS) ||
      call->isPrimitive(PRIM_TO_BORROWED_CLASS_CHECKED) ||
      call->isNamed("_to_borrowed"))
    return "borrowed";
  if (call->isNamed("_owned"))
    return "owned";
  if (call->isNamed("_shared"))
    return "shared";

  if (call->isPrimitive(PRIM_NEW) && call->numActuals() >= 1) {
    if (NamedExpr* ne = toNamedExpr(call->get(1))) {
      if (ne->name == astr_chpl_manager) {
        Type* t = ne->actual->typeInfo();
        if (t == dtBorrowed)
          return "borrowed";
        if (t == dtUnmanaged)
          return "unmanaged";
        if (t == dtOwned)
          return "owned";
        if (t == dtShared)
          return "shared";
      }
    }
  }

  return NULL;
}

static void checkManagedClassKinds(CallExpr* call) {
  const char* outer = getClassKindSpecifier(call);

  if (outer != NULL) {
    Expr* inner = call->get(1);
    // skip management decorator if present
    if (NamedExpr* ne = toNamedExpr(inner))
      if (ne->name == astr_chpl_manager)
        inner = call->get(2);

    CallExpr* innerCall = toCallExpr(inner);
    if (innerCall) {
      const char* inner = getClassKindSpecifier(innerCall);
      if (inner != NULL) {
        USR_FATAL_CONT(call,
                       "Type expression uses multiple class kinds: %s %s",
                       outer, inner);
      }
    }

    if (call->numActuals() >= 1) {
      if (SymExpr* se = toSymExpr(call->get(1))) {
        if (se->symbol() == gUninstantiated) {
          USR_FATAL(call, "Please use %s class? instead of %s?", outer, outer);
        }
      }
    }
  }
}

static VarSymbol*  deinitStrLiteral;

static void setupForCheckExplicitDeinitCalls() {
  SET_LINENO(rootModule); // for --minimal-modules
  deinitStrLiteral = new_CStringSymbol("deinit");
}

//
// Report error for the following cases:
//
// * non-method call e.g. deinit(args...)
//     ==> CallExpr(UnresolvedSymExpr("deinit"), args...)
//
// * method call e.g. cc.deinit(args...)
//     ==> CallExpr(UnresolvedSymExpr("."), CString("deinit"), args...)
//
static void checkExplicitDeinitCalls(CallExpr* call) {
  if (UnresolvedSymExpr* target = toUnresolvedSymExpr(call->baseExpr)) {
    if (target->unresolved == astrDeinit)
      USR_FATAL_CONT(call, "direct calls to deinit() are not allowed");
    else if (target->unresolved == astrSdot)
      if (SymExpr* arg2 = toSymExpr(call->get(2)))
        if (arg2->symbol() == deinitStrLiteral)
          // OK to invoke explicitly from chpl__delete()
          // which is our internal implementation of 'delete' statements.
          if (strcmp(call->parentSymbol->name, "chpl__delete") &&
              strcmp(call->parentSymbol->name, "chpl__deleteWithAllocator"))
            USR_FATAL_CONT(call, "direct calls to deinit() are not allowed");
  }
}

//
// Comment this out for now in favor of code running in 'dyno'. Eventually
// we will remove this check, hopefully at the same time as the rest of
// the code in this pass.
//
/*
static void checkPrivateDecls(DefExpr* def) {
  if (def->sym->hasFlag(FLAG_PRIVATE) == true) {
    // The symbol has been declared private.
    if (def->inTree()) {

      if (isTypeSymbol(def->sym) || def->sym->hasFlag(FLAG_TYPE_VARIABLE)) {
        USR_FATAL_CONT(def, "Can't apply private to types yet");
      }

      if (isFnSymbol(def->parentSymbol) == true) {
        // The parent symbol of this definition is a FnSymbol.
        // Private symbols at the function scope are meaningless
        // because there is no way for anything outside the function
        // to access ts locals, so warn the user.
        USR_WARN(def,
                 "Private declarations within function bodies "
                 "are meaningless");

        def->sym->removeFlag(FLAG_PRIVATE);

      } else if (ModuleSymbol* mod = toModuleSymbol(def->parentSymbol)) {
        FnSymbol* fn = toFnSymbol(def->sym);

        // The parent symbol is a module symbol.  Could still be invalid.
        if (fn != NULL && fn->isMethod() == true) {
          USR_FATAL_CONT(def,
                         "Can't apply private to the fields or methods of "
                         "a class or record yet");

        } else if (mod->block != def->parentExpr) {
          for (Expr* cur = def->parentExpr; cur; cur = cur->parentExpr) {
            if (cur == mod->block)
              break;

            if (BlockStmt* block = toBlockStmt(cur)) {
              // Scopeless blocks are used to define multiple symbols, for
              // instance.  Those are valid "nested" blocks for private symbols.
              if (block->blockTag != BLOCK_SCOPELESS) {
                // The block in which we are defined is not the top level module
                // block.  Private symbols at this scope are meaningless, so warn
                // the user.
                USR_WARN(def,
                         "Private declarations within nested blocks "
                         "are meaningless");

                def->sym->removeFlag(FLAG_PRIVATE);
                break;
              }

            } else {
              // There are many situations which could lead to this else branch.
              // Most of them will not reach here due to being banned at parse
              // time.  However, those that aren't excluded by syntax errors will
              // be caught here.
              USR_WARN(def,
                       "Private declarations are meaningless outside "
                       "of module level declarations");

              def->sym->removeFlag(FLAG_PRIVATE);
              break;
            }
          }
        }

      } else if (TypeSymbol* t = toTypeSymbol(def->parentSymbol)) {
        if (isAggregateType(t->type) == true) {
          USR_FATAL_CONT(def,
                         "Can't apply private to the fields or methods "
                         "of a class or record yet");
        }
      }
    }
  }
}
*/


static void
checkParsedVar(VarSymbol* var) {
  if (var->defPoint->init && var->defPoint->init->isNoInitExpr()) {
    if (var->hasFlag(FLAG_CONST))
      USR_FATAL_CONT(var, "const variables specified with noinit must be explicitly initialized.");
  }

  //
  // Verify that config variables are only at Module scope i.e. it is
  // an error if any config variable is not an immediate child of a
  // module

  if (var->hasFlag(FLAG_CONFIG) &&
      isModuleSymbol(var->defPoint->parentSymbol) == false) {
    const char* varType = NULL;

    if (var->hasFlag(FLAG_PARAM))
      varType = "parameters";
    else if (var->hasFlag(FLAG_CONST))
      varType = "constants";
    else
      varType = "variables";

    USR_FATAL_CONT(var->defPoint,
                   "Configuration %s are allowed only at module scope.", varType);
  }

  // Export vars are not yet supported
  if (var->hasFlag(FLAG_EXPORT))
    USR_FATAL_CONT(var->defPoint, "Export variables are not yet supported");
}


static void
checkFunction(FnSymbol* fn) {

  // Chapel doesn't really support procedures with no-op bodies (a
  // semicolon only).  Doing so is likely to cause confusion for C
  // programmers who will think of it as a prototype, but we don't
  // support prototypes, so require such programmers to type the
  // empty body instead.  This is consistent with the current draft
  // of the spec as well.
  bool doErrorAboutNoBody = false;
  if (fn->hasFlag(FLAG_NO_FN_BODY) && !fn->hasFlag(FLAG_EXTERN))
    if (!isInterfaceSymbol(fn->defPoint->parentSymbol))
      doErrorAboutNoBody = true;

  if (fn->hasFlag(FLAG_ANONYMOUS_FN))
    doErrorAboutNoBody = false;

  if (doErrorAboutNoBody)
    USR_FATAL_CONT(fn, "no-op procedures are only legal for extern "
                       "functions");

  if (fn->hasFlag(FLAG_EXTERN) && !fn->hasFlag(FLAG_NO_FN_BODY))
    USR_FATAL_CONT(fn, "Extern functions cannot have a body");

  if (fn->hasFlag(FLAG_EXTERN) && fn->throwsError())
    USR_FATAL_CONT(fn, "Extern functions cannot throw errors.");

  if (fn->hasFlag(FLAG_EXPORT) && fn->where != NULL)
    USR_FATAL_CONT(fn, "Exported functions cannot have where clauses.");


  if ((fn->name == astrThis) && fn->hasFlag(FLAG_NO_PARENS))
    USR_FATAL_CONT(fn, "method 'this' must have parentheses");

  if (!strcmp(fn->name, "these") && fn->hasFlag(FLAG_NO_PARENS))
    USR_FATAL_CONT(fn, "method 'these' must have parentheses");

  if (fn->thisTag != INTENT_BLANK && fn->isMethod() == false) {
    if (fn->thisTag == INTENT_TYPE) {
      USR_FATAL_CONT(fn, "Missing type for secondary type method");
    } else {
      USR_FATAL_CONT(fn, "'this' intents can only be applied to methods");
    }
  }

  if (fn->hasFlag(FLAG_OVERRIDE) && fn->_this == NULL) {
    USR_FATAL("'override' cannot be applied to non-method '%s'", fn->name);
  }

  if (fn->retTag == RET_TYPE || fn->retTag == RET_PARAM) {
    for_formals(formal, fn) {
      if (formal->intent == INTENT_OUT ||
          formal->intent == INTENT_INOUT) {
        USR_FATAL_CONT(formal,
                       "Cannot use %s in a "
                       "function returning with '%s' intent",
                       intentDescrString(formal->intent),
                       retTagDescrString(fn->retTag));
      }
    }
  }

  if (fn->isIterator()) {
    for_formals(formal, fn) {
      if (formal->intent == INTENT_OUT) {
        USR_FATAL_CONT(formal, "out intent is not yet supported for iterators");
      } else if (formal->intent == INTENT_INOUT) {
        USR_FATAL_CONT(formal, "inout intent is not yet supported for iterators");
      }
    }
  }
}

static void checkOperator(FnSymbol* fn) {
  if (!fn->hasFlag(FLAG_OPERATOR)) {
    if (isAstrOpName(fn->name)) {
      USR_FATAL_CONT(fn,
                     "Operators cannot be declared without the operator keyword");
    }
  } else {
    if (!isAstrOpName(fn->name)) {
      USR_FATAL_CONT(fn, "'%s' is not a legal operator name", fn->name);
    }
  }
}

static void includedStrictNames(ModuleSymbol* mod) {
  if (mod->defPoint == NULL) {
    return;
  }

  if (mod->hasFlag(FLAG_INCLUDED_MODULE)) {
    ModuleSymbol* parent = mod->defPoint->getModule();

    // module name should match file name
    const char* fname = filenameToModulename(parent->astloc.filename());
    if (fname != parent->name) {
      USR_FATAL("Cannot include modules from a module whose name doesn't match its filename");
    }

    // parent module must be top-level in its file.
    // in is not necessarily a top-level module, though.
    ModuleSymbol* lastParentSameFile = parent;
    for (ModuleSymbol* cur = parent;
         cur != NULL && cur->defPoint != NULL;
         cur = cur->defPoint->getModule()) {
      if (parent->astloc.filename() == cur->astloc.filename()) {
        lastParentSameFile = cur;
      } else {
        break;
      }
    }

    if (lastParentSameFile != parent) {
      USR_FATAL(parent, "Cannot include module from an in-line nested module");
    }
  }
}

//
// This is a special test to ensure that there are no instances of a return
// or yield statement at the top level of a module.  This "special" semantic
// check is needed to resolve 4 test applications that failed once the
// insertion of a Module init function was moved to a later pass.
//
// Those tests have historically relied on the matching call in checkFunction
// which was executed when scanning the module initFunction.
//
// This is probably a good anchor for a family of tests of this form.
//

static void
checkModule(ModuleSymbol* mod) {
  std::vector<CallExpr*> calls;
  collectCallExprs(mod->block, calls);
  for_vector(CallExpr, call, calls) {
    if (call->parentSymbol == mod) {
      if (call->isPrimitive(PRIM_RETURN)) {
        USR_FATAL_CONT(call, "return statement is not in a function or iterator");
      } else if (call->isPrimitive(PRIM_YIELD)) {
        USR_FATAL_CONT(call, "yield statement is outside an iterator");
      }
    }
  }
}

static void
checkExportedNames()
{
  // The right side of the map is a dummy Boolean.
  // We are just using the map to implement a set.
  HashMap<const char*, StringHashFns, bool> names;
  forv_Vec(FnSymbol, fn, gFnSymbols) {
    if (!fn->hasFlag(FLAG_EXPORT))
      continue;

    const char* name = fn->cname;
    if (names.get(name))
      USR_FATAL_CONT(fn, "The name %s cannot be exported twice from the same compilation unit.", name);
    names.put(name, true);
  }
}

/* check for 'public use M only' and warn in that event since
   it does nothing. (public use does not bring in the module named
   unless it renames the module e.g. 'public use M as M only' */
static void checkUseStmt(UseStmt* use) {
  if (!use->isPrivate && !use->isARename() && use->hasOnlyNothing()) {
    const char* name = "M";
    if (auto used = toUnresolvedSymExpr(use->src))
      name = used->unresolved;
    USR_WARN(use, "'public use %s only;' has no effect", name);
    USR_PRINT("try 'public import %s;' or 'public use %s as %s only;'",
              name, name, name);
  }
}
