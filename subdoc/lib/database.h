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

#include <unordered_map>

#include "subdoc/lib/friendly_names.h"
#include "subdoc/lib/method_qualifier.h"
#include "subdoc/lib/path.h"
#include "subdoc/lib/record_type.h"
#include "subdoc/lib/unique_symbol.h"
#include "subdoc/llvm.h"
#include "subspace/choice/choice.h"
#include "subspace/prelude.h"

namespace subdoc {

struct Comment {
  Comment() = default;
  Comment(std::string raw_text, std::string begin_loc)
      : raw_text(raw_text), begin_loc(sus::move(begin_loc)) {}

  std::string raw_text;
  std::string begin_loc;

  std::string_view summary() const& { return raw_text; }
  std::string_view summary() && = delete;
};

struct CommentElement {
  explicit CommentElement(sus::Vec<Namespace> namespace_path, Comment comment,
                          std::string name)
      : namespace_path(sus::move(namespace_path)),
        comment(sus::move(comment)),
        name(sus::move(name)) {
    // All elements have the Global namespace in their path.
    assert(this->namespace_path.len() > 0u);
  }

  sus::Vec<Namespace> namespace_path;
  Comment comment;
  std::string name;

  bool has_comment() const { return !comment.raw_text.empty(); }
};

struct MethodSpecific {
  bool is_static;
  bool is_volatile;
  bool is_virtual;

  // TODO: Find the Database element of the root ancestor virtual method,
  // when this one is virtual, and link to it.

  MethodQualifier qualifier;
};

struct FunctionOverload {
  std::string signature;
  sus::Option<MethodSpecific> method;

  // TODO: `noexcept` stuff from FunctionDecl::getExceptionSpecType().
};

struct FunctionElement : public CommentElement {
  explicit FunctionElement(sus::Vec<Namespace> containing_namespaces,
                           Comment comment, std::string name,
                           std::string signature,
                           clang::QualType return_qual_type)
      : CommentElement(sus::move(containing_namespaces), sus::move(comment),
                       sus::move(name)),
        return_type_name(friendly_type_name(return_qual_type)) {
    overloads.push(FunctionOverload{
        .signature = sus::move(signature),
        .method = sus::none(),
    });
  }

  std::string return_type_name;

  sus::Vec<FunctionOverload> overloads;

  bool has_any_comments() const noexcept { return has_comment(); }

  sus::Option<const CommentElement&> find_comment(
      std::string_view comment_loc) const noexcept {
    if (comment.begin_loc.ends_with(comment_loc))
      return sus::some(*this);
    else
      return sus::none();
  }
};

struct FieldElement : public CommentElement {
  enum StaticType {
    Static,
    NonStatic,
  };

  explicit FieldElement(sus::Vec<Namespace> containing_namespaces,
                        Comment comment, std::string name,
                        clang::QualType qual_type,
                        sus::Vec<std::string> record_path, StaticType is_static)
      : CommentElement(sus::move(containing_namespaces), sus::move(comment),
                       sus::move(name)),
        record_path(sus::move(record_path)),
        type_name(friendly_type_name(qual_type)),
        is_const(qual_type.getQualifiers().hasConst()),
        is_volatile(qual_type.getQualifiers().hasVolatile()),
        is_static(is_static) {}

  sus::Vec<std::string> record_path;
  std::string type_name;
  bool is_const;
  bool is_volatile;
  StaticType is_static;

  bool has_any_comments() const noexcept { return has_comment(); }

  sus::Option<const CommentElement&> find_comment(
      std::string_view comment_loc) const noexcept {
    if (comment.begin_loc.ends_with(comment_loc))
      return sus::some(*this);
    else
      return sus::none();
  }
};

struct NamespaceId {
  NamespaceId(std::string name) : name(sus::move(name)) {}

  std::string name;

  bool operator==(const NamespaceId&) const = default;

  struct Hash {
    std::size_t operator()(const NamespaceId& k) const {
      return std::hash<std::string>()(k.name);
    }
  };
};

struct FunctionId {
  FunctionId(std::string name, bool is_static, u32 overload_set)
      : name(sus::move(name)),
        is_static(is_static),
        overload_set(overload_set) {}

  std::string name;
  bool is_static;
  u32 overload_set;

  bool operator==(const FunctionId&) const = default;

  struct Hash {
    std::size_t operator()(const FunctionId& k) const {
      return (std::hash<std::string>()(k.name) << size_t{k.is_static}) +
             std::hash<u32>()(k.overload_set);
    }
  };
};

struct RecordElement : public CommentElement {
  explicit RecordElement(sus::Vec<Namespace> containing_namespaces,
                         Comment comment, std::string name,
                         sus::Vec<std::string> record_path,
                         RecordType record_type)
      : CommentElement(sus::move(containing_namespaces), sus::move(comment),
                       sus::move(name)),
        record_path(sus::move(record_path)),
        record_type(record_type) {}

  // TODO: Template parameters and requires clause.

  // TODO: Link to all base classes.

  /// The classes in which this class is nested.
  ///
  /// In this example, the class_path would be {S, R}.
  /// ```
  ///   struct R { struct S { struct T{}; }; };
  /// ```
  sus::Vec<std::string> class_path;
  sus::Vec<std::string> record_path;
  RecordType record_type;

  std::unordered_map<UniqueSymbol, RecordElement> records;
  std::unordered_map<UniqueSymbol, FieldElement> fields;
  std::unordered_map<FunctionId, FunctionElement, FunctionId::Hash> deductions;
  std::unordered_map<FunctionId, FunctionElement, FunctionId::Hash> ctors;
  std::unordered_map<FunctionId, FunctionElement, FunctionId::Hash> dtors;
  std::unordered_map<FunctionId, FunctionElement, FunctionId::Hash> conversions;
  std::unordered_map<FunctionId, FunctionElement, FunctionId::Hash> methods;

  bool has_any_comments() const noexcept {
    if (has_comment()) return true;
    for (const auto& [u, e] : records) {
      if (e.has_any_comments()) return true;
    }
    for (const auto& [u, e] : fields) {
      if (e.has_any_comments()) return true;
    }
    for (const auto& [u, e] : deductions) {
      if (e.has_any_comments()) return true;
    }
    for (const auto& [u, e] : ctors) {
      if (e.has_any_comments()) return true;
    }
    for (const auto& [u, e] : dtors) {
      if (e.has_any_comments()) return true;
    }
    for (const auto& [u, e] : conversions) {
      if (e.has_any_comments()) return true;
    }
    for (const auto& [u, e] : methods) {
      if (e.has_any_comments()) return true;
    }
    return false;
  }

  sus::Option<const CommentElement&> find_record_comment(
      std::string_view comment_loc) const noexcept {
    sus::Option<const CommentElement&> out;
    if (comment.begin_loc.ends_with(comment_loc)) {
      out.insert(*this);
      return out;
    }
    for (const auto& [u, e] : records) {
      out = e.find_record_comment(comment_loc);
      if (out.is_some()) return out;
    }
    return out;
  }

  sus::Option<const CommentElement&> find_method_comment(
      std::string_view comment_loc) const noexcept {
    sus::Option<const CommentElement&> out;
    for (const auto& [u, e] : methods) {
      out = e.find_comment(comment_loc);
      if (out.is_some()) return out;
    }
    return out;
  }

  sus::Option<const CommentElement&> find_field_comment(
      std::string_view comment_loc) const noexcept {
    sus::Option<const CommentElement&> out;
    for (const auto& [u, e] : fields) {
      out = e.find_comment(comment_loc);
      if (out.is_some()) return out;
    }
    return out;
  }
};

struct NamespaceElement : public CommentElement {
  explicit NamespaceElement(sus::Vec<Namespace> containing_namespaces,
                            Comment comment, std::string name)
      : CommentElement(sus::move(containing_namespaces), sus::move(comment),
                       sus::move(name)),
        // The front of `namespace_path` will be this NamespaceElement's
        // identity.
        namespace_name(namespace_path[0u]) {}

  Namespace namespace_name;
  std::unordered_map<NamespaceId, NamespaceElement, NamespaceId::Hash>
      namespaces;
  std::unordered_map<UniqueSymbol, RecordElement> records;
  std::unordered_map<FunctionId, FunctionElement, FunctionId::Hash> functions;

  bool has_any_comments() const noexcept {
    if (has_comment()) return true;
    for (const auto& [u, e] : namespaces) {
      if (e.has_any_comments()) return true;
    }
    for (const auto& [u, e] : records) {
      if (e.has_any_comments()) return true;
    }
    for (const auto& [u, e] : functions) {
      if (e.has_any_comments()) return true;
    }
    return false;
  }

  sus::Option<const CommentElement&> find_record_comment(
      std::string_view comment_loc) const noexcept {
    sus::Option<const CommentElement&> out;
    for (const auto& [u, e] : records) {
      out = e.find_record_comment(comment_loc);
      if (out.is_some()) return out;
    }
    for (const auto& [u, e] : namespaces) {
      out = e.find_record_comment(comment_loc);
      if (out.is_some()) return out;
    }
    return out;
  }

  sus::Option<const CommentElement&> find_namespace_comment(
      std::string_view comment_loc) const noexcept {
    sus::Option<const CommentElement&> out;
    if (comment.begin_loc.ends_with(comment_loc)) {
      out.insert(*this);
      return out;
    }
    for (const auto& [u, e] : namespaces) {
      out = e.find_namespace_comment(comment_loc);
      if (out.is_some()) return out;
    }
    return out;
  }

  sus::Option<const CommentElement&> find_function_comment(
      std::string_view comment_loc) const noexcept {
    sus::Option<const CommentElement&> out;
    for (const auto& [u, e] : namespaces) {
      out = e.find_function_comment(comment_loc);
      if (out.is_some()) return out;
    }
    for (const auto& [u, e] : functions) {
      out = e.find_comment(comment_loc);
      if (out.is_some()) return out;
    }
    return out;
  }

  sus::Option<const CommentElement&> find_method_comment(
      std::string_view comment_loc) const noexcept {
    sus::Option<const CommentElement&> out;
    for (const auto& [u, e] : records) {
      out = e.find_method_comment(comment_loc);
      if (out.is_some()) return out;
    }
    return out;
  }

  sus::Option<const CommentElement&> find_field_comment(
      std::string_view comment_loc) const noexcept {
    sus::Option<const CommentElement&> out;
    for (const auto& [u, e] : records) {
      out = e.find_field_comment(comment_loc);
      if (out.is_some()) return out;
    }
    return out;
  }
};

inline NamespaceId key_for_namespace(clang::NamespaceDecl* decl) noexcept {
  return NamespaceId(decl->getNameAsString());
}

inline FunctionId key_for_function(clang::FunctionDecl* decl,
                                   sus::Option<u32> overload_set) noexcept {
  bool is_static = [&]() {
    if (auto* mdecl = clang::dyn_cast<clang::CXXMethodDecl>(decl))
      return mdecl->isStatic();
    return false;
  }();
  return FunctionId(decl->getNameAsString(), is_static,
                    sus::move(overload_set).unwrap_or_default());
}

struct Database {
  NamespaceElement global =
      NamespaceElement(sus::vec(Namespace::with<Namespace::Tag::Global>()),
                       Comment(), std::string());

  bool has_any_comments() const noexcept { return global.has_any_comments(); }

  sus::Option<NamespaceElement&> find_namespace_mut(
      clang::NamespaceDecl* ndecl) & noexcept {
    if (!ndecl) return sus::some(global);

    sus::Option<NamespaceElement&> parent_element = find_namespace_mut(
        clang::dyn_cast<clang::NamespaceDecl>(ndecl->getParent()));
    if (parent_element.is_none()) return sus::none();
    if (auto it = parent_element->namespaces.find(key_for_namespace(ndecl));
        it != parent_element->namespaces.end()) {
      return sus::some(it->second);
    } else {
      return sus::none();
    }
  }
  sus::Option<RecordElement&> find_record_mut(
      clang::RecordDecl* rdecl) & noexcept {
    sus::Option<NamespaceElement&> ne =
        find_namespace_mut(find_nearest_namespace(rdecl));
    return sus::move(ne).and_then(
        [&](NamespaceElement& e) { return find_record_mut_impl(rdecl, e); });
  }

  /// Finds a comment whose location ends with the `comment_loc` suffix.
  ///
  /// The suffix can be used to look for the line:column and ignore the
  /// filename in the comment location format `filename:line:col`.
  sus::Option<const CommentElement&> find_record_comment(
      std::string_view comment_loc) const& noexcept {
    return global.find_record_comment(comment_loc);
  }
  sus::Option<const CommentElement&> find_record_comment(
      std::string_view comment_loc) && = delete;

  /// Finds a comment whose location ends with the `comment_loc` suffix.
  ///
  /// The suffix can be used to look for the line:column and ignore the
  /// filename in the comment location format `filename:line:col`.
  sus::Option<const CommentElement&> find_namespace_comment(
      std::string_view comment_loc) const& noexcept {
    return global.find_namespace_comment(comment_loc);
  }
  sus::Option<const CommentElement&> find_namespace_comment(
      std::string_view comment_loc) && = delete;

  /// Finds a comment whose location ends with the `comment_loc` suffix.
  ///
  /// The suffix can be used to look for the line:column and ignore the
  /// filename in the comment location format `filename:line:col`.
  sus::Option<const CommentElement&> find_function_comment(
      std::string_view comment_loc) const& noexcept {
    return global.find_function_comment(comment_loc);
  }
  sus::Option<const CommentElement&> find_function_comment(
      std::string_view comment_loc) && = delete;

  /// Finds a comment whose location ends with the `comment_loc` suffix.
  ///
  /// The suffix can be used to look for the line:column and ignore the
  /// filename in the comment location format `filename:line:col`.
  sus::Option<const CommentElement&> find_method_comment(
      std::string_view comment_loc) const noexcept {
    return global.find_method_comment(comment_loc);
  }

  /// Finds a comment whose location ends with the `comment_loc` suffix.
  ///
  /// The suffix can be used to look for the line:column and ignore the
  /// filename in the comment location format `filename:line:col`.
  sus::Option<const CommentElement&> find_field_comment(
      std::string_view comment_loc) const& noexcept {
    return global.find_field_comment(comment_loc);
  }
  sus::Option<const CommentElement&> find_field_comment(
      std::string_view comment_loc) && = delete;

 private:
  sus::Option<RecordElement&> find_record_mut_impl(clang::RecordDecl* rdecl,
                                                   NamespaceElement& ne) & {
    if (auto* parent = clang::dyn_cast<clang::RecordDecl>(rdecl->getParent())) {
      if (sus::Option<RecordElement&> parent_element =
              find_record_mut_impl(parent, ne);
          parent_element.is_some()) {
        return sus::some(parent_element->records.at(unique_from_decl(rdecl)));
      } else {
        return sus::none();
      }
    } else {
      if (auto it = ne.records.find(unique_from_decl(rdecl));
          it != ne.records.end()) {
        return sus::some(it->second);
      } else {
        return sus::none();
      }
    }
  }
};

static_assert(sus::mem::Move<Database>);

}  // namespace subdoc