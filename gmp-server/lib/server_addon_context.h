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

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "packets.h"


namespace gmp::addon {
class AddonVfs;
}

class ServerAddonContext {
public:
  ServerAddonContext();
  ~ServerAddonContext();

  ServerAddonContext(const ServerAddonContext&) = delete;
  ServerAddonContext& operator=(const ServerAddonContext&) = delete;

  struct Archive {
    AddonVdfInfoEntry descriptor;
    std::filesystem::path source_path;
  };

  struct Bundle {
    AddonBundleInfoEntry descriptor;
    std::filesystem::path archive_path;
    std::filesystem::path manifest_path;
  };

  bool Initialize(const std::vector<std::string>& configured_paths, const std::filesystem::path& server_root,
                  std::string& error);

  const std::vector<Archive>& Archives() const;
  const Bundle& TransportBundle() const;
  bool HasGothicDat() const;
  bool Empty() const;

private:
  std::vector<Archive> archives_;
  Bundle bundle_;
  std::unique_ptr<gmp::addon::AddonVfs> merged_vfs_;
  bool has_gothic_dat_{false};
};
