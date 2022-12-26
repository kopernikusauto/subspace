// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "cir/lib/source_span.h"
#include "cir/lib/syntax/type_reference.h"
#include "cir/llvm.h"
#include "subspace/prelude.h"
#include "subspace/union/union.h"

namespace cir::syntax {

enum LetClangTypeTag {
  Return,
  Variable,
};
using LetClangType =
    sus::Union<sus_value_types((LetClangTypeTag::Return, clang::QualType),
                               (LetClangTypeTag::Variable, clang::VarDecl&))>;

struct Let {
  Let(u32 id, TypeReference type, SourceSpan span, LetClangType clang_type)
      : id(id), type(type), span(span), clang_type(clang_type) {}

  u32 id;
  TypeReference type;
  SourceSpan span;

  LetClangType clang_type;

  std::string to_string() const& noexcept {
    // TODO: Use fmt library (or add such to subspace).
    std::ostringstream s;
    s << "let _" << id.primitive_value << ": " << type.to_string() << ";";
    return s.str();
  }
};

}  // namespace cir::syntax
