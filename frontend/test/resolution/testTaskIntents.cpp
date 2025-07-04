/*
 * Copyright 2021-2025 Hewlett Packard Enterprise Development LP
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

#include "test-resolution.h"

#include "chpl/framework/ErrorWriter.h"
#include "chpl/parsing/parsing-queries.h"
#include "chpl/resolution/ResolvedVisitor.h"
#include "chpl/resolution/resolution-queries.h"
#include "chpl/resolution/scope-queries.h"
#include "chpl/types/all-types.h"
#include "chpl/uast/For.h"
#include "chpl/uast/Identifier.h"
#include "chpl/uast/Module.h"
#include "chpl/uast/Qualifier.h"
#include "chpl/uast/Record.h"
#include "chpl/uast/Variable.h"
#include "chpl/uast/While.h"

#include <algorithm>
#include <sstream>
#include <map>

static bool debug = false;
static bool verbose = false;

std::vector<const ErrorBase*> errors;

static const Module* parseModule(Context* context, const char* src) {
  auto path = UniqueString::get(context, "input.chpl");
  std::string contents = src;
  setFileText(context, path, contents);

  const ModuleVec& vec = parseToplevel(context, path);
  assert(vec.size() == 1);

  return vec[0];
}

//
// Collects the types of Decls and Identifiers
//
struct Collector {
  using RV = ResolvedVisitor<Collector>;

  std::multimap<std::string, QualifiedType> declTypes;

  std::multimap<std::string, ID> declIds;
  std::multimap<std::string, ResolvedExpression> idents;
  std::multimap<std::string, ResolvedExpression> shadowVars;
  std::multimap<std::string, ID> shadowVarIds;

  Collector() { }

  bool enter(const uast::NamedDecl* decl, RV& rv) {
    if (rv.hasAst(decl)) {
      const ResolvedExpression& rr = rv.byAst(decl);
      auto name = decl->name().str();
      declTypes.emplace(name, rr.type());

      if (decl->isTaskVar() || decl->isReduceIntent()) {
        shadowVars.emplace(name, rr);
        shadowVarIds.emplace(name, decl->id());
      } else {
        declIds.emplace(name, decl->id());
      }
    }

    return true;
  }

  bool enter(const uast::Identifier* ident, RV& rv) {
    if (rv.hasAst(ident)) {
      const ResolvedExpression& rr = rv.byAst(ident);
      idents.emplace(ident->name().str(), rr);
    }

    return true;
  }

  bool enter(const uast::TypeQuery* tq, RV& rv) {
    if (rv.hasAst(tq)) {
      const ResolvedExpression& rr = rv.byAst(tq);
      declTypes.emplace(tq->name().str(), rr.type());
    }

    return true;
  }

  void exit(const uast::Call* call, RV& rv) {
    if (call->isTuple()) return;

    //
    // TODO: Eventually this logic should be moved into ResolvedVisitor as
    // a helper method.
    //
    if (rv.hasAst(call)) {
      const ResolvedExpression& result = rv.byAst(call);
      if (result.mostSpecific().isEmpty() == false) {
        const TypedFnSignature* sig = result.mostSpecific().only().fn();
        auto fn = resolveFunction(rv.rc(), sig, result.poiScope());

        ResolvedVisitor<Collector> newRV(rv.rc(), nullptr, *this, fn->resolutionById());
        auto untyped = idToAst(rv.rc()->context(), sig->id());
        assert(untyped->id() == sig->id());
        untyped->traverse(newRV);
      }
    }
  }

  bool enter(const uast::OpCall* op, RV& rv) {
    if (op->op() == "reduce=") {
      bool foundAction = false;
      for (auto& aa : rv.byAst(op).associatedActions()) {
        if (aa.action() == AssociatedAction::REDUCE_ASSIGN) {
          foundAction = true;
        }
      }
      assert(foundAction);
    }
    return true;
  }
  void exit(const uast::OpCall* op, RV& rv) {
  }

  bool enter(const uast::AstNode* ast, RV& rv) {
    return true;
  }
  void exit(const uast::AstNode* ast, RV& rv) {
  }

  QualifiedType onlyDecl(std::string name) {
    assert(declTypes.count(name) == 1);
    return declTypes.find(name)->second;
  }

  ResolvedExpression onlyIdent(std::string name) {
    assert(idents.count(name) == 1);
    return idents.find(name)->second;
  }

  const ResolvedExpression& onlyShadow(std::string name) {
    assert(shadowVars.count(name) == 1);
    return shadowVars.find(name)->second;
  }

  ID onlyShadowId(std::string name) {
    assert(shadowVarIds.count(name) == 1);
    return shadowVarIds.find(name)->second;
  }

  ID onlyDeclId(std::string name) {
    assert(declIds.count(name) == 1);
    return declIds.find(name)->second;
  }

  void dump() {
    std::ostringstream stream;
    auto kind = chpl::StringifyKind::CHPL_SYNTAX;
    stream << "===== Collector =====\n";
    stream << "--- Decls ---\n";
    for (auto p : declTypes) {
      stream << p.first << ": ";
      p.second.stringify(stream, kind);
      stream << "\n";
    }
    stream << "--- Idents ---\n";
    for (auto p : idents) {
      stream << p.first << ": ";
      p.second.stringify(stream, kind);
      stream << "\n";
    }
    stream << "=====================\n";

    std::cout << stream.str();
  }
};

static void printErrors(const ErrorGuard& guard) {
  if (!verbose) {
    printf("Found %lu errors.\n\n", guard.errors().size());
  } else {
    printf("======== Errors ========\n");
    guard.printErrors();
    printf("========================\n\n");
  }
}

static Collector customHelper(std::string program, ResolutionContext* rc, Module* moduleOut = nullptr, bool fail = false) {
  Context* context = rc->context();
  ErrorGuard guard(context);

  const Module* m = parseModule(context, program.c_str());

  const ResolutionResultByPostorderID& rr = resolveModule(context, m->id());
  Collector pc;
  ResolvedVisitor<Collector> rv(rc, m, pc, rr);
  m->traverse(rv);

  if (debug) {
    printf("\n################################################\n");
    printf("Resolving program:");
    if (fail) printf(" (note: resolution failure expected.)");
    printf("\n");
    printf("========================\n");
    printf("%s\n", program.c_str());
    printf("========================\n");

    if (guard.errors().size()) printErrors(guard);

    if (verbose) pc.dump();
    printf("\n");
  }

  if (!fail) {
    assert(!guard.realizeErrors(/* countWarnings */ false));
  }

  return pc;
}

// helper for running task intent tests
static void kindHelper(Qualifier kind, const std::string& constructName, const std::string& initializerValue, const std::string& expectedType) {
  Context* context = buildStdContext();
  ResolutionContext rcval(context);
  auto rc = &rcval;

  std::string program;
  program += "class C {}\n";
  program += "var x = ";
  program += initializerValue;
  program += ";\n";
  program += constructName;
  if (constructName == "forall" || constructName == "coforall") {
    program += " i in 1..10";
  }
  program += " with (";
  program += qualifierToString(kind);
  program += " x) {\n";
  program += "  var y = x;\n";
  program += "}\n";

  auto col = customHelper(program, rc);

  // Test shadow variable type is as expected
  {
    Qualifier useKind = kind;
    // const task intent corresponds to const shadow variable
    if (useKind == Qualifier::CONST_INTENT) {
      useKind = Qualifier::CONST_VAR;
    }
    auto expected = qualifierToString(useKind) + std::string(" ") + expectedType;
    QualifiedType shadowX = col.onlyIdent("x").type();

    std::ostringstream stream;
    shadowX.stringify(stream, chpl::StringifyKind::CHPL_SYNTAX);
    assert(stream.str() == expected);
  }


  // Test type of variable assigned value of shadow variable
  {
    QualifiedType yType = col.onlyDecl("y");
    std::ostringstream stream;
    yType.type()->stringify(stream, chpl::StringifyKind::CHPL_SYNTAX);
    assert(stream.str() == expectedType);
  }

  // Test that the shadow variable points to the original
  {
    auto& rr = col.onlyShadow("x");
    assert(rr.toId() == col.onlyDeclId("x"));
  }
}

static void testKinds() {
  // all potentially-task-spawning constructs
  static const std::string constructNames[] = {
    "forall",
    "coforall",
    "cobegin",
    "begin"
  };
  // all valid task intent kinds
  static const Qualifier qualifiers[] = {
    Qualifier::REF,
    Qualifier::IN,
    Qualifier::CONST_INTENT,
    Qualifier::CONST_IN,
    Qualifier::CONST_REF,
    // inout and out intents disallowed due to data races
    //Qualifier::INOUT,
    //Qualifier::OUT,
    // meaning of default task intent not well-defined
    //Qualifier::DEFAULT_INTENT
  };

  // for each construct, test all intent kinds
  for (const auto& constructName : constructNames) {
    for (const auto& qualifier : qualifiers) {
      kindHelper(qualifier, constructName, "0", "int(64)");
      kindHelper(qualifier, constructName, "new unmanaged C()", "unmanaged C");
    }
  }
}

static void testTaskVar() {
  auto ctx = buildStdContext();
  ErrorGuard guard(ctx);
  auto program =
    R"""(
      var outer : int;
      forall i in 1..10 with (var x : int,
                              ref outer) {
        outer = x;
        var y = x;
      }
    )""";

  auto vars = resolveTypesOfVariables(ctx, program, {"x", "y"});
  assert(guard.realizeErrors() == 0);

  for (auto& [name, var] : vars) {
    std::ignore = name; // avoid unused variable warning with old GCC (7.5)
    assert(!var.isUnknownOrErroneous() && var.type()->isIntType());
  }
}

// helper for running reduce intent tests
static void reduceHelper(const std::string& constructName, const char* op, const char* opAssign) {
  assert(constructName == "forall" || constructName == "coforall");

  Context* context = buildStdContext();
  ResolutionContext rcval(context);
  auto rc = &rcval;

  // Very simple test focusing on scope resolution
  std::string program;
  program += "var x = 0;\n";
  program += constructName;
  program += " i in 1..10 with (";
  program += op;
  program += " reduce x) {\n";
  program += "x ";
  program += opAssign;
  program += " i;\n";
  program += "}\n";

  auto col = customHelper(program, rc);

  // Test shadow variable type is as expected
  {
    const auto intType = IntType::get(context, 0);
    QualifiedType expected = QualifiedType(Qualifier::VAR, intType);
    QualifiedType shadowX = col.onlyShadow("x").type();
    assert(expected == shadowX);
  }

  // Test that the shadow variable points to the original
  assert(col.onlyShadow("x").toId() == col.onlyDeclId("x"));
  // Test that inner ident points to reduce shadow
  assert(col.onlyIdent("x").toId() == col.onlyShadowId("x"));
}

static void testReduce() {
  // all reduce-intent supporting constructs
  static const std::string constructNames[] = {
      "forall",
      "coforall",
      // reduce intents not defined for begin and cobegin
      // "cobegin",
      // "begin"
  };
  for (const auto& constructName : constructNames) {
    reduceHelper(constructName, "+", "+=");
    reduceHelper(constructName, "+", "reduce=");
  }
}

static void testReduceAssignNotVariable() {
  auto ctx = buildStdContext();
  auto program = "foreach 1..10 { (1) reduce= 1; }";
  ErrorGuard guard(ctx);
  std::ignore = resolveTypesOfVariables(ctx, program, {});

  assert(guard.numErrors() == 1);
  assert(guard.errors()[0]->type() == ErrorType::ReductionAssignNonIdentifier);
  guard.realizeErrors();
}

static void testReduceAssignNotReduceIntent() {
  auto ctx = buildStdContext();
  auto program = "var x = 0; forall 1..10 { x reduce= 1; }";
  ErrorGuard guard(ctx);
  std::ignore = resolveTypesOfVariables(ctx, program, {});

  assert(guard.numErrors() == 1);
  assert(guard.errors()[0]->type() == ErrorType::ReductionAssignNotReduceIntent);
  guard.realizeErrors();
}

static void testReduceAssignChangesType() {
  auto ctx = buildStdContext();
  auto program =
    R"""(
      var a = 0;
      forall i in 1..10 with (minmax reduce a) {
        a reduce= i;
      }
      var x = 0;
    )""";

  ErrorGuard guard(ctx);
  std::ignore = resolveTypesOfVariables(ctx, program, {});

  assert(guard.numErrors() == 1);
  assert(guard.errors()[0]->type() == ErrorType::ReductionIntentChangesType);
  guard.realizeErrors();
}

//
// TODO:
// - implicit shadow variables (flat, nested)
// - reduce intents for begin/cobegin, if those are implemented in future
// - type resolve in-intents where type can change (e.g. array slices)
//
int main(int argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--debug") == 0) {
      debug = true;
    } else if (strcmp(argv[i], "--verbose") == 0) {
      verbose = true;
    }
  }

  // perform actual tests
  testKinds();
  testTaskVar();
  testReduce();
  testReduceAssignNotVariable();
  testReduceAssignNotReduceIntent();
  testReduceAssignChangesType();

  printf("\nAll tests passed successfully.\n");

  return 0;
}
