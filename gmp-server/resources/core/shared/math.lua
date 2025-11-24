print('[math.lua] Demonstrating shared math classes')

local v2 = Vec2.new(3, 4)
print(string.format('[math.lua] Vec2 len=%.2f len2=%.2f', v2:len(), v2:len2()))
local v2_norm = Vec2.new(v2.x, v2.y)
v2_norm:normalize()
print(string.format('[math.lua] Vec2 normalized -> (%.2f, %.2f)', v2_norm.x, v2_norm.y))

local a = Vec3.new(1, 0, 0)
local b = Vec3.new(0, 1, 0)
local cross = Vec3.cross(a, b)
print(string.format('[math.lua] Vec3 cross -> (%.2f, %.2f, %.2f)', cross.x, cross.y, cross.z))

local mat3 = Mat3.new()
mat3:postRotateZ(90.0)
local rotated = mat3:rotate(Vec3.new(1, 0, 0))
print(string.format('[math.lua] Mat3 rotate (90 deg around Z) -> (%.2f, %.2f, %.2f)', rotated.x, rotated.y, rotated.z))

local view = Mat4.lookAt(Vec3.new(0, 0, -5), Vec3.new(0, 0, 0), Vec3.new(0, 1, 0))
local translation = view:getTranslation()
print(string.format('[math.lua] Mat4 lookAt translation -> (%.2f, %.2f, %.2f)', translation.x, translation.y, translation.z))

local quat = Quat.lookRotation(Vec3.new(0, 0, 1), Vec3.new(0, 1, 0))
local euler = quat:toEuler()
print(string.format('[math.lua] Quat lookRotation toEuler -> pitch=%.2f yaw=%.2f roll=%.2f', euler.x, euler.y, euler.z))
local axis_angle = quat:toAxisAngle()
print(string.format('[math.lua] Quat axis-angle -> axis(%.2f, %.2f, %.2f) angle=%.2f', axis_angle.x, axis_angle.y, axis_angle.z, axis_angle.w))
