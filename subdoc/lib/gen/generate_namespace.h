// Copyright 2023 Google LLC
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

#include "subdoc/lib/database.h"
#include "subdoc/lib/gen/html_writer.h"
#include "subdoc/lib/gen/markdown_to_html.h"
#include "subdoc/lib/gen/options.h"
#include "sus/prelude.h"
#include "sus/result/result.h"

namespace subdoc::gen {

sus::Result<void, MarkdownToHtmlError> generate_namespace(
    const Database& db, const NamespaceElement& element,
    sus::Vec<const NamespaceElement*> ancestors,
    const Options& options) noexcept;

sus::Result<void, MarkdownToHtmlError> generate_namespace_reference(
    HtmlWriter::OpenLi& li, const NamespaceElement& element,
    ParseMarkdownPageState& page_state) noexcept;

}  // namespace subdoc::gen
