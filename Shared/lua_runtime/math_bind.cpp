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

#include "math_bind.h"

#include <algorithm>
#include <cmath>
#include <utility>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/fast_square_root.hpp>
#include <glm/gtx/norm.hpp>

namespace lua {
namespace types {

namespace {
constexpr float kEpsilon = 0.001f;

inline glm::vec3 ToGlm(const Vec3& vec) { return glm::vec3(vec.x, vec.y, vec.z); }
inline glm::vec4 ToGlm(const Vec4& vec) { return glm::vec4(vec.x, vec.y, vec.z, vec.w); }
inline Vec3 FromGlm(const glm::vec3& vec) { return Vec3{vec.x, vec.y, vec.z}; }
inline Vec4 FromGlm(const glm::vec4& vec) { return Vec4{vec.x, vec.y, vec.z, vec.w}; }

glm::mat3 MakeRowMatrix(const Vec3& r0, const Vec3& r1, const Vec3& r2) {
  glm::mat3 mat(1.0f);
  mat[0][0] = r0.x;
  mat[1][0] = r0.y;
  mat[2][0] = r0.z;
  mat[0][1] = r1.x;
  mat[1][1] = r1.y;
  mat[2][1] = r1.z;
  mat[0][2] = r2.x;
  mat[1][2] = r2.y;
  mat[2][2] = r2.z;
  return mat;
}

glm::mat4 MakeRowMatrix(const Vec4& r0, const Vec4& r1, const Vec4& r2, const Vec4& r3) {
  glm::mat4 mat(1.0f);
  mat[0][0] = r0.x;
  mat[1][0] = r0.y;
  mat[2][0] = r0.z;
  mat[3][0] = r0.w;
  mat[0][1] = r1.x;
  mat[1][1] = r1.y;
  mat[2][1] = r1.z;
  mat[3][1] = r1.w;
  mat[0][2] = r2.x;
  mat[1][2] = r2.y;
  mat[2][2] = r2.z;
  mat[3][2] = r2.w;
  mat[0][3] = r3.x;
  mat[1][3] = r3.y;
  mat[2][3] = r3.z;
  mat[3][3] = r3.w;
  return mat;
}
}  // namespace

// ---------------- Vec2 ----------------

Vec2::Vec2(float value) : x(value), y(value) {}

Vec2::Vec2(float x_in, float y_in) : x(x_in), y(y_in) {}

float Vec2::len() const { return std::sqrt(len2()); }

float Vec2::len2() const { return x * x + y * y; }

float Vec2::lenApprox() const {
  const float length_sq = len2();
  if (length_sq <= 0.0f) {
    return 0.0f;
  }
  return 1.0f / glm::fastInverseSqrt(length_sq);
}

float Vec2::distance(const Vec2& vec) const {
  const float dx = x - vec.x;
  const float dy = y - vec.y;
  return std::sqrt(dx * dx + dy * dy);
}

Vec2& Vec2::normalize() {
  const float length = len();
  if (length > 0.0f) {
    const float inv = 1.0f / length;
    x *= inv;
    y *= inv;
  }
  return *this;
}

Vec2& Vec2::normalizeSafe() {
  const float length = len();
  if (length > kEpsilon) {
    const float inv = 1.0f / length;
    x *= inv;
    y *= inv;
  }
  return *this;
}

Vec2& Vec2::normalizeApprox() {
  const float length_sq = len2();
  if (length_sq > 0.0f) {
    const float inv = glm::fastInverseSqrt(length_sq);
    x *= inv;
    y *= inv;
  }
  return *this;
}

void Vec2::set(float x_in, float y_in) {
  x = x_in;
  y = y_in;
}

bool Vec2::isEqualEps(const Vec2& vec) const {
  return std::abs(x - vec.x) < kEpsilon && std::abs(y - vec.y) < kEpsilon;
}

Vec2 Vec2::abs() const { return Vec2{std::fabs(x), std::fabs(y)}; }

void Vec2::swap(Vec2& vec1, Vec2& vec2) { std::swap(vec1, vec2); }

Vec2 Vec2::min(const Vec2& vec1, const Vec2& vec2) {
  return Vec2{std::min(vec1.x, vec2.x), std::min(vec1.y, vec2.y)};
}

Vec2 Vec2::max(const Vec2& vec1, const Vec2& vec2) {
  return Vec2{std::max(vec1.x, vec2.x), std::max(vec1.y, vec2.y)};
}

Vec2 Vec2::prod(const Vec2& vec1, const Vec2& vec2) {
  return Vec2{vec1.x * vec2.x, vec1.y * vec2.y};
}

float Vec2::dot(const Vec2& vec1, const Vec2& vec2) { return vec1.x * vec2.x + vec1.y * vec2.y; }

Vec2 Vec2::lerp(float t, const Vec2& v1, const Vec2& v2) { return Vec2{v1.x + t * (v2.x - v1.x), v1.y + t * (v2.y - v1.y)}; }

// ---------------- Vec3 ----------------

Vec3::Vec3(float value) : x(value), y(value), z(value) {}

Vec3::Vec3(float x_in, float y_in, float z_in) : x(x_in), y(y_in), z(z_in) {}

float Vec3::len() const { return std::sqrt(len2()); }

float Vec3::len2() const { return x * x + y * y + z * z; }

float Vec3::lenApprox() const {
  const float length_sq = len2();
  if (length_sq <= 0.0f) {
    return 0.0f;
  }
  return 1.0f / glm::fastInverseSqrt(length_sq);
}

float Vec3::distance(const Vec3& vec) const { return glm::distance(ToGlm(*this), ToGlm(vec)); }

float Vec3::distance2d(const Vec3& vec) const {
  const float dx = x - vec.x;
  const float dy = y - vec.y;
  return std::sqrt(dx * dx + dy * dy);
}

Vec3& Vec3::normalize() {
  const float length = len();
  if (length > 0.0f) {
    const float inv = 1.0f / length;
    x *= inv;
    y *= inv;
    z *= inv;
  }
  return *this;
}

Vec3& Vec3::normalizeSafe() {
  const float length = len();
  if (length > kEpsilon) {
    const float inv = 1.0f / length;
    x *= inv;
    y *= inv;
    z *= inv;
  }
  return *this;
}

Vec3& Vec3::normalizeApprox() {
  const float length_sq = len2();
  if (length_sq > 0.0f) {
    const float inv = glm::fastInverseSqrt(length_sq);
    x *= inv;
    y *= inv;
    z *= inv;
  }
  return *this;
}

void Vec3::set(float x_in, float y_in, float z_in) {
  x = x_in;
  y = y_in;
  z = z_in;
}

bool Vec3::isEqualEps(const Vec3& vec) const {
  return std::abs(x - vec.x) < kEpsilon && std::abs(y - vec.y) < kEpsilon && std::abs(z - vec.z) < kEpsilon;
}

Vec3 Vec3::abs() const { return Vec3{std::fabs(x), std::fabs(y), std::fabs(z)}; }

Vec3 Vec3::reflect(const Vec3& normal) const {
  const float dot_val = dot(*this, normal);
  return Vec3{x - 2.0f * dot_val * normal.x, y - 2.0f * dot_val * normal.y, z - 2.0f * dot_val * normal.z};
}

void Vec3::swap(Vec3& vec1, Vec3& vec2) { std::swap(vec1, vec2); }

Vec3 Vec3::min(const Vec3& vec1, const Vec3& vec2) {
  return Vec3{std::min(vec1.x, vec2.x), std::min(vec1.y, vec2.y), std::min(vec1.z, vec2.z)};
}

Vec3 Vec3::max(const Vec3& vec1, const Vec3& vec2) {
  return Vec3{std::max(vec1.x, vec2.x), std::max(vec1.y, vec2.y), std::max(vec1.z, vec2.z)};
}

Vec3 Vec3::prod(const Vec3& vec1, const Vec3& vec2) { return Vec3{vec1.x * vec2.x, vec1.y * vec2.y, vec1.z * vec2.z}; }

float Vec3::dot(const Vec3& vec1, const Vec3& vec2) { return glm::dot(ToGlm(vec1), ToGlm(vec2)); }

Vec3 Vec3::cross(const Vec3& vec1, const Vec3& vec2) { return FromGlm(glm::cross(ToGlm(vec1), ToGlm(vec2))); }

Vec3 Vec3::lerp(float t, const Vec3& v1, const Vec3& v2) { return FromGlm(glm::mix(ToGlm(v1), ToGlm(v2), t)); }

// ---------------- Vec4 ----------------

Vec4::Vec4(float value) : x(value), y(value), z(value), w(value) {}

Vec4::Vec4(float x_in, float y_in, float z_in, float w_in) : x(x_in), y(y_in), z(z_in), w(w_in) {}

float Vec4::len() const { return std::sqrt(len2()); }

float Vec4::len2() const { return x * x + y * y + z * z + w * w; }

float Vec4::lenApprox() const {
  const float length_sq = len2();
  if (length_sq <= 0.0f) {
    return 0.0f;
  }
  return 1.0f / glm::fastInverseSqrt(length_sq);
}

Vec4& Vec4::normalize() {
  const float length = len();
  if (length > 0.0f) {
    const float inv = 1.0f / length;
    x *= inv;
    y *= inv;
    z *= inv;
    w *= inv;
  }
  return *this;
}

Vec4& Vec4::normalizeSafe() {
  const float length = len();
  if (length > kEpsilon) {
    const float inv = 1.0f / length;
    x *= inv;
    y *= inv;
    z *= inv;
    w *= inv;
  }
  return *this;
}

Vec4& Vec4::normalizeApprox() {
  const float length_sq = len2();
  if (length_sq > 0.0f) {
    const float inv = glm::fastInverseSqrt(length_sq);
    x *= inv;
    y *= inv;
    z *= inv;
    w *= inv;
  }
  return *this;
}

void Vec4::set(float x_in, float y_in, float z_in, float w_in) {
  x = x_in;
  y = y_in;
  z = z_in;
  w = w_in;
}

bool Vec4::isEqualEps(const Vec4& vec) const {
  return std::abs(x - vec.x) < kEpsilon && std::abs(y - vec.y) < kEpsilon && std::abs(z - vec.z) < kEpsilon &&
         std::abs(w - vec.w) < kEpsilon;
}

Vec4 Vec4::abs() const { return Vec4{std::fabs(x), std::fabs(y), std::fabs(z), std::fabs(w)}; }

void Vec4::swap(Vec4& vec1, Vec4& vec2) { std::swap(vec1, vec2); }

Vec4 Vec4::min(const Vec4& vec1, const Vec4& vec2) {
  return Vec4{std::min(vec1.x, vec2.x), std::min(vec1.y, vec2.y), std::min(vec1.z, vec2.z), std::min(vec1.w, vec2.w)};
}

Vec4 Vec4::max(const Vec4& vec1, const Vec4& vec2) {
  return Vec4{std::max(vec1.x, vec2.x), std::max(vec1.y, vec2.y), std::max(vec1.z, vec2.z), std::max(vec1.w, vec2.w)};
}

Vec4 Vec4::prod(const Vec4& vec1, const Vec4& vec2) {
  return Vec4{vec1.x * vec2.x, vec1.y * vec2.y, vec1.z * vec2.z, vec1.w * vec2.w};
}

float Vec4::dot(const Vec4& vec1, const Vec4& vec2) {
  return vec1.x * vec2.x + vec1.y * vec2.y + vec1.z * vec2.z + vec1.w * vec2.w;
}

Vec4 Vec4::lerp(float t, const Vec4& v1, const Vec4& v2) { return Vec4{v1.x + t * (v2.x - v1.x), v1.y + t * (v2.y - v1.y), v1.z + t * (v2.z - v1.z), v1.w + t * (v2.w - v1.w)}; }

// ---------------- Mat3 ----------------

Mat3::Mat3() : mat_(1.0f) {}

Mat3::Mat3(float value) : mat_(value) {}

Mat3::Mat3(const Vec3& v0, const Vec3& v1, const Vec3& v2) : mat_(MakeRowMatrix(v0, v1, v2)) {}

void Mat3::makeIdentity() { mat_ = glm::mat3(1.0f); }

void Mat3::makeZero() { mat_ = glm::mat3(0.0f); }

void Mat3::makeOrthonormal() {
  glm::vec3 right = glm::normalize(mat_[0]);
  glm::vec3 up = mat_[1] - right * glm::dot(right, mat_[1]);
  if (glm::length2(up) > 0.0f) {
    up = glm::normalize(up);
  } else {
    up = glm::vec3(0.0f, 1.0f, 0.0f);
  }
  glm::vec3 at = glm::normalize(glm::cross(right, up));
  mat_[0] = right;
  mat_[1] = up;
  mat_[2] = at;
}

bool Mat3::isUpper3x3Orthonormal() const {
  const glm::vec3 right = mat_[0];
  const glm::vec3 up = mat_[1];
  const glm::vec3 at = mat_[2];
  const bool unit_lengths = std::abs(glm::length2(right) - 1.0f) < kEpsilon && std::abs(glm::length2(up) - 1.0f) < kEpsilon &&
                            std::abs(glm::length2(at) - 1.0f) < kEpsilon;
  const bool orthogonal = std::abs(glm::dot(right, up)) < kEpsilon && std::abs(glm::dot(right, at)) < kEpsilon &&
                          std::abs(glm::dot(up, at)) < kEpsilon;
  return unit_lengths && orthogonal;
}

Mat3 Mat3::transpose() const {
  Mat3 result;
  result.mat_ = glm::transpose(mat_);
  return result;
}

Mat3 Mat3::inverse() const {
  Mat3 result;
  result.mat_ = glm::inverse(mat_);
  return result;
}

Vec3 Mat3::rotate(const Vec3& vec) const { return FromGlm(mat_ * ToGlm(vec)); }

Vec3 Mat3::getRightVector() const { return FromGlm(mat_[0]); }

void Mat3::setRightVector(const Vec3& vec) { mat_[0] = ToGlm(vec); }

Vec3 Mat3::getUpVector() const { return FromGlm(mat_[1]); }

void Mat3::setUpVector(const Vec3& vec) { mat_[1] = ToGlm(vec); }

Vec3 Mat3::getAtVector() const { return FromGlm(mat_[2]); }

void Mat3::setAtVector(const Vec3& vec) { mat_[2] = ToGlm(vec); }

void Mat3::resetRotation() { makeIdentity(); }

Mat3 Mat3::extractRotation() const {
  Mat3 result = *this;
  Vec3 scaling = result.extractScaling();
  if (std::abs(scaling.x) > kEpsilon && std::abs(scaling.y) > kEpsilon && std::abs(scaling.z) > kEpsilon) {
    result.postScale(Vec3{1.0f / scaling.x, 1.0f / scaling.y, 1.0f / scaling.z});
  }
  return result;
}

Vec3 Mat3::extractScaling() const {
  return Vec3{std::sqrt(glm::length2(mat_[0])), std::sqrt(glm::length2(mat_[1])), std::sqrt(glm::length2(mat_[2]))};
}

void Mat3::postRotateX(float angle_degrees) {
  const float radians = glm::radians(angle_degrees);
  const glm::mat3 rotation = glm::mat3(glm::rotate(glm::mat4(1.0f), radians, glm::vec3(1.0f, 0.0f, 0.0f)));
  mat_ *= rotation;
}

void Mat3::postRotateY(float angle_degrees) {
  const float radians = glm::radians(angle_degrees);
  const glm::mat3 rotation = glm::mat3(glm::rotate(glm::mat4(1.0f), radians, glm::vec3(0.0f, 1.0f, 0.0f)));
  mat_ *= rotation;
}

void Mat3::postRotateZ(float angle_degrees) {
  const float radians = glm::radians(angle_degrees);
  const glm::mat3 rotation = glm::mat3(glm::rotate(glm::mat4(1.0f), radians, glm::vec3(0.0f, 0.0f, 1.0f)));
  mat_ *= rotation;
}

void Mat3::preScale(const Vec3& scale) {
  const glm::mat3 scale_mat = glm::mat3(glm::scale(glm::mat4(1.0f), glm::vec3(scale.x, scale.y, scale.z)));
  mat_ = scale_mat * mat_;
}

void Mat3::postScale(const Vec3& scale) {
  const glm::mat3 scale_mat = glm::mat3(glm::scale(glm::mat4(1.0f), glm::vec3(scale.x, scale.y, scale.z)));
  mat_ *= scale_mat;
}

void Mat3::swap(Mat3& mat1, Mat3& mat2) { std::swap(mat1.mat_, mat2.mat_); }

const float* Mat3::data() const { return reinterpret_cast<const float*>(&mat_[0][0]); }

// ---------------- Mat4 ----------------

Mat4::Mat4() : mat_(1.0f) {}

Mat4::Mat4(float value) : mat_(value) {}

Mat4::Mat4(const Vec4& v0, const Vec4& v1, const Vec4& v2, const Vec4& v3) : mat_(MakeRowMatrix(v0, v1, v2, v3)) {}

void Mat4::makeIdentity() { mat_ = glm::mat4(1.0f); }

void Mat4::makeZero() { mat_ = glm::mat4(0.0f); }

void Mat4::makeOrthonormal() {
  glm::mat3 upper_left(mat_);
  Mat3 helper;
  helper.mat_ = upper_left;
  helper.makeOrthonormal();
  glm::vec4 last_column = mat_[3];
  mat_ = glm::mat4(helper.mat_);
  mat_[3] = last_column;
}

bool Mat4::isUpper3x3Orthonormal() const {
  return Mat3(Vec3{mat_[0][0], mat_[1][0], mat_[2][0]}, Vec3{mat_[0][1], mat_[1][1], mat_[2][1]}, Vec3{mat_[0][2], mat_[1][2], mat_[2][2]}).
      isUpper3x3Orthonormal();
}

Mat4 Mat4::transpose() const {
  Mat4 result;
  result.mat_ = glm::transpose(mat_);
  return result;
}

Mat4 Mat4::inverse() const {
  Mat4 result;
  result.mat_ = glm::inverse(mat_);
  return result;
}

Mat4 Mat4::inverseLinTrafo() const {
  Mat4 result;
  const glm::mat3 rot_scale(mat_);
  const glm::mat3 inv_rot = glm::inverse(rot_scale);
  const glm::vec3 translation = glm::vec3(mat_[3]);
  const glm::vec3 new_translation = -(inv_rot * translation);
  result.mat_ = glm::mat4(inv_rot);
  result.mat_[3] = glm::vec4(new_translation, 1.0f);
  return result;
}

Vec3 Mat4::rotate(const Vec3& vec) const { return FromGlm(glm::vec3(mat_ * glm::vec4(ToGlm(vec), 0.0f))); }

Vec3 Mat4::getRightVector() const { return FromGlm(glm::vec3(mat_[0])); }

void Mat4::setRightVector(const Vec3& vec) { mat_[0] = glm::vec4(ToGlm(vec), mat_[0][3]); }

Vec3 Mat4::getAtVector() const { return FromGlm(glm::vec3(mat_[2])); }

void Mat4::setAtVector(const Vec3& vec) { mat_[2] = glm::vec4(ToGlm(vec), mat_[2][3]); }

Vec3 Mat4::getUpVector() const { return FromGlm(glm::vec3(mat_[1])); }

void Mat4::setUpVector(const Vec3& vec) { mat_[1] = glm::vec4(ToGlm(vec), mat_[1][3]); }

Vec3 Mat4::getTranslation() const { return FromGlm(glm::vec3(mat_[3])); }

void Mat4::setTranslation(const Vec3& vec) { mat_[3] = glm::vec4(ToGlm(vec), 1.0f); }

void Mat4::resetRotation() {
  mat_[0] = glm::vec4(1.0f, 0.0f, 0.0f, mat_[0][3]);
  mat_[1] = glm::vec4(0.0f, 1.0f, 0.0f, mat_[1][3]);
  mat_[2] = glm::vec4(0.0f, 0.0f, 1.0f, mat_[2][3]);
}

Mat3 Mat4::extractRotation() const {
  Mat3 rotation;
  rotation.mat_ = glm::mat3(mat_);
  Vec3 scaling = rotation.extractScaling();
  if (std::abs(scaling.x) > kEpsilon && std::abs(scaling.y) > kEpsilon && std::abs(scaling.z) > kEpsilon) {
    rotation.postScale(Vec3{1.0f / scaling.x, 1.0f / scaling.y, 1.0f / scaling.z});
  }
  return rotation;
}

Vec3 Mat4::extractScaling() const {
  return Vec3{std::sqrt(glm::length2(glm::vec3(mat_[0]))), std::sqrt(glm::length2(glm::vec3(mat_[1]))), std::sqrt(glm::length2(glm::vec3(mat_[2])))};
}

void Mat4::postRotateX(float angle_degrees) {
  const float radians = glm::radians(angle_degrees);
  const glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), radians, glm::vec3(1.0f, 0.0f, 0.0f));
  mat_ *= rotation;
}

void Mat4::postRotateY(float angle_degrees) {
  const float radians = glm::radians(angle_degrees);
  const glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), radians, glm::vec3(0.0f, 1.0f, 0.0f));
  mat_ *= rotation;
}

void Mat4::postRotateZ(float angle_degrees) {
  const float radians = glm::radians(angle_degrees);
  const glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), radians, glm::vec3(0.0f, 0.0f, 1.0f));
  mat_ *= rotation;
}

void Mat4::preScale(const Vec3& scale) {
  const glm::mat4 scale_mat = glm::scale(glm::mat4(1.0f), glm::vec3(scale.x, scale.y, scale.z));
  mat_ = scale_mat * mat_;
}

void Mat4::postScale(const Vec3& scale) {
  const glm::mat4 scale_mat = glm::scale(glm::mat4(1.0f), glm::vec3(scale.x, scale.y, scale.z));
  mat_ *= scale_mat;
}

void Mat4::swap(Mat4& mat1, Mat4& mat2) { std::swap(mat1.mat_, mat2.mat_); }

Mat4 Mat4::lookAt(const Vec3& from, const Vec3& to, const Vec3& up) {
  Mat4 result;
  glm::vec3 forward = ToGlm(to) - ToGlm(from);
  if (glm::length2(forward) < kEpsilon) {
    forward = glm::vec3(0.0f, 0.0f, 1.0f);
  } else {
    forward = glm::normalize(forward);
  }
  glm::vec3 right = glm::cross(ToGlm(up), forward);
  if (glm::length2(right) < kEpsilon) {
    right = glm::vec3(1.0f, 0.0f, 0.0f);
  } else {
    right = glm::normalize(right);
  }
  const glm::vec3 corrected_up = glm::normalize(glm::cross(forward, right));
  result.mat_[0] = glm::vec4(right, 0.0f);
  result.mat_[1] = glm::vec4(corrected_up, 0.0f);
  result.mat_[2] = glm::vec4(forward, 0.0f);
  result.mat_[3] = glm::vec4(ToGlm(from), 1.0f);
  return result;
}

const float* Mat4::data() const { return reinterpret_cast<const float*>(&mat_[0][0]); }

// ---------------- Quat ----------------

Quat::Quat() = default;

Quat::Quat(float w_in) : w(w_in) {}

Quat::Quat(float x_in, float y_in, float z_in, float w_in) : x(x_in), y(y_in), z(z_in), w(w_in) {}

Mat3 Quat::toMat3() const {
  Mat3 result;
  const glm::quat quat(w, x, y, z);
  result.mat_ = glm::mat3_cast(quat);
  return result;
}

void Quat::fromMat3(const Mat3& mat) {
  const glm::quat quat = glm::quat_cast(mat.mat_);
  x = quat.x;
  y = quat.y;
  z = quat.z;
  w = quat.w;
}

Mat4 Quat::toMat4() const {
  Mat4 result;
  const glm::quat quat(w, x, y, z);
  result.mat_ = glm::mat4_cast(quat);
  return result;
}

void Quat::fromMat4(const Mat4& mat) {
  const glm::quat quat = glm::quat_cast(mat.mat_);
  x = quat.x;
  y = quat.y;
  z = quat.z;
  w = quat.w;
}

Vec3 Quat::toEuler() const {
  const glm::quat quat(w, x, y, z);
  const glm::vec3 euler = glm::eulerAngles(quat);
  return Vec3{euler.x, euler.y, euler.z};
}

void Quat::fromEuler(const Vec3& vec) {
  const glm::quat quat = glm::quat(glm::vec3(vec.x, vec.y, vec.z));
  x = quat.x;
  y = quat.y;
  z = quat.z;
  w = quat.w;
}

Vec4 Quat::toAxisAngle() const {
  const glm::quat quat(w, x, y, z);
  const glm::vec3 axis = glm::axis(quat);
  const float angle = glm::angle(quat);
  return Vec4{axis.x, axis.y, axis.z, angle};
}

void Quat::fromAxisAngle(const Vec3& axis, float angle) {
  const glm::quat quat = glm::angleAxis(angle, ToGlm(axis));
  x = quat.x;
  y = quat.y;
  z = quat.z;
  w = quat.w;
}

void Quat::makeIdentity() {
  x = 0.0f;
  y = 0.0f;
  z = 0.0f;
  w = 1.0f;
}

bool Quat::isIdentity() const { return std::abs(x) < kEpsilon && std::abs(y) < kEpsilon && std::abs(z) < kEpsilon && std::abs(w - 1.0f) < kEpsilon; }

float Quat::len() const { return std::sqrt(len2()); }

float Quat::len2() const { return x * x + y * y + z * z + w * w; }

float Quat::lenApprox() const {
  const float length_sq = len2();
  if (length_sq <= 0.0f) {
    return 0.0f;
  }
  return 1.0f / glm::fastInverseSqrt(length_sq);
}

Quat& Quat::normalize() {
  const float length = len();
  if (length > 0.0f) {
    const float inv = 1.0f / length;
    x *= inv;
    y *= inv;
    z *= inv;
    w *= inv;
  }
  return *this;
}

Quat& Quat::normalizeSafe() {
  const float length = len();
  if (length > kEpsilon) {
    const float inv = 1.0f / length;
    x *= inv;
    y *= inv;
    z *= inv;
    w *= inv;
  }
  return *this;
}

Quat& Quat::normalizeApprox() {
  const float length_sq = len2();
  if (length_sq > 0.0f) {
    const float inv = glm::fastInverseSqrt(length_sq);
    x *= inv;
    y *= inv;
    z *= inv;
    w *= inv;
  }
  return *this;
}

void Quat::set(float x_in, float y_in, float z_in, float w_in) {
  x = x_in;
  y = y_in;
  z = z_in;
  w = w_in;
}

Quat Quat::inverse() const {
  const float length_sq = len2();
  if (length_sq <= 0.0f) {
    return Quat();
  }
  const float inv = 1.0f / length_sq;
  return Quat{-x * inv, -y * inv, -z * inv, w * inv};
}

Quat Quat::conjugate() const { return Quat{-x, -y, -z, w}; }

float Quat::dot(const Quat& quat1, const Quat& quat2) { return quat1.x * quat2.x + quat1.y * quat2.y + quat1.z * quat2.z + quat1.w * quat2.w; }

Quat Quat::lerp(float t, const Quat& q1, const Quat& q2) {
  return Quat{q1.x + t * (q2.x - q1.x), q1.y + t * (q2.y - q1.y), q1.z + t * (q2.z - q1.z), q1.w + t * (q2.w - q1.w)};
}

Quat Quat::slerp(float t, const Quat& q1, const Quat& q2) {
  const glm::quat lhs(q1.w, q1.x, q1.y, q1.z);
  const glm::quat rhs(q2.w, q2.x, q2.y, q2.z);
  const glm::quat result = glm::slerp(lhs, rhs, t);
  return Quat{result.x, result.y, result.z, result.w};
}

Quat Quat::squad(float t, const Quat& q1, const Quat& q2, const Quat& q3) {
  const glm::quat qa(q1.w, q1.x, q1.y, q1.z);
  const glm::quat qb(q2.w, q2.x, q2.y, q2.z);
  const glm::quat qc(q3.w, q3.x, q3.y, q3.z);
  const glm::quat ab = glm::slerp(qa, qb, t);
  const glm::quat bc = glm::slerp(qb, qc, t);
  const glm::quat result = glm::slerp(ab, bc, 2.0f * t * (1.0f - t));
  return Quat{result.x, result.y, result.z, result.w};
}

Quat Quat::lookRotation(const Vec3& forward, const Vec3& up) {
  const glm::vec3 fwd = glm::normalize(ToGlm(forward));
  const glm::vec3 right = glm::normalize(glm::cross(ToGlm(up), fwd));
  const glm::vec3 corrected_up = glm::cross(fwd, right);
  const glm::mat3 orientation(right, corrected_up, fwd);
  const glm::quat quat = glm::quat_cast(orientation);
  return Quat{quat.x, quat.y, quat.z, quat.w};
}

}  // namespace types

namespace bindings {

using namespace types;

void BindMath(sol::state& lua) {
  lua.new_usertype<Vec2>("Vec2", sol::constructors<Vec2(), Vec2(float), Vec2(float, float)>(), "x", &Vec2::x, "y", &Vec2::y,
                         "len", &Vec2::len, "len2", &Vec2::len2, "lenApprox", &Vec2::lenApprox, "distance", &Vec2::distance,
                         "normalize", &Vec2::normalize, "normalizeSafe", &Vec2::normalizeSafe, "normalizeApprox", &Vec2::normalizeApprox,
                         "set", &Vec2::set, "isEqualEps", &Vec2::isEqualEps, "abs", &Vec2::abs, "swap", &Vec2::swap, "min", &Vec2::min,
                         "max", &Vec2::max, "prod", &Vec2::prod, "dot", &Vec2::dot, "lerp", &Vec2::lerp);

  lua.new_usertype<Vec3>("Vec3", sol::constructors<Vec3(), Vec3(float), Vec3(float, float, float)>(), "x", &Vec3::x, "y", &Vec3::y,
                         "z", &Vec3::z, "len", &Vec3::len, "len2", &Vec3::len2, "lenApprox", &Vec3::lenApprox, "distance", &Vec3::distance,
                         "distance2d", &Vec3::distance2d, "normalize", &Vec3::normalize, "normalizeSafe", &Vec3::normalizeSafe,
                         "normalizeApprox", &Vec3::normalizeApprox, "set", &Vec3::set, "isEqualEps", &Vec3::isEqualEps, "abs", &Vec3::abs,
                         "reflect", &Vec3::reflect, "swap", &Vec3::swap, "min", &Vec3::min, "max", &Vec3::max, "prod", &Vec3::prod, "dot",
                         &Vec3::dot, "cross", &Vec3::cross, "lerp", &Vec3::lerp);

  lua.new_usertype<Vec4>("Vec4", sol::constructors<Vec4(), Vec4(float), Vec4(float, float, float, float)>(), "x", &Vec4::x, "y",
                         &Vec4::y, "z", &Vec4::z, "w", &Vec4::w, "len", &Vec4::len, "len2", &Vec4::len2, "lenApprox", &Vec4::lenApprox,
                         "normalize", &Vec4::normalize, "normalizeSafe", &Vec4::normalizeSafe, "normalizeApprox", &Vec4::normalizeApprox,
                         "set", &Vec4::set, "isEqualEps", &Vec4::isEqualEps, "abs", &Vec4::abs, "swap", &Vec4::swap, "min", &Vec4::min, "max",
                         &Vec4::max, "prod", &Vec4::prod, "dot", &Vec4::dot, "lerp", &Vec4::lerp);

  lua.new_usertype<Mat3>(
      "Mat3", sol::constructors<Mat3(), Mat3(float), Mat3(const Vec3&, const Vec3&, const Vec3&)>(), "makeIdentity", &Mat3::makeIdentity,
      "makeZero", &Mat3::makeZero, "makeOrthonormal", &Mat3::makeOrthonormal, "isUpper3x3Orthonormal", &Mat3::isUpper3x3Orthonormal,
      "transpose", &Mat3::transpose, "inverse", &Mat3::inverse, "rotate", &Mat3::rotate, "getRightVector", &Mat3::getRightVector,
      "setRightVector", &Mat3::setRightVector, "getUpVector", &Mat3::getUpVector, "setUpVector", &Mat3::setUpVector, "getAtVector",
      &Mat3::getAtVector, "setAtVector", &Mat3::setAtVector, "resetRotation", &Mat3::resetRotation, "extractRotation", &Mat3::extractRotation,
      "extractScaling", &Mat3::extractScaling, "postRotateX", &Mat3::postRotateX, "postRotateY", &Mat3::postRotateY, "postRotateZ",
      &Mat3::postRotateZ, "preScale", &Mat3::preScale, "postScale", &Mat3::postScale, "swap", &Mat3::swap);

  lua.new_usertype<Mat4>(
      "Mat4", sol::constructors<Mat4(), Mat4(float), Mat4(const Vec4&, const Vec4&, const Vec4&, const Vec4&)>(), "makeIdentity",
      &Mat4::makeIdentity, "makeZero", &Mat4::makeZero, "makeOrthonormal", &Mat4::makeOrthonormal, "isUpper3x3Orthonormal",
      &Mat4::isUpper3x3Orthonormal, "transpose", &Mat4::transpose, "inverse", &Mat4::inverse, "inverseLinTrafo", &Mat4::inverseLinTrafo,
      "rotate", &Mat4::rotate, "getRightVector", &Mat4::getRightVector, "setRightVector", &Mat4::setRightVector, "getAtVector",
      &Mat4::getAtVector, "setAtVector", &Mat4::setAtVector, "getUpVector", &Mat4::getUpVector, "setUpVector", &Mat4::setUpVector,
      "getTranslation", &Mat4::getTranslation, "setTranslation", &Mat4::setTranslation, "resetRotation", &Mat4::resetRotation,
      "extractRotation", &Mat4::extractRotation, "extractScaling", &Mat4::extractScaling, "postRotateX", &Mat4::postRotateX, "postRotateY",
      &Mat4::postRotateY, "postRotateZ", &Mat4::postRotateZ, "preScale", &Mat4::preScale, "postScale", &Mat4::postScale, "swap", &Mat4::swap,
      "lookAt", &Mat4::lookAt);

  lua.new_usertype<Quat>("Quat", sol::constructors<Quat(), Quat(float), Quat(float, float, float, float)>(), "x", &Quat::x, "y", &Quat::y,
                         "z", &Quat::z, "w", &Quat::w, "toMat3", &Quat::toMat3, "fromMat3", &Quat::fromMat3, "toMat4", &Quat::toMat4,
                         "fromMat4", &Quat::fromMat4, "toEuler", &Quat::toEuler, "fromEuler", &Quat::fromEuler, "toAxisAngle",
                         &Quat::toAxisAngle, "fromAxisAngle", &Quat::fromAxisAngle, "makeIdentity", &Quat::makeIdentity, "isIdentity",
                         &Quat::isIdentity, "len", &Quat::len, "len2", &Quat::len2, "lenApprox", &Quat::lenApprox, "normalize",
                         &Quat::normalize, "normalizeSafe", &Quat::normalizeSafe, "normalizeApprox", &Quat::normalizeApprox, "set", &Quat::set,
                         "inverse", &Quat::inverse, "conjugate", &Quat::conjugate, "dot", &Quat::dot, "lerp", &Quat::lerp, "slerp",
                         &Quat::slerp, "squad", &Quat::squad, "lookRotation", &Quat::lookRotation);
}

}  // namespace bindings
}  // namespace lua