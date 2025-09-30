#include "HTTPServer.h"

#include <spdlog/spdlog.h>

#include <fstream>
#include <future>

using namespace std;
using namespace httplib;

HTTPServer::HTTPServer() {
}

HTTPServer::~HTTPServer() {
  server.stop();
}

void HTTPServer::Start(int port) {
  server.Get("/wb_file", [](const Request& req, Response& res) {
    ifstream file("world.wbm", ios::binary | ios::in);
    if (!file.is_open()) {
      res.set_content("EMPTY", "text/plain");
      return;
    }
    stringstream buffer;
    buffer << file.rdbuf();
    res.set_content(buffer.str(), "text/plain");
  });

  http_thread_future_ = std::async([this, port] {
    bool result = server.listen("0.0.0.0", port + 1);
    if (!result) {
      SPDLOG_ERROR("File server cannot start");
    }
  });
}