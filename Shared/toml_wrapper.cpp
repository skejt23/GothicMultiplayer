
/*
MIT License

Copyright (c) 2022 Gothic Multiplayer Team (pampi, skejt23, mecio)

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

#include "shared/toml_wrapper.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {
std::string FormatOutputWithSectionSpacing(const std::string& content) {
  std::string output;
  output.reserve(content.size() + 512);

  std::istringstream input_ss(content);
  std::string line;
  bool first_line = true;
  bool previous_was_empty = false;

  while (std::getline(input_ss, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }

    // Check if this is a section header (starts with "# ---")
    // and if we need to insert a blank line before it.
    if (line.find("# ---") == 0) {
      if (!first_line && !previous_was_empty) {
        output += '\n';
      }
    }

    output += line;
    output += '\n';

    first_line = false;
    previous_was_empty = line.empty();
  }

  return output;
}
}  // namespace

TomlWrapper TomlWrapper::CreateFromFile(std::string_view file_path) {
  TomlWrapper val;
  val.data_ = toml::parse<toml::ordered_type_config>(std::string(file_path));
  return val;
}

void TomlWrapper::Serialize(std::string_view file_path) const {
  if (!data_.is_empty()) {
    std::stringstream ss;
    ss << data_;
    std::string content = ss.str();

    std::string output = FormatOutputWithSectionSpacing(content);

    std::ofstream ofs(std::string(file_path), std::ios_base::out | std::ios_base::binary);
    ofs << output;
  }
}