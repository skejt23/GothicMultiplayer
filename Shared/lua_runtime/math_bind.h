/*
MIT License

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

#include "sol/sol.hpp"

#include <glm/glm.hpp>

namespace lua {
namespace types {

struct Vec2 {
  float x = 0.0f;
  float y = 0.0f;

  Vec2() = default;
  explicit Vec2(float value);
  Vec2(float x, float y);

  float len() const;
  float len2() const;
  float lenApprox() const;
  float distance(const Vec2& vec) const;
  Vec2& normalize();
  Vec2& normalizeSafe();
  Vec2& normalizeApprox();
  void set(float x, float y);
  bool isEqualEps(const Vec2& vec) const;
  Vec2 abs() const;
  static void swap(Vec2& vec1, Vec2& vec2);
  static Vec2 min(const Vec2& vec1, const Vec2& vec2);
  static Vec2 max(const Vec2& vec1, const Vec2& vec2);
  static Vec2 prod(const Vec2& vec1, const Vec2& vec2);
  static float dot(const Vec2& vec1, const Vec2& vec2);
  static Vec2 lerp(float t, const Vec2& v1, const Vec2& v2);
};

struct Vec3 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;

  Vec3() = default;
  explicit Vec3(float value);
  Vec3(float x, float y, float z);

  float len() const;
  float len2() const;
  float lenApprox() const;
  float distance(const Vec3& vec) const;
  float distance2d(const Vec3& vec) const;
  Vec3& normalize();
  Vec3& normalizeSafe();
  Vec3& normalizeApprox();
  void set(float x, float y, float z);
  bool isEqualEps(const Vec3& vec) const;
  Vec3 abs() const;
  Vec3 reflect(const Vec3& normal) const;
  static void swap(Vec3& vec1, Vec3& vec2);
  static Vec3 min(const Vec3& vec1, const Vec3& vec2);
  static Vec3 max(const Vec3& vec1, const Vec3& vec2);
  static Vec3 prod(const Vec3& vec1, const Vec3& vec2);
  static float dot(const Vec3& vec1, const Vec3& vec2);
  static Vec3 cross(const Vec3& vec1, const Vec3& vec2);
  static Vec3 lerp(float t, const Vec3& v1, const Vec3& v2);
};

struct Vec4 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float w = 0.0f;

  Vec4() = default;
  explicit Vec4(float value);
  Vec4(float x, float y, float z, float w);

  float len() const;
  float len2() const;
  float lenApprox() const;
  Vec4& normalize();
  Vec4& normalizeSafe();
  Vec4& normalizeApprox();
  void set(float x, float y, float z, float w);
  bool isEqualEps(const Vec4& vec) const;
  Vec4 abs() const;
  static void swap(Vec4& vec1, Vec4& vec2);
  static Vec4 min(const Vec4& vec1, const Vec4& vec2);
  static Vec4 max(const Vec4& vec1, const Vec4& vec2);
  static Vec4 prod(const Vec4& vec1, const Vec4& vec2);
  static float dot(const Vec4& vec1, const Vec4& vec2);
  static Vec4 lerp(float t, const Vec4& v1, const Vec4& v2);
};

struct Mat3 {
  Mat3();
  explicit Mat3(float value);
  Mat3(const Vec3& v0, const Vec3& v1, const Vec3& v2);

  Mat3(const Mat3& other) = default;
  Mat3(Mat3&& other) noexcept = default;
  Mat3& operator=(const Mat3& other) = default;
  Mat3& operator=(Mat3&& other) noexcept = default;
  ~Mat3() = default;

  void makeIdentity();
  void makeZero();
  void makeOrthonormal();
  bool isUpper3x3Orthonormal() const;
  Mat3 transpose() const;
  Mat3 inverse() const;
  Vec3 rotate(const Vec3& vec) const;
  Vec3 getRightVector() const;
  void setRightVector(const Vec3& vec);
  Vec3 getUpVector() const;
  void setUpVector(const Vec3& vec);
  Vec3 getAtVector() const;
  void setAtVector(const Vec3& vec);
  void resetRotation();
  Mat3 extractRotation() const;
  Vec3 extractScaling() const;
  void postRotateX(float angle_degrees);
  void postRotateY(float angle_degrees);
  void postRotateZ(float angle_degrees);
  void preScale(const Vec3& scale);
  void postScale(const Vec3& scale);
  static void swap(Mat3& mat1, Mat3& mat2);

  const float* data() const;

  glm::mat3 mat_{};
};

struct Mat4 {
  Mat4();
  explicit Mat4(float value);
  Mat4(const Vec4& v0, const Vec4& v1, const Vec4& v2, const Vec4& v3);

  Mat4(const Mat4& other) = default;
  Mat4(Mat4&& other) noexcept = default;
  Mat4& operator=(const Mat4& other) = default;
  Mat4& operator=(Mat4&& other) noexcept = default;
  ~Mat4() = default;

  void makeIdentity();
  void makeZero();
  void makeOrthonormal();
  bool isUpper3x3Orthonormal() const;
  Mat4 transpose() const;
  Mat4 inverse() const;
  Mat4 inverseLinTrafo() const;
  Vec3 rotate(const Vec3& vec) const;
  Vec3 getRightVector() const;
  void setRightVector(const Vec3& vec);
  Vec3 getAtVector() const;
  void setAtVector(const Vec3& vec);
  Vec3 getUpVector() const;
  void setUpVector(const Vec3& vec);
  Vec3 getTranslation() const;
  void setTranslation(const Vec3& vec);
  void resetRotation();
  Mat3 extractRotation() const;
  Vec3 extractScaling() const;
  void postRotateX(float angle_degrees);
  void postRotateY(float angle_degrees);
  void postRotateZ(float angle_degrees);
  void preScale(const Vec3& scale);
  void postScale(const Vec3& scale);
  static void swap(Mat4& mat1, Mat4& mat2);
  static Mat4 lookAt(const Vec3& from, const Vec3& to, const Vec3& up);

  const float* data() const;

  glm::mat4 mat_{};
};

struct Quat {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float w = 1.0f;

  Quat();
  explicit Quat(float w);
  Quat(float x, float y, float z, float w);

  Mat3 toMat3() const;
  void fromMat3(const Mat3& mat);
  Mat4 toMat4() const;
  void fromMat4(const Mat4& mat);
  Vec3 toEuler() const;
  void fromEuler(const Vec3& vec);
  Vec4 toAxisAngle() const;
  void fromAxisAngle(const Vec3& axis, float angle);
  void makeIdentity();
  bool isIdentity() const;
  float len() const;
  float len2() const;
  float lenApprox() const;
  Quat& normalize();
  Quat& normalizeSafe();
  Quat& normalizeApprox();
  void set(float x, float y, float z, float w);
  Quat inverse() const;
  Quat conjugate() const;
  static float dot(const Quat& quat1, const Quat& quat2);
  static Quat lerp(float t, const Quat& q1, const Quat& q2);
  static Quat slerp(float t, const Quat& q1, const Quat& q2);
  static Quat squad(float t, const Quat& q1, const Quat& q2, const Quat& q3);
  static Quat lookRotation(const Vec3& forward, const Vec3& up);
};

}  // namespace types

namespace bindings {

// Bind math classes to Lua state
void BindMath(sol::state& lua);

}  // namespace bindings
}  // namespace lua