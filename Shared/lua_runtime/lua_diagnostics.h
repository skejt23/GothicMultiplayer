/*
MIT License

Copyright (c) 2026 Gothic Multiplayer Team.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include <string>
#include <string_view>

#include "sol/sol.hpp"

namespace lua::diagnostics {

struct ErrorContext {
  std::string resource;
  std::string operation;
  std::string subject;
};

// Installs a message handler used by subsequently created protected functions.
// Lua invokes this handler before unwinding the failing call, which allows it to
// capture the call stack, native-call arguments, and active local variables.
void InstallErrorHandler(sol::state& lua);

// Logs one atomic multi-line report so call stacks and locals cannot interleave
// with messages from other threads.
void LogRuntimeError(std::string_view error, const ErrorContext& context = {});
void LogSyntaxError(std::string_view error, const ErrorContext& context = {});

}  // namespace lua::diagnostics
