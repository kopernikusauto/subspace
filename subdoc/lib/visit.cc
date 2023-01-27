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

#include "subdoc/lib/visit.h"

#include "subdoc/lib/database.h"
#include "subdoc/lib/doc_attributes.h"
#include "subdoc/lib/parse_comment.h"
#include "subdoc/lib/path.h"
#include "subdoc/lib/record_type.h"
#include "subdoc/lib/unique_symbol.h"
#include "subspace/assertions/unreachable.h"
#include "subspace/mem/swap.h"
#include "subspace/prelude.h"

namespace subdoc {

struct DiagnosticIds {
  static DiagnosticIds with(clang::ASTContext& ast_cx) noexcept {
    return DiagnosticIds{
        .superceded_comment = ast_cx.getDiagnostics().getCustomDiagID(
            clang::DiagnosticsEngine::Error,
            "ignored API comment, superceded by comment at %0"),
        .malformed_comment = ast_cx.getDiagnostics().getCustomDiagID(
            clang::DiagnosticsEngine::Error, "malformed API comment: %0"),
    };
  }

  const unsigned int superceded_comment;
  const unsigned int malformed_comment;
};

static bool should_skip_decl(clang::Decl* decl) {
  auto* ndecl = clang::dyn_cast<clang::NamedDecl>(decl);
  if (!ndecl) return true;

  // TODO: These could be configurable. As well as user-defined namespaces to
  // skip.
  if (path_contains_namespace(ndecl,
                              sus::choice<Namespace::Tag::Anonymous>())) {
    return true;
  }
  if (path_contains_namespace(
          ndecl, sus::choice<Namespace::Tag::Named>("__private"))) {
    return true;
  }
  if (path_is_private(ndecl)) {
    return true;
  }
  return false;
}

static clang::RawComment* get_raw_comment(clang::Decl* decl) {
  return decl->getASTContext().getRawCommentForDeclNoCache(decl);
}

struct CommentAndDocAttributes {
  DocAttributes attributes;
  Comment comment;
};

static CommentAndDocAttributes make_db_comment(
    const DiagnosticIds& diag_ids, clang::Decl* decl,
    const clang::RawComment* raw) noexcept {
  auto& ast_cx = decl->getASTContext();
  auto& src_manager = ast_cx.getSourceManager();
  if (raw) {
    sus::result::Result<ParsedComment, ParseCommentError> comment_result =
        parse_comment(ast_cx, *raw);
    if (comment_result.is_ok()) {
      auto&& [attrs, string] = sus::move(comment_result).unwrap();
      return CommentAndDocAttributes{
          .attributes = attrs,
          .comment =
              Comment(string, raw->getBeginLoc().printToString(src_manager)),
      };
    }
    ast_cx.getDiagnostics()
        .Report(raw->getBeginLoc(), diag_ids.malformed_comment)
        .AddString(sus::move(comment_result).unwrap_err().message);
  }
  return CommentAndDocAttributes();
}

class Visitor : public clang::RecursiveASTVisitor<Visitor> {
 public:
  Visitor(VisitCx& cx, Database& docs_db, DiagnosticIds ids)
      : cx_(cx), docs_db_(docs_db), diag_ids_(sus::move(ids)) {}
  bool shouldVisitLambdaBody() const { return false; }

  bool VisitStaticAssertDecl(clang::StaticAssertDecl*) noexcept {
    // llvm::errs() << "StaticAssertDecl\n";
    return true;
  }

  bool VisitNamespaceDecl(clang::NamespaceDecl* decl) noexcept {
    if (should_skip_decl(decl)) return true;
    clang::RawComment* raw_comment = get_raw_comment(decl);

    auto [attrs, comment] = make_db_comment(diag_ids_, decl, raw_comment);
    auto ne = NamespaceElement(collect_namespace_path(decl), sus::move(comment),
                               decl->getNameAsString());
    NamespaceElement& parent = [&]() -> NamespaceElement& {
      clang::DeclContext* context = decl->getDeclContext();
      // TODO: Save the linkage spec (`extern "C"`) so we can show it.
      while (clang::isa<clang::LinkageSpecDecl>(context))
        context = context->getParent();

      if (clang::isa<clang::TranslationUnitDecl>(context)) {
        return docs_db_.find_namespace_mut(nullptr).unwrap();
      } else {
        return docs_db_
            .find_namespace_mut(clang::cast<clang::NamespaceDecl>(context))
            .unwrap();
      }
    }();
    add_namespace_to_db(decl, sus::move(attrs), sus::move(ne),
                        mref(parent.namespaces));
    return true;
  }

  bool VisitRecordDecl(clang::RecordDecl* decl) noexcept {
    if (should_skip_decl(decl)) return true;
    clang::RawComment* raw_comment = get_raw_comment(decl);

    if (auto* cxxdecl = clang::dyn_cast<clang::CXXRecordDecl>(decl)) {
      // It's a C++ class. So it may have a template decl which has the template
      // parameters. And it may be a specialization.
      // llvm::errs() << "Visiting CXX class with template, specialization
      // kind:"
      //              << cxxdecl->getTemplateSpecializationKind() << "\n";
    }

    RecordType type = [&]() {
      if (decl->isStruct()) return RecordType::Struct;
      if (decl->isUnion()) return RecordType::Union;
      return RecordType::Class;
    }();

    // TODO: collect_record_path() too.
    auto [attrs, comment] = make_db_comment(diag_ids_, decl, raw_comment);
    auto ce =
        RecordElement(collect_namespace_path(decl), sus::move(comment),
                      decl->getNameAsString(), collect_record_path(decl), type);

    clang::DeclContext* context = decl->getDeclContext();
    // TODO: Save the linkage spec (`extern "C"`) so we can show it.
    while (clang::isa<clang::LinkageSpecDecl>(context))
      context = context->getParent();

    if (clang::isa<clang::TranslationUnitDecl>(context)) {
      NamespaceElement& parent = docs_db_.find_namespace_mut(nullptr).unwrap();
      add_comment_to_db(decl, sus::move(attrs), sus::move(ce),
                        mref(parent.records));
    } else if (clang::isa<clang::NamespaceDecl>(context)) {
      NamespaceElement& parent =
          docs_db_
              .find_namespace_mut(clang::cast<clang::NamespaceDecl>(context))
              .unwrap();
      add_comment_to_db(decl, sus::move(attrs), sus::move(ce),
                        mref(parent.records));
    } else {
      assert(clang::isa<clang::RecordDecl>(context));
      if (sus::Option<RecordElement&> parent =
              docs_db_.find_record_mut(clang::cast<clang::RecordDecl>(context));
          parent.is_some()) {
        add_comment_to_db(decl, sus::move(attrs), sus::move(ce),
                          mref(parent->records));
      }
    }
    return true;
  }

  bool VisitFieldDecl(clang::FieldDecl* decl) noexcept {
    if (should_skip_decl(decl)) return true;
    clang::RawComment* raw_comment = get_raw_comment(decl);

    auto [attrs, comment] = make_db_comment(diag_ids_, decl, raw_comment);
    auto fe = FieldElement(collect_namespace_path(decl), sus::move(comment),
                           std::string(decl->getName()), decl->getType(),
                           collect_record_path(decl->getParent()),
                           // Static data members are found in VisitVarDecl.
                           FieldElement::NonStatic);

    assert(clang::isa<clang::RecordDecl>(decl->getDeclContext()));
    if (sus::Option<RecordElement&> parent = docs_db_.find_record_mut(
            clang::cast<clang::RecordDecl>(decl->getDeclContext()));
        parent.is_some()) {
      add_comment_to_db(decl, sus::move(attrs), sus::move(fe),
                        mref(parent->fields));
    }
    return true;
  }

  bool VisitVarDecl(clang::VarDecl* decl) noexcept {
    // Static data members are represented as VarDecl, not FieldDecl. So we
    // visit VarDecls but only for them.
    if (!decl->isStaticDataMember()) return true;

    if (should_skip_decl(decl)) return true;
    clang::RawComment* raw_comment = get_raw_comment(decl);

    auto [attrs, comment] = make_db_comment(diag_ids_, decl, raw_comment);
    auto* record = clang::cast<clang::RecordDecl>(decl->getDeclContext());
    auto fe =
        FieldElement(collect_namespace_path(decl), sus::move(comment),
                     std::string(decl->getName()), decl->getType(),
                     collect_record_path(record),
                     // NonStatic data members are found in VisitFieldDecl.
                     FieldElement::Static);

    if (sus::Option<RecordElement&> parent = docs_db_.find_record_mut(
            clang::cast<clang::RecordDecl>(decl->getDeclContext()));
        parent.is_some()) {
      add_comment_to_db(decl, sus::move(attrs), sus::move(fe),
                        mref(parent->fields));
    }
    return true;
  }

  bool VisitEnumDecl(clang::EnumDecl* decl) noexcept {
    if (should_skip_decl(decl)) return true;
    clang::RawComment* raw_comment = get_raw_comment(decl);
    if (raw_comment)
      llvm::errs() << "EnumDecl " << raw_comment->getKind() << "\n";
    return true;
  }

  bool VisitTypedefDecl(clang::TypedefDecl* decl) noexcept {
    if (should_skip_decl(decl)) return true;
    clang::RawComment* raw_comment = get_raw_comment(decl);
    if (raw_comment)
      llvm::errs() << "TypedefDecl " << raw_comment->getKind() << "\n";
    return true;
  }

  bool VisitTypeAliasDecl(clang::TypeAliasDecl* decl) noexcept {
    if (should_skip_decl(decl)) return true;
    clang::RawComment* raw_comment = get_raw_comment(decl);
    if (raw_comment)
      llvm::errs() << "TypeAliasDecl " << raw_comment->getKind() << "\n";
    return true;
  }

  bool VisitFunctionDecl(clang::FunctionDecl* decl) noexcept {
    if (should_skip_decl(decl)) return true;
    clang::RawComment* raw_comment = get_raw_comment(decl);

    auto signature = decl->getQualifiedNameAsString();
    // TODO: Add parameters.
    if (auto* mdecl = clang::dyn_cast<clang::CXXMethodDecl>(decl)) {
      signature += " ";
      signature += mdecl->getMethodQualifiers().getAsString();
      switch (mdecl->getRefQualifier()) {
        case clang::RQ_None: break;
        case clang::RQ_LValue: signature += "&"; break;
        case clang::RQ_RValue: signature += "&&"; break;
      }
    }
    auto [attrs, comment] = make_db_comment(diag_ids_, decl, raw_comment);
    auto fe = FunctionElement(collect_namespace_path(decl), sus::move(comment),
                              decl->getNameAsString(), sus::move(signature),
                              decl->getReturnType());

    // TODO: Allow constructors to overload with different commments.
    // Specifically, default cons, other cons, copy cons, move cons.

    // TODO: It's possible to overload a method in a base class. What should we
    // show then?

    // TODO: Store what base class methods comes from so we can denote them
    // as being inherited.

    if (clang::isa<clang::CXXConstructorDecl>(decl)) {
      assert(clang::isa<clang::RecordDecl>(decl->getDeclContext()));
      if (sus::Option<RecordElement&> parent = docs_db_.find_record_mut(
              clang::cast<clang::RecordDecl>(decl->getDeclContext()));
          parent.is_some()) {
        add_function_overload_to_db(decl, sus::move(attrs), sus::move(fe),
                                    mref(parent->ctors));
      }
    } else if (clang::isa<clang::CXXDestructorDecl>(decl)) {
      assert(clang::isa<clang::RecordDecl>(decl->getDeclContext()));
      if (sus::Option<RecordElement&> parent = docs_db_.find_record_mut(
              clang::cast<clang::RecordDecl>(decl->getDeclContext()));
          parent.is_some()) {
        add_function_overload_to_db(decl, sus::move(attrs), sus::move(fe),
                                    mref(parent->dtors));
      }
    } else if (clang::isa<clang::CXXConversionDecl>(decl)) {
      assert(clang::isa<clang::RecordDecl>(decl->getDeclContext()));
      if (sus::Option<RecordElement&> parent = docs_db_.find_record_mut(
              clang::cast<clang::RecordDecl>(decl->getDeclContext()));
          parent.is_some()) {
        add_function_overload_to_db(decl, sus::move(attrs), sus::move(fe),
                                    mref(parent->conversions));
      }
    } else if (clang::isa<clang::CXXMethodDecl>(decl)) {
      assert(clang::isa<clang::RecordDecl>(decl->getDeclContext()));
      if (sus::Option<RecordElement&> parent = docs_db_.find_record_mut(
              clang::cast<clang::RecordDecl>(decl->getDeclContext()));
          parent.is_some()) {
        auto* mdecl = clang::cast<clang::CXXMethodDecl>(decl);
        fe.overloads[0u].method = sus::some(MethodSpecific{
            .is_static = mdecl->isStatic(),
            .is_volatile = mdecl->isVolatile(),
            .is_virtual = mdecl->isVirtual(),
            .qualifier =
                [mdecl]() {
                  switch (mdecl->getRefQualifier()) {
                    case clang::RQ_None:
                      if (mdecl->isConst())
                        return MethodQualifier::Const;
                      else
                        return MethodQualifier::Mutable;
                    case clang::RQ_LValue:
                      if (mdecl->isConst())
                        return MethodQualifier::ConstLValue;
                      else
                        return MethodQualifier::MutableLValue;
                    case clang::RQ_RValue:
                      if (mdecl->isConst())
                        return MethodQualifier::ConstRValue;
                      else
                        return MethodQualifier::MutableRValue;
                  }
                  sus::unreachable();
                }(),
        });
        add_function_overload_to_db(decl, sus::move(attrs), sus::move(fe),
                                    mref(parent->methods));
      }
    } else if (clang::isa<clang::CXXDeductionGuideDecl>(decl)) {
      clang::DeclContext* context = decl->getDeclContext();
      // TODO: Save the linkage spec (`extern "C"`) so we can show it.
      while (clang::isa<clang::LinkageSpecDecl>(context))
        context = context->getParent();

      // TODO: How do we get from here to the class that the deduction guide is
      // for reliably? getCorrespondingConstructor() would work if it's
      // generated only. Will the getDeclContext find it?
      assert(clang::isa<clang::NamespaceDecl>(decl->getDeclContext()));
      /* TODO:
      if (sus::Option<RecordElement&> parent = docs_db_.find_record_mut(
              clang::cast<clang::RecordDecl>(decl->getDeclContext()));
          parent.is_some()) {
        add_function_overload_to_db(decl, sus::move(attrs),
                                    sus::move(fe), mref(parent->deductions));
      }
      */
    } else {
      if (sus::Option<NamespaceElement&> parent =
              docs_db_.find_namespace_mut(find_nearest_namespace(decl));
          parent.is_some()) {
        add_function_overload_to_db(decl, sus::move(attrs), sus::move(fe),
                                    mref(parent->functions));
      }
    }

    return true;
  }

 private:
  template <class MapT>
  void add_function_overload_to_db(clang::FunctionDecl* decl,
                                   DocAttributes attrs,
                                   FunctionElement db_element,
                                   MapT& db_map) noexcept {
    FunctionId key = key_for_function(decl, attrs.overload_set);
    bool add_overload = true;
    auto it = db_map.find(key);
    if (it == db_map.end()) {
      db_map.emplace(key, std::move(db_element));
      add_overload = false;
    } else if (!it->second.has_comment()) {
      // Steal the comment.
      sus::mem::swap(db_map.at(key).comment, db_element.comment);
    } else if (!db_element.has_comment()) {
      // Leave the existing comment in place.
    } else if (db_element.comment.begin_loc == it->second.comment.begin_loc) {
      // We already visited this thing, from another translation unit.
    } else {
      auto& ast_cx = decl->getASTContext();
      const FunctionElement& old_element = it->second;
      ast_cx.getDiagnostics()
          .Report(attrs.location, diag_ids_.superceded_comment)
          .AddString(old_element.comment.begin_loc);
    }

    if (add_overload) {
      ::sus::check_with_message(
          db_element.overloads.len() == 1u,
          *"Expected to add FunctionElement with 1 overload");
      db_map.at(key).overloads.push(sus::move(db_element.overloads[0u]));
    }
  }

  template <class MapT>
  void add_namespace_to_db(clang::NamespaceDecl* decl, DocAttributes attrs,
                           NamespaceElement db_element, MapT& db_map) noexcept {
    auto key = key_for_namespace(decl);
    auto it = db_map.find(key);
    if (it == db_map.end()) {
      db_map.emplace(key, std::move(db_element));
    } else if (!it->second.has_comment()) {
      // Steal the comment.
      sus::mem::swap(db_map.at(key).comment, db_element.comment);
    } else if (!db_element.has_comment()) {
      // Leave the existing comment in place, do nothing.
    } else if (db_element.comment.begin_loc == it->second.comment.begin_loc) {
      // We already visited this thing, from another translation unit.
    } else {
      auto& ast_cx = decl->getASTContext();
      const NamespaceElement& old_element = it->second;
      ast_cx.getDiagnostics()
          .Report(attrs.location, diag_ids_.superceded_comment)
          .AddString(old_element.comment.begin_loc);
    }
  }

  template <class ElementT, class MapT>
    requires ::sus::convert::SameOrSubclassOf<ElementT*, CommentElement*>
  void add_comment_to_db(clang::Decl* decl, DocAttributes attrs,
                         ElementT db_element, MapT& db_map) noexcept {
    UniqueSymbol uniq = unique_from_decl(decl);
    auto it = db_map.find(uniq);
    if (it == db_map.end()) {
      db_map.emplace(uniq, std::move(db_element));
    } else if (!it->second.has_comment()) {
      // Steal the comment.
      sus::mem::swap(db_map.at(uniq).comment, db_element.comment);
    } else if (!db_element.has_comment()) {
      // Leave the existing comment in place, do nothing.
    } else if (db_element.comment.begin_loc == it->second.comment.begin_loc) {
      // We already visited this thing, from another translation unit.
    } else {
      auto& ast_cx = decl->getASTContext();
      const ElementT& old_element = it->second;
      ast_cx.getDiagnostics()
          .Report(attrs.location, diag_ids_.superceded_comment)
          .AddString(old_element.comment.begin_loc);
    }
  }

  VisitCx& cx_;
  Database& docs_db_;
  DiagnosticIds diag_ids_;
};

class AstConsumer : public clang::ASTConsumer {
 public:
  AstConsumer(VisitCx& cx, Database& docs_db) : cx_(cx), docs_db_(docs_db) {}

  bool HandleTopLevelDecl(clang::DeclGroupRef group_ref) noexcept override {
    for (clang::Decl* decl : group_ref) {
      if (!Visitor(cx_, docs_db_, DiagnosticIds::with(decl->getASTContext()))
               .TraverseDecl(decl))
        return false;
    }
    return true;
  }

 private:
  VisitCx& cx_;
  Database& docs_db_;
};

std::unique_ptr<clang::FrontendAction> VisitorFactory::create() noexcept {
  return std::make_unique<VisitorAction>(cx, docs_db);
}

bool VisitorAction::PrepareToExecuteAction(
    clang::CompilerInstance& inst) noexcept {
  // Speed things up by skipping things we're not looking at.
  inst.getFrontendOpts().SkipFunctionBodies = true;
  return true;
}

std::unique_ptr<clang::ASTConsumer> VisitorAction::CreateASTConsumer(
    clang::CompilerInstance&, llvm::StringRef) noexcept {
  return std::make_unique<AstConsumer>(cx, docs_db);
}

}  // namespace subdoc