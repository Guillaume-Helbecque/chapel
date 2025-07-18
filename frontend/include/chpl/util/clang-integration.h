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

#ifndef CHPL_UTIL_CLANG_INTEGRATION_H
#define CHPL_UTIL_CLANG_INTEGRATION_H

#include "chpl/framework/ID.h"
#include "chpl/types/QualifiedType.h"
#include "chpl/util/memory.h"
#include "chpl/resolution/resolution-types.h"

#include "llvm/ADT/ArrayRef.h"

#include <string>
#include <vector>

#ifdef HAVE_LLVM
namespace clang {
  class DiagnosticOptions;
}
#endif

namespace chpl {
class Context;
class TemporaryFileResult;

namespace util {


/** Return the currently set clang flags.
    The first vector element is the clang to run (like argv[0]). */
const std::vector<std::string>& clangFlags(Context* context);

/** Set the clang flags for this revision if they have not been set already.
    The first vector element should be the clang to run (like argv[0]). */
void setClangFlags(Context* context, std::vector<std::string> clangFlags);

/** Initialize all LLVM targets */
void initializeLlvmTargets();

/** Given arguments to 'clang', convert them into arguments for 'cc1'.
    The first element of 'arg' should be which clang to use (like argv[0]). */
const std::vector<std::string>& getCC1Arguments(Context* context,
                                                std::vector<std::string> args,
                                                bool forGpuCodegen);

/** Given arguments to 'clang' and some code (normally, the contents of an
    extern block), create a precompiled header with clang and return
    its contents. */
const owned<TemporaryFileResult>&
createClangPrecompiledHeader(Context* context, ID externBlockId);

/** Given a TemporaryFileResult created from createClangPrecompiledHeader,
    returns 'true' if the passed name is present as a defined macro
    or declaration name and 'false' otherwise.
 */
const bool& precompiledHeaderContainsName(Context* context,
                                          const TemporaryFileResult* pch,
                                          UniqueString name);

/** Given a TemporaryFileResult created from createClangPrecompiledHeader,
    returns 'true' if the passed name is present as a defined function
    name and 'false' otherwise.
 */
const bool& precompiledHeaderContainsFunction(Context* context,
                                              const TemporaryFileResult* pch,
                                              UniqueString name);

/** Given a TemporaryFileResult created from createClangPrecompiledHeader,
    return the Chapel equivalent of the type of the symbol with the passed name
    if present, or an empty QualifiedType otherwise.
 */
const types::QualifiedType& precompiledHeaderTypeForSymbol(
    Context* context, const TemporaryFileResult* pch, UniqueString name);

/** Given a TemporaryFileResult created from createClangPrecompiledHeader,
    generate a TypedFnSignature from the function with the passed ID
    if present, or nullptr otherwise.
 */
const resolution::TypedFnSignature* const& precompiledHeaderSigForFn(
    Context* context, const TemporaryFileResult* pch, ID fnId);

/** Given a TemporaryFileResult created from createClangPrecompiledHeader,
    return the Chapel equivalent of the return type of the function
    with the passed name if present, or an empty QualifiedType otherwise.
 */
const types::QualifiedType& precompiledHeaderRetTypeForFn(
    Context* context, const TemporaryFileResult* pch, UniqueString name);

}  // end namespace util
}  // end namespace chpl

#endif
