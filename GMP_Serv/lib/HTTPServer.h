#pragma once

#include <future>
#include <httplib.h>
class HTTPServer {
private:
  int port;
  httplib::Server server;
  std::future<void> http_thread_future_;

public:
  HTTPServer();
  ~HTTPServer();

  void Start(int);
};
