#pragma once

#include <httplib.h>

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

#include "master_server_endpoint.h"

namespace master_server {

inline std::unique_ptr<httplib::Client> CreateClient(const EndpointInfo& info) {
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
  if (!info.use_https || info.host.empty() || info.port <= 0 || info.port > 65535) {
    return nullptr;
  }

  const auto host = info.host.find(':') == std::string::npos ? info.host : "[" + info.host + "]";
  auto client = std::make_unique<httplib::Client>("https://" + host + ":" + std::to_string(info.port));
  if (!client->is_valid() || client->ssl_context() == nullptr) {
    return nullptr;
  }

  client->enable_server_certificate_verification(true);
  client->enable_server_hostname_verification(true);
#ifdef __linux__
  // Bundled OpenSSL may not use the distribution's CA directory by default.
  if (std::getenv("SSL_CERT_FILE") == nullptr && std::getenv("SSL_CERT_DIR") == nullptr) {
    for (const auto* ca_file : {"/etc/ssl/certs/ca-certificates.crt", "/etc/pki/tls/certs/ca-bundle.crt"}) {
      std::error_code error;
      if (std::filesystem::is_regular_file(ca_file, error)) {
        client->set_ca_cert_path(ca_file);
        break;
      }
    }
  }
#endif
  // Keep the token on the configured endpoint; do not follow redirects.
  client->set_follow_location(false);
  return client;
#else
  return nullptr;
#endif
}

}  // namespace master_server
