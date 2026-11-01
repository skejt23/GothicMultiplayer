#include <gtest/gtest.h>

#include <string_view>

#include "master_server_client.h"

#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
#error Master server requests require OpenSSL support
#endif

namespace {

TEST(MasterServerEndpointTest, ParsesDefaultHttpsPort) {
  const auto endpoint = master_server::ParseEndpoint("https://master.gothic-multiplayer.pl/servers");
  ASSERT_TRUE(endpoint.has_value());
  EXPECT_EQ(endpoint->host, "master.gothic-multiplayer.pl");
  EXPECT_EQ(endpoint->path, "/servers");
  EXPECT_EQ(endpoint->port, 443);
  EXPECT_TRUE(endpoint->use_https);
}

TEST(MasterServerEndpointTest, PreservesExplicitPortAndQuery) {
  const auto endpoint = master_server::ParseEndpoint(" https://master.example.com:8443/servers?region=eu \r\n");
  ASSERT_TRUE(endpoint.has_value());
  EXPECT_EQ(endpoint->host, "master.example.com");
  EXPECT_EQ(endpoint->path, "/servers?region=eu");
  EXPECT_EQ(endpoint->port, 8443);
}

TEST(MasterServerEndpointTest, ParsesIpv6Address) {
  const auto endpoint = master_server::ParseEndpoint("https://[::1]:8443/servers");
  ASSERT_TRUE(endpoint.has_value());
  EXPECT_EQ(endpoint->host, "::1");
  EXPECT_EQ(endpoint->port, 8443);
}

TEST(MasterServerEndpointTest, RejectsPlaintextAndMissingScheme) {
  for (const std::string_view url : {"http://master.example.com/servers", "master.example.com/servers", "//master.example.com/servers"}) {
    EXPECT_FALSE(master_server::ParseEndpoint(url).has_value()) << url;
  }
}

TEST(MasterServerEndpointTest, RejectsMissingHostAndInvalidPorts) {
  for (const std::string_view url : {"https://", "https:///servers", "https://master.example.com:/servers", "https://master.example.com:0/servers",
                                     "https://master.example.com:65536/servers", "https://master.example.com:abc/servers", "https://[::1/servers"}) {
    EXPECT_FALSE(master_server::ParseEndpoint(url).has_value()) << url;
  }
}

TEST(MasterServerEndpointTest, CreatesTlsClientForHostnameAndIpv6) {
  for (const std::string_view url : {"https://master.example.com/servers", "https://[::1]:8443/servers"}) {
    const auto endpoint = master_server::ParseEndpoint(url);
    ASSERT_TRUE(endpoint.has_value());
    const auto client = master_server::CreateClient(*endpoint);
    ASSERT_NE(client, nullptr);
    EXPECT_NE(client->ssl_context(), nullptr);
  }
}

TEST(MasterServerEndpointTest, RejectsCredentialsAndMalformedHosts) {
  for (const std::string_view url : {"https://owner@master.example.com/servers", "https://master example.com/servers",
                                     "https://master.example.com#fragment", "https://[::1]]/servers"}) {
    EXPECT_FALSE(master_server::ParseEndpoint(url).has_value()) << url;
  }
}

TEST(MasterServerEndpointTest, ClientFactoryCannotFallBackToPlaintextForMalformedHost) {
  master_server::EndpointInfo endpoint;
  endpoint.host = "malformed host";
  EXPECT_EQ(master_server::CreateClient(endpoint), nullptr);
}

TEST(MasterServerEndpointTest, ClientFactoryRejectsPlaintext) {
  master_server::EndpointInfo endpoint;
  endpoint.host = "master.example.com";
  endpoint.use_https = false;
  EXPECT_EQ(master_server::CreateClient(endpoint), nullptr);
}

}  // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
