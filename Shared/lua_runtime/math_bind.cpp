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
#include <glm/gtx/fast_square_root.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace lua {
namespace types {

namespace {
constexpr float kEpsilon = 0.001f;

inline glm::vec3 ToGlm(const Vec3& vec) {
  return glm::vec3(vec.x, vec.y, vec.z);
}
inline glm::vec4 ToGlm(const Vec4& vec) {
  return glm::vec4(vec.x, vec.y, vec.z, vec.w);
}
inline Vec3 FromGlm(const glm::vec3& vec) {
  return Vec3{vec.x, vec.y, vec.z};
}
inline Vec4 FromGlm(const glm::vec4& vec) {
  return Vec4{vec.x, vec.y, vec.z, vec.w};
}

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
/* luadoc (class)
 *
 * 2D vector with basic math utilities.
 *
 * @name     Vec2
 * @side     shared
 * @category Math
 *
 */

/* luadoc (constructor)
 *
 * Creates a Vec2 with both components set to the same value.
 *
 * @param    (number) value Value assigned to x and y.
 *
 */
Vec2::Vec2(float value) : x(value), y(value) {
}

/* luadoc (constructor)
 *
 * Creates a Vec2 with explicit x and y components.
 *
 * @param    (number) x X component.
 * @param    (number) y Y component.
 *
 */
Vec2::Vec2(float x_in, float y_in) : x(x_in), y(y_in) {
}

/* luadoc (property)
 *
 * X component.
 *
 * @name     x
 * @return   (number) X component value.
 *
 */
/* luadoc (property)
 *
 * Y component.
 *
 * @name     y
 * @return   (number) Y component value.
 *
 */

/* luadoc (method)
 *
 * Returns the vector length (magnitude).
 *
 * @name     len
 * @return   (number) Vector length.
 *
 */
float Vec2::len() const {
  return std::sqrt(len2());
}

/* luadoc (method)
 *
 * Returns the squared vector length.
 *
 * @name     len2
 * @return   (number) Squared vector length.
 *
 */
float Vec2::len2() const {
  return x * x + y * y;
}

/* luadoc (method)
 *
 * Returns an approximate vector length.
 *
 * @name     lenApprox
 * @return   (number) Approximate vector length.
 *
 */
float Vec2::lenApprox() const {
  const float length_sq = len2();
  if (length_sq <= 0.0f) {
    return 0.0f;
  }
  return 1.0f / glm::fastInverseSqrt(length_sq);
}

/* luadoc (method)
 *
 * Returns the distance to another vector.
 *
 * @name     distance
 * @param    (Vec2) vec Other vector.
 * @return   (number) Distance between vectors.
 *
 */
float Vec2::distance(const Vec2& vec) const {
  const float dx = x - vec.x;
  const float dy = y - vec.y;
  return std::sqrt(dx * dx + dy * dy);
}

/* luadoc (method)
 *
 * Normalizes the vector in-place.
 *
 * If the vector length is zero, no change is applied.
 *
 * @name     normalize
 * @return   (Vec2) This vector (normalized).
 *
 */
Vec2& Vec2::normalize() {
  const float length = len();
  if (length > 0.0f) {
    const float inv = 1.0f / length;
    x *= inv;
    y *= inv;
  }
  return *this;
}

/* luadoc (method)
 *
 * Normalizes the vector in-place using an epsilon check.
 *
 * If the vector length is below a small threshold, no change is applied.
 *
 * @name     normalizeSafe
 * @return   (Vec2) This vector (normalized).
 *
 */
Vec2& Vec2::normalizeSafe() {
  const float length = len();
  if (length > kEpsilon) {
    const float inv = 1.0f / length;
    x *= inv;
    y *= inv;
  }
  return *this;
}

/* luadoc (method)
 *
 * Normalizes the vector in-place using an approximate inverse square root.
 *
 * @name     normalizeApprox
 * @return   (Vec2) This vector (normalized).
 *
 */
Vec2& Vec2::normalizeApprox() {
  const float length_sq = len2();
  if (length_sq > 0.0f) {
    const float inv = glm::fastInverseSqrt(length_sq);
    x *= inv;
    y *= inv;
  }
  return *this;
}

/* luadoc (method)
 *
 * Sets both components of the vector.
 *
 * @name     set
 * @param    (number) x X component.
 * @param    (number) y Y component.
 *
 */
void Vec2::set(float x_in, float y_in) {
  x = x_in;
  y = y_in;
}

/* luadoc (method)
 *
 * Compares this vector with another vector using an epsilon tolerance.
 *
 * @name     isEqualEps
 * @param    (Vec2) vec Other vector.
 * @return   (bool) True if both components are equal within epsilon.
 *
 */
bool Vec2::isEqualEps(const Vec2& vec) const {
  return std::abs(x - vec.x) < kEpsilon && std::abs(y - vec.y) < kEpsilon;
}

/* luadoc (method)
 *
 * Returns a vector with absolute component values.
 *
 * @name     abs
 * @return   (Vec2) Vector with abs(x) and abs(y).
 *
 */
Vec2 Vec2::abs() const {
  return Vec2{std::fabs(x), std::fabs(y)};
}

/* luadoc (method)
 *
 * Swaps two vectors.
 *
 * @static
 * @name     swap
 * @param    (Vec2) vec1 First vector.
 * @param    (Vec2) vec2 Second vector.
 *
 */
void Vec2::swap(Vec2& vec1, Vec2& vec2) {
  std::swap(vec1, vec2);
}

/* luadoc (method)
 *
 * Returns the component-wise minimum of two vectors.
 *
 * @static
 * @name     min
 * @param    (Vec2) vec1 First vector.
 * @param    (Vec2) vec2 Second vector.
 * @return   (Vec2) Component-wise minimum.
 *
 */
Vec2 Vec2::min(const Vec2& vec1, const Vec2& vec2) {
  return Vec2{std::min(vec1.x, vec2.x), std::min(vec1.y, vec2.y)};
}

/* luadoc (method)
 *
 * Returns the component-wise maximum of two vectors.
 *
 * @static
 * @name     max
 * @param    (Vec2) vec1 First vector.
 * @param    (Vec2) vec2 Second vector.
 * @return   (Vec2) Component-wise maximum.
 *
 */
Vec2 Vec2::max(const Vec2& vec1, const Vec2& vec2) {
  return Vec2{std::max(vec1.x, vec2.x), std::max(vec1.y, vec2.y)};
}

/* luadoc (method)
 *
 * Returns the component-wise product of two vectors.
 *
 * @static
 * @name     prod
 * @param    (Vec2) vec1 First vector.
 * @param    (Vec2) vec2 Second vector.
 * @return   (Vec2) Component-wise product.
 *
 */
Vec2 Vec2::prod(const Vec2& vec1, const Vec2& vec2) {
  return Vec2{vec1.x * vec2.x, vec1.y * vec2.y};
}

/* luadoc (method)
 *
 * Returns the dot product of two vectors.
 *
 * @static
 * @name     dot
 * @param    (Vec2) vec1 First vector.
 * @param    (Vec2) vec2 Second vector.
 * @return   (number) Dot product.
 *
 */
float Vec2::dot(const Vec2& vec1, const Vec2& vec2) {
  return vec1.x * vec2.x + vec1.y * vec2.y;
}

/* luadoc (method)
 *
 * Linearly interpolates between two vectors.
 *
 * @static
 * @name     lerp
 * @param    (number) t Interpolation factor (typically 0..1).
 * @param    (Vec2) v1 Start vector.
 * @param    (Vec2) v2 End vector.
 * @return   (Vec2) Interpolated vector.
 *
 */
Vec2 Vec2::lerp(float t, const Vec2& v1, const Vec2& v2) {
  return Vec2{v1.x + t * (v2.x - v1.x), v1.y + t * (v2.y - v1.y)};
}

// ---------------- Vec3 ----------------
/* luadoc (class)
 *
 * 3D vector with basic math utilities.
 *
 * @name     Vec3
 * @side     shared
 * @category Math
 *
 */

/* luadoc (constructor)
 *
 * Creates a Vec3 with all components set to the same value.
 *
 * @param    (number) value Value assigned to x, y and z.
 *
 */
Vec3::Vec3(float value) : x(value), y(value), z(value) {
}

/* luadoc (constructor)
 *
 * Creates a Vec3 with explicit x, y and z components.
 *
 * @param    (number) x X component.
 * @param    (number) y Y component.
 * @param    (number) z Z component.
 *
 */
Vec3::Vec3(float x_in, float y_in, float z_in) : x(x_in), y(y_in), z(z_in) {
}

/* luadoc (property)
 *
 * X component.
 *
 * @name     x
 * @return   (number) X component value.
 *
 */
/* luadoc (property)
 *
 * Y component.
 *
 * @name     y
 * @return   (number) Y component value.
 *
 */
/* luadoc (property)
 *
 * Z component.
 *
 * @name     z
 * @return   (number) Z component value.
 *
 */

/* luadoc (method)
 *
 * Returns the vector length (magnitude).
 *
 * @name     len
 * @return   (number) Vector length.
 *
 */
float Vec3::len() const {
  return std::sqrt(len2());
}

/* luadoc (method)
 *
 * Returns the squared vector length.
 *
 * @name     len2
 * @return   (number) Squared vector length.
 *
 */
float Vec3::len2() const {
  return x * x + y * y + z * z;
}

/* luadoc (method)
 *
 * Returns an approximate vector length.
 *
 * @name     lenApprox
 * @return   (number) Approximate vector length.
 *
 */
float Vec3::lenApprox() const {
  const float length_sq = len2();
  if (length_sq <= 0.0f) {
    return 0.0f;
  }
  return 1.0f / glm::fastInverseSqrt(length_sq);
}

/* luadoc (method)
 *
 * Returns the distance to another vector.
 *
 * @name     distance
 * @param    (Vec3) vec Other vector.
 * @return   (number) Distance between vectors.
 *
 */
float Vec3::distance(const Vec3& vec) const {
  return glm::distance(ToGlm(*this), ToGlm(vec));
}

/* luadoc (method)
 *
 * Returns the 2D distance to another vector (ignores Z component).
 *
 * @name     distance2d
 * @param    (Vec3) vec Other vector.
 * @return   (number) 2D distance between vectors.
 *
 */
float Vec3::distance2d(const Vec3& vec) const {
  const float dx = x - vec.x;
  const float dy = y - vec.y;
  return std::sqrt(dx * dx + dy * dy);
}

/* luadoc (method)
 *
 * Normalizes the vector in-place.
 *
 * If the vector length is zero, no change is applied.
 *
 * @name     normalize
 * @return   (Vec3) This vector (normalized).
 *
 */
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

/* luadoc (method)
 *
 * Normalizes the vector in-place using an epsilon check.
 *
 * If the vector length is below a small threshold, no change is applied.
 *
 * @name     normalizeSafe
 * @return   (Vec3) This vector (normalized).
 *
 */
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

/* luadoc (method)
 *
 * Normalizes the vector in-place using an approximate inverse square root.
 *
 * @name     normalizeApprox
 * @return   (Vec3) This vector (normalized).
 *
 */
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

/* luadoc (method)
 *
 * Sets all components of the vector.
 *
 * @name     set
 * @param    (number) x X component.
 * @param    (number) y Y component.
 * @param    (number) z Z component.
 *
 */
void Vec3::set(float x_in, float y_in, float z_in) {
  x = x_in;
  y = y_in;
  z = z_in;
}

/* luadoc (method)
 *
 * Compares this vector with another vector using an epsilon tolerance.
 *
 * @name     isEqualEps
 * @param    (Vec3) vec Other vector.
 * @return   (bool) True if all components are equal within epsilon.
 *
 */
bool Vec3::isEqualEps(const Vec3& vec) const {
  return std::abs(x - vec.x) < kEpsilon && std::abs(y - vec.y) < kEpsilon && std::abs(z - vec.z) < kEpsilon;
}

/* luadoc (method)
 *
 * Returns a vector with absolute component values.
 *
 * @name     abs
 * @return   (Vec3) Vector with abs(x), abs(y) and abs(z).
 *
 */
Vec3 Vec3::abs() const {
  return Vec3{std::fabs(x), std::fabs(y), std::fabs(z)};
}

/* luadoc (method)
 *
 * Returns the reflection of this vector around a surface normal.
 *
 * @name     reflect
 * @param    (Vec3) normal Surface normal (typically normalized).
 * @return   (Vec3) Reflected vector.
 *
 */
Vec3 Vec3::reflect(const Vec3& normal) const {
  const float dot_val = dot(*this, normal);
  return Vec3{x - 2.0f * dot_val * normal.x, y - 2.0f * dot_val * normal.y, z - 2.0f * dot_val * normal.z};
}

/* luadoc (method)
 *
 * Swaps two vectors.
 *
 * @static
 * @name     swap
 * @param    (Vec3) vec1 First vector.
 * @param    (Vec3) vec2 Second vector.
 *
 */
void Vec3::swap(Vec3& vec1, Vec3& vec2) {
  std::swap(vec1, vec2);
}

/* luadoc (method)
 *
 * Returns the component-wise minimum of two vectors.
 *
 * @static
 * @name     min
 * @param    (Vec3) vec1 First vector.
 * @param    (Vec3) vec2 Second vector.
 * @return   (Vec3) Component-wise minimum.
 *
 */
Vec3 Vec3::min(const Vec3& vec1, const Vec3& vec2) {
  return Vec3{std::min(vec1.x, vec2.x), std::min(vec1.y, vec2.y), std::min(vec1.z, vec2.z)};
}

/* luadoc (method)
 *
 * Returns the component-wise maximum of two vectors.
 *
 * @static
 * @name     max
 * @param    (Vec3) vec1 First vector.
 * @param    (Vec3) vec2 Second vector.
 * @return   (Vec3) Component-wise maximum.
 *
 */
Vec3 Vec3::max(const Vec3& vec1, const Vec3& vec2) {
  return Vec3{std::max(vec1.x, vec2.x), std::max(vec1.y, vec2.y), std::max(vec1.z, vec2.z)};
}

/* luadoc (method)
 *
 * Returns the component-wise product of two vectors.
 *
 * @static
 * @name     prod
 * @param    (Vec3) vec1 First vector.
 * @param    (Vec3) vec2 Second vector.
 * @return   (Vec3) Component-wise product.
 *
 */
Vec3 Vec3::prod(const Vec3& vec1, const Vec3& vec2) {
  return Vec3{vec1.x * vec2.x, vec1.y * vec2.y, vec1.z * vec2.z};
}

/* luadoc (method)
 *
 * Returns the dot product of two vectors.
 *
 * @static
 * @name     dot
 * @param    (Vec3) vec1 First vector.
 * @param    (Vec3) vec2 Second vector.
 * @return   (number) Dot product.
 *
 */
float Vec3::dot(const Vec3& vec1, const Vec3& vec2) {
  return glm::dot(ToGlm(vec1), ToGlm(vec2));
}

/* luadoc (method)
 *
 * Returns the cross product of two vectors.
 *
 * @static
 * @name     cross
 * @param    (Vec3) vec1 First vector.
 * @param    (Vec3) vec2 Second vector.
 * @return   (Vec3) Cross product.
 *
 */
Vec3 Vec3::cross(const Vec3& vec1, const Vec3& vec2) {
  return FromGlm(glm::cross(ToGlm(vec1), ToGlm(vec2)));
}

/* luadoc (method)
 *
 * Linearly interpolates between two vectors.
 *
 * @static
 * @name     lerp
 * @param    (number) t Interpolation factor (typically 0..1).
 * @param    (Vec3) v1 Start vector.
 * @param    (Vec3) v2 End vector.
 * @return   (Vec3) Interpolated vector.
 *
 */
Vec3 Vec3::lerp(float t, const Vec3& v1, const Vec3& v2) {
  return FromGlm(glm::mix(ToGlm(v1), ToGlm(v2), t));
}

// ---------------- Vec4 ----------------
/* luadoc (class)
 *
 * 4D vector with basic math utilities.
 *
 * @name     Vec4
 * @side     shared
 * @category Math
 *
 */

/* luadoc (constructor)
 *
 * Creates a Vec4 with all components set to the same value.
 *
 * @param    (number) value Value assigned to x, y, z and w.
 *
 */
Vec4::Vec4(float value) : x(value), y(value), z(value), w(value) {
}

/* luadoc (constructor)
 *
 * Creates a Vec4 with explicit x, y, z and w components.
 *
 * @param    (number) x X component.
 * @param    (number) y Y component.
 * @param    (number) z Z component.
 * @param    (number) w W component.
 *
 */
Vec4::Vec4(float x_in, float y_in, float z_in, float w_in) : x(x_in), y(y_in), z(z_in), w(w_in) {
}
/* luadoc (property)
 *
 * X component.
 *
 * @name     x
 * @return   (number) X component value.
 *
 */
/* luadoc (property)
 *
 * Y component.
 *
 * @name     y
 * @return   (number) Y component value.
 *
 */
/* luadoc (property)
 *
 * Z component.
 *
 * @name     z
 * @return   (number) Z component value.
 *
 */
/* luadoc (property)
 *
 * W component.
 *
 * @name     w
 * @return   (number) W component value.
 *
 */

/* luadoc (method)
 *
 * Returns the vector length (magnitude).
 *
 * @name     len
 * @return   (number) Vector length.
 *
 */
float Vec4::len() const {
  return std::sqrt(len2());
}

/* luadoc (method)
 *
 * Returns the squared vector length.
 *
 * @name     len2
 * @return   (number) Squared vector length.
 *
 */
float Vec4::len2() const {
  return x * x + y * y + z * z + w * w;
}

/* luadoc (method)
 *
 * Returns an approximate vector length.
 *
 * @name     lenApprox
 * @return   (number) Approximate vector length.
 *
 */
float Vec4::lenApprox() const {
  const float length_sq = len2();
  if (length_sq <= 0.0f) {
    return 0.0f;
  }
  return 1.0f / glm::fastInverseSqrt(length_sq);
}

/* luadoc (method)
 *
 * Normalizes the vector in-place.
 *
 * If the vector length is zero, no change is applied.
 *
 * @name     normalize
 * @return   (Vec4) This vector (normalized).
 *
 */
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

/* luadoc (method)
 *
 * Normalizes the vector in-place using an epsilon check.
 *
 * If the vector length is below a small threshold, no change is applied.
 *
 * @name     normalizeSafe
 * @return   (Vec4) This vector (normalized).
 *
 */
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

/* luadoc (method)
 *
 * Normalizes the vector in-place using an approximate inverse square root.
 *
 * @name     normalizeApprox
 * @return   (Vec4) This vector (normalized).
 *
 */
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

/* luadoc (method)
 *
 * Sets all components of the vector.
 *
 * @name     set
 * @param    (number) x X component.
 * @param    (number) y Y component.
 * @param    (number) z Z component.
 * @param    (number) w W component.
 *
 */
void Vec4::set(float x_in, float y_in, float z_in, float w_in) {
  x = x_in;
  y = y_in;
  z = z_in;
  w = w_in;
}

/* luadoc (method)
 *
 * Compares this vector with another vector using an epsilon tolerance.
 *
 * @name     isEqualEps
 * @param    (Vec4) vec Other vector.
 * @return   (bool) True if all components are equal within epsilon.
 *
 */
bool Vec4::isEqualEps(const Vec4& vec) const {
  return std::abs(x - vec.x) < kEpsilon && std::abs(y - vec.y) < kEpsilon && std::abs(z - vec.z) < kEpsilon && std::abs(w - vec.w) < kEpsilon;
}

/* luadoc (method)
 *
 * Returns a vector with absolute component values.
 *
 * @name     abs
 * @return   (Vec4) Vector with abs(x), abs(y), abs(z) and abs(w).
 *
 */
Vec4 Vec4::abs() const {
  return Vec4{std::fabs(x), std::fabs(y), std::fabs(z), std::fabs(w)};
}

/* luadoc (method)
 *
 * Swaps two vectors.
 *
 * @static
 * @name     swap
 * @param    (Vec4) vec1 First vector.
 * @param    (Vec4) vec2 Second vector.
 *
 */
void Vec4::swap(Vec4& vec1, Vec4& vec2) {
  std::swap(vec1, vec2);
}

/* luadoc (method)
 *
 * Returns the component-wise minimum of two vectors.
 *
 * @static
 * @name     min
 * @param    (Vec4) vec1 First vector.
 * @param    (Vec4) vec2 Second vector.
 * @return   (Vec4) Component-wise minimum.
 *
 */
Vec4 Vec4::min(const Vec4& vec1, const Vec4& vec2) {
  return Vec4{std::min(vec1.x, vec2.x), std::min(vec1.y, vec2.y), std::min(vec1.z, vec2.z), std::min(vec1.w, vec2.w)};
}

/* luadoc (method)
 *
 * Returns the component-wise maximum of two vectors.
 *
 * @static
 * @name     max
 * @param    (Vec4) vec1 First vector.
 * @param    (Vec4) vec2 Second vector.
 * @return   (Vec4) Component-wise maximum.
 *
 */
Vec4 Vec4::max(const Vec4& vec1, const Vec4& vec2) {
  return Vec4{std::max(vec1.x, vec2.x), std::max(vec1.y, vec2.y), std::max(vec1.z, vec2.z), std::max(vec1.w, vec2.w)};
}

/* luadoc (method)
 *
 * Returns the component-wise product of two vectors.
 *
 * @static
 * @name     prod
 * @param    (Vec4) vec1 First vector.
 * @param    (Vec4) vec2 Second vector.
 * @return   (Vec4) Component-wise product.
 *
 */
Vec4 Vec4::prod(const Vec4& vec1, const Vec4& vec2) {
  return Vec4{vec1.x * vec2.x, vec1.y * vec2.y, vec1.z * vec2.z, vec1.w * vec2.w};
}

/* luadoc (method)
 *
 * Returns the dot product of two vectors.
 *
 * @static
 * @name     dot
 * @param    (Vec4) vec1 First vector.
 * @param    (Vec4) vec2 Second vector.
 * @return   (number) Dot product.
 *
 */
float Vec4::dot(const Vec4& vec1, const Vec4& vec2) {
  return vec1.x * vec2.x + vec1.y * vec2.y + vec1.z * vec2.z + vec1.w * vec2.w;
}

/* luadoc (method)
 *
 * Linearly interpolates between two vectors.
 *
 * @static
 * @name     lerp
 * @param    (number) t Interpolation factor (typically 0..1).
 * @param    (Vec4) v1 Start vector.
 * @param    (Vec4) v2 End vector.
 * @return   (Vec4) Interpolated vector.
 *
 */
Vec4 Vec4::lerp(float t, const Vec4& v1, const Vec4& v2) {
  return Vec4{v1.x + t * (v2.x - v1.x), v1.y + t * (v2.y - v1.y), v1.z + t * (v2.z - v1.z), v1.w + t * (v2.w - v1.w)};
}

// ---------------- Mat3 ----------------
/* luadoc (class)
 *
 * 3x3 matrix with common transformation utilities.
 *
 * @name     Mat3
 * @side     shared
 * @category Math
 *
 */
/* luadoc (constructor)
 *
 * Creates an identity matrix.
 *
 */
Mat3::Mat3() : mat_(1.0f) {
}

/* luadoc (constructor)
 *
 * Creates a matrix initialized with a scalar value.
 *
 * @param    (number) value Scalar initialization value.
 *
 */
Mat3::Mat3(float value) : mat_(value) {
}

/* luadoc (constructor)
 *
 * Creates a matrix from three row vectors.
 *
 * @param    (Vec3) v0 First row vector.
 * @param    (Vec3) v1 Second row vector.
 * @param    (Vec3) v2 Third row vector.
 *
 */
Mat3::Mat3(const Vec3& v0, const Vec3& v1, const Vec3& v2) : mat_(MakeRowMatrix(v0, v1, v2)) {
}

/* luadoc (method)
 *
 * Resets the matrix to identity.
 *
 * @name     makeIdentity
 *
 */
void Mat3::makeIdentity() {
  mat_ = glm::mat3(1.0f);
}

/* luadoc (method)
 *
 * Sets all matrix elements to zero.
 *
 * @name     makeZero
 *
 */
void Mat3::makeZero() {
  mat_ = glm::mat3(0.0f);
}

/* luadoc (method)
 *
 * Orthonormalizes the matrix basis vectors.
 *
 * This makes the basis vectors unit-length and mutually orthogonal.
 *
 * @name     makeOrthonormal
 *
 */
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

/* luadoc (method)
 *
 * Returns true if the matrix basis vectors are orthonormal.
 *
 * @name     isUpper3x3Orthonormal
 * @return   (bool) True if orthonormal within epsilon tolerance.
 *
 */
bool Mat3::isUpper3x3Orthonormal() const {
  const glm::vec3 right = mat_[0];
  const glm::vec3 up = mat_[1];
  const glm::vec3 at = mat_[2];
  const bool unit_lengths =
      std::abs(glm::length2(right) - 1.0f) < kEpsilon && std::abs(glm::length2(up) - 1.0f) < kEpsilon && std::abs(glm::length2(at) - 1.0f) < kEpsilon;
  const bool orthogonal =
      std::abs(glm::dot(right, up)) < kEpsilon && std::abs(glm::dot(right, at)) < kEpsilon && std::abs(glm::dot(up, at)) < kEpsilon;
  return unit_lengths && orthogonal;
}

/* luadoc (method)
 *
 * Returns the transposed matrix.
 *
 * @name     transpose
 * @return   (Mat3) Transposed matrix.
 *
 */
Mat3 Mat3::transpose() const {
  Mat3 result;
  result.mat_ = glm::transpose(mat_);
  return result;
}

/* luadoc (method)
 *
 * Returns the inverse matrix.
 *
 * @name     inverse
 * @return   (Mat3) Inverse matrix.
 *
 */
Mat3 Mat3::inverse() const {
  Mat3 result;
  result.mat_ = glm::inverse(mat_);
  return result;
}

/* luadoc (method)
 *
 * Rotates a vector by this matrix.
 *
 * @name     rotate
 * @param    (Vec3) vec Vector to rotate.
 * @return   (Vec3) Rotated vector.
 *
 */
Vec3 Mat3::rotate(const Vec3& vec) const {
  return FromGlm(mat_ * ToGlm(vec));
}

/* luadoc (method)
 *
 * Sets the right basis vector.
 *
 * @name     setRightVector
 * @param    (Vec3) vec New right vector.
 *
 */
void Mat3::setRightVector(const Vec3& vec) {
  mat_[0] = ToGlm(vec);
}

/* luadoc (method)
 *
 * Returns the right basis vector.
 *
 * @name     getRightVector
 * @return   (Vec3) Right vector.
 *
 */
Vec3 Mat3::getRightVector() const {
  return FromGlm(mat_[0]);
}

/* luadoc (method)
 *
 * Sets the up basis vector.
 *
 * @name     setUpVector
 * @param    (Vec3) vec New up vector.
 *
 */
void Mat3::setUpVector(const Vec3& vec) {
  mat_[1] = ToGlm(vec);
}

/* luadoc (method)
 *
 * Returns the up basis vector.
 *
 * @name     getUpVector
 * @return   (Vec3) Up vector.
 *
 */
Vec3 Mat3::getUpVector() const {
  return FromGlm(mat_[1]);
}

/* luadoc (method)
 *
 * Sets the forward (at) basis vector.
 *
 * @name     setAtVector
 * @param    (Vec3) vec New at vector.
 *
 */
void Mat3::setAtVector(const Vec3& vec) {
  mat_[2] = ToGlm(vec);
}

/* luadoc (method)
 *
 * Returns the forward (at) basis vector.
 *
 * @name     getAtVector
 * @return   (Vec3) At vector.
 *
 */
Vec3 Mat3::getAtVector() const {
  return FromGlm(mat_[2]);
}

/* luadoc (method)
 *
 * Resets rotation to identity.
 *
 * @name     resetRotation
 *
 */
void Mat3::resetRotation() {
  makeIdentity();
}

/* luadoc (method)
 *
 * Returns a copy of this matrix with scaling removed.
 *
 * @name     extractRotation
 * @return   (Mat3) Rotation-only matrix.
 *
 */
Mat3 Mat3::extractRotation() const {
  Mat3 result = *this;
  Vec3 scaling = result.extractScaling();
  if (std::abs(scaling.x) > kEpsilon && std::abs(scaling.y) > kEpsilon && std::abs(scaling.z) > kEpsilon) {
    result.postScale(Vec3{1.0f / scaling.x, 1.0f / scaling.y, 1.0f / scaling.z});
  }
  return result;
}

/* luadoc (method)
 *
 * Extracts scaling factors from the basis vectors.
 *
 * @name     extractScaling
 * @return   (Vec3) Scaling factors for x, y and z axes.
 *
 */
Vec3 Mat3::extractScaling() const {
  return Vec3{std::sqrt(glm::length2(mat_[0])), std::sqrt(glm::length2(mat_[1])), std::sqrt(glm::length2(mat_[2]))};
}

/* luadoc (method)
 *
 * Post-multiplies a rotation around the X axis (degrees).
 *
 * @name     postRotateX
 * @param    (number) angle_degrees Rotation angle in degrees.
 *
 */
void Mat3::postRotateX(float angle_degrees) {
  const float radians = glm::radians(angle_degrees);
  const glm::mat3 rotation = glm::mat3(glm::rotate(glm::mat4(1.0f), radians, glm::vec3(1.0f, 0.0f, 0.0f)));
  mat_ *= rotation;
}

/* luadoc (method)
 *
 * Post-multiplies a rotation around the Y axis (degrees).
 *
 * @name     postRotateY
 * @param    (number) angle_degrees Rotation angle in degrees.
 *
 */
void Mat3::postRotateY(float angle_degrees) {
  const float radians = glm::radians(angle_degrees);
  const glm::mat3 rotation = glm::mat3(glm::rotate(glm::mat4(1.0f), radians, glm::vec3(0.0f, 1.0f, 0.0f)));
  mat_ *= rotation;
}

/* luadoc (method)
 *
 * Post-multiplies a rotation around the Z axis (degrees).
 *
 * @name     postRotateZ
 * @param    (number) angle_degrees Rotation angle in degrees.
 *
 */
void Mat3::postRotateZ(float angle_degrees) {
  const float radians = glm::radians(angle_degrees);
  const glm::mat3 rotation = glm::mat3(glm::rotate(glm::mat4(1.0f), radians, glm::vec3(0.0f, 0.0f, 1.0f)));
  mat_ *= rotation;
}

/* luadoc (method)
 *
 * Pre-multiplies a scaling transformation.
 *
 * @name     preScale
 * @param    (Vec3) scale Scaling factors for x, y and z axes.
 *
 */
void Mat3::preScale(const Vec3& scale) {
  const glm::mat3 scale_mat = glm::mat3(glm::scale(glm::mat4(1.0f), glm::vec3(scale.x, scale.y, scale.z)));
  mat_ = scale_mat * mat_;
}

/* luadoc (method)
 *
 * Post-multiplies a scaling transformation.
 *
 * @name     postScale
 * @param    (Vec3) scale Scaling factors for x, y and z axes.
 *
 */
void Mat3::postScale(const Vec3& scale) {
  const glm::mat3 scale_mat = glm::mat3(glm::scale(glm::mat4(1.0f), glm::vec3(scale.x, scale.y, scale.z)));
  mat_ *= scale_mat;
}

/* luadoc (method)
 *
 * Swaps two matrices.
 *
 * @static
 * @name     swap
 * @param    (Mat3) mat1 First matrix.
 * @param    (Mat3) mat2 Second matrix.
 *
 */
void Mat3::swap(Mat3& mat1, Mat3& mat2) {
  std::swap(mat1.mat_, mat2.mat_);
}

/* luadoc (method)
 *
 * Returns a pointer to the raw matrix data (column-major float array).
 *
 * @name     data
 * @return   (userdata) Pointer to matrix float data.
 *
 */
const float* Mat3::data() const {
  return reinterpret_cast<const float*>(&mat_[0][0]);
}

// ---------------- Mat4 ----------------
/* luadoc (class)
 *
 * 4x4 matrix with common transformation utilities (rotation, scaling, translation).
 *
 * @name     Mat4
 * @side     shared
 * @category Math
 *
 */

/* luadoc (constructor)
 *
 * Creates an identity matrix.
 *
 */
Mat4::Mat4() : mat_(1.0f) {
}

/* luadoc (constructor)
 *
 * Creates a matrix initialized with a scalar value.
 *
 * @param    (number) value Scalar initialization value.
 *
 */
Mat4::Mat4(float value) : mat_(value) {
}

/* luadoc (constructor)
 *
 * Creates a matrix from four row vectors.
 *
 * @param    (Vec4) v0 First row vector.
 * @param    (Vec4) v1 Second row vector.
 * @param    (Vec4) v2 Third row vector.
 * @param    (Vec4) v3 Fourth row vector.
 *
 */
Mat4::Mat4(const Vec4& v0, const Vec4& v1, const Vec4& v2, const Vec4& v3) : mat_(MakeRowMatrix(v0, v1, v2, v3)) {
}

/* luadoc (method)
 *
 * Resets the matrix to identity.
 *
 * @name     makeIdentity
 *
 */
void Mat4::makeIdentity() {
  mat_ = glm::mat4(1.0f);
}

/* luadoc (method)
 *
 * Sets all matrix elements to zero.
 *
 * @name     makeZero
 *
 */
void Mat4::makeZero() {
  mat_ = glm::mat4(0.0f);
}

/* luadoc (method)
 *
 * Orthonormalizes the upper-left 3x3 basis vectors while preserving translation.
 *
 * @name     makeOrthonormal
 *
 */
void Mat4::makeOrthonormal() {
  glm::mat3 upper_left(mat_);
  Mat3 helper;
  helper.mat_ = upper_left;
  helper.makeOrthonormal();
  glm::vec4 last_column = mat_[3];
  mat_ = glm::mat4(helper.mat_);
  mat_[3] = last_column;
}

/* luadoc (method)
 *
 * Returns true if the upper 3x3 basis vectors are orthonormal.
 *
 * @name     isUpper3x3Orthonormal
 * @return   (bool) True if orthonormal within epsilon tolerance.
 *
 */
bool Mat4::isUpper3x3Orthonormal() const {
  return Mat3(Vec3{mat_[0][0], mat_[1][0], mat_[2][0]}, Vec3{mat_[0][1], mat_[1][1], mat_[2][1]}, Vec3{mat_[0][2], mat_[1][2], mat_[2][2]})
      .isUpper3x3Orthonormal();
}

/* luadoc (method)
 *
 * Returns the transposed matrix.
 *
 * @name     transpose
 * @return   (Mat4) Transposed matrix.
 *
 */
Mat4 Mat4::transpose() const {
  Mat4 result;
  result.mat_ = glm::transpose(mat_);
  return result;
}

/* luadoc (method)
 *
 * Returns the inverse matrix.
 *
 * @name     inverse
 * @return   (Mat4) Inverse matrix.
 *
 */
Mat4 Mat4::inverse() const {
  Mat4 result;
  result.mat_ = glm::inverse(mat_);
  return result;
}

/* luadoc (method)
 *
 * Returns the inverse of a linear transform with translation.
 *
 * This is intended for typical transform matrices (rotation/scale + translation).
 *
 * @name     inverseLinTrafo
 * @return   (Mat4) Inverse linear transform matrix.
 *
 */
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

/* luadoc (method)
 *
 * Rotates a vector by the matrix (ignores translation).
 *
 * @name     rotate
 * @param    (Vec3) vec Vector to rotate.
 * @return   (Vec3) Rotated vector.
 *
 */
Vec3 Mat4::rotate(const Vec3& vec) const {
  return FromGlm(glm::vec3(mat_ * glm::vec4(ToGlm(vec), 0.0f)));
}

/* luadoc (method)
 *
 * Sets the right basis vector.
 *
 * @name     setRightVector
 * @param    (Vec3) vec New right vector.
 *
 */
void Mat4::setRightVector(const Vec3& vec) {
  mat_[0] = glm::vec4(ToGlm(vec), mat_[0][3]);
}

/* luadoc (method)
 *
 * Returns the right basis vector.
 *
 * @name     getRightVector
 * @return   (Vec3) Right vector.
 *
 */
Vec3 Mat4::getRightVector() const {
  return FromGlm(glm::vec3(mat_[0]));
}

/* luadoc (method)
 *
 * Sets the forward (at) basis vector.
 *
 * @name     setAtVector
 * @param    (Vec3) vec New at vector.
 *
 */
void Mat4::setAtVector(const Vec3& vec) {
  mat_[2] = glm::vec4(ToGlm(vec), mat_[2][3]);
}

/* luadoc (method)
 *
 * Returns the forward (at) basis vector.
 *
 * @name     getAtVector
 * @return   (Vec3) At vector.
 *
 */
Vec3 Mat4::getAtVector() const {
  return FromGlm(glm::vec3(mat_[2]));
}

/* luadoc (method)
 *
 * Sets the up basis vector.
 *
 * @name     setUpVector
 * @param    (Vec3) vec New up vector.
 *
 */
void Mat4::setUpVector(const Vec3& vec) {
  mat_[1] = glm::vec4(ToGlm(vec), mat_[1][3]);
}

/* luadoc (method)
 *
 * Returns the up basis vector.
 *
 * @name     getUpVector
 * @return   (Vec3) Up vector.
 *
 */
Vec3 Mat4::getUpVector() const {
  return FromGlm(glm::vec3(mat_[1]));
}

/* luadoc (method)
 *
 * Sets the translation component.
 *
 * @name     setTranslation
 * @param    (Vec3) vec New translation vector.
 *
 */
void Mat4::setTranslation(const Vec3& vec) {
  mat_[3] = glm::vec4(ToGlm(vec), 1.0f);
}

/* luadoc (method)
 *
 * Returns the translation component.
 *
 * @name     getTranslation
 * @return   (Vec3) Translation vector.
 *
 */
Vec3 Mat4::getTranslation() const {
  return FromGlm(glm::vec3(mat_[3]));
}

/* luadoc (method)
 *
 * Resets rotation to identity while preserving translation.
 *
 * @name     resetRotation
 *
 */
void Mat4::resetRotation() {
  mat_[0] = glm::vec4(1.0f, 0.0f, 0.0f, mat_[0][3]);
  mat_[1] = glm::vec4(0.0f, 1.0f, 0.0f, mat_[1][3]);
  mat_[2] = glm::vec4(0.0f, 0.0f, 1.0f, mat_[2][3]);
}

/* luadoc (method)
 *
 * Extracts the rotation part of the matrix (scaling removed).
 *
 * @name     extractRotation
 * @return   (Mat3) Rotation matrix.
 *
 */
Mat3 Mat4::extractRotation() const {
  Mat3 rotation;
  rotation.mat_ = glm::mat3(mat_);
  Vec3 scaling = rotation.extractScaling();
  if (std::abs(scaling.x) > kEpsilon && std::abs(scaling.y) > kEpsilon && std::abs(scaling.z) > kEpsilon) {
    rotation.postScale(Vec3{1.0f / scaling.x, 1.0f / scaling.y, 1.0f / scaling.z});
  }
  return rotation;
}

/* luadoc (method)
 *
 * Extracts scaling factors from the matrix basis vectors.
 *
 * @name     extractScaling
 * @return   (Vec3) Scaling factors for x, y and z axes.
 *
 */
Vec3 Mat4::extractScaling() const {
  return Vec3{std::sqrt(glm::length2(glm::vec3(mat_[0]))), std::sqrt(glm::length2(glm::vec3(mat_[1]))), std::sqrt(glm::length2(glm::vec3(mat_[2])))};
}

/* luadoc (method)
 *
 * Post-multiplies a rotation around the X axis (degrees).
 *
 * @name     postRotateX
 * @param    (number) angle_degrees Rotation angle in degrees.
 *
 */
void Mat4::postRotateX(float angle_degrees) {
  const float radians = glm::radians(angle_degrees);
  const glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), radians, glm::vec3(1.0f, 0.0f, 0.0f));
  mat_ *= rotation;
}

/* luadoc (method)
 *
 * Post-multiplies a rotation around the Y axis (degrees).
 *
 * @name     postRotateY
 * @param    (number) angle_degrees Rotation angle in degrees.
 *
 */
void Mat4::postRotateY(float angle_degrees) {
  const float radians = glm::radians(angle_degrees);
  const glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), radians, glm::vec3(0.0f, 1.0f, 0.0f));
  mat_ *= rotation;
}

/* luadoc (method)
 *
 * Post-multiplies a rotation around the Z axis (degrees).
 *
 * @name     postRotateZ
 * @param    (number) angle_degrees Rotation angle in degrees.
 *
 */
void Mat4::postRotateZ(float angle_degrees) {
  const float radians = glm::radians(angle_degrees);
  const glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), radians, glm::vec3(0.0f, 0.0f, 1.0f));
  mat_ *= rotation;
}

/* luadoc (method)
 *
 * Pre-multiplies a scaling transformation.
 *
 * @name     preScale
 * @param    (Vec3) scale Scaling factors for x, y and z axes.
 *
 */
void Mat4::preScale(const Vec3& scale) {
  const glm::mat4 scale_mat = glm::scale(glm::mat4(1.0f), glm::vec3(scale.x, scale.y, scale.z));
  mat_ = scale_mat * mat_;
}

/* luadoc (method)
 *
 * Post-multiplies a scaling transformation.
 *
 * @name     postScale
 * @param    (Vec3) scale Scaling factors for x, y and z axes.
 *
 */
void Mat4::postScale(const Vec3& scale) {
  const glm::mat4 scale_mat = glm::scale(glm::mat4(1.0f), glm::vec3(scale.x, scale.y, scale.z));
  mat_ *= scale_mat;
}

/* luadoc (method)
 *
 * Swaps two matrices.
 *
 * @static
 * @name     swap
 * @param    (Mat4) mat1 First matrix.
 * @param    (Mat4) mat2 Second matrix.
 *
 */
void Mat4::swap(Mat4& mat1, Mat4& mat2) {
  std::swap(mat1.mat_, mat2.mat_);
}

/* luadoc (method)
 *
 * Builds a look-at transform from a position, target and up vector.
 *
 * @static
 * @name     lookAt
 * @param    (Vec3) from Camera/world position.
 * @param    (Vec3) to Target position to look at.
 * @param    (Vec3) up Up direction vector.
 * @return   (Mat4) Look-at matrix.
 *
 */
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

/* luadoc (method)
 *
 * Returns a pointer to the raw matrix data (column-major float array).
 *
 * @name     data
 * @return   (userdata) Pointer to matrix float data.
 *
 */
const float* Mat4::data() const {
  return reinterpret_cast<const float*>(&mat_[0][0]);
}

// ---------------- Quat ----------------
/* luadoc (class)
 *
 * Quaternion representing 3D rotation.
 *
 * Provides conversion to/from matrices, Euler angles and axis-angle, as well as
 * common quaternion operations and interpolation utilities.
 *
 * @name     Quat
 * @side     shared
 * @category Math
 *
 */

/* luadoc (constructor)
 *
 * Creates a quaternion with default values.
 *
 */
Quat::Quat() = default;

/* luadoc (constructor)
 *
 * Creates a quaternion with W component set to the given value.
 *
 * @param    (number) w W component.
 *
 */
Quat::Quat(float w_in) : w(w_in) {
}

/* luadoc (constructor)
 *
 * Creates a quaternion with explicit x, y, z and w components.
 *
 * @param    (number) x X component.
 * @param    (number) y Y component.
 * @param    (number) z Z component.
 * @param    (number) w W component.
 *
 */
Quat::Quat(float x_in, float y_in, float z_in, float w_in) : x(x_in), y(y_in), z(z_in), w(w_in) {
}

/* luadoc (property)
 *
 * X component.
 *
 * @name     x
 * @return   (number) X component value.
 *
 */
/* luadoc (property)
 *
 * Y component.
 *
 * @name     y
 * @return   (number) Y component value.
 *
 */
/* luadoc (property)
 *
 * Z component.
 *
 * @name     z
 * @return   (number) Z component value.
 *
 */
/* luadoc (property)
 *
 * W component.
 *
 * @name     w
 * @return   (number) W component value.
 *
 */

/* luadoc (method)
 *
 * Converts this quaternion to a 3x3 rotation matrix.
 *
 * @name     toMat3
 * @return   (Mat3) Rotation matrix.
 *
 */
Mat3 Quat::toMat3() const {
  Mat3 result;
  const glm::quat quat(w, x, y, z);
  result.mat_ = glm::mat3_cast(quat);
  return result;
}

/* luadoc (method)
 *
 * Sets this quaternion from a 3x3 rotation matrix.
 *
 * @name     fromMat3
 * @param    (Mat3) mat Rotation matrix.
 *
 */
void Quat::fromMat3(const Mat3& mat) {
  const glm::quat quat = glm::quat_cast(mat.mat_);
  x = quat.x;
  y = quat.y;
  z = quat.z;
  w = quat.w;
}

/* luadoc (method)
 *
 * Converts this quaternion to a 4x4 rotation matrix.
 *
 * @name     toMat4
 * @return   (Mat4) Rotation matrix.
 *
 */
Mat4 Quat::toMat4() const {
  Mat4 result;
  const glm::quat quat(w, x, y, z);
  result.mat_ = glm::mat4_cast(quat);
  return result;
}

/* luadoc (method)
 *
 * Sets this quaternion from a 4x4 transform matrix (rotation part).
 *
 * @name     fromMat4
 * @param    (Mat4) mat Transform matrix.
 *
 */
void Quat::fromMat4(const Mat4& mat) {
  const glm::quat quat = glm::quat_cast(mat.mat_);
  x = quat.x;
  y = quat.y;
  z = quat.z;
  w = quat.w;
}

/* luadoc (method)
 *
 * Converts this quaternion to Euler angles.
 *
 * @name     toEuler
 * @return   (Vec3) Euler angles as a 3D vector.
 *
 */
Vec3 Quat::toEuler() const {
  const glm::quat quat(w, x, y, z);
  const glm::vec3 euler = glm::eulerAngles(quat);
  return Vec3{euler.x, euler.y, euler.z};
}

/* luadoc (method)
 *
 * Sets this quaternion from Euler angles.
 *
 * @name     fromEuler
 * @param    (Vec3) vec Euler angles as a 3D vector.
 *
 */
void Quat::fromEuler(const Vec3& vec) {
  const glm::quat quat = glm::quat(glm::vec3(vec.x, vec.y, vec.z));
  x = quat.x;
  y = quat.y;
  z = quat.z;
  w = quat.w;
}

/* luadoc (method)
 *
 * Converts this quaternion to axis-angle representation.
 *
 * The returned Vec4 contains (axis.x, axis.y, axis.z, angle).
 *
 * @name     toAxisAngle
 * @return   (Vec4) Axis-angle as (x, y, z, angle).
 *
 */
Vec4 Quat::toAxisAngle() const {
  const glm::quat quat(w, x, y, z);
  const glm::vec3 axis = glm::axis(quat);
  const float angle = glm::angle(quat);
  return Vec4{axis.x, axis.y, axis.z, angle};
}

/* luadoc (method)
 *
 * Sets this quaternion from an axis and angle.
 *
 * @name     fromAxisAngle
 * @param    (Vec3) axis Rotation axis (typically normalized).
 * @param    (number) angle Rotation angle.
 *
 */
void Quat::fromAxisAngle(const Vec3& axis, float angle) {
  const glm::quat quat = glm::angleAxis(angle, ToGlm(axis));
  x = quat.x;
  y = quat.y;
  z = quat.z;
  w = quat.w;
}

/* luadoc (method)
 *
 * Sets this quaternion to identity (no rotation).
 *
 * @name     makeIdentity
 *
 */
void Quat::makeIdentity() {
  x = 0.0f;
  y = 0.0f;
  z = 0.0f;
  w = 1.0f;
}

/* luadoc (method)
 *
 * Returns true if this quaternion is identity (within epsilon tolerance).
 *
 * @name     isIdentity
 * @return   (bool) True if identity.
 *
 */
bool Quat::isIdentity() const {
  return std::abs(x) < kEpsilon && std::abs(y) < kEpsilon && std::abs(z) < kEpsilon && std::abs(w - 1.0f) < kEpsilon;
}

/* luadoc (method)
 *
 * Returns the quaternion length.
 *
 * @name     len
 * @return   (number) Quaternion length.
 *
 */
float Quat::len() const {
  return std::sqrt(len2());
}

/* luadoc (method)
 *
 * Returns the squared quaternion length.
 *
 * @name     len2
 * @return   (number) Squared quaternion length.
 *
 */
float Quat::len2() const {
  return x * x + y * y + z * z + w * w;
}

/* luadoc (method)
 *
 * Returns an approximate quaternion length.
 *
 * @name     lenApprox
 * @return   (number) Approximate length.
 *
 */
float Quat::lenApprox() const {
  const float length_sq = len2();
  if (length_sq <= 0.0f) {
    return 0.0f;
  }
  return 1.0f / glm::fastInverseSqrt(length_sq);
}

/* luadoc (method)
 *
 * Normalizes the quaternion in-place.
 *
 * If the quaternion length is zero, no change is applied.
 *
 * @name     normalize
 * @return   (Quat) This quaternion (normalized).
 *
 */
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

/* luadoc (method)
 *
 * Normalizes the quaternion in-place using an epsilon check.
 *
 * If the quaternion length is below a small threshold, no change is applied.
 *
 * @name     normalizeSafe
 * @return   (Quat) This quaternion (normalized).
 *
 */
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

/* luadoc (method)
 *
 * Normalizes the quaternion in-place using an approximate inverse square root.
 *
 * @name     normalizeApprox
 * @return   (Quat) This quaternion (normalized).
 *
 */
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

/* luadoc (method)
 *
 * Sets quaternion components.
 *
 * @name     set
 * @param    (number) x X component.
 * @param    (number) y Y component.
 * @param    (number) z Z component.
 * @param    (number) w W component.
 *
 */
void Quat::set(float x_in, float y_in, float z_in, float w_in) {
  x = x_in;
  y = y_in;
  z = z_in;
  w = w_in;
}

/* luadoc (method)
 *
 * Returns the inverse quaternion.
 *
 * For unit quaternions, this is equivalent to conjugate().
 *
 * @name     inverse
 * @return   (Quat) Inverse quaternion.
 *
 */
Quat Quat::inverse() const {
  const float length_sq = len2();
  if (length_sq <= 0.0f) {
    return Quat();
  }
  const float inv = 1.0f / length_sq;
  return Quat{-x * inv, -y * inv, -z * inv, w * inv};
}

/* luadoc (method)
 *
 * Returns the conjugate quaternion.
 *
 * @name     conjugate
 * @return   (Quat) Conjugated quaternion.
 *
 */
Quat Quat::conjugate() const {
  return Quat{-x, -y, -z, w};
}

/* luadoc (method)
 *
 * Returns the dot product of two quaternions.
 *
 * @static
 * @name     dot
 * @param    (Quat) quat1 First quaternion.
 * @param    (Quat) quat2 Second quaternion.
 * @return   (number) Dot product.
 *
 */
float Quat::dot(const Quat& quat1, const Quat& quat2) {
  return quat1.x * quat2.x + quat1.y * quat2.y + quat1.z * quat2.z + quat1.w * quat2.w;
}

/* luadoc (method)
 *
 * Linearly interpolates between two quaternions.
 *
 * @static
 * @name     lerp
 * @param    (number) t Interpolation factor (typically 0..1).
 * @param    (Quat) q1 Start quaternion.
 * @param    (Quat) q2 End quaternion.
 * @return   (Quat) Interpolated quaternion.
 *
 */
Quat Quat::lerp(float t, const Quat& q1, const Quat& q2) {
  return Quat{q1.x + t * (q2.x - q1.x), q1.y + t * (q2.y - q1.y), q1.z + t * (q2.z - q1.z), q1.w + t * (q2.w - q1.w)};
}

/* luadoc (method)
 *
 * Spherically interpolates between two quaternions.
 *
 * @static
 * @name     slerp
 * @param    (number) t Interpolation factor (typically 0..1).
 * @param    (Quat) q1 Start quaternion.
 * @param    (Quat) q2 End quaternion.
 * @return   (Quat) Spherically interpolated quaternion.
 *
 */
Quat Quat::slerp(float t, const Quat& q1, const Quat& q2) {
  const glm::quat lhs(q1.w, q1.x, q1.y, q1.z);
  const glm::quat rhs(q2.w, q2.x, q2.y, q2.z);
  const glm::quat result = glm::slerp(lhs, rhs, t);
  return Quat{result.x, result.y, result.z, result.w};
}

/* luadoc (method)
 *
 * Performs a squad-style interpolation using three quaternions.
 *
 * @static
 * @name     squad
 * @param    (number) t Interpolation factor (typically 0..1).
 * @param    (Quat) q1 First quaternion.
 * @param    (Quat) q2 Second quaternion.
 * @param    (Quat) q3 Third quaternion.
 * @return   (Quat) Interpolated quaternion.
 *
 */
Quat Quat::squad(float t, const Quat& q1, const Quat& q2, const Quat& q3) {
  const glm::quat qa(q1.w, q1.x, q1.y, q1.z);
  const glm::quat qb(q2.w, q2.x, q2.y, q2.z);
  const glm::quat qc(q3.w, q3.x, q3.y, q3.z);
  const glm::quat ab = glm::slerp(qa, qb, t);
  const glm::quat bc = glm::slerp(qb, qc, t);
  const glm::quat result = glm::slerp(ab, bc, 2.0f * t * (1.0f - t));
  return Quat{result.x, result.y, result.z, result.w};
}

/* luadoc (method)
 *
 * Creates a quaternion that looks in the given forward direction with the given up direction.
 *
 * @static
 * @name     lookRotation
 * @param    (Vec3) forward Forward direction.
 * @param    (Vec3) up Up direction.
 * @return   (Quat) Resulting rotation quaternion.
 *
 */
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
/* luadoc (func)
*
* This function will get the 2d distance between two points.
*
* @name     getDistance2d
* @side     shared
* @category Math
* @param  (float) x1      The position on X axis of the first point.
* @param  (float) y1      The position on Y axis of the first point.
* @param  (float) x2      The position on X axis of the second point.
* @param  (float) y2      The position on Y axis of the second point.
* @return (float)        The distance between the two points.
*
*/
  lua["getDistance2d"] = [](float x1, float y1, float x2, float y2) {
    const float dx = x1 - x2;
    const float dy = y1 - y2;
    return std::sqrt(dx * dx + dy * dy);
  };

/* luadoc (func)
*
* This function will get the 3d distance between two points.
*
* @name     getDistance3d
* @side     shared
* @category Math
* @param  (float) x1      The position on X axis of the first point.
* @param  (float) y1      The position on Y axis of the first point.
* @param  (float) z1      The position on Z axis of the first point.
* @param  (float) x2      The position on X axis of the second point.
* @param  (float) y2      The position on Y axis of the second point.
* @param  (float) z2      The position on Z axis of the second point.
* @return (float)        The distance between the two points.
*
*/
  lua["getDistance3d"] = [](float x1, float y1, float z1, float x2, float y2, float z2) {
    const float dx = x1 - x2;
    const float dy = y1 - y2;
    const float dz = z1 - z2;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
  };
  
/* luadoc (func)
*
* This function will get angle on Y axis directed towards the second point.
*
* @name     getVectorAngle
* @side     shared
* @category Math
* @param  (float) x1      The position on X axis of the first point.
* @param  (float) y1      The position on Y axis of the first point.
* @param  (float) x2      The position on X axis of the second point.
* @param  (float) y2      The position on Y axis of the second point.
* @return (float)        The angle on Y axis directed towards the second point.
*
*/
  lua["getVectorAngle"] = [](float x1, float y1, float x2, float y2) {
    return std::atan2(y2 - y1, x2 - x1);
  };

  lua.new_usertype<Vec2>("Vec2", sol::constructors<Vec2(), Vec2(float), Vec2(float, float)>(), "x", &Vec2::x, "y", &Vec2::y, "len", &Vec2::len,
                         "len2", &Vec2::len2, "lenApprox", &Vec2::lenApprox, "distance", &Vec2::distance, "normalize", &Vec2::normalize,
                         "normalizeSafe", &Vec2::normalizeSafe, "normalizeApprox", &Vec2::normalizeApprox, "set", &Vec2::set, "isEqualEps",
                         &Vec2::isEqualEps, "abs", &Vec2::abs, "swap", &Vec2::swap, "min", &Vec2::min, "max", &Vec2::max, "prod", &Vec2::prod, "dot",
                         &Vec2::dot, "lerp", &Vec2::lerp);

  lua.new_usertype<Vec3>("Vec3", sol::constructors<Vec3(), Vec3(float), Vec3(float, float, float)>(), "x", &Vec3::x, "y", &Vec3::y, "z", &Vec3::z,
                         "len", &Vec3::len, "len2", &Vec3::len2, "lenApprox", &Vec3::lenApprox, "distance", &Vec3::distance, "distance2d",
                         &Vec3::distance2d, "normalize", &Vec3::normalize, "normalizeSafe", &Vec3::normalizeSafe, "normalizeApprox",
                         &Vec3::normalizeApprox, "set", &Vec3::set, "isEqualEps", &Vec3::isEqualEps, "abs", &Vec3::abs, "reflect", &Vec3::reflect,
                         "swap", &Vec3::swap, "min", &Vec3::min, "max", &Vec3::max, "prod", &Vec3::prod, "dot", &Vec3::dot, "cross", &Vec3::cross,
                         "lerp", &Vec3::lerp);

  lua.new_usertype<Vec4>("Vec4", sol::constructors<Vec4(), Vec4(float), Vec4(float, float, float, float)>(), "x", &Vec4::x, "y", &Vec4::y, "z",
                         &Vec4::z, "w", &Vec4::w, "len", &Vec4::len, "len2", &Vec4::len2, "lenApprox", &Vec4::lenApprox, "normalize",
                         &Vec4::normalize, "normalizeSafe", &Vec4::normalizeSafe, "normalizeApprox", &Vec4::normalizeApprox, "set", &Vec4::set,
                         "isEqualEps", &Vec4::isEqualEps, "abs", &Vec4::abs, "swap", &Vec4::swap, "min", &Vec4::min, "max", &Vec4::max, "prod",
                         &Vec4::prod, "dot", &Vec4::dot, "lerp", &Vec4::lerp);

  lua.new_usertype<Mat3>("Mat3", sol::constructors<Mat3(), Mat3(float), Mat3(const Vec3&, const Vec3&, const Vec3&)>(), "makeIdentity",
                         &Mat3::makeIdentity, "makeZero", &Mat3::makeZero, "makeOrthonormal", &Mat3::makeOrthonormal, "isUpper3x3Orthonormal",
                         &Mat3::isUpper3x3Orthonormal, "transpose", &Mat3::transpose, "inverse", &Mat3::inverse, "rotate", &Mat3::rotate,
                         "getRightVector", &Mat3::getRightVector, "setRightVector", &Mat3::setRightVector, "getUpVector", &Mat3::getUpVector,
                         "setUpVector", &Mat3::setUpVector, "getAtVector", &Mat3::getAtVector, "setAtVector", &Mat3::setAtVector, "resetRotation",
                         &Mat3::resetRotation, "extractRotation", &Mat3::extractRotation, "extractScaling", &Mat3::extractScaling, "postRotateX",
                         &Mat3::postRotateX, "postRotateY", &Mat3::postRotateY, "postRotateZ", &Mat3::postRotateZ, "preScale", &Mat3::preScale,
                         "postScale", &Mat3::postScale, "swap", &Mat3::swap);

  lua.new_usertype<Mat4>(
      "Mat4", sol::constructors<Mat4(), Mat4(float), Mat4(const Vec4&, const Vec4&, const Vec4&, const Vec4&)>(), "makeIdentity", &Mat4::makeIdentity,
      "makeZero", &Mat4::makeZero, "makeOrthonormal", &Mat4::makeOrthonormal, "isUpper3x3Orthonormal", &Mat4::isUpper3x3Orthonormal, "transpose",
      &Mat4::transpose, "inverse", &Mat4::inverse, "inverseLinTrafo", &Mat4::inverseLinTrafo, "rotate", &Mat4::rotate, "getRightVector",
      &Mat4::getRightVector, "setRightVector", &Mat4::setRightVector, "getAtVector", &Mat4::getAtVector, "setAtVector", &Mat4::setAtVector,
      "getUpVector", &Mat4::getUpVector, "setUpVector", &Mat4::setUpVector, "getTranslation", &Mat4::getTranslation, "setTranslation",
      &Mat4::setTranslation, "resetRotation", &Mat4::resetRotation, "extractRotation", &Mat4::extractRotation, "extractScaling",
      &Mat4::extractScaling, "postRotateX", &Mat4::postRotateX, "postRotateY", &Mat4::postRotateY, "postRotateZ", &Mat4::postRotateZ, "preScale",
      &Mat4::preScale, "postScale", &Mat4::postScale, "swap", &Mat4::swap, "lookAt", &Mat4::lookAt);

  lua.new_usertype<Quat>(
      "Quat", sol::constructors<Quat(), Quat(float), Quat(float, float, float, float)>(), "x", &Quat::x, "y", &Quat::y, "z", &Quat::z, "w", &Quat::w,
      "toMat3", &Quat::toMat3, "fromMat3", &Quat::fromMat3, "toMat4", &Quat::toMat4, "fromMat4", &Quat::fromMat4, "toEuler", &Quat::toEuler,
      "fromEuler", &Quat::fromEuler, "toAxisAngle", &Quat::toAxisAngle, "fromAxisAngle", &Quat::fromAxisAngle, "makeIdentity", &Quat::makeIdentity,
      "isIdentity", &Quat::isIdentity, "len", &Quat::len, "len2", &Quat::len2, "lenApprox", &Quat::lenApprox, "normalize", &Quat::normalize,
      "normalizeSafe", &Quat::normalizeSafe, "normalizeApprox", &Quat::normalizeApprox, "set", &Quat::set, "inverse", &Quat::inverse, "conjugate",
      &Quat::conjugate, "dot", &Quat::dot, "lerp", &Quat::lerp, "slerp", &Quat::slerp, "squad", &Quat::squad, "lookRotation", &Quat::lookRotation);
}

}  // namespace bindings
}  // namespace lua