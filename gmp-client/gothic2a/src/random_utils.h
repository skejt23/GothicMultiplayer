#pragma once

#include <random>

namespace gmp::client::random {

inline std::mt19937 &Engine() {
  static std::mt19937 engine{std::random_device{}()};
  return engine;
}

inline int Int(int min, int max) {
  std::uniform_int_distribution<int> dist(min, max);
  return dist(Engine());
}

}  // namespace gmp::client::random
