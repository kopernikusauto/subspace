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

/// A macro that runs a macro against any arguments.
///
/// This is useful to join together macros, for instance to run one macro
/// against the output of another macro.
///
/// # Example
/// ```
/// #define f(s) hello(s)
/// sus_eval_macro(f, "world")
/// // Evaluates to: `hello("world")`
/// ```
#define sus_eval_macro(x, ...) x(__VA_ARGS__)
