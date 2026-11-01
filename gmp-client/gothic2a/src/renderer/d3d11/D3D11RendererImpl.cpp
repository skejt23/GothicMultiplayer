/*
MIT License

Copyright (c) 2025 Gothic Multiplayer Team.

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

#include "D3D11RendererImpl.h"

#include <d3dcommon.h>
#include <d3dcompiler.h>
#include <dxgi1_5.h>  // For IDXGIFactory5 and tearing support
#include <spdlog/spdlog.h>
#include <wrl/client.h>

// DXGI_FEATURE_PRESENT_ALLOW_TEARING may not be defined in older Windows SDK versions
#ifndef DXGI_PRESENT_ALLOW_TEARING
#define DXGI_PRESENT_ALLOW_TEARING 0x00000200UL
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "../renderer_config.h"
#include "D3D11MapTracker.h"
#include "D3D11VertexBuffer.h"

#ifdef _WIN32
#include <windows.h>
#endif

using Microsoft::WRL::ComPtr;

namespace gmp::renderer::d3d11 {

namespace {

void SetDebugObjectName(ID3D11DeviceChild* obj, const char* name) {
  if (!obj || !name) {
    return;
  }
  // Avoid linking against dxguid.lib just for WKPDID_D3DDebugObjectName.
  // This GUID is stable and used by D3D11 tools (RenderDoc/PIX) to display object names.
  static const GUID kD3DDebugObjectName = {0x429b8c22, 0x9188, 0x4b0c, {0x87, 0x42, 0xac, 0xb0, 0xbf, 0x85, 0xc2, 0x00}};
  obj->SetPrivateData(kD3DDebugObjectName, static_cast<UINT>(strlen(name)), name);
}

// Convert UTF-8/ASCII to wide for ID3DUserDefinedAnnotation.
#ifndef NDEBUG
std::wstring WidenForAnnotation(const char* text) {
  if (!text || text[0] == '\0') {
    return std::wstring();
  }

#ifdef _WIN32
  // Fast path for common short strings.
  wchar_t stack_buf[256];
  const int written = MultiByteToWideChar(CP_UTF8, 0, text, -1, stack_buf, static_cast<int>(std::size(stack_buf)));
  if (written > 0) {
    return std::wstring(stack_buf);
  }

  const int needed = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
  if (needed <= 0) {
    return std::wstring();
  }
  std::wstring out;
  out.resize(static_cast<size_t>(needed));
  MultiByteToWideChar(CP_UTF8, 0, text, -1, out.data(), needed);
  // Remove the trailing null terminator stored in the string.
  if (!out.empty() && out.back() == L'\0') {
    out.pop_back();
  }
  return out;
#else
  (void)text;
  return std::wstring();
#endif
}

struct ScopedGpuEvent {
  ID3DUserDefinedAnnotation* ann = nullptr;
  bool active = false;
  std::wstring label;

  ScopedGpuEvent(ID3DUserDefinedAnnotation* annotation, const char* text) : ann(annotation) {
    if (!ann) {
      return;
    }
    label = WidenForAnnotation(text);
    if (!label.empty()) {
      ann->BeginEvent(label.c_str());
      active = true;
    }
  }

  ~ScopedGpuEvent() {
    if (ann && active) {
      ann->EndEvent();
    }
  }

  ScopedGpuEvent(const ScopedGpuEvent&) = delete;
  ScopedGpuEvent& operator=(const ScopedGpuEvent&) = delete;
};

std::string GetDebugObjectName(ID3D11DeviceChild* obj) {
  if (!obj) {
    return std::string();
  }
  static const GUID kD3DDebugObjectName = {0x429b8c22, 0x9188, 0x4b0c, {0x87, 0x42, 0xac, 0xb0, 0xbf, 0x85, 0xc2, 0x00}};
  UINT size = 0;
  if (FAILED(obj->GetPrivateData(kD3DDebugObjectName, &size, nullptr)) || size == 0) {
    return std::string();
  }
  std::string out;
  out.resize(size);
  if (FAILED(obj->GetPrivateData(kD3DDebugObjectName, &size, out.data())) || size == 0) {
    return std::string();
  }
  while (!out.empty() && out.back() == '\0') {
    out.pop_back();
  }
  return out;
}
#endif  // !NDEBUG

// Check if the system supports DXGI tearing (variable refresh rate).
// Requires Windows 10+ with IDXGIFactory5.
bool CheckTearingSupport(IDXGIFactory* factory) {
  ComPtr<IDXGIFactory5> factory5;
  if (FAILED(factory->QueryInterface(__uuidof(IDXGIFactory5), &factory5))) {
    return false;
  }
  BOOL allow_tearing = FALSE;
  HRESULT hr = factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, sizeof(allow_tearing));
  return SUCCEEDED(hr) && allow_tearing;
}

// Configure swap chain description based on presentation model.
void ConfigureSwapChainDesc(DXGI_SWAP_CHAIN_DESC& scd, HWND hwnd, int width, int height, bool use_flip_model) {
  scd = {};
  scd.BufferDesc.Width = width;
  scd.BufferDesc.Height = height;
  scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  scd.BufferDesc.RefreshRate.Numerator = 0;  // Let DXGI find the best rate
  scd.BufferDesc.RefreshRate.Denominator = 0;
  scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  scd.OutputWindow = hwnd;
  scd.SampleDesc.Count = 1;
  scd.SampleDesc.Quality = 0;
  scd.Windowed = TRUE;  // Always start windowed, switch to fullscreen after

  if (use_flip_model) {
    scd.BufferCount = 2;  // FLIP requires at least 2 buffers
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
  } else {
    scd.BufferCount = 1;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
  }
}

}  // namespace

// ---------------------------------------------------------------------------
// FFP enum conversion helpers - translate D3D9 numeric codes to typed enums.
// ---------------------------------------------------------------------------

// Convert D3D9 TEXCOORDINDEX low bits to TexCoordSource.
constexpr TexCoordSource ToTexCoordSource(unsigned long tci_value) {
  const unsigned long uv_idx = tci_value & 0x7;
  return (uv_idx == 1) ? TexCoordSource::kUV1 : TexCoordSource::kUV0;
}

// Convert D3D9 TEXCOORDINDEX high bits to TexGenMode.
// D3DTSS_TCI_PASSTHRU = 0x00000000
// D3DTSS_TCI_CAMERASPACENORMAL = 0x00010000
// D3DTSS_TCI_CAMERASPACEPOSITION = 0x00020000
// D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR = 0x00030000
constexpr TexGenMode ToTexGenMode(unsigned long tci_value) {
  const unsigned long gen_flags = tci_value & 0xFFFF0000;
  switch (gen_flags) {
    case 0x00010000:
      return TexGenMode::kCameraSpaceNormal;
    case 0x00020000:
      return TexGenMode::kCameraSpacePosition;
    case 0x00030000:
      return TexGenMode::kCameraSpaceReflection;
    case 0x00040000:
      return TexGenMode::kSphereMap;
    default:
      return TexGenMode::kNone;
  }
}

// Convert D3D9 D3DTOP_* / zRND_TOP_* to CombineOp.
// zRND_TOP_DISABLE=0, SELECTARG1=1, SELECTARG2=2, MODULATE=3, MODULATE2X=4, MODULATE4X=5, ADD=6
constexpr CombineOp ToCombineOp(unsigned long d3d_top) {
  switch (d3d_top) {
    case 0:
      return CombineOp::kDisable;
    case 1:
      return CombineOp::kSelectArg1;
    case 2:
      return CombineOp::kSelectArg2;
    case 3:
      return CombineOp::kModulate;
    case 4:
      return CombineOp::kModulate2X;
    case 5:
      return CombineOp::kModulate4X;
    case 6:
      return CombineOp::kAdd;
    case 10:
      return CombineOp::kAddSmooth;
    case 11:
      return CombineOp::kBlendDiffuseAlpha;
    default:
      return CombineOp::kModulate;
  }
}

// Pack uv source + texgen mode into a float payload for AlphaTestCB.
// Format: packed = (uint8)TexCoordSource | ((uint8)TexGenMode << 8)
constexpr float PackUvSourceAndTexGen(TexCoordSource uv_source, TexGenMode texgen_mode) {
  const std::uint32_t packed =
      static_cast<std::uint32_t>(static_cast<std::uint8_t>(uv_source)) | (static_cast<std::uint32_t>(static_cast<std::uint8_t>(texgen_mode)) << 8);
  return static_cast<float>(packed);
}

// Convert D3D9 D3DTA_* / zRND_TA_* to CombineArg.
// zRND_TA_CURRENT=0, DIFFUSE=1, TEXTURE=3, TFACTOR=4, SPECULAR=5
constexpr CombineArg ToCombineArg(unsigned long d3d_ta) {
  switch (d3d_ta) {
    case 0:
      return CombineArg::kCurrent;
    case 1:
      return CombineArg::kDiffuse;
    case 3:
      return CombineArg::kTexture;
    case 4:
      return CombineArg::kTFactor;
    case 5:
      return CombineArg::kSpecular;
    default:
      return CombineArg::kTexture;  // Default to TEXTURE for unknown values
  }
}

// Convert CombineOp to float value for shader CB.
constexpr float CombineOpToFloat(CombineOp op) {
  return static_cast<float>(static_cast<std::uint8_t>(op));
}

// Convert CombineArg to float value for shader CB.
constexpr float CombineArgToFloat(CombineArg arg) {
  return static_cast<float>(static_cast<std::uint8_t>(arg));
}

namespace {

#pragma pack(push, 1)
struct VertexLitNormal2Uv44 {
  float x, y, z;
  float nx, ny, nz;
  std::uint32_t color;
  float u0, v0;
  float u1, v1;
};
#pragma pack(pop)

static_assert(sizeof(VertexLitNormal2Uv44) == 44, "VertexLitNormal2Uv44 must match 44-byte Gothic water vertex stride");

DirectX::XMFLOAT4 MulRowVecMat4(const DirectX::XMFLOAT4& v, const DirectX::XMFLOAT4X4& m) {
  // Matches HLSL mul(float4, row_major matrix): row-vector times matrix.
  DirectX::XMFLOAT4 o;
  o.x = v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + v.w * m.m[3][0];
  o.y = v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + v.w * m.m[3][1];
  o.z = v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + v.w * m.m[3][2];
  o.w = v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + v.w * m.m[3][3];
  return o;
}

DirectX::XMFLOAT4 MulMat4ColVec(const DirectX::XMFLOAT4X4& m, const DirectX::XMFLOAT4& v) {
  // Matches HLSL mul(row_major matrix, float4): matrix times column-vector.
  DirectX::XMFLOAT4 o;
  o.x = m.m[0][0] * v.x + m.m[0][1] * v.y + m.m[0][2] * v.z + m.m[0][3] * v.w;
  o.y = m.m[1][0] * v.x + m.m[1][1] * v.y + m.m[1][2] * v.z + m.m[1][3] * v.w;
  o.z = m.m[2][0] * v.x + m.m[2][1] * v.y + m.m[2][2] * v.z + m.m[2][3] * v.w;
  o.w = m.m[3][0] * v.x + m.m[3][1] * v.y + m.m[3][2] * v.z + m.m[3][3] * v.w;
  return o;
}

DirectX::XMFLOAT3 Normalize3(const DirectX::XMFLOAT3& v) {
  const float len2 = v.x * v.x + v.y * v.y + v.z * v.z;
  if (len2 <= 1e-12f) {
    return {0.0f, 0.0f, 0.0f};
  }
  const float inv_len = 1.0f / std::sqrt(len2);
  return {v.x * inv_len, v.y * inv_len, v.z * inv_len};
}

DirectX::XMFLOAT3 Reflect3(const DirectX::XMFLOAT3& i, const DirectX::XMFLOAT3& n) {
  const float dot_ni = n.x * i.x + n.y * i.y + n.z * i.z;
  return {
      i.x - 2.0f * dot_ni * n.x,
      i.y - 2.0f * dot_ni * n.y,
      i.z - 2.0f * dot_ni * n.z,
  };
}

}  // namespace

// --- Global D3D11 State ---
ID3D11Device* g_D3D11Device = nullptr;
ID3D11DeviceContext* g_D3D11Context = nullptr;

// --- Embedded Shader Source ---
// These are compiled at runtime using D3DCompile.
// Note: Gothic matrices are row-major, so we use row_major qualifier.
// Gothic's VIEW matrix is actually a combined model-view (WORLD) matrix.
// We set that as World and leave View at identity (same as D3D9 path).
// WVP = World * Projection (since View is identity)

// Vertex shader for 3D geometry with normals - stride=32
// Format 0x15: XYZ(12) + NORMAL(12) + TEX(8) = 32 bytes, NO COLOR
// Used for zVBUFFER_VERTTYPE_UT_UL (Untransformed, Unlit)
static const char* kBasicVS = R"(
cbuffer TransformCB : register(b0) {
    row_major matrix World;
    row_major matrix View;
    row_major matrix Projection;
    row_major matrix WorldViewProj;  // Unused - computed in shader
};

cbuffer MaterialCB : register(b1) {
  float4 MatDiffuse;
  float4 MatAmbient;
  float4 MatSpecular;
  float4 MatEmissive;
  float MatPower;
  float MatAlphaRef;
  float MatAlphaTestEnabled;
  float _MatPad0;
};

cbuffer FogCB : register(b2) {
  float4 FogColor;
  float FogStart;
  float FogEnd;
  float FogDensity;
  float FogEnabled;
  float FogRangeEnabled;
  float3 _FogPad;
};

// Light types: 0=off, 1=point, 2=spot, 3=directional
struct GpuLight {
    float4 position;     // xyz = position, w = range
    float4 direction;    // xyz = direction, w = type
    float4 diffuse;      // rgb = color, a = enabled
    float4 attenuation;  // x=const, y=linear, z=quadratic, w=spotCutoff
};

cbuffer LightCB : register(b5) {
    GpuLight lights[8];
    float4 ambient;
    int numActiveLights;
    float3 lightPadding;
};

struct VS_INPUT {
    float3 Pos : POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
};

struct VS_OUTPUT {
    float4 Pos : SV_POSITION;
    float4 Color : COLOR0;
    float2 UV : TEXCOORD0;
    float2 UV2 : TEXCOORD1;    // Dummy UV2 for PS_INPUT compatibility
    float FogDist : TEXCOORD2;
};

float3 ComputeLighting(float3 worldPos, float3 worldNormal) {
  // Match D3D9 fixed-function expectations:
  // DIFFUSE output includes material modulation.
  float3 totalLight = ambient.rgb * MatAmbient.rgb + MatEmissive.rgb;

    for (int i = 0; i < 8; i++) {
        // Skip disabled lights (enabled flag is in diffuse.w)
        if (lights[i].diffuse.w < 0.5)
            continue;

        float3 lightColor = lights[i].diffuse.rgb;
        int lightType = (int)lights[i].direction.w;

        if (lightType == 1) {
            // Point light
            float3 lightDir = lights[i].position.xyz - worldPos;
            float dist = length(lightDir);
            lightDir = normalize(lightDir);

            float range = lights[i].position.w;
            if (dist < range) {
                float NdotL = max(dot(worldNormal, lightDir), 0.0);
                float atten = 1.0 / (lights[i].attenuation.x +
                                     lights[i].attenuation.y * dist +
                                     lights[i].attenuation.z * dist * dist);
                // Match D3D9: Range is a hard cutoff, not an extra falloff term.
                totalLight += (lightColor * MatDiffuse.rgb) * (NdotL * atten);
            }
        }
        else if (lightType == 3) {
            // Directional light
            float3 lightDir = -normalize(lights[i].direction.xyz);
            float NdotL = max(dot(worldNormal, lightDir), 0.0);
            totalLight += (lightColor * MatDiffuse.rgb) * NdotL;
        }
        // Type 2 (spot) not commonly used in Gothic, skip for now
    }

    // Do not clamp here. D3D9's fixed-function pipeline can accumulate lighting
    // above 1.0 before later stage ops clamp the final color.
    // Clamping too early makes MODULATE textures (like nets/leaves) appear darker.
    return totalLight;
}

VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;
    // Transform: World * Projection (View is identity)
    float4 worldPos = mul(float4(input.Pos, 1.0), World);
    output.Pos = mul(worldPos, Projection);

    // Transform normal to world space
    float3 worldNormal = normalize(mul(input.Normal, (float3x3)World));

    // Compute lighting
    float3 litColor = ComputeLighting(worldPos.xyz, worldNormal);
    output.Color = float4(litColor, MatDiffuse.a);

    output.UV = input.UV;
    output.UV2 = input.UV;  // Copy UV to UV2 for shader compatibility
    // Fog distance: range fog uses eye-space distance; otherwise use eye-space Z.
    output.FogDist = (FogRangeEnabled > 0.5) ? length(worldPos.xyz) : worldPos.z;
    return output;
}
)";

// Vertex shader for UT_UL geometry when legacy LIGHTING is disabled.
// D3D9 fixed-function uses vertex diffuse (or white when absent) and ignores material when lighting is off.
// For stride=32 (no COLOR element) we output white so stage0 MODULATE becomes texture-only.
static const char* kBasicUnlitVS = R"(
cbuffer TransformCB : register(b0) {
  row_major matrix World;
  row_major matrix View;
  row_major matrix Projection;
  row_major matrix WorldViewProj;
};

cbuffer FogCB : register(b2) {
  float4 FogColor;
  float FogStart;
  float FogEnd;
  float FogDensity;
  float FogEnabled;
  float FogRangeEnabled;
  float3 _FogPad;
};

struct VS_INPUT {
  float3 Pos : POSITION;
  float3 Normal : NORMAL;
  float2 UV : TEXCOORD0;
};

struct VS_OUTPUT {
  float4 Pos : SV_POSITION;
  float4 Color : COLOR0;
  float2 UV : TEXCOORD0;
  float2 UV2 : TEXCOORD1;
  float FogDist : TEXCOORD2;
};

VS_OUTPUT main(VS_INPUT input) {
  VS_OUTPUT output;
  float4 worldPos = mul(float4(input.Pos, 1.0), World);
  output.Pos = mul(worldPos, Projection);

  // Lighting disabled: default diffuse is white when no vertex color is present.
  output.Color = float4(1.0, 1.0, 1.0, 1.0);

  output.UV = input.UV;
  output.UV2 = input.UV;
  output.FogDist = (FogRangeEnabled > 0.5) ? length(worldPos.xyz) : worldPos.z;
  return output;
}
)";

// Vertex shader for 3D geometry with normals + vertex color - stride=36
// Format: XYZ(12) + NORMAL(12) + COLOR(4) + TEX(8) = 36 bytes
// Used by some UT_UL and UT_L paths depending on zCVertexBuffer format.
static const char* kBasicColorVS = R"(
cbuffer TransformCB : register(b0) {
  row_major matrix World;
  row_major matrix View;
  row_major matrix Projection;
  row_major matrix WorldViewProj;  // Unused - computed in shader
};

cbuffer MaterialCB : register(b1) {
  float4 MatDiffuse;
  float4 MatAmbient;
  float4 MatSpecular;
  float4 MatEmissive;
  float MatPower;
  float MatAlphaRef;
  float MatAlphaTestEnabled;
  float _MatPad0;
};

cbuffer FogCB : register(b2) {
  float4 FogColor;
  float FogStart;
  float FogEnd;
  float FogDensity;
  float FogEnabled;
  float FogRangeEnabled;
  float3 _FogPad;
};

// Light types: 0=off, 1=point, 2=spot, 3=directional
struct GpuLight {
  float4 position;     // xyz = position, w = range
  float4 direction;    // xyz = direction, w = type
  float4 diffuse;      // rgb = color, a = enabled
  float4 attenuation;  // x=const, y=linear, z=quadratic, w=spotCutoff
};

cbuffer LightCB : register(b5) {
  GpuLight lights[8];
  float4 ambient;
  int numActiveLights;
  float3 lightPadding;
};

struct VS_INPUT {
  float3 Pos : POSITION;
  float3 Normal : NORMAL;
  float4 Color : COLOR0;
  float2 UV : TEXCOORD0;
};

struct VS_OUTPUT {
  float4 Pos : SV_POSITION;
  float4 Color : COLOR0;
  float2 UV : TEXCOORD0;
  float2 UV2 : TEXCOORD1;    // Dummy UV2 for PS_INPUT compatibility
  float FogDist : TEXCOORD2;
};

float3 ComputeLighting(float3 worldPos, float3 worldNormal) {
  float3 totalLight = ambient.rgb * MatAmbient.rgb + MatEmissive.rgb;
  for (int i = 0; i < 8; i++) {
    if (lights[i].diffuse.w < 0.5)
      continue;
    float3 lightColor = lights[i].diffuse.rgb;
    int lightType = (int)lights[i].direction.w;

    if (lightType == 1) {
      float3 lightDir = lights[i].position.xyz - worldPos;
      float dist = length(lightDir);
      lightDir = normalize(lightDir);
      float range = lights[i].position.w;
      if (dist < range) {
        float NdotL = max(dot(worldNormal, lightDir), 0.0);
        float atten = 1.0 / (lights[i].attenuation.x +
                   lights[i].attenuation.y * dist +
                   lights[i].attenuation.z * dist * dist);
        // Match D3D9: Range is a hard cutoff, not an extra falloff term.
        totalLight += (lightColor * MatDiffuse.rgb) * (NdotL * atten);
      }
    } else if (lightType == 3) {
      float3 lightDir = -normalize(lights[i].direction.xyz);
      float NdotL = max(dot(worldNormal, lightDir), 0.0);
      totalLight += (lightColor * MatDiffuse.rgb) * NdotL;
    }
  }
  // Do not clamp here; see kBasicVS for rationale.
  return totalLight;
}

VS_OUTPUT main(VS_INPUT input) {
  VS_OUTPUT output;
  float4 worldPos = mul(float4(input.Pos, 1.0), World);
  output.Pos = mul(worldPos, Projection);
  float3 worldNormal = normalize(mul(input.Normal, (float3x3)World));
  float3 litColor = ComputeLighting(worldPos.xyz, worldNormal);
  output.Color = float4(litColor, MatDiffuse.a) * input.Color;
  output.UV = input.UV;
  output.UV2 = input.UV;
  output.FogDist = (FogRangeEnabled > 0.5) ? length(worldPos.xyz) : worldPos.z;
  return output;
}
)";

// Unlit variant for stride=36 UT_UL geometry (has vertex color).
// When lighting is disabled, D3D9 uses the vertex diffuse color directly.
static const char* kBasicColorUnlitVS = R"(
cbuffer TransformCB : register(b0) {
  row_major matrix World;
  row_major matrix View;
  row_major matrix Projection;
  row_major matrix WorldViewProj;
};

cbuffer FogCB : register(b2) {
  float4 FogColor;
  float FogStart;
  float FogEnd;
  float FogDensity;
  float FogEnabled;
  float FogRangeEnabled;
  float3 _FogPad;
};

struct VS_INPUT {
  float3 Pos : POSITION;
  float3 Normal : NORMAL;
  float4 Color : COLOR0;
  float2 UV : TEXCOORD0;
};

struct VS_OUTPUT {
  float4 Pos : SV_POSITION;
  float4 Color : COLOR0;
  float2 UV : TEXCOORD0;
  float2 UV2 : TEXCOORD1;
  float FogDist : TEXCOORD2;
};

VS_OUTPUT main(VS_INPUT input) {
  VS_OUTPUT output;
  float4 worldPos = mul(float4(input.Pos, 1.0), World);
  output.Pos = mul(worldPos, Projection);
  output.Color = input.Color;
  output.UV = input.UV;
  output.UV2 = input.UV;
  output.FogDist = (FogRangeEnabled > 0.5) ? length(worldPos.xyz) : worldPos.z;
  return output;
}
)";

// Vertex shader for pre-lit geometry (no normals) - stride=24
// Used for zVBUFFER_VERTTYPE_UT_L (Untransformed, Lit)
static const char* kLitVS = R"(
cbuffer TransformCB : register(b0) {
    row_major matrix World;
    row_major matrix View;
    row_major matrix Projection;
    row_major matrix WorldViewProj;  // Unused - computed in shader
};

cbuffer FogCB : register(b2) {
  float4 FogColor;
  float FogStart;
  float FogEnd;
  float FogDensity;
  float FogEnabled;
  float FogRangeEnabled;
  float3 _FogPad;
};

struct VS_INPUT {
    float3 Pos : POSITION;
    float4 Color : COLOR0;
    float2 UV : TEXCOORD0;
};

struct VS_OUTPUT {
    float4 Pos : SV_POSITION;
    float4 Color : COLOR0;
    float2 UV : TEXCOORD0;
    float2 UV2 : TEXCOORD1;    // Dummy UV2 for PS_INPUT compatibility
    float FogDist : TEXCOORD2;
};

VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;
    // Transform: World * Projection (View is identity)
    float4 worldPos = mul(float4(input.Pos, 1.0), World);
    output.Pos = mul(worldPos, Projection);
    output.Color = input.Color;
    output.UV = input.UV;
    output.UV2 = input.UV;  // Copy UV to UV2 for shader compatibility
    output.FogDist = (FogRangeEnabled > 0.5) ? length(worldPos.xyz) : worldPos.z;
    return output;
}
)";

// Vertex shader for pre-lit geometry with TWO UV sets (stride=32)
// Format: XYZ(12) + COLOR(4) + TEX0(8) + TEX1(8) = 32 bytes
// Used by many lightmapped world/BSP batches where stage0 and stage1 use different UVs.
static const char* kLitTwoUvVS = R"(
cbuffer TransformCB : register(b0) {
  row_major matrix World;
  row_major matrix View;
  row_major matrix Projection;
  row_major matrix WorldViewProj;  // Unused - computed in shader
};

cbuffer FogCB : register(b2) {
  float4 FogColor;
  float FogStart;
  float FogEnd;
  float FogDensity;
  float FogEnabled;
  float FogRangeEnabled;
  float3 _FogPad;
};

struct VS_INPUT {
  float3 Pos : POSITION;
  float4 Color : COLOR0;
  float2 UV : TEXCOORD0;
  float2 UV2 : TEXCOORD1;
};

struct VS_OUTPUT {
  float4 Pos : SV_POSITION;
  float4 Color : COLOR0;
  float2 UV : TEXCOORD0;
  float2 UV2 : TEXCOORD1;
  float FogDist : TEXCOORD2;
};

VS_OUTPUT main(VS_INPUT input) {
  VS_OUTPUT output;
  float4 worldPos = mul(float4(input.Pos, 1.0), World);
  output.Pos = mul(worldPos, Projection);
  output.Color = input.Color;
  output.UV = input.UV;
  output.UV2 = input.UV2;
  output.FogDist = (FogRangeEnabled > 0.5) ? length(worldPos.xyz) : worldPos.z;
  return output;
}
)";

// Vertex shader for pre-lit geometry WITH normals - stride=44
// Format 0x2D: XYZ(12) + NORMAL(12) + COLOR(4) + TEX(8) + TEX(8) = 44 bytes
// Used for water and other lit geometry with normals
// The normal is read but ignored (pre-lit means lighting already computed)
static const char* kLitNormalVS = R"(
cbuffer TransformCB : register(b0) {
    row_major matrix World;
    row_major matrix View;
    row_major matrix Projection;
    row_major matrix WorldViewProj;  // Unused - computed in shader
};

cbuffer FogCB : register(b2) {
  float4 FogColor;
  float FogStart;
  float FogEnd;
  float FogDensity;
  float FogEnabled;
  float FogRangeEnabled;
  float3 _FogPad;
};

cbuffer AlphaTestCB : register(b4) {
  float AlphaRef;
  float AlphaTestEnabled;
  float AlphaBlendFunc;
  float TexCoordIndex0;
  float TexCoordIndex1;
  float AlphaOp;
  float ColorOp0;
  float ColorOp1;

  float BaseRgbGen;
  float BaseFactorR;
  float BaseFactorG;
  float BaseFactorB;

  float SemFfpEnabled;
  float SemS0ColorOp;
  float SemS0ColorArg1;
  float SemS0ColorArg2;
  float SemS1ColorOp;
  float SemS1ColorArg1;
  float SemS1ColorArg2;

  float TexFactorR;
  float TexFactorG;
  float TexFactorB;
  float TexFactorA;

  float AlphaArg1;
  float AlphaArg2;
  float AlphaArgPad0;
  float AlphaArgPad1;

  float TexTransformFlags0;
  float TexTransformFlags1;
  float2 _PadTexTransformFlags;
  row_major matrix TexTransform0;
  row_major matrix TexTransform1;
};

struct VS_INPUT {
    float3 Pos : POSITION;
    float3 Normal : NORMAL;    // Read but ignored (pre-lit)
    float4 Color : COLOR0;
    float2 UV : TEXCOORD0;
    float2 UV2 : TEXCOORD1;    // Second UV (environment map for water)
};

struct VS_OUTPUT {
    float4 Pos : SV_POSITION;
    float4 Color : COLOR0;
    float2 UV : TEXCOORD0;
    float2 UV2 : TEXCOORD1;    // Second UV passed through
    float FogDist : TEXCOORD2;
};

float2 ComputeTcGen(uint mode, float3 n_view, float3 p_view, float3 v, float stageFlags,
                    row_major matrix stageMat) {

  if (mode == 1) {
    // TexGenMode::kCameraSpaceNormal
    return float2(n_view.x * 0.5 + 0.5, -n_view.y * 0.5 + 0.5);
  }
  if (mode == 2) {
    // TexGenMode::kCameraSpacePosition (rare)
    return frac(p_view.xy * 0.01);
  }
  if (mode == 3) {
    // TexGenMode::kCameraSpaceReflection
    // Camera-space reflection vector. v points from the surface to the camera.
    // HLSL reflect() expects the incident direction, so use -v (camera -> surface).
    float3 r = reflect(-v, n_view);
    if (stageFlags > 0.5) {
      // D3D9 applies the texture matrix to the generated coords.
      // ZenGin commonly uses COUNT2 here.
      if (stageFlags > 1.5 && stageFlags < 2.5) {
        // COUNT2: still apply the matrix to the generated 3D vector, but return XY.
        // Reflection vector is a direction; apply the transform with w=0 so translation
        // (often camera-position encoded) does not cause excessive UV motion/flicker.
        float4 tc = float4(r.x, r.y, r.z, 0.0);
        float4 t = mul(tc, stageMat);
        return t.xy;
      }

      float4 t = mul(float4(r.x, r.y, r.z, 0.0), stageMat);
      return t.xy;
    }

    return float2(r.x, r.y);
  }

  // TexGenMode::kSphereMap (fallback)
  float3 r = reflect(-v, n_view);
  float m = 2.0 * sqrt(r.x * r.x + r.y * r.y + (r.z + 1.0) * (r.z + 1.0));
  m = max(m, 1e-4);
  return float2(r.x / m + 0.5, -r.y / m + 0.5);
}

VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;
    // Transform: World * Projection (View is identity)
    float4 worldPos = mul(float4(input.Pos, 1.0), World);
    output.Pos = mul(worldPos, Projection);
    output.Color = input.Color;
  // Start with the mesh-provided UV sets.
  float2 in_tc0 = input.UV;
  float2 in_tc1 = input.UV2;
  float2 out_tc0 = in_tc0;
  float2 out_tc1 = in_tc1;

    // Packed uv+texgen semantics (from CPU):
    // packed = (uint8)TexCoordSource | ((uint8)TexGenMode << 8)
    uint tci0 = (uint)(TexCoordIndex0 + 0.5);
    uint tci1 = (uint)(TexCoordIndex1 + 0.5);
    uint idx0 = (tci0 & 0xFF);
    uint idx1 = (tci1 & 0xFF);
    uint gen0 = ((tci0 >> 8) & 0xFF);
    uint gen1 = ((tci1 >> 8) & 0xFF);

    // World is already model-view in this renderer.
    float3 n_view = normalize(mul(float4(input.Normal, 0.0), World).xyz);
    float3 p_view = worldPos.xyz;
    float3 v = normalize(-p_view);

    bool conflict = (idx0 == idx1);

    // Compute per-stage coords from vertex UV sets (FFP stages do not chain).
    float2 base0 = (idx0 == 1) ? in_tc1 : in_tc0;
    float2 base1 = (idx1 == 1) ? in_tc1 : in_tc0;

    float2 tc0 = base0;
    if (gen0 != 0) {
      tc0 = ComputeTcGen(gen0, n_view, p_view, v, TexTransformFlags0, TexTransform0);
    } else if (TexTransformFlags0 > 0.5) {
      float4 t0 = mul(float4(base0, 1.0, 1.0), TexTransform0);
      tc0 = t0.xy;
    }

    float2 tc1 = base1;
    if (gen1 != 0) {
      tc1 = ComputeTcGen(gen1, n_view, p_view, v, TexTransformFlags1, TexTransform1);
    } else if (TexTransformFlags1 > 0.5) {
      float4 t1 = mul(float4(base1, 1.0, 1.0), TexTransform1);
      tc1 = t1.xy;
    }

    // Write stage0 into its selected coord set.
    if (idx0 == 0) {
      out_tc0 = tc0;
    } else if (idx0 == 1) {
      out_tc1 = tc0;
    }

    // Write stage1 into its selected set, but when both stages target the same set,
    // route stage1 into the other available slot so PS can still sample distinct coords.
    if (idx1 == 0) {
      if (conflict) {
        out_tc1 = tc1;
      } else {
        out_tc0 = tc1;
      }
    } else if (idx1 == 1) {
      if (conflict) {
        out_tc0 = tc1;
      } else {
        out_tc1 = tc1;
      }
    }

    output.UV = out_tc0;
    output.UV2 = out_tc1;
    output.FogDist = (FogRangeEnabled > 0.5) ? length(worldPos.xyz) : worldPos.z;
    return output;
}
)";

// Vertex shader for pre-lit geometry WITH normals and a single UV - stride=36
// Format: XYZ(12) + NORMAL(12) + COLOR(4) + TEX(8) = 36 bytes
static const char* kLitNormalSingleUvVS = R"(
cbuffer TransformCB : register(b0) {
  row_major matrix World;
  row_major matrix View;
  row_major matrix Projection;
  row_major matrix WorldViewProj;  // Unused - computed in shader
};

cbuffer FogCB : register(b2) {
  float4 FogColor;
  float FogStart;
  float FogEnd;
  float FogDensity;
  float FogEnabled;
  float FogRangeEnabled;
  float3 _FogPad;
};

struct VS_INPUT {
  float3 Pos : POSITION;
  float3 Normal : NORMAL;    // Read but ignored (pre-lit)
  float4 Color : COLOR0;
  float2 UV : TEXCOORD0;
};

struct VS_OUTPUT {
  float4 Pos : SV_POSITION;
  float4 Color : COLOR0;
  float2 UV : TEXCOORD0;
  float2 UV2 : TEXCOORD1;    // Dummy UV2 for PS_INPUT compatibility
  float FogDist : TEXCOORD2;
};

VS_OUTPUT main(VS_INPUT input) {
  VS_OUTPUT output;
  float4 worldPos = mul(float4(input.Pos, 1.0), World);
  output.Pos = mul(worldPos, Projection);
  output.Color = input.Color;
  output.UV = input.UV;
  output.UV2 = input.UV;
  output.FogDist = (FogRangeEnabled > 0.5) ? length(worldPos.xyz) : worldPos.z;
  return output;
}
)";

static const char* kRhwVS = R"(
cbuffer ScreenCB : register(b3) {
    float2 ScreenSize;
    float2 InvScreenSize;
};

struct VS_INPUT {
    float4 PosRhw : POSITION;   // x, y, z, rhw
    float4 Color : COLOR0;
    float2 UV : TEXCOORD0;
};

struct VS_OUTPUT {
    float4 Pos : SV_POSITION;
    float4 Color : COLOR0;
    float2 UV : TEXCOORD0;
};

VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;
  // Convert screen coords to NDC (-1 to 1).
  //
  // IMPORTANT (D3D9 XYZRHW emulation):
  // Gothic supplies rhw = 1/w and expects perspective-correct interpolation
  // driven by that w (even though x/y/z are already post-projection).
  //
  // If we output SV_Position.w = 1.0, D3D11 will interpolate UVs linearly in
  // screen space, which can cause visible artifacts on sky/particle geometry
  // at extreme view angles.
  //
  // To match D3D9, reconstruct clip-space w from rhw and scale the clip-space
  // position so that after division we still land on the same NDC.
  //
  // HALF-PIXEL OFFSET (D3D9->D3D11 migration):
  // D3D9 pre-transformed vertices (XYZRHW) target pixel corners, while D3D11
  // expects pixel centers. Subtracting 0.5 from input coordinates aligns the
  // sampling to match D3D9 behavior, making fonts/UI crisp instead of blurry.
  const float2 pos = input.PosRhw.xy - 0.5;
  const float2 ndc = float2(
    (pos.x * InvScreenSize.x) * 2.0 - 1.0,
    1.0 - (pos.y * InvScreenSize.y) * 2.0);

  const float rhw = input.PosRhw.w;
  const float w = (abs(rhw) > 1e-8) ? (1.0 / rhw) : 1.0;

  // For RHW vertices, z is already in [0,1] range for depth buffer.
  // Provide z in clip space scaled by w, so after division it stays unchanged.
  output.Pos = float4(ndc.x * w, ndc.y * w, input.PosRhw.z * w, w);
    output.Color = input.Color;
    output.UV = input.UV;
    return output;
}
)";

static const char* kBasicPS = R"(
cbuffer FogCB : register(b2) {
    float4 FogColor;
    float FogStart;
    float FogEnd;
    float FogDensity;
    float FogEnabled;
  float FogRangeEnabled;
  float3 _FogPad;
};

cbuffer GammaCB : register(b6) {
  float BrightnessOffset;
  float ContrastScale;
  float GammaExp;
  float GammaEnabled;  // 1.0 when active, 0.0 for fast path (optional)
  float4 _GammaPad;
};

float3 ApplyColorCorrection(float3 rgb) {
  // Modern color grading: Brightness → Contrast → Gamma.
  // At neutral (0.5), all ops are identity: +0, *1 around 0.5, pow(x,1).
  if (GammaEnabled < 0.5) return rgb;  // Optional fast path
  rgb = saturate(rgb + BrightnessOffset);
  rgb = saturate((rgb - 0.5) * ContrastScale + 0.5);
  rgb = pow(rgb, GammaExp);
  return rgb;
}

cbuffer AlphaTestCB : register(b4) {
    float AlphaRef;       // Alpha reference value (0-1)
    float AlphaTestEnabled;
    float AlphaBlendFunc; // 0=MAT_DEFAULT, 1=NONE, 2=BLEND, 3=ADD, 5=MUL, 6=MUL2, 7=TEST, 8=BLEND_TEST
  float TexCoordIndex0; // Stage 0 TEXCOORDINDEX (0 or 1)
  float TexCoordIndex1; // Stage 1 TEXCOORDINDEX (0 or 1)
    float AlphaOp;        // 0=MODULATE (tex*vtx), 1=SELECTARG1 (tex only), 2=SELECTARG2 (vtx only)
    float ColorOp0;       // Stage 0 COLOROP (Gothic zRND_TOP_*): 1=SELECTARG1,2=SELECTARG2,3=MODULATE,4=MODULATE2X,5=MODULATE4X
    float ColorOp1;       // Stage 1 COLOROP (same enum; used for dual-texture combines)

  // Optional semantic data (kept for layout parity with CPU constant buffer).
  float BaseRgbGen;
  float BaseFactorR;
  float BaseFactorG;
  float BaseFactorB;

  float SemFfpEnabled;
  float SemS0ColorOp;
  float SemS0ColorArg1;
  float SemS0ColorArg2;
  float SemS1ColorOp;
  float SemS1ColorArg1;
  float SemS1ColorArg2;

  float TexFactorR;
  float TexFactorG;
  float TexFactorB;
  float TexFactorA;

  float AlphaArg1;
  float AlphaArg2;
  float AlphaArgPad0;
  float AlphaArgPad1;
};

Texture2D tex0 : register(t0);
SamplerState samp0 : register(s0);

struct PS_INPUT {
    float4 Pos : SV_POSITION;
    float4 Color : COLOR0;
    float2 UV : TEXCOORD0;
    float2 UV2 : TEXCOORD1;
    float FogDist : TEXCOORD2;
};

float ResolveAlphaArg(float arg, float texA, float diffuseA, float tfactorA) {
  // Gothic zRND_TA_*: CURRENT=0, DIFFUSE=1, TEXTURE=3, TFACTOR=4
  if (arg > 2.5 && arg < 3.5) {
    return texA;
  }
  if (arg > 0.5 && arg < 1.5) {
    return diffuseA;
  }
  if (arg > 3.5 && arg < 4.5) {
    return tfactorA;
  }
  // CURRENT or unknown: for stage0, CURRENT effectively maps to DIFFUSE.
  return diffuseA;
}

float4 main(PS_INPUT input) : SV_TARGET {
  // Select UV set based on packed uv_source low byte.
  uint tci0 = (uint)(TexCoordIndex0 + 0.5);
  uint idx0 = (tci0 & 0xFF);
  float2 uv = (idx0 == 1) ? input.UV2 : input.UV;
    float4 texColor = tex0.Sample(samp0, uv);
    
    // Alpha test with BC2 compensation via AlphaArgPad0 scale factor.
    // D3D11's BC2 decode yields higher alpha than D3D9; CPU sets scale < 1.0 for BC2.
    if (AlphaTestEnabled > 0.5) {
      float scaledAlpha = texColor.a * AlphaArgPad0;
      if (scaledAlpha < AlphaRef) {
        discard;
      }
    }
    
    // Color: emulate Gothic stage0 COLOROP (subset used by Gothic)
    // zRND_TOP_DISABLE=0, SELECTARG1=1, SELECTARG2=2, MODULATE=3, MODULATE2X=4, MODULATE4X=5,
    // ADD=6, ADDSMOOTH=10, BLENDDIFFUSEALPHA=11
    float4 color;
    if (ColorOp0 > 0.5 && ColorOp0 < 1.5) {
      color.rgb = texColor.rgb;  // SELECTARG1
    } else if (ColorOp0 > 1.5 && ColorOp0 < 2.5) {
      color.rgb = input.Color.rgb;  // SELECTARG2
    } else if (ColorOp0 > 5.5 && ColorOp0 < 6.5) {
      // ADD
      color.rgb = texColor.rgb + input.Color.rgb;
    } else if (ColorOp0 > 9.5 && ColorOp0 < 10.5) {
      // ADDSMOOTH: Arg1 + Arg2 - Arg1*Arg2
      color.rgb = texColor.rgb + input.Color.rgb - (texColor.rgb * input.Color.rgb);
    } else if (ColorOp0 > 10.5 && ColorOp0 < 11.5) {
      // BLENDDIFFUSEALPHA: Arg1*Arg2.a + Arg2*(1-Arg2.a)
      // Gothic typically uses Arg1=TEXTURE and Arg2=DIFFUSE for this op.
      color.rgb = lerp(input.Color.rgb, texColor.rgb, input.Color.a);
    } else {
      // Default: MODULATE
      color.rgb = texColor.rgb * input.Color.rgb;
      if (ColorOp0 > 3.5 && ColorOp0 < 4.5) {
        // MODULATE2X
        color.rgb *= 2.0;
      } else if (ColorOp0 > 4.5) {
        // MODULATE4X
        color.rgb *= 4.0;
      }
    }

    // Do NOT clamp here - D3D9 allows overbright values from lighting to propagate.
    // Clamping happens implicitly at framebuffer write or via ApplyColorCorrection saturate.
    // Premature clamping darkens overbright lighting * texture results.
    
    // Note: Avoid hardcoded brightness boosts for BLEND mode.
    // Water and other alpha-sorted geometry should match the reference renderer
    // purely via fixed-function stage ops + gamma settings.
    
    // Alpha: emulate stage0 alpha args/op (important for water: TFACTOR * DIFFUSE).
    float a1 = ResolveAlphaArg(AlphaArg1, texColor.a, input.Color.a, TexFactorA);
    float a2 = ResolveAlphaArg(AlphaArg2, texColor.a, input.Color.a, TexFactorA);
    // AlphaOp is CPU-normalized: 0=MODULATE, 1=SELECTARG1, 2=SELECTARG2
    if (AlphaOp > 1.5) {
      color.a = a2;
    } else if (AlphaOp > 0.5) {
      color.a = a1;
    } else {
      color.a = a1 * a2;
    }
    
    // IMPORTANT: Do not override alpha for ADD blend.
    // Water env-map pass relies on ALPHAARG1=TFACTOR and ALPHAARG2=DIFFUSE,
    // so alpha must come from the fixed-function alpha args/op emulation above.

    // Multiplicative shadow/darkening passes (MUL/MUL2):
    // D3D9-style projected shadows often encode the falloff in alpha, while RGB may be black outside the circle.
    // If we multiply dest by RGB directly, alpha=0 areas still darken (square shadow).
    // Fix: use alpha as a mask and lerp from a neutral multiply color.
    // - MUL  (DestColor, Zero): neutral is 1.0 (no change).
    // - MUL2 (DestColor, SrcColor): result = src*dest + dest*src = 2*src*dest, neutral is 0.5.
    if (AlphaBlendFunc > 4.5 && AlphaBlendFunc < 6.5) {
      float mask = saturate(color.a);
      float neutral = (AlphaBlendFunc > 5.5) ? 0.5 : 1.0;
      color.rgb = lerp(neutral.xxx, color.rgb, mask);
      color.a = 1.0;
    }
    
    // Apply fog if enabled
    if (FogEnabled > 0.5) {
        float fogFactor = saturate((FogEnd - input.FogDist) / (FogEnd - FogStart));
        
        // For additive blending (ADD=3), fog should fade to black (0,0,0) not fog color.
        // Adding black does nothing, which is the correct behavior for distant additive effects.
        // For other blend modes, fade to fog color as normal.
        // AlphaBlendFunc is a boolean flag (set by CPU) indicating true additive blending.
        float3 fogTarget = (AlphaBlendFunc > 0.5) ? float3(0, 0, 0) : FogColor.rgb;
        color.rgb = lerp(fogTarget, color.rgb, fogFactor);
    }
    
    color.rgb = ApplyColorCorrection(color.rgb);
    return color;
}
)";

// RHW pixel shader for 2D/UI rendering.
// Samples texture and multiplies by vertex color with gamma correction.
static const char* kRhwPS = R"(
Texture2D tex0 : register(t0);
SamplerState samp0 : register(s0);

cbuffer GammaCB : register(b6) {
  float BrightnessOffset;
  float ContrastScale;
  float GammaExp;
  float GammaEnabled;
  float4 _GammaPad;
};

float3 ApplyColorCorrection(float3 rgb) {
  if (GammaEnabled < 0.5) return rgb;
  rgb = saturate(rgb + BrightnessOffset);
  rgb = saturate((rgb - 0.5) * ContrastScale + 0.5);
  rgb = pow(rgb, GammaExp);
  return rgb;
}

struct PS_INPUT {
    float4 Pos : SV_POSITION;
    float4 Color : COLOR0;
    float2 UV : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET {
    float4 texColor = tex0.Sample(samp0, input.UV);
  float4 color = texColor * input.Color;
  color.rgb = ApplyColorCorrection(color.rgb);
  return color;
}
)";

// RHW alpha-poly PS: honors stage0 COLOROP/ALPHAOP (via AlphaTestCB) to emulate
// the fixed-function setup used by alpha polys (notably MUL shadows where COLOROP=SELECTARG1).
static const char* kRhwAlphaPS = R"(
Texture2D tex0 : register(t0);
SamplerState samp0 : register(s0);

cbuffer GammaCB : register(b6) {
  float BrightnessOffset;
  float ContrastScale;
  float GammaExp;
  float GammaEnabled;
  float4 _GammaPad;
};

float3 ApplyColorCorrection(float3 rgb) {
  if (GammaEnabled < 0.5) return rgb;
  rgb = saturate(rgb + BrightnessOffset);
  rgb = saturate((rgb - 0.5) * ContrastScale + 0.5);
  rgb = pow(rgb, GammaExp);
  return rgb;
}

cbuffer AlphaTestCB : register(b4) {
  float AlphaRef;
  float AlphaTestEnabled;
  float AlphaBlendFunc;
  float TexCoordIndex0;
  float TexCoordIndex1;
  float AlphaOp;   // 0=MODULATE, 1=SELECTARG1, 2=SELECTARG2
  float ColorOp0;  // zRND_TOP_* (stage0)
  float ColorOp1;

  float BaseRgbGen;
  float BaseFactorR;
  float BaseFactorG;
  float BaseFactorB;

  float SemFfpEnabled;
  float SemS0ColorOp;
  float SemS0ColorArg1;
  float SemS0ColorArg2;
  float SemS1ColorOp;
  float SemS1ColorArg1;
  float SemS1ColorArg2;

  float TexFactorR;
  float TexFactorG;
  float TexFactorB;
  float TexFactorA;

  float AlphaArg1;
  float AlphaArg2;
  float AlphaArgPad0;  // Alpha scale factor (1.0 = no scale, <1.0 for BC2)
  float AlphaArgPad1;
};

struct PS_INPUT {
  float4 Pos : SV_POSITION;
  float4 Color : COLOR0;
  float2 UV : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET {
  float4 texColor = tex0.Sample(samp0, input.UV);

  // Stage0 COLOROP subset (Gothic zRND_TOP_*).
  float3 rgb;
  if (ColorOp0 > 0.5 && ColorOp0 < 1.5) {
    rgb = texColor.rgb;  // SELECTARG1
  } else if (ColorOp0 > 1.5 && ColorOp0 < 2.5) {
    rgb = input.Color.rgb;  // SELECTARG2
  } else {
    // Default: MODULATE
    rgb = texColor.rgb * input.Color.rgb;
    if (ColorOp0 > 3.5 && ColorOp0 < 4.5) {
      rgb *= 2.0;
    } else if (ColorOp0 > 4.5) {
      rgb *= 4.0;
    }
  }
  rgb = saturate(rgb);

  float a;
  if (AlphaOp > 1.5) {
    a = input.Color.a;      // SELECTARG2
  } else if (AlphaOp > 0.5) {
    a = texColor.a;         // SELECTARG1
  } else {
    a = texColor.a * input.Color.a;  // MODULATE
  }

  // Alpha test with BC2 compensation via AlphaArgPad0 scale factor.
  // D3D11's BC2 decode yields higher alpha than D3D9; CPU sets scale < 1.0 for BC2.
  if (AlphaTestEnabled > 0.5) {
    float scaledAlpha = a * AlphaArgPad0;
    if (scaledAlpha < AlphaRef) {
      discard;
    }
  }

  float4 outColor = float4(rgb, a);
  outColor.rgb = ApplyColorCorrection(outColor.rgb);
  return outColor;
}
)";

static const char* kVertexColorPS = R"(
cbuffer GammaCB : register(b6) {
  float BrightnessOffset;
  float ContrastScale;
  float GammaExp;
  float GammaEnabled;
  float4 _GammaPad;
};

float3 ApplyColorCorrection(float3 rgb) {
  if (GammaEnabled < 0.5) return rgb;
  rgb = saturate(rgb + BrightnessOffset);
  rgb = saturate((rgb - 0.5) * ContrastScale + 0.5);
  rgb = pow(rgb, GammaExp);
  return rgb;
}

struct PS_INPUT {
    float4 Pos : SV_POSITION;
    float4 Color : COLOR0;
    float2 UV : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET {
    float4 color = input.Color;
    color.rgb = ApplyColorCorrection(color.rgb);
    return color;
}
)";

// Lightmap pixel shader - samples diffuse (tex0) and lightmap (tex1), multiplies them
// This is needed for indoor BSP geometry where Gothic binds:
// - Stage 0: Diffuse texture
// - Stage 1: Lightmap texture (pre-baked lighting)
// The lightmap UV coordinates are the same as diffuse UVs for indoor geometry.
static const char* kLightmapPS = R"(
cbuffer FogCB : register(b2) {
    float4 FogColor;
    float FogStart;
    float FogEnd;
    float FogDensity;
    float FogEnabled;
  float FogRangeEnabled;
  float3 _FogPad;
};

cbuffer GammaCB : register(b6) {
  float BrightnessOffset;
  float ContrastScale;
  float GammaExp;
  float GammaEnabled;
  float4 _GammaPad;
};

float3 ApplyColorCorrection(float3 rgb) {
  if (GammaEnabled < 0.5) return rgb;
  rgb = saturate(rgb + BrightnessOffset);
  rgb = saturate((rgb - 0.5) * ContrastScale + 0.5);
  rgb = pow(rgb, GammaExp);
  return rgb;
}

cbuffer AlphaTestCB : register(b4) {
    float AlphaRef;
    float AlphaTestEnabled;
    float AlphaBlendFunc;
  float TexCoordIndex0;
  float TexCoordIndex1;
    float AlphaOp;
    float ColorOp0;
    float ColorOp1;

    float BaseRgbGen;
    float BaseFactorR;
    float BaseFactorG;
    float BaseFactorB;

    float SemFfpEnabled;
    float SemS0ColorOp;
    float SemS0ColorArg1;
    float SemS0ColorArg2;
    float SemS1ColorOp;
    float SemS1ColorArg1;
    float SemS1ColorArg2;

    float TexFactorR;
    float TexFactorG;
    float TexFactorB;
    float TexFactorA;
};

Texture2D tex0 : register(t0);  // Diffuse texture
Texture2D tex1 : register(t1);  // Lightmap texture
SamplerState samp0 : register(s0);
SamplerState samp1 : register(s1);

struct PS_INPUT {
    float4 Pos : SV_POSITION;
    float4 Color : COLOR0;
    float2 UV : TEXCOORD0;
    float2 UV2 : TEXCOORD1;   // Second UV for lightmap (often same as UV)
    float FogDist : TEXCOORD2;
};

float3 ResolveSemanticArg(float arg, float3 tex, float3 current, float3 diffuse, float3 tfactor) {
  // ZenGin forwards zRND_TA_* values through texture stage state:
  // zRND_TA_CURRENT=0, zRND_TA_DIFFUSE=1, zRND_TA_TEXTURE=3, zRND_TA_TFACTOR=4, zRND_TA_SPECULAR=5.
  // Note: 0xFFFFFFFF may appear when the engine doesn't touch COLORARG; CPU normalizes to defaults,
  // but keep a defensive fallback here.
  if (arg > 1e9) {
    return tex;  // treat unset as TEXTURE
  }

  // CURRENT
  if (arg > -0.5 && arg < 0.5) {
    return current;
  }
  // DIFFUSE
  if (arg > 0.5 && arg < 1.5) {
    return diffuse;
  }
  // TEXTURE
  if (arg > 2.5 && arg < 3.5) {
    return tex;
  }
  // TFACTOR
  if (arg > 3.5 && arg < 4.5) {
    return tfactor;
  }

  // SPECULAR or unknown: treat as 1.0 (ZenGin mostly doesn't use it here)
  return float3(1.0, 1.0, 1.0);
}

float3 ApplySemanticOp(float op, float3 a1, float3 a2) {
  // ZenGin forwards zRND_TOP_* values through texture stage state:
  // zRND_TOP_DISABLE=0, SELECTARG1=1, SELECTARG2=2, MODULATE=3, MODULATE2X=4, MODULATE4X=5, ADD=6.

  // DISABLE: pass through CURRENT (represented by a2/a1 is ambiguous here). We choose a1.
  if (op > -0.5 && op < 0.5) {
    return a1;
  }
  if (op > 0.5 && op < 1.5) {
    return a1;
  }
  if (op > 1.5 && op < 2.5) {
    return a2;
  }
  if (op > 5.5 && op < 6.5) {
    return a1 + a2;
  }

  float3 outv = a1 * a2;
  if (op > 3.5 && op < 4.5) {
    outv *= 2.0;
  } else if (op > 4.5 && op < 5.5) {
    outv *= 4.0;
  }
  return outv;
}

float4 main(PS_INPUT input) : SV_TARGET {
    // Sample diffuse texture
  uint tci0 = (uint)(TexCoordIndex0 + 0.5);
  uint idx0 = (tci0 & 0xFF);
  float2 diffuseUV = (idx0 == 1) ? input.UV2 : input.UV;
  float4 diffuseColor = tex0.Sample(samp0, diffuseUV);
    
    // Sample lightmap texture (uses UV2 if available, otherwise UV)
    // For most indoor geometry, UV2 == UV
    uint tci1 = (uint)(TexCoordIndex1 + 0.5);
    uint idx1 = (tci1 & 0xFF);
    float2 lightmapUV = (idx1 == 1) ? input.UV2 : input.UV;
    float4 lightmapColor = tex1.Sample(samp1, lightmapUV);
    
    float4 color;

    // Semantic path: emulate ZenGin's 2-stage fixed function pipeline using COLORARG/COLOROP.
    // Engine convention for world lightmaps:
    // - engine stage0: LIGHTMAP (we bind as tex1)
    // - engine stage1: BASE    (we bind as tex0)
    if (SemFfpEnabled > 0.5) {
      float3 diffuse = input.Color.rgb;
      float3 current = diffuse;  // stage0 CURRENT is effectively DIFFUSE.
      float3 tfactor = float3(TexFactorR, TexFactorG, TexFactorB);

      // Stage0: LIGHTMAP (tex1)
      float3 s0_tex = lightmapColor.rgb;
      float3 s0_a1 = ResolveSemanticArg(SemS0ColorArg1, s0_tex, current, diffuse, tfactor);
      float3 s0_a2 = ResolveSemanticArg(SemS0ColorArg2, s0_tex, current, diffuse, tfactor);
      float3 s0 = ApplySemanticOp(SemS0ColorOp, s0_a1, s0_a2);
      s0 = saturate(s0);

      // Stage1: BASE (tex0)
      current = s0;
      float3 s1_tex = diffuseColor.rgb;
      float3 s1_a1 = ResolveSemanticArg(SemS1ColorArg1, s1_tex, current, diffuse, tfactor);
      float3 s1_a2 = ResolveSemanticArg(SemS1ColorArg2, s1_tex, current, diffuse, tfactor);
      float3 s1 = ApplySemanticOp(SemS1ColorOp, s1_a1, s1_a2);
      color.rgb = saturate(s1);
      color.a = diffuseColor.a * input.Color.a;
    } else {
      // Heuristic path: base (tex0) combined with vertex color, then multiplied by lightmap (tex1).
      float3 baseRgb;
      if (BaseRgbGen > -0.5) {
        // Gothic_II_Addon::zTShaderRGBGen: 0=IDENTITY,1=VERTEX,2=FACTOR,3=WAVE
        if (BaseRgbGen < 0.5) {
          baseRgb = diffuseColor.rgb;  // IDENTITY
        } else if (BaseRgbGen < 1.5) {
          baseRgb = diffuseColor.rgb * input.Color.rgb;  // VERTEX
        } else if (BaseRgbGen < 2.5) {
          baseRgb = diffuseColor.rgb * float3(BaseFactorR, BaseFactorG, BaseFactorB);  // FACTOR
        } else {
          baseRgb = diffuseColor.rgb;  // WAVE (not emulated)
        }
      } else {
        if (ColorOp0 > 0.5 && ColorOp0 < 1.5) {
          baseRgb = diffuseColor.rgb;  // SELECTARG1
        } else if (ColorOp0 > 1.5 && ColorOp0 < 2.5) {
          baseRgb = input.Color.rgb;   // SELECTARG2
        } else {
          baseRgb = diffuseColor.rgb * input.Color.rgb;  // MODULATE
          if (ColorOp0 > 3.5 && ColorOp0 < 4.5) {
            baseRgb *= 2.0;  // MODULATE2X
          } else if (ColorOp0 > 4.5) {
            baseRgb *= 4.0;  // MODULATE4X
          }
        }
      }

      baseRgb = saturate(baseRgb);
      color.rgb = baseRgb * lightmapColor.rgb;
      if (ColorOp1 > 3.5 && ColorOp1 < 4.5) {
        color.rgb *= 2.0;  // MODULATE2X
      } else if (ColorOp1 > 4.5) {
        color.rgb *= 4.0;  // MODULATE4X
      }
      color.rgb = saturate(color.rgb);
      color.a = diffuseColor.a * input.Color.a;
    }
    
    // Alpha test.
    float alphaRef = saturate(AlphaRef);
    if (AlphaTestEnabled > 0.5 && color.a < alphaRef) {
      discard;
    }
    
    // Apply fog
    if (FogEnabled > 0.5) {
        float fogFactor = saturate((FogEnd - input.FogDist) / (FogEnd - FogStart));
        color.rgb = lerp(FogColor.rgb, color.rgb, fogFactor);
    }

    color.rgb = ApplyColorCorrection(color.rgb);
    return color;
}
)";

// Dual-texture additive pixel shader (water/env maps)
static const char* kDualAddPS = R"(
cbuffer FogCB : register(b2) {
  float4 FogColor;
  float FogStart;
  float FogEnd;
  float FogDensity;
  float FogEnabled;
  float FogRangeEnabled;
  float3 _FogPad;
};

cbuffer GammaCB : register(b6) {
  float BrightnessOffset;
  float ContrastScale;
  float GammaExp;
  float GammaEnabled;
  float4 _GammaPad;
};

float3 ApplyColorCorrection(float3 rgb) {
  if (GammaEnabled < 0.5) return rgb;
  rgb = saturate(rgb + BrightnessOffset);
  rgb = saturate((rgb - 0.5) * ContrastScale + 0.5);
  rgb = pow(rgb, GammaExp);
  return rgb;
}

cbuffer AlphaTestCB : register(b4) {
  float AlphaRef;
  float AlphaTestEnabled;
  float AlphaBlendFunc;
  float TexCoordIndex0;
  float TexCoordIndex1;
  float AlphaOp;
  float ColorOp0;
  float ColorOp1;

  // Optional semantic data (layout parity with CPU constant buffer).
  float BaseRgbGen;
  float BaseFactorR;
  float BaseFactorG;
  float BaseFactorB;

  float SemFfpEnabled;
  float SemS0ColorOp;
  float SemS0ColorArg1;
  float SemS0ColorArg2;
  float SemS1ColorOp;
  float SemS1ColorArg1;
  float SemS1ColorArg2;

  float TexFactorR;
  float TexFactorG;
  float TexFactorB;
  float TexFactorA;

  float AlphaArg1;
  float AlphaArg2;
  float AlphaArgPad0;
  float AlphaArgPad1;
};

Texture2D tex0 : register(t0);
Texture2D tex1 : register(t1);
SamplerState samp0 : register(s0);
SamplerState samp1 : register(s1);

struct PS_INPUT {
  float4 Pos : SV_POSITION;
  float4 Color : COLOR0;
  float2 UV : TEXCOORD0;
  float2 UV2 : TEXCOORD1;
  float FogDist : TEXCOORD2;
};

float ResolveAlphaArg(float arg, float texA, float diffuseA, float tfactorA) {
  // Gothic zRND_TA_*: CURRENT=0, DIFFUSE=1, TEXTURE=3, TFACTOR=4
  if (arg > 2.5 && arg < 3.5) {
    return texA;
  }
  if (arg > 0.5 && arg < 1.5) {
    return diffuseA;
  }
  if (arg > 3.5 && arg < 4.5) {
    return tfactorA;
  }
  // CURRENT or unknown: for stage0, CURRENT effectively maps to DIFFUSE.
  return diffuseA;
}

float3 ApplyColorOp(float op, float3 a1, float3 a2) {
  // zRND_TOP_DISABLE=0, SELECTARG1=1, SELECTARG2=2, MODULATE=3, MODULATE2X=4, MODULATE4X=5,
  // ADD=6, ADDSMOOTH=10, BLENDDIFFUSEALPHA=11
  float3 outRgb;
  if (op > 0.5 && op < 1.5) {
    outRgb = a1;
  } else if (op > 1.5 && op < 2.5) {
    outRgb = a2;
  } else if (op > 5.5 && op < 6.5) {
    outRgb = a1 + a2;
  } else if (op > 9.5 && op < 10.5) {
    outRgb = a1 + a2 - (a1 * a2);
  } else if (op > 10.5 && op < 11.5) {
    // BLENDDIFFUSEALPHA requires the diffuse alpha term; for stage1 we don't have it here.
    // Fall back to MODULATE to stay close to typical env-map usage.
    outRgb = a1 * a2;
  } else {
    outRgb = a1 * a2;
    if (op > 3.5 && op < 4.5) {
      outRgb *= 2.0;
    } else if (op > 4.5) {
      outRgb *= 4.0;
    }
  }
  return saturate(outRgb);
}

float4 main(PS_INPUT input) : SV_TARGET {
  // Base texture
  uint tci0 = (uint)(TexCoordIndex0 + 0.5);
  uint idx0 = (tci0 & 0xFF);
  float2 baseUV = (idx0 == 1) ? input.UV2 : input.UV;
  float4 baseColor = tex0.Sample(samp0, baseUV);

  // Secondary texture (env map) can use its own TEXCOORDINDEX
  uint tci1 = (uint)(TexCoordIndex1 + 0.5);
  uint idx1 = (tci1 & 0xFF);
  bool conflict = (idx1 == idx0);
  // If both stages select the same coord set, the VS routes stage1 into the other slot.
  float2 secondaryUV = conflict ? ((idx1 == 1) ? input.UV : input.UV2)
                                : ((idx1 == 1) ? input.UV2 : input.UV);
  float4 secondary = tex1.Sample(samp1, secondaryUV);

  // Emulate 2-stage fixed-function combine (s0 then s1).
  // Stage0 args are assumed to be (TEXTURE0, DIFFUSE).
  // Stage1 args are assumed to be (TEXTURE1, CURRENT).
  float3 s0;
  if (ColorOp0 > 0.5 && ColorOp0 < 1.5) {
    // SELECTARG1
    s0 = baseColor.rgb;
  } else if (ColorOp0 > 1.5 && ColorOp0 < 2.5) {
    // SELECTARG2
    s0 = input.Color.rgb;
  } else if (ColorOp0 > 5.5 && ColorOp0 < 6.5) {
    // ADD
    s0 = baseColor.rgb + input.Color.rgb;
  } else if (ColorOp0 > 9.5 && ColorOp0 < 10.5) {
    // ADDSMOOTH
    s0 = baseColor.rgb + input.Color.rgb - (baseColor.rgb * input.Color.rgb);
  } else if (ColorOp0 > 10.5 && ColorOp0 < 11.5) {
    // BLENDDIFFUSEALPHA: Arg1*Arg2.a + Arg2*(1-Arg2.a)
    // Gothic typically uses Arg1=TEXTURE and Arg2=DIFFUSE for this op.
    s0 = lerp(input.Color.rgb, baseColor.rgb, input.Color.a);
  } else {
    // MODULATE / MODULATE2X / MODULATE4X
    s0 = baseColor.rgb * input.Color.rgb;
    if (ColorOp0 > 3.5 && ColorOp0 < 4.5) {
      s0 *= 2.0;
    } else if (ColorOp0 > 4.5) {
      s0 *= 4.0;
    }
  }
  s0 = saturate(s0);
  float3 s1 = s0;
  if (ColorOp1 > 0.5) {
    s1 = ApplyColorOp(ColorOp1, secondary.rgb, s0);
  }

  float4 color;
  color.rgb = s1;

  // Alpha: stage0 alpha args/op (important for water env-map pass: TFACTOR * DIFFUSE).
  float a1 = ResolveAlphaArg(AlphaArg1, baseColor.a, input.Color.a, TexFactorA);
  float a2 = ResolveAlphaArg(AlphaArg2, baseColor.a, input.Color.a, TexFactorA);
  if (AlphaOp > 1.5) {
    color.a = a2;
  } else if (AlphaOp > 0.5) {
    color.a = a1;
  } else {
    color.a = a1 * a2;
  }

  // Alpha test.
  float alphaRef = saturate(AlphaRef);
  if (AlphaTestEnabled > 0.5 && color.a < alphaRef) {
    discard;
  }

  // Fog
  if (FogEnabled > 0.5) {
    float fogFactor = saturate((FogEnd - input.FogDist) / (FogEnd - FogStart));
    // AlphaBlendFunc is a boolean flag (set by CPU) indicating true additive blending.
    float3 fogTarget = (AlphaBlendFunc > 0.5) ? float3(0, 0, 0) : FogColor.rgb;
    color.rgb = lerp(fogTarget, color.rgb, fogFactor);
  }

  color.rgb = ApplyColorCorrection(color.rgb);
  return color;
}
)";

// --- Helper: Compile Shader ---
static ID3DBlob* CompileShader(const char* source, const char* entry_point, const char* target) {
  ID3DBlob* blob = nullptr;
  ID3DBlob* error_blob = nullptr;

  UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
  flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
  flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

  HRESULT hr = D3DCompile(source, strlen(source), nullptr, nullptr, nullptr, entry_point, target, flags, 0, &blob, &error_blob);

  if (FAILED(hr)) {
    if (error_blob) {
      SPDLOG_ERROR("Shader compilation failed: {}", static_cast<const char*>(error_blob->GetBufferPointer()));
      error_blob->Release();
    }
    return nullptr;
  }

  if (error_blob) {
    error_blob->Release();
  }

  return blob;
}

// --- Helper: Safe Release ---
template <typename T>
static void SafeRelease(T*& ptr) {
  if (ptr) {
    ptr->Release();
    ptr = nullptr;
  }
}

// Helper to update a DYNAMIC constant buffer using Map/DISCARD
template <typename T>
static void UpdateDynamicCB(ID3D11DeviceContext* ctx, ID3D11Buffer* cb, const T& data) {
  D3D11_MAPPED_SUBRESOURCE mapped;
  if (SUCCEEDED(ctx->Map(cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
    memcpy(mapped.pData, &data, sizeof(T));
    ctx->Unmap(cb, 0);
  }
}

// --- D3D11RendererImpl Implementation ---

bool D3D11RendererImpl::Init(void* hwnd_ptr, int width, int height, bool is_fullscreen) {
  // Clean up any existing resources before re-initializing.
  // This is critical when changing resolution - all cached state objects
  // (samplers, blend states, etc.) are tied to the old device and must be released.
  if (device) {
    Cleanup();
  }

  hwnd = static_cast<HWND>(hwnd_ptr);
  screen_width = width;
  screen_height = height;
  fullscreen = is_fullscreen;
  vsync = RendererConfig::Instance().vsync_enabled;

  if (!CreateDeviceAndSwapChain(hwnd, width, height, is_fullscreen)) {
    SPDLOG_ERROR("Failed to create D3D11 device and swap chain");
    return false;
  }

  if (!CreateRenderTargetView()) {
    SPDLOG_ERROR("Failed to create render target view");
    Cleanup();
    return false;
  }

  if (!CreateDepthStencilView(width, height)) {
    SPDLOG_ERROR("Failed to create depth stencil view");
    Cleanup();
    return false;
  }

  if (!CreateShaders()) {
    SPDLOG_ERROR("Failed to create shaders");
    Cleanup();
    return false;
  }

  if (!CreateConstantBuffers()) {
    SPDLOG_ERROR("Failed to create constant buffers");
    Cleanup();
    return false;
  }

  if (!CreateStateObjects()) {
    SPDLOG_ERROR("Failed to create state objects");
    Cleanup();
    return false;
  }

  if (!CreateDynamicBuffers()) {
    SPDLOG_ERROR("Failed to create dynamic buffers");
    Cleanup();
    return false;
  }

  // Set global device pointers for resource classes
  g_D3D11Device = device;
  g_D3D11Context = context;

  // Initialize transform matrices to identity
  transform_data.world = DirectX::XMMatrixIdentity();
  transform_data.view = DirectX::XMMatrixIdentity();
  transform_data.projection = DirectX::XMMatrixIdentity();
  transform_data.world_view_proj = DirectX::XMMatrixIdentity();
  transform_dirty = true;

  // Set initial viewport
  SetViewport(0, 0, width, height);

  // Set initial render state
  context->OMSetRenderTargets(1, &rtv, dsv);
  ApplyOpaqueState();

  // Update screen constant buffer
  screen_data.screen_size = DirectX::XMFLOAT2(static_cast<float>(width), static_cast<float>(height));
  screen_data.inv_screen_size = DirectX::XMFLOAT2(1.0f / width, 1.0f / height);
  UpdateDynamicCB(context, cb_screen, screen_data);
  BindVSConstantBuffer(3, cb_screen);

  return true;
}

bool D3D11RendererImpl::CreateDeviceAndSwapChain(HWND target_hwnd, int width, int height, bool is_fullscreen) {
  // First create device without swap chain
  D3D_FEATURE_LEVEL feature_levels[] = {
      D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_11_0,
      D3D_FEATURE_LEVEL_10_1,
      D3D_FEATURE_LEVEL_10_0,
  };

  UINT create_device_flags = 0;
#ifndef NDEBUG
  create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

  D3D_FEATURE_LEVEL obtained_feature_level;

  HRESULT hr = D3D11CreateDevice(nullptr,                   // Use default adapter
                                 D3D_DRIVER_TYPE_HARDWARE,  // Hardware acceleration
                                 nullptr,                   // No software rasterizer
                                 create_device_flags, feature_levels, ARRAYSIZE(feature_levels), D3D11_SDK_VERSION, &device, &obtained_feature_level,
                                 &context);

  if (FAILED(hr)) {
    SPDLOG_ERROR("D3D11CreateDevice failed: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  // Optional GPU event markers for RenderDoc/PIX.
  // RenderDoc will show these as named events in the capture.
  SafeRelease(user_annotation);
#ifndef NDEBUG
  if (context) {
    HRESULT ann_hr = context->QueryInterface(__uuidof(ID3DUserDefinedAnnotation), reinterpret_cast<void**>(&user_annotation));
    (void)ann_hr;
  }
#endif

  // Enable D3D11 multithread protection - Gothic may call D3D11 from multiple threads
  // (e.g., texture CacheIn from background thread while rendering on main thread)
  ComPtr<ID3D10Multithread> multithread;
  if (SUCCEEDED(device->QueryInterface(__uuidof(ID3D10Multithread), &multithread))) {
    multithread->SetMultithreadProtected(TRUE);
  } else {
    SPDLOG_WARN("Failed to enable D3D11 multithread protection");
  }

  // Get DXGI interfaces from device
  ComPtr<IDXGIDevice> dxgi_device;
  hr = device->QueryInterface(__uuidof(IDXGIDevice), &dxgi_device);
  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to get DXGI device: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  ComPtr<IDXGIAdapter> dxgi_adapter;
  hr = dxgi_device->GetAdapter(&dxgi_adapter);
  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to get DXGI adapter: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  ComPtr<IDXGIFactory> dxgi_factory;
  hr = dxgi_adapter->GetParent(__uuidof(IDXGIFactory), &dxgi_factory);
  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to get DXGI factory: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  // Cache the primary output for fullscreen transitions
  SafeRelease(dxgi_output);
  hr = dxgi_adapter->EnumOutputs(0, &dxgi_output);
  if (FAILED(hr)) {
    dxgi_output = nullptr;
    SPDLOG_WARN("Failed to get DXGI output (fullscreen may not work): 0x{:08X}", static_cast<unsigned int>(hr));
  }

  // Determine presentation model based on system capabilities and user preference
  tearing_supported = CheckTearingSupport(dxgi_factory.Get());

  bool use_exclusive_fullscreen = RendererConfig::Instance().exclusive_fullscreen;
  using_flip_model = tearing_supported && !use_exclusive_fullscreen;

  // Create swap chain with fallback
  DXGI_SWAP_CHAIN_DESC scd;
  ConfigureSwapChainDesc(scd, target_hwnd, width, height, using_flip_model);

  hr = dxgi_factory->CreateSwapChain(device, &scd, &swap_chain);
  if (FAILED(hr) && using_flip_model) {
    // FLIP model failed, fall back to DISCARD
    SPDLOG_WARN("FLIP_DISCARD swap chain creation failed, falling back to DISCARD");
    using_flip_model = false;
    ConfigureSwapChainDesc(scd, target_hwnd, width, height, false);
    hr = dxgi_factory->CreateSwapChain(device, &scd, &swap_chain);
  }
  if (FAILED(hr)) {
    SPDLOG_ERROR("CreateSwapChain failed: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  // Disable Alt+Enter (we handle fullscreen ourselves)
  dxgi_factory->MakeWindowAssociation(target_hwnd, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);

  capabilities_.feature_level = obtained_feature_level;
  capabilities_.max_texture_size = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;

  // Apply initial fullscreen state
  if (is_fullscreen) {
    if (using_flip_model) {
      // FLIP model: borderless fullscreen (window sizing handled by application)
      fullscreen = true;
    } else if (dxgi_output) {
      // Legacy: exclusive fullscreen via DXGI
      hr = swap_chain->SetFullscreenState(TRUE, dxgi_output);
      if (FAILED(hr)) {
        SPDLOG_WARN("Failed to set fullscreen state: 0x{:08X}. Continuing in windowed mode.", static_cast<unsigned int>(hr));
        fullscreen = false;
      } else {
        fullscreen = true;
        hr = swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
        if (FAILED(hr)) {
          SPDLOG_WARN("Failed to resize buffers after fullscreen switch: 0x{:08X}", static_cast<unsigned int>(hr));
        }
      }
    }
  }

  return true;
}

bool D3D11RendererImpl::CreateRenderTargetView() {
  // With FLIP_DISCARD, GetBuffer(0) always returns the current back buffer.
  // DXGI handles the buffer rotation internally after Present().
  ID3D11Texture2D* back_buffer = nullptr;
  HRESULT hr = swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back_buffer));
  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to get back buffer: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  hr = device->CreateRenderTargetView(back_buffer, nullptr, &rtv);
  back_buffer->Release();

  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to create RTV: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  return true;
}

bool D3D11RendererImpl::RefreshBackBufferRTV() {
  // FLIP model backbuffer refresh:
  // After Present() with FLIP_DISCARD/FLIP_SEQUENTIAL, the swap chain has rotated
  // and GetBuffer(0) now returns a different underlying texture. Our old RTV points
  // to a buffer that may be displayed or queued for display - we must not render to it.
  //
  // This function acquires the new current backbuffer and creates a fresh RTV for it.
  // It's called from BeginFrame()/Clear() when flip_rtv_needs_refresh_ is set.
  //
  // Note: With FLIP models, only GetBuffer(0) is valid. Attempting GetBuffer(1), etc.
  // returns DXGI_ERROR_INVALID_CALL (0x887A0001). The buffer index parameter does NOT
  // let you access specific buffers - DXGI manages rotation internally.

  if (!swap_chain || !device || !context) {
    return false;
  }

  // Unbind current RTV before releasing it (required by D3D11)
  context->OMSetRenderTargets(0, nullptr, nullptr);

  // Release old RTV (it pointed to what is now a front/displayed buffer)
  SafeRelease(rtv);

  // Acquire the new current backbuffer and create RTV
  ID3D11Texture2D* back_buffer = nullptr;
  HRESULT hr = swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back_buffer));
  if (FAILED(hr) || !back_buffer) {
    SPDLOG_ERROR("RefreshBackBufferRTV: Failed to get backbuffer: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  hr = device->CreateRenderTargetView(back_buffer, nullptr, &rtv);
  back_buffer->Release();

  if (FAILED(hr)) {
    SPDLOG_ERROR("RefreshBackBufferRTV: Failed to create RTV: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  // Re-bind the new RTV with existing DSV
  context->OMSetRenderTargets(1, &rtv, dsv);

  // Clear the refresh flag.
  flip_rtv_needs_refresh_ = false;

  return true;
}

bool D3D11RendererImpl::CreateDepthStencilView(int width, int height) {
  D3D11_TEXTURE2D_DESC depth_desc = {};
  depth_desc.Width = width;
  depth_desc.Height = height;
  depth_desc.MipLevels = 1;
  depth_desc.ArraySize = 1;
  depth_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  depth_desc.SampleDesc.Count = 1;
  depth_desc.SampleDesc.Quality = 0;
  depth_desc.Usage = D3D11_USAGE_DEFAULT;
  depth_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

  HRESULT hr = device->CreateTexture2D(&depth_desc, nullptr, &depth_stencil_texture);
  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to create depth texture: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  hr = device->CreateDepthStencilView(depth_stencil_texture, nullptr, &dsv);
  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to create DSV: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  return true;
}

bool D3D11RendererImpl::CreateShaders() {
  // Compile and create basic vertex shader
  ID3DBlob* vs_blob = CompileShader(kBasicVS, "main", "vs_5_0");
  if (!vs_blob) {
    return false;
  }

  HRESULT hr = device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), nullptr, &vs_basic);
  if (FAILED(hr)) {
    vs_blob->Release();
    SPDLOG_ERROR("Failed to create basic VS: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  // Create input layout for Vertex3D using the VS bytecode
  // Format 0x15: XYZ(12) + NORMAL(12) + TEX(8) = 32 bytes, NO COLOR
  // The shader uses a default white color when no COLOR element is present
  D3D11_INPUT_ELEMENT_DESC layout_3d_desc[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
  };

  hr = device->CreateInputLayout(layout_3d_desc, ARRAYSIZE(layout_3d_desc), vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), &layout_3d);
  vs_blob->Release();

  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to create layout_3d: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  // Compile and create basic UNLIT vertex shader (stride=32)
  ID3DBlob* vs_unlit_blob = CompileShader(kBasicUnlitVS, "main", "vs_5_0");
  if (!vs_unlit_blob) {
    return false;
  }

  hr = device->CreateVertexShader(vs_unlit_blob->GetBufferPointer(), vs_unlit_blob->GetBufferSize(), nullptr, &vs_basic_unlit);
  vs_unlit_blob->Release();
  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to create basic_unlit VS: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  // Compile and create 3D vertex shader WITH vertex color (stride=36)
  ID3DBlob* vs_color_blob = CompileShader(kBasicColorVS, "main", "vs_5_0");
  if (!vs_color_blob) {
    return false;
  }

  hr = device->CreateVertexShader(vs_color_blob->GetBufferPointer(), vs_color_blob->GetBufferSize(), nullptr, &vs_basic_color);
  if (FAILED(hr)) {
    vs_color_blob->Release();
    SPDLOG_ERROR("Failed to create basic_color VS: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  // Create input layout for Vertex3D with color (pos+normal+color+uv)
  // Format: XYZ(12) + NORMAL(12) + COLOR(4) + TEX(8) = 36 bytes
  D3D11_INPUT_ELEMENT_DESC layout_3d_color_desc[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
  };

  hr = device->CreateInputLayout(layout_3d_color_desc, ARRAYSIZE(layout_3d_color_desc), vs_color_blob->GetBufferPointer(),
                                 vs_color_blob->GetBufferSize(), &layout_3d_color);
  vs_color_blob->Release();

  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to create layout_3d_color: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  // Compile and create basic COLOR UNLIT vertex shader (stride=36)
  ID3DBlob* vs_color_unlit_blob = CompileShader(kBasicColorUnlitVS, "main", "vs_5_0");
  if (!vs_color_unlit_blob) {
    return false;
  }

  hr = device->CreateVertexShader(vs_color_unlit_blob->GetBufferPointer(), vs_color_unlit_blob->GetBufferSize(), nullptr, &vs_basic_color_unlit);
  vs_color_unlit_blob->Release();
  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to create basic_color_unlit VS: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  // Compile and create pre-lit vertex shader (no normals, stride=24)
  ID3DBlob* lit_vs_blob = CompileShader(kLitVS, "main", "vs_5_0");
  if (!lit_vs_blob) {
    return false;
  }

  hr = device->CreateVertexShader(lit_vs_blob->GetBufferPointer(), lit_vs_blob->GetBufferSize(), nullptr, &vs_lit);
  if (FAILED(hr)) {
    lit_vs_blob->Release();
    SPDLOG_ERROR("Failed to create lit VS: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  // Create input layout for pre-lit vertices (pos+color+uv, no normal)
  D3D11_INPUT_ELEMENT_DESC layout_lit_desc[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
  };

  hr = device->CreateInputLayout(layout_lit_desc, ARRAYSIZE(layout_lit_desc), lit_vs_blob->GetBufferPointer(), lit_vs_blob->GetBufferSize(),
                                 &layout_lit);
  lit_vs_blob->Release();

  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to create layout_lit: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  // Compile and create pre-lit vertex shader (2 UVs, stride=32)
  ID3DBlob* lit_2uv_vs_blob = CompileShader(kLitTwoUvVS, "main", "vs_5_0");
  if (!lit_2uv_vs_blob) {
    return false;
  }

  hr = device->CreateVertexShader(lit_2uv_vs_blob->GetBufferPointer(), lit_2uv_vs_blob->GetBufferSize(), nullptr, &vs_lit_2uv);
  if (FAILED(hr)) {
    lit_2uv_vs_blob->Release();
    SPDLOG_ERROR("Failed to create lit_2uv VS: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  // Create input layout for pre-lit vertices with 2 UVs
  // Format: XYZ(12) + COLOR(4) + TEX0(8) + TEX1(8) = 32 bytes
  D3D11_INPUT_ELEMENT_DESC layout_lit_2uv_desc[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
  };

  hr = device->CreateInputLayout(layout_lit_2uv_desc, ARRAYSIZE(layout_lit_2uv_desc), lit_2uv_vs_blob->GetBufferPointer(),
                                 lit_2uv_vs_blob->GetBufferSize(), &layout_lit_2uv);
  lit_2uv_vs_blob->Release();

  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to create layout_lit_2uv: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  // Compile and create pre-lit vertex shader WITH normals (stride=44, for water)
  // Format 0x2D: XYZ(12) + NORMAL(12) + COLOR(4) + TEX(8) + TEX(8) = 44 bytes
  ID3DBlob* lit_normal_vs_blob = CompileShader(kLitNormalVS, "main", "vs_5_0");
  if (!lit_normal_vs_blob) {
    return false;
  }

  hr = device->CreateVertexShader(lit_normal_vs_blob->GetBufferPointer(), lit_normal_vs_blob->GetBufferSize(), nullptr, &vs_lit_normal);
  if (FAILED(hr)) {
    lit_normal_vs_blob->Release();
    SPDLOG_ERROR("Failed to create lit_normal VS: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  // Create input layout for pre-lit vertices with normals (water format)
  // Format 0x2D: XYZ(12) + NORMAL(12) + COLOR(4) + TEX(8) + TEX(8) = 44 bytes
  D3D11_INPUT_ELEMENT_DESC layout_lit_normal_desc[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
  };

  hr = device->CreateInputLayout(layout_lit_normal_desc, ARRAYSIZE(layout_lit_normal_desc), lit_normal_vs_blob->GetBufferPointer(),
                                 lit_normal_vs_blob->GetBufferSize(), &layout_lit_normal);
  lit_normal_vs_blob->Release();

  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to create layout_lit_normal: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  // Compile and create pre-lit vertex shader WITH normals and single UV (stride=36)
  ID3DBlob* lit_normal_singleuv_vs_blob = CompileShader(kLitNormalSingleUvVS, "main", "vs_5_0");
  if (!lit_normal_singleuv_vs_blob) {
    return false;
  }

  hr = device->CreateVertexShader(lit_normal_singleuv_vs_blob->GetBufferPointer(), lit_normal_singleuv_vs_blob->GetBufferSize(), nullptr,
                                  &vs_lit_normal_singleuv);
  if (FAILED(hr)) {
    lit_normal_singleuv_vs_blob->Release();
    SPDLOG_ERROR("Failed to create lit_normal_singleuv VS: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  D3D11_INPUT_ELEMENT_DESC layout_lit_normal_singleuv_desc[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
  };

  hr = device->CreateInputLayout(layout_lit_normal_singleuv_desc, ARRAYSIZE(layout_lit_normal_singleuv_desc),
                                 lit_normal_singleuv_vs_blob->GetBufferPointer(), lit_normal_singleuv_vs_blob->GetBufferSize(),
                                 &layout_lit_normal_singleuv);
  lit_normal_singleuv_vs_blob->Release();

  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to create layout_lit_normal_singleuv: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  // Compile and create RHW vertex shader
  ID3DBlob* rhw_vs_blob = CompileShader(kRhwVS, "main", "vs_5_0");
  if (!rhw_vs_blob) {
    return false;
  }

  hr = device->CreateVertexShader(rhw_vs_blob->GetBufferPointer(), rhw_vs_blob->GetBufferSize(), nullptr, &vs_rhw);
  if (FAILED(hr)) {
    rhw_vs_blob->Release();
    SPDLOG_ERROR("Failed to create RHW VS: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  // Create input layout for VertexRHW
  D3D11_INPUT_ELEMENT_DESC layout_rhw_desc[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0},
  };

  hr = device->CreateInputLayout(layout_rhw_desc, ARRAYSIZE(layout_rhw_desc), rhw_vs_blob->GetBufferPointer(), rhw_vs_blob->GetBufferSize(),
                                 &layout_rhw);
  rhw_vs_blob->Release();

  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to create layout_rhw: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  // Compile and create basic pixel shader
  ID3DBlob* ps_blob = CompileShader(kBasicPS, "main", "ps_5_0");
  if (!ps_blob) {
    return false;
  }

  hr = device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, &ps_basic);
  ps_blob->Release();

  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to create basic PS: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  // Compile and create RHW pixel shader (for 2D/UI, no fog)
  ID3DBlob* rhw_ps_blob = CompileShader(kRhwPS, "main", "ps_5_0");
  if (!rhw_ps_blob) {
    return false;
  }

  hr = device->CreatePixelShader(rhw_ps_blob->GetBufferPointer(), rhw_ps_blob->GetBufferSize(), nullptr, &ps_rhw);
  rhw_ps_blob->Release();

  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to create RHW PS: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  // Compile and create RHW alpha pixel shader (alpha polys with stage0 ops)
  ID3DBlob* rhw_alpha_ps_blob = CompileShader(kRhwAlphaPS, "main", "ps_5_0");
  if (!rhw_alpha_ps_blob) {
    return false;
  }

  hr = device->CreatePixelShader(rhw_alpha_ps_blob->GetBufferPointer(), rhw_alpha_ps_blob->GetBufferSize(), nullptr, &ps_rhw_alpha);
  rhw_alpha_ps_blob->Release();

  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to create RHW alpha PS: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  // Compile and create vertex color pixel shader
  ID3DBlob* vc_ps_blob = CompileShader(kVertexColorPS, "main", "ps_5_0");
  if (!vc_ps_blob) {
    return false;
  }

  hr = device->CreatePixelShader(vc_ps_blob->GetBufferPointer(), vc_ps_blob->GetBufferSize(), nullptr, &ps_vertex_color);
  vc_ps_blob->Release();

  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to create vertex color PS: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  // Compile and create lightmap pixel shader (for indoor BSP geometry with lightmaps)
  ID3DBlob* lm_ps_blob = CompileShader(kLightmapPS, "main", "ps_5_0");
  if (!lm_ps_blob) {
    SPDLOG_ERROR("Failed to compile lightmap PS");
    return false;
  }

  hr = device->CreatePixelShader(lm_ps_blob->GetBufferPointer(), lm_ps_blob->GetBufferSize(), nullptr, &ps_lightmap);
  lm_ps_blob->Release();

  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to create lightmap PS: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  // Compile and create dual-add pixel shader (water/env)
  ID3DBlob* da_ps_blob = CompileShader(kDualAddPS, "main", "ps_5_0");
  if (!da_ps_blob) {
    SPDLOG_ERROR("Failed to compile dual-add PS");
    return false;
  }

  hr = device->CreatePixelShader(da_ps_blob->GetBufferPointer(), da_ps_blob->GetBufferSize(), nullptr, &ps_dual_add);
  da_ps_blob->Release();

  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to create dual-add PS: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  return true;
}

bool D3D11RendererImpl::CreateConstantBuffers() {
  // Use DYNAMIC + WRITE_DISCARD for frequently-updated constant buffers.
  // This allows the driver to give us fresh memory each update, avoiding GPU stalls
  // when the GPU is still reading the previous frame's data.
  D3D11_BUFFER_DESC cbd = {};
  cbd.Usage = D3D11_USAGE_DYNAMIC;
  cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  // Transform CB
  cbd.ByteWidth = sizeof(TransformCB);
  if (FAILED(device->CreateBuffer(&cbd, nullptr, &cb_transform))) {
    SPDLOG_ERROR("Failed to create transform CB");
    return false;
  }
  SetDebugObjectName(cb_transform, "cb_transform");

  // Material CB
  cbd.ByteWidth = sizeof(MaterialCB);
  if (FAILED(device->CreateBuffer(&cbd, nullptr, &cb_material))) {
    SPDLOG_ERROR("Failed to create material CB");
    return false;
  }
  SetDebugObjectName(cb_material, "cb_material");

  // Initialize material CB with D3D9-like defaults.
  // Many draws rely on the implicit default material (diffuse/ambient = white).
  material_data.diffuse = {1.0f, 1.0f, 1.0f, 1.0f};
  material_data.ambient = {1.0f, 1.0f, 1.0f, 1.0f};
  material_data.specular = {0.0f, 0.0f, 0.0f, 0.0f};
  material_data.emissive = {0.0f, 0.0f, 0.0f, 0.0f};
  material_data.power = 0.0f;
  material_data.alpha_ref = 0.0f;
  material_data.alpha_test_enabled = 0.0f;
  material_data._pad[0] = 0.0f;

  UpdateDynamicCB(context, cb_material, material_data);
  last_material_data_ = material_data;
  has_last_material_data_ = true;
  material_dirty = false;

  // Fog CB
  cbd.ByteWidth = sizeof(FogCB);
  if (FAILED(device->CreateBuffer(&cbd, nullptr, &cb_fog))) {
    SPDLOG_ERROR("Failed to create fog CB");
    return false;
  }
  SetDebugObjectName(cb_fog, "cb_fog");

  // Initialize fog CB with safe defaults.
  // Fog is disabled initially; if any draw happens before Gothic calls SetFog(),
  // at least we won't get black fog blending (color defaults to white/opaque).
  fog_data.color = {1.0f, 1.0f, 1.0f, 1.0f};  // White fog (not black!)
  fog_data.start = 0.0f;
  fog_data.end = 10000.0f;  // Very far, minimal fog effect
  fog_data.density = 0.01f;
  fog_data.enabled = 0.0f;        // Disabled
  fog_data.range_enabled = 1.0f;  // Use range fog by default (Gothic's typical mode)
  fog_data._pad[0] = 0.0f;
  fog_data._pad[1] = 0.0f;
  fog_data._pad[2] = 0.0f;

  UpdateDynamicCB(context, cb_fog, fog_data);
  last_fog_data_ = fog_data;
  has_last_fog_data_ = true;
  fog_dirty = false;

  // Screen CB
  cbd.ByteWidth = sizeof(ScreenCB);
  if (FAILED(device->CreateBuffer(&cbd, nullptr, &cb_screen))) {
    SPDLOG_ERROR("Failed to create screen CB");
    return false;
  }
  SetDebugObjectName(cb_screen, "cb_screen");

  // Alpha Test CB
  cbd.ByteWidth = sizeof(AlphaTestCB);
  if (FAILED(device->CreateBuffer(&cbd, nullptr, &cb_alpha_test))) {
    SPDLOG_ERROR("Failed to create alpha test CB");
    return false;
  }
  SetDebugObjectName(cb_alpha_test, "cb_alpha_test");

  // Initialize alpha test CB with disabled state to prevent garbage reads
  alpha_test_data.alpha_test_enabled = 0.0f;
  alpha_test_data.alpha_ref = 0.0f;
  alpha_test_data.alpha_blend_func = 0.0f;  // MAT_DEFAULT
  alpha_test_data.uv_source0 = PackUvSourceAndTexGen(TexCoordSource::kUV0, TexGenMode::kNone);
  alpha_test_data.uv_source1 = PackUvSourceAndTexGen(TexCoordSource::kUV0, TexGenMode::kNone);
  alpha_test_data.alpha_op = 0.0f;   // MODULATE by default (tex * vtx)
  alpha_test_data.color_op0 = 3.0f;  // MODULATE by default
  alpha_test_data.color_op1 = 0.0f;  // Stage 1 disabled by default
  alpha_test_data.tex_factor_r = 1.0f;
  alpha_test_data.tex_factor_g = 1.0f;
  alpha_test_data.tex_factor_b = 1.0f;
  alpha_test_data.tex_factor_a = 1.0f;
  // Default alpha args: TEXTURE (3) * DIFFUSE (1)
  alpha_test_data.alpha_arg1 = 3.0f;
  alpha_test_data.alpha_arg2 = 1.0f;
  // Alpha scale: 1.0 = no scaling, <1.0 = scale down alpha before test (for BC2).
  alpha_test_data._pad_alpha_args[0] = 1.0f;
  alpha_test_data._pad_alpha_args[1] = 0.0f;
  // Texture transforms disabled by default.
  alpha_test_data.tex_transform_enabled0 = 0.0f;
  alpha_test_data.tex_transform_enabled1 = 0.0f;
  alpha_test_data.tex_transform0 = DirectX::XMMatrixIdentity();
  alpha_test_data.tex_transform1 = DirectX::XMMatrixIdentity();

  // Initialize tracked texture transforms to identity.
  for (int i = 0; i < 8; ++i) {
    stage_tex_transform_enabled_[i] = false;
    tex_transform_matrix_raw_[i] = DirectX::XMMatrixIdentity();
    tex_transform_matrix_[i] = DirectX::XMMatrixIdentity();
  }
  UpdateDynamicCB(context, cb_alpha_test, alpha_test_data);
  last_alpha_test_data_ = alpha_test_data;
  has_last_alpha_test_data_ = true;
  // Bind it to PS slot 4 so it's always available
  BindPSConstantBuffer(4, cb_alpha_test);
  BindVSConstantBuffer(4, cb_alpha_test);

  // Light CB
  cbd.ByteWidth = sizeof(LightCB);
  if (FAILED(device->CreateBuffer(&cbd, nullptr, &cb_light))) {
    SPDLOG_ERROR("Failed to create light CB");
    return false;
  }
  SetDebugObjectName(cb_light, "cb_light");

  // Initialize light CB with all lights disabled
  memset(&light_data, 0, sizeof(LightCB));
  light_data.ambient = {0.2f, 0.2f, 0.2f, 1.0f};  // Default ambient
  light_data.num_active_lights = 0;
  UpdateDynamicCB(context, cb_light, light_data);
  // Bind it to VS slot 5 and PS slot 5
  BindVSConstantBuffer(5, cb_light);
  BindPSConstantBuffer(5, cb_light);

  // Gamma CB (PS slot 6) - emulates Gothic's D3D9 gamma ramp.
  cbd.ByteWidth = sizeof(GammaCB);
  if (FAILED(device->CreateBuffer(&cbd, nullptr, &cb_gamma))) {
    SPDLOG_ERROR("Failed to create gamma CB");
    return false;
  }
  SetDebugObjectName(cb_gamma, "cb_gamma");

  // Initialize to neutral state (identity transform).
  // At neutral: brightness_offset=0, contrast_scale=1.0, gamma_exp=1.0 produces rgb unchanged.
  gamma_data.brightness_offset = 0.0f;
  gamma_data.contrast_scale = 1.0f;
  gamma_data.gamma_exp = 1.0f;
  gamma_data.gamma_enabled = 0.0f;  // Fast path: skip shader math when neutral.
  UpdateDynamicCB(context, cb_gamma, gamma_data);
  BindPSConstantBuffer(6, cb_gamma);

  return true;
}

void D3D11RendererImpl::SetGammaCorrection(float gamma, float contrast, float brightness) {
  if (!cb_gamma || !context) {
    return;
  }

  // Modern color correction pipeline: Brightness → Contrast → Gamma.
  // All parameters: 0.5 = neutral (identity transform), range [0.1, 0.9].
  // Unlike Gothic's original hardware LUT, this is applied per-pixel in shaders,
  // but the modern algorithm produces identity at neutral.
  //
  // At neutral (0.5): brightness_offset=0, contrast_scale=1.0, gamma_exp=1.0
  // Result: rgb + 0 → (rgb - 0.5) * 1.0 + 0.5 → pow(rgb, 1.0) = rgb (unchanged)

  // Clamp inputs to valid range
  gamma = std::clamp(gamma, 0.1f, 0.9f);
  contrast = std::clamp(contrast, 0.1f, 0.9f);
  brightness = std::clamp(brightness, 0.1f, 0.9f);

  // Compute parameters with identity at 0.5:
  // - Brightness: additive offset, ±0.32 range
  gamma_data.brightness_offset = (brightness - 0.5f) * 0.8f;
  // - Contrast: scale around midpoint 0.5, range [0.2, 1.8]
  gamma_data.contrast_scale = contrast * 2.0f;
  // - Gamma: power curve exponent, range [0.56, 5.0] (0.5 → 1.0)
  gamma_data.gamma_exp = 1.0f / (std::max)(0.1f, gamma * 2.0f);

  // Optional optimization: skip shader math when all parameters are identity.
  // This is purely for performance - the math produces correct results either way.
  constexpr float kNeutral = 0.5f;
  constexpr float kEpsilon = 0.03f;  // Catches Gothic's quantized 0.51428574
  const bool is_neutral =
      (std::abs(gamma - kNeutral) < kEpsilon) && (std::abs(contrast - kNeutral) < kEpsilon) && (std::abs(brightness - kNeutral) < kEpsilon);
  gamma_data.gamma_enabled = is_neutral ? 0.0f : 1.0f;

  UpdateDynamicCB(context, cb_gamma, gamma_data);

  // Ensure the buffer is bound (some paths rebind CBs explicitly).
  BindPSConstantBuffer(6, cb_gamma);
}

bool D3D11RendererImpl::CreateStateObjects() {
  // --- Blend States ---
  D3D11_BLEND_DESC bd = {};
  bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

  // Opaque (no blending)
  bd.RenderTarget[0].BlendEnable = FALSE;
  if (FAILED(device->CreateBlendState(&bd, &bs_opaque))) {
    SPDLOG_ERROR("Failed to create opaque blend state");
    return false;
  }

  // Alpha blend (SrcAlpha, InvSrcAlpha)
  bd.RenderTarget[0].BlendEnable = TRUE;
  bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
  bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
  bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
  bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
  bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
  bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
  if (FAILED(device->CreateBlendState(&bd, &bs_alpha_blend))) {
    SPDLOG_ERROR("Failed to create alpha blend state");
    return false;
  }

  // Additive blend (SrcAlpha, One)
  bd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
  if (FAILED(device->CreateBlendState(&bd, &bs_alpha_add))) {
    SPDLOG_ERROR("Failed to create alpha add blend state");
    return false;
  }

  // Multiplicative blend (DestColor, Zero) - for shadows/darkening
  bd.RenderTarget[0].SrcBlend = D3D11_BLEND_DEST_COLOR;
  bd.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
  bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_DEST_ALPHA;
  bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
  if (FAILED(device->CreateBlendState(&bd, &bs_alpha_mul))) {
    SPDLOG_ERROR("Failed to create alpha mul blend state");
    return false;
  }

  // MUL2 blend (DestColor, SrcColor) - enhanced multiply for water/env reflections
  // result = src * destColor + dest * srcColor
  bd.RenderTarget[0].SrcBlend = D3D11_BLEND_DEST_COLOR;
  bd.RenderTarget[0].DestBlend = D3D11_BLEND_SRC_COLOR;
  bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_DEST_ALPHA;
  bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_SRC_ALPHA;
  if (FAILED(device->CreateBlendState(&bd, &bs_alpha_mul2))) {
    SPDLOG_ERROR("Failed to create alpha mul2 blend state");
    return false;
  }

  // --- Depth Stencil States ---
  D3D11_DEPTH_STENCIL_DESC dsd = {};
  dsd.DepthEnable = TRUE;
  dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
  dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

  // Default (Z-test and Z-write)
  if (FAILED(device->CreateDepthStencilState(&dsd, &dss_default))) {
    SPDLOG_ERROR("Failed to create default depth stencil state");
    return false;
  }

  // Alpha (Z-test LESS_EQUAL, no Z-write)
  dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
  if (FAILED(device->CreateDepthStencilState(&dsd, &dss_alpha))) {
    SPDLOG_ERROR("Failed to create alpha depth stencil state");
    return false;
  }

  // Alpha with LESS comparison (for sky dome rendering)
  dsd.DepthFunc = D3D11_COMPARISON_LESS;
  if (FAILED(device->CreateDepthStencilState(&dsd, &dss_alpha_less))) {
    SPDLOG_ERROR("Failed to create alpha-less depth stencil state");
    return false;
  }

  // No depth
  dsd.DepthEnable = FALSE;
  if (FAILED(device->CreateDepthStencilState(&dsd, &dss_no_depth))) {
    SPDLOG_ERROR("Failed to create no-depth stencil state");
    return false;
  }

  // --- Rasterizer States ---
  // Gothic uses D3DCULL_CW which means cull clockwise faces, so counter-clockwise = front.
  // In D3D11: FrontCounterClockwise=TRUE means CCW is front, and CULL_BACK culls CW faces.
  D3D11_RASTERIZER_DESC rd = {};
  rd.FillMode = D3D11_FILL_SOLID;
  rd.CullMode = D3D11_CULL_BACK;
  rd.FrontCounterClockwise = TRUE;  // Match Gothic's CCW front-face winding
  rd.DepthClipEnable = TRUE;

  if (FAILED(device->CreateRasterizerState(&rd, &rs_default))) {
    SPDLOG_ERROR("Failed to create default rasterizer state");
    return false;
  }

  rd.CullMode = D3D11_CULL_NONE;
  if (FAILED(device->CreateRasterizerState(&rd, &rs_no_cull))) {
    SPDLOG_ERROR("Failed to create no-cull rasterizer state");
    return false;
  }

  rd.FillMode = D3D11_FILL_WIREFRAME;
  if (FAILED(device->CreateRasterizerState(&rd, &rs_wireframe))) {
    SPDLOG_ERROR("Failed to create wireframe rasterizer state");
    return false;
  }

  // --- Sampler States ---
  D3D11_SAMPLER_DESC sd = {};
  sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
  sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
  sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
  sd.MaxAnisotropy = 1;
  sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
  // D3D9 default mip LOD bias is 0.0f. Previously tried -0.5f to sharpen cutouts,
  // but that made textures appear brighter than the D3D9 reference.
  sd.MipLODBias = 0.0f;
  sd.MaxLOD = D3D11_FLOAT32_MAX;

  if (FAILED(device->CreateSamplerState(&sd, &ss_linear_wrap))) {
    SPDLOG_ERROR("Failed to create linear wrap sampler");
    return false;
  }

  sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
  sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  if (FAILED(device->CreateSamplerState(&sd, &ss_linear_wrap_clamp))) {
    SPDLOG_ERROR("Failed to create linear wrap/clamp sampler");
    return false;
  }

  sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
  if (FAILED(device->CreateSamplerState(&sd, &ss_linear_clamp_wrap))) {
    SPDLOG_ERROR("Failed to create linear clamp/wrap sampler");
    return false;
  }

  sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  if (FAILED(device->CreateSamplerState(&sd, &ss_linear_clamp))) {
    SPDLOG_ERROR("Failed to create linear clamp sampler");
    return false;
  }

  // Linear MIN/MAG with POINT MIP (common D3D9-style sampling).
  sd.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;

  sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
  sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
  if (FAILED(device->CreateSamplerState(&sd, &ss_linear_mip_point_wrap))) {
    SPDLOG_ERROR("Failed to create linear mip-point wrap sampler");
    return false;
  }

  sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
  sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  if (FAILED(device->CreateSamplerState(&sd, &ss_linear_mip_point_wrap_clamp))) {
    SPDLOG_ERROR("Failed to create linear mip-point wrap/clamp sampler");
    return false;
  }

  sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
  if (FAILED(device->CreateSamplerState(&sd, &ss_linear_mip_point_clamp_wrap))) {
    SPDLOG_ERROR("Failed to create linear mip-point clamp/wrap sampler");
    return false;
  }

  sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  if (FAILED(device->CreateSamplerState(&sd, &ss_linear_mip_point_clamp))) {
    SPDLOG_ERROR("Failed to create linear mip-point clamp sampler");
    return false;
  }

  sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
  sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
  sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
  if (FAILED(device->CreateSamplerState(&sd, &ss_point_wrap))) {
    SPDLOG_ERROR("Failed to create point wrap sampler");
    return false;
  }

  sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
  sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  if (FAILED(device->CreateSamplerState(&sd, &ss_point_wrap_clamp))) {
    SPDLOG_ERROR("Failed to create point wrap/clamp sampler");
    return false;
  }

  sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
  if (FAILED(device->CreateSamplerState(&sd, &ss_point_clamp_wrap))) {
    SPDLOG_ERROR("Failed to create point clamp/wrap sampler");
    return false;
  }

  sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  if (FAILED(device->CreateSamplerState(&sd, &ss_point_clamp))) {
    SPDLOG_ERROR("Failed to create point clamp sampler");
    return false;
  }

  sd.Filter = D3D11_FILTER_ANISOTROPIC;
  sd.MaxAnisotropy = max_anisotropy;
  if (FAILED(device->CreateSamplerState(&sd, &ss_aniso_wrap))) {
    SPDLOG_ERROR("Failed to create aniso wrap sampler");
    return false;
  }

  sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
  sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  if (FAILED(device->CreateSamplerState(&sd, &ss_aniso_wrap_clamp))) {
    SPDLOG_ERROR("Failed to create aniso wrap/clamp sampler");
    return false;
  }

  sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
  if (FAILED(device->CreateSamplerState(&sd, &ss_aniso_clamp_wrap))) {
    SPDLOG_ERROR("Failed to create aniso clamp/wrap sampler");
    return false;
  }

  sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  if (FAILED(device->CreateSamplerState(&sd, &ss_aniso_clamp))) {
    SPDLOG_ERROR("Failed to create aniso clamp sampler");
    return false;
  }

  // Create default white texture (1x1) for when no texture is bound
  // This prevents stale textures from being sampled
  {
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = 1;
    texDesc.Height = 1;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_IMMUTABLE;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    uint32_t white_pixel = 0xFFFFFFFF;  // RGBA white
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = &white_pixel;
    initData.SysMemPitch = sizeof(uint32_t);

    if (FAILED(device->CreateTexture2D(&texDesc, &initData, &white_texture))) {
      SPDLOG_ERROR("Failed to create white fallback texture");
      return false;
    }

    if (FAILED(device->CreateShaderResourceView(white_texture, nullptr, &white_srv))) {
      SPDLOG_ERROR("Failed to create white fallback SRV");
      return false;
    }

#ifndef NDEBUG
    SetDebugObjectName(white_texture, "tex:fallback_white");
    SetDebugObjectName(white_srv, "srv:fallback_white");
#endif
  }

  // Create default black texture (1x1) for additive blend with no texture
  // Adding black does nothing - prevents white artifacts on water reflections
  {
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = 1;
    texDesc.Height = 1;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_IMMUTABLE;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    uint32_t black_pixel = 0x00000000;  // RGBA black/transparent
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = &black_pixel;
    initData.SysMemPitch = sizeof(uint32_t);

    if (FAILED(device->CreateTexture2D(&texDesc, &initData, &black_texture))) {
      SPDLOG_ERROR("Failed to create black fallback texture");
      return false;
    }

    if (FAILED(device->CreateShaderResourceView(black_texture, nullptr, &black_srv))) {
      SPDLOG_ERROR("Failed to create black fallback SRV");
      return false;
    }

#ifndef NDEBUG
    SetDebugObjectName(black_texture, "tex:fallback_black");
    SetDebugObjectName(black_srv, "srv:fallback_black");
#endif
  }

  return true;
}

bool D3D11RendererImpl::CreateDynamicBuffers() {
  // Set up capacities
  dynamic_vb_capacity = kDynamicVBSize;
  dynamic_ib_capacity = kDynamicIBSize / sizeof(uint16_t);  // Store as index count

  // Create triple-buffered resources
  for (int i = 0; i < kFramesInFlight; ++i) {
    auto& fr = frame_resources_[i];

    // Dynamic vertex buffer for this frame
    D3D11_BUFFER_DESC vbd = {};
    vbd.ByteWidth = static_cast<UINT>(dynamic_vb_capacity);
    vbd.Usage = D3D11_USAGE_DYNAMIC;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(device->CreateBuffer(&vbd, nullptr, &fr.dynamic_vb))) {
      SPDLOG_ERROR("Failed to create dynamic VB for frame {}", i);
      return false;
    }
    {
      char name[64];
      std::snprintf(name, sizeof(name), "dynamic_vb_f%d", i);
      SetDebugObjectName(fr.dynamic_vb, name);
    }

    // Dynamic index buffer for this frame
    D3D11_BUFFER_DESC ibd = {};
    ibd.ByteWidth = static_cast<UINT>(kDynamicIBSize);
    ibd.Usage = D3D11_USAGE_DYNAMIC;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(device->CreateBuffer(&ibd, nullptr, &fr.dynamic_ib))) {
      SPDLOG_ERROR("Failed to create dynamic IB for frame {}", i);
      return false;
    }
    {
      char name[64];
      std::snprintf(name, sizeof(name), "dynamic_ib_f%d", i);
      SetDebugObjectName(fr.dynamic_ib, name);
    }

    // Frame fence query - signals when GPU finishes this frame's work
    D3D11_QUERY_DESC qd = {};
    qd.Query = D3D11_QUERY_EVENT;
    qd.MiscFlags = 0;
    if (FAILED(device->CreateQuery(&qd, &fr.fence))) {
      SPDLOG_ERROR("Failed to create frame fence for frame {}", i);
      return false;
    }

    fr.vb_offset = 0;
    fr.ib_offset = 0;
    fr.fence_pending = false;
  }

  // Initialize current frame pointers
  current_frame_index_ = 0;
  UpdateCurrentFramePointers();

  return true;
}

bool D3D11RendererImpl::Resize(int width, int height) {
  if (!swap_chain || width <= 0 || height <= 0) {
    return false;
  }

  // Release old views
  context->OMSetRenderTargets(0, nullptr, nullptr);
  SafeRelease(rtv);
  SafeRelease(dsv);
  SafeRelease(depth_stencil_texture);

  HRESULT hr;

  // In exclusive fullscreen mode (not FLIP model), we need to call ResizeTarget
  // to change the display mode. This actually changes the monitor resolution.
  // ResizeBuffers alone only changes the back buffer size, not the display mode.
  // With FLIP model, the window is borderless and we don't change display mode.
  if (fullscreen && !using_flip_model) {
    DXGI_MODE_DESC target_mode = {};
    target_mode.Width = width;
    target_mode.Height = height;
    target_mode.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    target_mode.RefreshRate.Numerator = 0;  // Let DXGI find the best refresh rate
    target_mode.RefreshRate.Denominator = 0;
    target_mode.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    target_mode.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;

    hr = swap_chain->ResizeTarget(&target_mode);
    if (FAILED(hr)) {
      SPDLOG_ERROR("ResizeTarget failed: 0x{:08X}", static_cast<unsigned int>(hr));
      // Try to continue anyway - ResizeBuffers might still work
    }
  }

  // Resize swap chain buffers with appropriate flags for the presentation model
  UINT resize_flags = using_flip_model ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
  hr = swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, resize_flags);
  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to resize swap chain: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  // Recreate views
  if (!CreateRenderTargetView() || !CreateDepthStencilView(width, height)) {
    return false;
  }

  screen_width = width;
  screen_height = height;

  // Update screen constant buffer
  screen_data.screen_size = DirectX::XMFLOAT2(static_cast<float>(width), static_cast<float>(height));
  screen_data.inv_screen_size = DirectX::XMFLOAT2(1.0f / width, 1.0f / height);
  UpdateDynamicCB(context, cb_screen, screen_data);

  // Rebind render targets
  context->OMSetRenderTargets(1, &rtv, dsv);
  SetViewport(0, 0, width, height);

  return true;
}

bool D3D11RendererImpl::SetFullscreenState(bool want_fullscreen) {
  if (!swap_chain) {
    SPDLOG_ERROR("SetFullscreenState: No swap chain");
    return false;
  }

  if (want_fullscreen == fullscreen) {
    return true;
  }

  // With FLIP model, we don't use DXGI exclusive fullscreen.
  // The window is sized/styled by the application (borderless fullscreen).
  // We just update our tracking flag.
  if (using_flip_model) {
    fullscreen = want_fullscreen;
    return true;
  }

  // Legacy DISCARD model: use actual DXGI exclusive fullscreen

  // Release views before mode switch (required by DXGI)
  context->OMSetRenderTargets(0, nullptr, nullptr);
  SafeRelease(rtv);
  SafeRelease(dsv);
  SafeRelease(depth_stencil_texture);

  // Switch fullscreen state
  HRESULT hr = swap_chain->SetFullscreenState(want_fullscreen ? TRUE : FALSE, want_fullscreen ? dxgi_output : nullptr);
  if (FAILED(hr)) {
    SPDLOG_ERROR("SetFullscreenState failed: 0x{:08X}", static_cast<unsigned int>(hr));

    // Query actual DXGI state to keep our flag in sync
    BOOL actual_fullscreen = FALSE;
    IDXGIOutput* actual_output = nullptr;
    if (SUCCEEDED(swap_chain->GetFullscreenState(&actual_fullscreen, &actual_output))) {
      fullscreen = (actual_fullscreen == TRUE);
      if (actual_output) {
        actual_output->Release();
      }
    }

    // Try to recover by recreating views at current size
    if (!CreateRenderTargetView() || !CreateDepthStencilView(screen_width, screen_height)) {
      SPDLOG_ERROR("Failed to recover after SetFullscreenState failure");
    }
    context->OMSetRenderTargets(1, &rtv, dsv);
    return false;
  }

  fullscreen = want_fullscreen;

  // When entering fullscreen, set the display mode to the current resolution
  if (want_fullscreen) {
    DXGI_MODE_DESC target_mode = {};
    target_mode.Width = screen_width;
    target_mode.Height = screen_height;
    target_mode.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    target_mode.RefreshRate.Numerator = 0;  // Let DXGI find the best refresh rate
    target_mode.RefreshRate.Denominator = 0;
    target_mode.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    target_mode.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;

    hr = swap_chain->ResizeTarget(&target_mode);
    if (FAILED(hr)) {
      SPDLOG_WARN("ResizeTarget failed after fullscreen switch: 0x{:08X}", static_cast<unsigned int>(hr));
      // Continue anyway - the display mode might still be acceptable
    }
  }

  // Resize buffers to match the new mode
  hr = swap_chain->ResizeBuffers(0, screen_width, screen_height, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
  if (FAILED(hr)) {
    SPDLOG_ERROR("ResizeBuffers failed after fullscreen switch: 0x{:08X}", static_cast<unsigned int>(hr));
    return false;
  }

  // Recreate views
  if (!CreateRenderTargetView() || !CreateDepthStencilView(screen_width, screen_height)) {
    SPDLOG_ERROR("Failed to recreate views after fullscreen switch");
    return false;
  }

  // Rebind render targets
  context->OMSetRenderTargets(1, &rtv, dsv);
  SetViewport(0, 0, screen_width, screen_height);

  return true;
}

void D3D11RendererImpl::Cleanup() {
  // Exit exclusive fullscreen mode before cleanup (required by DXGI before releasing swap chain).
  // FLIP model doesn't use DXGI exclusive fullscreen, so no need to exit.
  if (swap_chain && fullscreen && !using_flip_model) {
    swap_chain->SetFullscreenState(FALSE, nullptr);
    fullscreen = false;
  }

  // Ensure any engine-owned vertex buffers release their D3D11 buffers before we tear down the device.
  // This reduces debug-layer live-object warnings during shutdown.
  zCVertexBuffer_D3D11::DestroyAllBuffers();

  if (context) {
    context->ClearState();
    context->Flush();
  }

  // Release triple-buffered frame resources
  for (auto& fr : frame_resources_) {
    SafeRelease(fr.fence);
    SafeRelease(fr.dynamic_ib);
    SafeRelease(fr.dynamic_vb);
    fr.vb_offset = 0;
    fr.ib_offset = 0;
    fr.fence_pending = false;
  }
  dynamic_vb = nullptr;
  dynamic_ib = nullptr;

  for (auto& [key, state] : sampler_state_cache_) {
    SafeRelease(state);
  }
  sampler_state_cache_.clear();

  for (auto& [key, state] : raster_state_cache_) {
    SafeRelease(state);
  }
  raster_state_cache_.clear();

  for (auto& [key, state] : depth_state_cache_) {
    SafeRelease(state);
  }
  depth_state_cache_.clear();

  for (auto& [key, state] : blend_state_cache_) {
    SafeRelease(state);
  }
  blend_state_cache_.clear();

  SafeRelease(white_srv);
  SafeRelease(white_texture);
  SafeRelease(black_srv);
  SafeRelease(black_texture);

  SafeRelease(ss_aniso_wrap);
  SafeRelease(ss_aniso_clamp);
  SafeRelease(ss_aniso_wrap_clamp);
  SafeRelease(ss_aniso_clamp_wrap);
  SafeRelease(ss_point_clamp);
  SafeRelease(ss_point_wrap);
  SafeRelease(ss_point_wrap_clamp);
  SafeRelease(ss_point_clamp_wrap);
  SafeRelease(ss_linear_clamp);
  SafeRelease(ss_linear_wrap);
  SafeRelease(ss_linear_wrap_clamp);
  SafeRelease(ss_linear_clamp_wrap);

  SafeRelease(rs_biased);
  SafeRelease(rs_wireframe);
  SafeRelease(rs_no_cull);
  SafeRelease(rs_default);

  SafeRelease(dss_alpha_less);
  SafeRelease(dss_no_depth);
  SafeRelease(dss_alpha);
  SafeRelease(dss_default);

  SafeRelease(bs_alpha_mul2);
  SafeRelease(bs_alpha_mul);
  SafeRelease(bs_alpha_add);
  SafeRelease(bs_alpha_blend);
  SafeRelease(bs_opaque);

  SafeRelease(cb_light);
  SafeRelease(cb_gamma);
  SafeRelease(cb_alpha_test);
  SafeRelease(cb_screen);
  SafeRelease(cb_fog);
  SafeRelease(cb_material);
  SafeRelease(cb_transform);

  SafeRelease(layout_rhw2);
  SafeRelease(layout_rhw);
  SafeRelease(layout_3d);
  SafeRelease(layout_3d_color);
  SafeRelease(layout_lit_normal_singleuv);
  SafeRelease(layout_lit_2uv);

  SafeRelease(ps_vertex_color);
  SafeRelease(ps_lightmap);
  SafeRelease(ps_dual_add);
  SafeRelease(ps_rhw_alpha);
  SafeRelease(ps_rhw);
  SafeRelease(ps_basic);
  SafeRelease(vs_rhw);
  SafeRelease(vs_lit_normal);
  SafeRelease(vs_lit_normal_singleuv);
  SafeRelease(vs_lit_2uv);
  SafeRelease(vs_lit);
  SafeRelease(vs_basic_color_unlit);
  SafeRelease(vs_basic_unlit);
  SafeRelease(vs_basic_color);
  SafeRelease(vs_basic);

  SafeRelease(layout_rhw2);
  SafeRelease(layout_rhw);
  SafeRelease(layout_lit_normal);
  SafeRelease(layout_lit_normal_singleuv);
  SafeRelease(layout_lit_2uv);
  SafeRelease(layout_lit);
  SafeRelease(layout_3d);
  SafeRelease(layout_3d_color);

  SafeRelease(depth_stencil_texture);
  SafeRelease(dsv);
  SafeRelease(rtv);
  SafeRelease(swap_chain);
  SafeRelease(dxgi_output);
  SafeRelease(user_annotation);
  SafeRelease(context);

  SafeRelease(device);

  g_D3D11Device = nullptr;
  g_D3D11Context = nullptr;
}

void D3D11RendererImpl::BeginFrame() {
  if (frame_begun_) {
    return;
  }
  frame_begun_ = true;

  // FLIP model: refresh RTV if needed (after Present rotated the backbuffer).
  // This must happen before any rendering to ensure we target the correct buffer.
  if (using_flip_model && flip_rtv_needs_refresh_) {
    RefreshBackBufferRTV();
  }

  // Rotate to the next frame's buffers (waits for GPU if needed)
  RotateFrameResources();

  batch_.Reset();
}

void D3D11RendererImpl::EndFrame() {
  FlushBatch();
  // Note: Fence is issued in Present() after swap chain present
}

void D3D11RendererImpl::Present() {
  // Determine present parameters based on vsync and presentation model
  UINT sync_interval = vsync ? 1 : 0;
  UINT present_flags = 0;

  // When using FLIP model with tearing support and vsync off, use ALLOW_TEARING
  // for uncapped FPS. Note: ALLOW_TEARING must NOT be used with SyncInterval > 0.
  if (!vsync && using_flip_model && tearing_supported) {
    present_flags = DXGI_PRESENT_ALLOW_TEARING;
  }

  HRESULT hr = swap_chain->Present(sync_interval, present_flags);

  // FLIP model: Mark that RTV needs refresh before next frame's rendering.
  // The actual refresh happens in BeginFrame() to keep Present() lightweight
  // and place the binding logic where it conceptually belongs (frame start).
  if (using_flip_model && SUCCEEDED(hr)) {
    flip_rtv_needs_refresh_ = true;
  }

  // Issue fence AFTER present - this ensures the GPU has received all commands
  // for this frame including the present before we signal completion
  auto& fr = frame_resources_[current_frame_index_];
  context->End(fr.fence);
  fr.fence_pending = true;

  // Reset frame_begun_ so next frame's BeginFrame() will run
  frame_begun_ = false;
}

void D3D11RendererImpl::Clear(unsigned long color) {
  Clear(color, true, true);
}

void D3D11RendererImpl::Clear(unsigned long color, bool clear_color, bool clear_depth) {
  if (context == nullptr) {
    return;
  }

  // FLIP model: ensure RTV is valid before clearing.
  // Vid_Clear can be called before BeginFrame (e.g., loading screens, sky rendering).
  if (using_flip_model && flip_rtv_needs_refresh_) {
    RefreshBackBufferRTV();
  }

  float clear_rgba[4];
  clear_rgba[0] = ((color >> 16) & 0xFF) / 255.0f;  // R
  clear_rgba[1] = ((color >> 8) & 0xFF) / 255.0f;   // G
  clear_rgba[2] = (color & 0xFF) / 255.0f;          // B
  clear_rgba[3] = ((color >> 24) & 0xFF) / 255.0f;  // A

  if (clear_color && rtv != nullptr) {
    context->ClearRenderTargetView(rtv, clear_rgba);
  }
  if (clear_depth && dsv != nullptr) {
    context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
  }
}

void D3D11RendererImpl::SetViewport(int x, int y, int width, int height) {
  if (current_viewport_x == x && current_viewport_y == y && current_viewport_w == width && current_viewport_h == height) {
    return;
  }
  current_viewport_x = x;
  current_viewport_y = y;
  current_viewport_w = width;
  current_viewport_h = height;

  D3D11_VIEWPORT vp = {};
  vp.TopLeftX = static_cast<float>(x);
  vp.TopLeftY = static_cast<float>(y);
  vp.Width = static_cast<float>(width);
  vp.Height = static_cast<float>(height);
  vp.MinDepth = 0.0f;
  vp.MaxDepth = 1.0f;
  context->RSSetViewports(1, &vp);
}

void D3D11RendererImpl::SetBlendState(ID3D11BlendState* state) {
  if (current_blend_state != state) {
    current_blend_state = state;
    context->OMSetBlendState(state, nullptr, 0xFFFFFFFF);
  }
}

void D3D11RendererImpl::SetDepthStencilState(ID3D11DepthStencilState* state, UINT stencil_ref) {
  if (current_depth_state != state || current_stencil_ref != stencil_ref) {
    current_depth_state = state;
    current_stencil_ref = stencil_ref;
    context->OMSetDepthStencilState(state, stencil_ref);
  }
}

void D3D11RendererImpl::SetRasterizerState(ID3D11RasterizerState* state) {
  // Remember what higher-level code requested (before applying Z-bias overrides).
  requested_rasterizer_state_ = state;

  ID3D11RasterizerState* resolved = state;

  // If Z-bias is active and the caller requested one of our prebuilt rasterizer states,
  // switch to a biased variant so coplanar geometry can be ordered deterministically.
  const bool bias_active = (raster_depth_bias_ != 0.0f) || (raster_slope_scaled_depth_bias_ != 0.0f) || (raster_depth_bias_clamp_ != 0.0f);
  if (bias_active && state) {
    RasterDesc desc = {};
    bool can_rebuild = false;

    if (state == rs_default) {
      desc.fill = FillMode::kSolid;
      desc.cull = CullMode::kBack;
      // Match CreateStateObjects(): Gothic treats CCW as front-face.
      desc.front_ccw = true;
      desc.scissor_enable = false;
      desc.multisample_enable = false;
      can_rebuild = true;
    } else if (state == rs_no_cull) {
      desc.fill = FillMode::kSolid;
      desc.cull = CullMode::kNone;
      desc.front_ccw = true;
      desc.scissor_enable = false;
      desc.multisample_enable = false;
      can_rebuild = true;
    } else if (state == rs_wireframe) {
      desc.fill = FillMode::kWireframe;
      desc.cull = CullMode::kBack;
      desc.front_ccw = true;
      desc.scissor_enable = false;
      desc.multisample_enable = false;
      can_rebuild = true;
    }

    if (can_rebuild) {
      // For D24_UNORM depth buffers, D3D11's integer DepthBias is scaled by 1/2^24.
      // Convert normalized depth bias into integer units.
      const float depth_bias_int_units = raster_depth_bias_ * 16777216.0f;  // 2^24
      desc.depth_bias = static_cast<std::int32_t>(std::lround(depth_bias_int_units));
      desc.slope_scaled_depth_bias = raster_slope_scaled_depth_bias_;
      desc.depth_bias_clamp = raster_depth_bias_clamp_;
      resolved = GetOrCreateRasterizerState(desc);
    }
  }

  if (current_rasterizer_state != resolved) {
    current_rasterizer_state = resolved;
    context->RSSetState(resolved);
  }
}

void D3D11RendererImpl::SetSamplerState(int stage, ID3D11SamplerState* state) {
  if (!context) {
    return;
  }
  if (stage >= 0 && static_cast<std::size_t>(stage) < bound_samplers.size()) {
    if (bound_samplers[stage] == state) {
      return;
    }
    bound_samplers[stage] = state;
  }
  context->PSSetSamplers(stage, 1, &state);
}

void D3D11RendererImpl::SetShaders(ID3D11VertexShader* vs, ID3D11PixelShader* ps) {
  if (current_vs != vs) {
    current_vs = vs;
    context->VSSetShader(vs, nullptr, 0);
  }
  if (current_ps != ps) {
    current_ps = ps;
    context->PSSetShader(ps, nullptr, 0);
  }
}

void D3D11RendererImpl::SetInputLayout(ID3D11InputLayout* layout) {
  if (current_layout != layout) {
    current_layout = layout;
    context->IASetInputLayout(layout);
  }
}

void D3D11RendererImpl::BindVertexBuffer(UINT slot, ID3D11Buffer* buffer, UINT stride, UINT offset) {
  if (!context) {
    return;
  }
  if (slot < bound_vbs.size()) {
    auto& bound = bound_vbs[slot];
    if (bound.buffer == buffer && bound.stride == stride && bound.offset == offset) {
      return;
    }
    bound.buffer = buffer;
    bound.stride = stride;
    bound.offset = offset;
  }
  context->IASetVertexBuffers(slot, 1, &buffer, &stride, &offset);
}

void D3D11RendererImpl::BindIndexBuffer(ID3D11Buffer* buffer, DXGI_FORMAT format, UINT offset) {
  if (!context) {
    return;
  }
  if (bound_ib == buffer && bound_ib_format == format && bound_ib_offset == offset) {
    return;
  }
  bound_ib = buffer;
  bound_ib_format = format;
  bound_ib_offset = offset;
  context->IASetIndexBuffer(buffer, format, offset);
}

void D3D11RendererImpl::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY topology) {
  if (!context) {
    return;
  }
  if (current_topology == topology) {
    return;
  }
  current_topology = topology;
  context->IASetPrimitiveTopology(topology);
}

void D3D11RendererImpl::BindVSConstantBuffer(UINT slot, ID3D11Buffer* buffer) {
  if (!context) {
    return;
  }
  if (slot < bound_vs_cbs.size()) {
    if (bound_vs_cbs[slot] == buffer) {
      return;
    }
    bound_vs_cbs[slot] = buffer;
  }
  context->VSSetConstantBuffers(slot, 1, &buffer);
}

void D3D11RendererImpl::BindPSConstantBuffer(UINT slot, ID3D11Buffer* buffer) {
  if (!context) {
    return;
  }
  if (slot < bound_ps_cbs.size()) {
    if (bound_ps_cbs[slot] == buffer) {
      return;
    }
    bound_ps_cbs[slot] = buffer;
  }
  context->PSSetConstantBuffers(slot, 1, &buffer);
}

void D3D11RendererImpl::SetTexture(int stage, ID3D11ShaderResourceView* srv, DXGI_FORMAT format) {
  // Use white fallback texture when no texture is provided
  // This prevents stale textures (like font atlas) from being sampled
  ID3D11ShaderResourceView* actual_srv = srv ? srv : white_srv;

  if (bound_srvs[stage] != actual_srv) {
    bound_srvs[stage] = actual_srv;

    // Use the provided format if available, otherwise query it (fallback for legacy callers).
    DXGI_FORMAT fmt = format;
    if (fmt == DXGI_FORMAT_UNKNOWN && actual_srv && actual_srv != white_srv && actual_srv != black_srv) {
      D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
      actual_srv->GetDesc(&desc);
      fmt = desc.Format;
    }
    bound_srv_formats_[stage] = fmt;

    if (stage == 0 && alpha_test_data.alpha_test_enabled >= 0.5f) {
      alpha_test_data.alpha_ref = ComputeEffectiveAlphaRef(material_data.alpha_ref);
    }

    context->PSSetShaderResources(stage, 1, &actual_srv);

    // Diagnostic: detect when both slots end up with the same non-white SRV (potential bug).
    if (stage == 1 && actual_srv != white_srv && bound_srvs[0] == actual_srv) {
      static int warned_same_srv_count = 0;
      if (warned_same_srv_count < 5) {
        ++warned_same_srv_count;
        SPDLOG_WARN("[SRV COLLISION] SetTexture(1) bound SAME SRV as slot 0: srv={} (this can cause black textures!)", (void*)actual_srv);
      }
    }
  }
}

void D3D11RendererImpl::SetHasLightmap(bool has_lightmap) {
  has_lightmap_ = has_lightmap;
}

void D3D11RendererImpl::ApplyOpaqueState() {
  SetBlendState(bs_opaque);
  SetDepthStencilState(dss_default);
  SetRasterizerState(rs_default);
  ApplyPrebuiltSamplerState(0);
}

void D3D11RendererImpl::ApplyAlphaBlendState() {
  SetBlendState(bs_alpha_blend);
  SetDepthStencilState(dss_alpha);
  SetRasterizerState(rs_no_cull);
}

void D3D11RendererImpl::ApplyAlphaAddState() {
  SetBlendState(bs_alpha_add);
  SetDepthStencilState(dss_alpha);
  SetRasterizerState(rs_no_cull);
}

void D3D11RendererImpl::ApplyAlphaTestState() {
  SetBlendState(bs_opaque);
  SetDepthStencilState(dss_default);
  SetRasterizerState(rs_no_cull);
}

namespace {

D3D11_BLEND ToD3D11Blend(gmp::renderer::d3d11::BlendFactor factor) {
  using gmp::renderer::d3d11::BlendFactor;
  switch (factor) {
    case BlendFactor::kZero:
      return D3D11_BLEND_ZERO;
    case BlendFactor::kOne:
      return D3D11_BLEND_ONE;
    case BlendFactor::kSrcColor:
      return D3D11_BLEND_SRC_COLOR;
    case BlendFactor::kInvSrcColor:
      return D3D11_BLEND_INV_SRC_COLOR;
    case BlendFactor::kSrcAlpha:
      return D3D11_BLEND_SRC_ALPHA;
    case BlendFactor::kInvSrcAlpha:
      return D3D11_BLEND_INV_SRC_ALPHA;
    case BlendFactor::kDestColor:
      return D3D11_BLEND_DEST_COLOR;
    case BlendFactor::kInvDestColor:
      return D3D11_BLEND_INV_DEST_COLOR;
    case BlendFactor::kDestAlpha:
      return D3D11_BLEND_DEST_ALPHA;
    case BlendFactor::kInvDestAlpha:
      return D3D11_BLEND_INV_DEST_ALPHA;
  }
  return D3D11_BLEND_ONE;
}

D3D11_BLEND_OP ToD3D11BlendOp(gmp::renderer::d3d11::BlendOp op) {
  using gmp::renderer::d3d11::BlendOp;
  switch (op) {
    case BlendOp::kAdd:
      return D3D11_BLEND_OP_ADD;
    case BlendOp::kSub:
      return D3D11_BLEND_OP_SUBTRACT;
    case BlendOp::kRevSub:
      return D3D11_BLEND_OP_REV_SUBTRACT;
    case BlendOp::kMin:
      return D3D11_BLEND_OP_MIN;
    case BlendOp::kMax:
      return D3D11_BLEND_OP_MAX;
  }
  return D3D11_BLEND_OP_ADD;
}

D3D11_COMPARISON_FUNC ToD3D11Compare(gmp::renderer::d3d11::CompareFunc func) {
  using gmp::renderer::d3d11::CompareFunc;
  switch (func) {
    case CompareFunc::kNever:
      return D3D11_COMPARISON_NEVER;
    case CompareFunc::kLess:
      return D3D11_COMPARISON_LESS;
    case CompareFunc::kEqual:
      return D3D11_COMPARISON_EQUAL;
    case CompareFunc::kLessEqual:
      return D3D11_COMPARISON_LESS_EQUAL;
    case CompareFunc::kGreater:
      return D3D11_COMPARISON_GREATER;
    case CompareFunc::kNotEqual:
      return D3D11_COMPARISON_NOT_EQUAL;
    case CompareFunc::kGreaterEqual:
      return D3D11_COMPARISON_GREATER_EQUAL;
    case CompareFunc::kAlways:
      return D3D11_COMPARISON_ALWAYS;
  }
  return D3D11_COMPARISON_ALWAYS;
}

D3D11_CULL_MODE ToD3D11Cull(gmp::renderer::d3d11::CullMode mode) {
  using gmp::renderer::d3d11::CullMode;
  switch (mode) {
    case CullMode::kNone:
      return D3D11_CULL_NONE;
    case CullMode::kFront:
      return D3D11_CULL_FRONT;
    case CullMode::kBack:
      return D3D11_CULL_BACK;
  }
  return D3D11_CULL_BACK;
}

D3D11_FILL_MODE ToD3D11Fill(gmp::renderer::d3d11::FillMode mode) {
  using gmp::renderer::d3d11::FillMode;
  switch (mode) {
    case FillMode::kWireframe:
      return D3D11_FILL_WIREFRAME;
    case FillMode::kSolid:
      return D3D11_FILL_SOLID;
  }
  return D3D11_FILL_SOLID;
}

D3D11_TEXTURE_ADDRESS_MODE ToD3D11Address(gmp::renderer::d3d11::AddressMode mode) {
  using gmp::renderer::d3d11::AddressMode;
  switch (mode) {
    case AddressMode::kWrap:
      return D3D11_TEXTURE_ADDRESS_WRAP;
    case AddressMode::kMirror:
      return D3D11_TEXTURE_ADDRESS_MIRROR;
    case AddressMode::kClamp:
      return D3D11_TEXTURE_ADDRESS_CLAMP;
  }
  return D3D11_TEXTURE_ADDRESS_WRAP;
}

D3D11_FILTER ToD3D11Filter(const SamplerDesc& desc) {
  using gmp::renderer::d3d11::FilterMode;
  switch (desc.filter) {
    case FilterMode::kPoint:
      return desc.mip_linear ? D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR : D3D11_FILTER_MIN_MAG_MIP_POINT;
    case FilterMode::kAnisotropic:
      // D3D11 anisotropic uses linear mip sampling.
      return D3D11_FILTER_ANISOTROPIC;
    case FilterMode::kLinear:
      return desc.mip_linear ? D3D11_FILTER_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
  }
  return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
}

}  // namespace

ID3D11BlendState* D3D11RendererImpl::GetOrCreateBlendState(const BlendDesc& desc) {
  const auto it = blend_state_cache_.find(desc);
  if (it != blend_state_cache_.end()) {
    return it->second;
  }

  if (!device) {
    return bs_opaque;
  }

  D3D11_BLEND_DESC bd = {};
  bd.AlphaToCoverageEnable = desc.alpha_to_coverage ? TRUE : FALSE;
  bd.IndependentBlendEnable = FALSE;

  auto& rt = bd.RenderTarget[0];
  rt.BlendEnable = desc.enable ? TRUE : FALSE;
  rt.SrcBlend = ToD3D11Blend(desc.src_color);
  rt.DestBlend = ToD3D11Blend(desc.dst_color);
  rt.BlendOp = ToD3D11BlendOp(desc.color_op);
  rt.SrcBlendAlpha = ToD3D11Blend(desc.src_alpha);
  rt.DestBlendAlpha = ToD3D11Blend(desc.dst_alpha);
  rt.BlendOpAlpha = ToD3D11BlendOp(desc.alpha_op);
  rt.RenderTargetWriteMask = desc.write_mask;

  ID3D11BlendState* state = nullptr;
  const HRESULT hr = device->CreateBlendState(&bd, &state);
  if (FAILED(hr) || !state) {
    SPDLOG_WARN("GetOrCreateBlendState: CreateBlendState failed: 0x{:08X}", static_cast<unsigned int>(hr));
    return bs_opaque;
  }

  blend_state_cache_.emplace(desc, state);
  return state;
}

ID3D11DepthStencilState* D3D11RendererImpl::GetOrCreateDepthStencilState(const DepthDesc& desc) {
  const auto it = depth_state_cache_.find(desc);
  if (it != depth_state_cache_.end()) {
    return it->second;
  }

  if (!device) {
    return dss_default;
  }

  D3D11_DEPTH_STENCIL_DESC dsd = {};
  dsd.DepthEnable = desc.enable ? TRUE : FALSE;
  dsd.DepthWriteMask = desc.write_enable ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
  dsd.DepthFunc = ToD3D11Compare(desc.func);
  dsd.StencilEnable = FALSE;

  ID3D11DepthStencilState* state = nullptr;
  const HRESULT hr = device->CreateDepthStencilState(&dsd, &state);
  if (FAILED(hr) || !state) {
    SPDLOG_WARN("GetOrCreateDepthStencilState: CreateDepthStencilState failed: 0x{:08X}", static_cast<unsigned int>(hr));
    return dss_default;
  }

  depth_state_cache_.emplace(desc, state);
  return state;
}

ID3D11RasterizerState* D3D11RendererImpl::GetOrCreateRasterizerState(const RasterDesc& desc) {
  const auto it = raster_state_cache_.find(desc);
  if (it != raster_state_cache_.end()) {
    return it->second;
  }

  if (!device) {
    return rs_default;
  }

  D3D11_RASTERIZER_DESC rd = {};
  rd.FillMode = ToD3D11Fill(desc.fill);
  rd.CullMode = ToD3D11Cull(desc.cull);
  rd.FrontCounterClockwise = desc.front_ccw ? TRUE : FALSE;
  rd.DepthBias = desc.depth_bias;
  rd.SlopeScaledDepthBias = desc.slope_scaled_depth_bias;
  rd.DepthBiasClamp = desc.depth_bias_clamp;
  rd.DepthClipEnable = TRUE;
  rd.ScissorEnable = desc.scissor_enable ? TRUE : FALSE;
  rd.MultisampleEnable = desc.multisample_enable ? TRUE : FALSE;
  rd.AntialiasedLineEnable = FALSE;

  ID3D11RasterizerState* state = nullptr;
  const HRESULT hr = device->CreateRasterizerState(&rd, &state);
  if (FAILED(hr) || !state) {
    SPDLOG_WARN("GetOrCreateRasterizerState: CreateRasterizerState failed: 0x{:08X}", static_cast<unsigned int>(hr));
    return rs_default;
  }

  raster_state_cache_.emplace(desc, state);
  return state;
}

ID3D11SamplerState* D3D11RendererImpl::GetOrCreateSamplerState(const SamplerDesc& desc) {
  const auto it = sampler_state_cache_.find(desc);
  if (it != sampler_state_cache_.end()) {
    return it->second;
  }

  if (!device) {
    return ss_linear_wrap;
  }

  D3D11_SAMPLER_DESC sd = {};
  sd.Filter = ToD3D11Filter(desc);
  sd.AddressU = ToD3D11Address(desc.address_u);
  sd.AddressV = ToD3D11Address(desc.address_v);
  sd.AddressW = ToD3D11Address(desc.address_w);
  sd.MipLODBias = 0.0f;
  sd.MaxAnisotropy = 1;
  if (desc.filter == FilterMode::kAnisotropic) {
    const int max_allowed = static_cast<int>((std::max)(1u, max_anisotropy));
    sd.MaxAnisotropy = static_cast<UINT>(std::clamp(static_cast<int>(desc.max_anisotropy), 1, max_allowed));
  }
  sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
  sd.BorderColor[0] = 0.0f;
  sd.BorderColor[1] = 0.0f;
  sd.BorderColor[2] = 0.0f;
  sd.BorderColor[3] = 0.0f;
  sd.MinLOD = 0.0f;
  sd.MaxLOD = D3D11_FLOAT32_MAX;

  ID3D11SamplerState* state = nullptr;
  const HRESULT hr = device->CreateSamplerState(&sd, &state);
  if (FAILED(hr) || !state) {
    SPDLOG_WARN("GetOrCreateSamplerState: CreateSamplerState failed: 0x{:08X}", static_cast<unsigned int>(hr));
    return ss_linear_wrap;
  }

  sampler_state_cache_.emplace(desc, state);
  return state;
}

void D3D11RendererImpl::SetBlendDesc(const BlendDesc& desc) {
  current_blend_desc_ = desc;

  SetBlendState(GetOrCreateBlendState(desc));
}

void D3D11RendererImpl::SetDepthDesc(const DepthDesc& desc) {
  SetDepthStencilState(GetOrCreateDepthStencilState(desc));
}

void D3D11RendererImpl::SetRasterDesc(const RasterDesc& desc) {
  RasterDesc effective = desc;
  // Apply current Z-bias to any typed raster descriptor the higher-level code sets.
  const float depth_bias_int_units = raster_depth_bias_ * 16777216.0f;  // 2^24 for D24_UNORM
  effective.depth_bias = static_cast<std::int32_t>(std::lround(depth_bias_int_units));
  effective.slope_scaled_depth_bias = raster_slope_scaled_depth_bias_;
  effective.depth_bias_clamp = raster_depth_bias_clamp_;
  engine_raster_desc_ = effective;
  has_engine_raster_desc_ = true;
  SetRasterizerState(GetOrCreateRasterizerState(effective));
}

void D3D11RendererImpl::SetSamplerDesc(int stage, const SamplerDesc& desc) {
  if (stage < 0 || stage >= static_cast<int>(kMaxTextureStages)) {
    return;
  }

  SetSamplerState(stage, GetOrCreateSamplerState(desc));
}

//------------------------------------------------------------------------------
// Native Texture Stage State API
//------------------------------------------------------------------------------
// These methods replace D3D9-style SetTextureStageState calls with typed,
// D3D11-native methods. They update internal tracking arrays that are used
// by shaders for FFP emulation.

void D3D11RendererImpl::SetTextureStageColorOp(int stage, CombineOp op) {
  if (stage < 0 || stage >= static_cast<int>(kMaxTextureStages)) {
    return;
  }
  stage_color_op_[stage] = op;
}

void D3D11RendererImpl::SetTextureStageAlphaOp(int stage, CombineOp op) {
  if (stage < 0 || stage >= static_cast<int>(kMaxTextureStages)) {
    return;
  }
  stage_alpha_op_[stage] = op;
}

void D3D11RendererImpl::SetTextureStageColorArg(int stage, int arg_index, CombineArg arg) {
  if (stage < 0 || stage >= static_cast<int>(kMaxTextureStages)) {
    return;
  }
  if (arg_index == 1) {
    stage_color_arg1_[stage] = arg;
  } else if (arg_index == 2) {
    stage_color_arg2_[stage] = arg;
  }
}

void D3D11RendererImpl::SetTextureStageAlphaArg(int stage, int arg_index, CombineArg arg) {
  if (stage < 0 || stage >= static_cast<int>(kMaxTextureStages)) {
    return;
  }
  if (arg_index == 1) {
    stage_alpha_arg1_[stage] = arg;
  } else if (arg_index == 2) {
    stage_alpha_arg2_[stage] = arg;
  }
}

void D3D11RendererImpl::SetTextureStageUVSource(int stage, TexCoordSource source, TexGenMode gen) {
  if (stage < 0 || stage >= static_cast<int>(kMaxTextureStages)) {
    return;
  }
  stage_uv_source_[stage] = source;
  stage_texgen_mode_[stage] = gen;
}

void D3D11RendererImpl::SetTextureStageTransform(int stage, bool enable) {
  if (stage < 0 || stage >= static_cast<int>(kMaxTextureStages)) {
    return;
  }
  stage_tex_transform_enabled_[stage] = enable;
}

//------------------------------------------------------------------------------
// Native Render State API
//------------------------------------------------------------------------------
// These methods replace D3D9-style SetRenderState calls with typed,
// D3D11-native methods.

float D3D11RendererImpl::ComputeEffectiveAlphaRef(float raw_ref) {
  // Only adjust when alpha test is actually enabled.
  if (alpha_test_data.alpha_test_enabled < 0.5f) {
    alpha_test_data._pad_alpha_args[0] = 1.0f;  // No scaling when alpha test disabled.
    return raw_ref;
  }

  // BC2/DXT3 textures require special handling due to differences in how D3D9 and
  // D3D11 decode and filter 4-bit alpha. Empirical testing shows D3D11 yields
  // higher effective alpha coverage than D3D9 for the same BC2 texture data.
  //
  // This is NOT a generic "integer vs float" alpha test issue - it's specific to
  // BC2's 4-bit alpha expansion and filtering pipeline differences between APIs.
  //
  // Compensation: raise the threshold (+32/255) AND scale down sampled alpha (*0.90)
  // to match D3D9 visual output for punch-through cutouts (fishnets, leaves, etc.).
  const DXGI_FORMAT fmt0 = bound_srv_formats_[0];
  const bool is_bc2 = (fmt0 == DXGI_FORMAT_BC2_UNORM || fmt0 == DXGI_FORMAT_BC2_UNORM_SRGB);
  if (!is_bc2) {
    alpha_test_data._pad_alpha_args[0] = 1.0f;  // No scaling for non-BC2.
    return std::clamp(raw_ref, 0.0f, 1.0f);
  }

  // BC2: bump threshold and scale alpha to compensate for D3D11's higher coverage.
  const float clamped = std::clamp(raw_ref, 0.0f, 1.0f);
  int ref8 = static_cast<int>(std::lround(clamped * 255.0f));
  ref8 = std::clamp(ref8 + 32, 0, 255);        // Raise threshold by ~12.5%.
  alpha_test_data._pad_alpha_args[0] = 0.90f;  // Scale alpha down by 10%.

  return static_cast<float>(ref8) / 255.0f;
}

void D3D11RendererImpl::SetAlphaTest(bool enable, float ref) {
  material_data.alpha_test_enabled = enable ? 1.0f : 0.0f;
  material_data.alpha_ref = ref;
  material_dirty = true;

  // Pixel shaders performing alpha test read from AlphaTestCB, not MaterialCB.
  alpha_test_data.alpha_test_enabled = enable ? 1.0f : 0.0f;
  alpha_test_data.alpha_ref = ComputeEffectiveAlphaRef(ref);
}

void D3D11RendererImpl::SetAlphaBlend(bool enable, BlendFactor src, BlendFactor dst) {
  BlendDesc desc;
  desc.enable = enable;
  desc.src_color = src;
  desc.dst_color = dst;
  desc.src_alpha = BlendFactor::kOne;
  desc.dst_alpha = BlendFactor::kZero;
  desc.color_op = BlendOp::kAdd;
  desc.alpha_op = BlendOp::kAdd;
  SetBlendDesc(desc);
}

void D3D11RendererImpl::SetCullMode(CullMode mode) {
  RasterDesc desc;
  desc.cull = mode;
  desc.fill = FillMode::kSolid;
  // Gothic uses CCW as front-face winding.
  desc.front_ccw = true;
  SetRasterDesc(desc);
}

void D3D11RendererImpl::SetDepthTest(bool enable, bool write_enable, CompareFunc func) {
  DepthDesc desc;
  desc.enable = enable;
  desc.write_enable = write_enable;
  desc.func = func;
  SetDepthDesc(desc);
}

void D3D11RendererImpl::SetTextureFactor(unsigned long argb) {
  const float a = static_cast<float>((argb >> 24) & 0xFF) / 255.0f;
  const float r = static_cast<float>((argb >> 16) & 0xFF) / 255.0f;
  const float g = static_cast<float>((argb >> 8) & 0xFF) / 255.0f;
  const float b = static_cast<float>((argb) & 0xFF) / 255.0f;
  texture_factor_rgba_ = {r, g, b, a};
}

//------------------------------------------------------------------------------
// Native Sampler State API
//------------------------------------------------------------------------------
// These methods replace D3D9-style SetSamplerState calls with typed methods.

void D3D11RendererImpl::SetSamplerAddressing(int stage, AddressMode u, AddressMode v) {
  if (stage < 0 || stage >= static_cast<int>(kMaxTextureStages)) {
    return;
  }

  SamplerDesc desc = stage_sampler_desc_[stage];
  desc.address_u = u;
  desc.address_v = v;
  stage_sampler_desc_[stage] = desc;
  SetSamplerState(stage, GetOrCreateSamplerState(desc));
}

void D3D11RendererImpl::SetSamplerFilter(int stage, FilterMode filter) {
  if (stage < 0 || stage >= static_cast<int>(kMaxTextureStages)) {
    return;
  }

  SamplerDesc desc = stage_sampler_desc_[stage];
  desc.filter = filter;
  desc.max_anisotropy = static_cast<std::uint8_t>(max_anisotropy);
  stage_sampler_desc_[stage] = desc;
  SetSamplerState(stage, GetOrCreateSamplerState(desc));
}

void D3D11RendererImpl::SetSamplerMipFilter(int stage, bool mip_linear) {
  if (stage < 0 || stage >= static_cast<int>(kMaxTextureStages)) {
    return;
  }

  SamplerDesc desc = stage_sampler_desc_[stage];
  desc.mip_linear = mip_linear;
  stage_sampler_desc_[stage] = desc;
  SetSamplerState(stage, GetOrCreateSamplerState(desc));
}

bool D3D11RendererImpl::IsAdditiveBlendForAlphaPoly() const {
  // Heuristic: treat classic additive as (SRC_ALPHA, ONE, ADD).
  // This matches how the engine draws sun/flares and is used by alpha-poly
  // shader paths to pick appropriate behavior.
  if (!current_blend_desc_.enable) {
    return false;
  }
  return current_blend_desc_.src_color == BlendFactor::kSrcAlpha && current_blend_desc_.dst_color == BlendFactor::kOne &&
         current_blend_desc_.color_op == BlendOp::kAdd;
}

void D3D11RendererImpl::RotateFrameResources() {
  // Move to the next frame's buffers
  current_frame_index_ = (current_frame_index_ + 1) % kFramesInFlight;
  auto& fr = frame_resources_[current_frame_index_];

  // Wait for the GPU to finish with this frame's buffers (from kFramesInFlight frames ago)
  if (fr.fence_pending) {
    BOOL query_data = FALSE;
    // This should almost always complete immediately since we're 3 frames ahead.
    // Add timeout to prevent infinite loop if something goes wrong.
    int wait_iterations = 0;
    constexpr int kMaxWaitIterations = 100000;  // ~1 second at Sleep(0)
    while (context->GetData(fr.fence, &query_data, sizeof(query_data), 0) == S_FALSE) {
      Sleep(0);
      if (++wait_iterations > kMaxWaitIterations) {
        SPDLOG_WARN("RotateFrameResources: GPU fence wait timeout for frame {}", current_frame_index_);
        break;  // Don't freeze forever
      }
    }
    fr.fence_pending = false;
  }

  // Reset offsets for this frame - we'll DISCARD on first map
  fr.vb_offset = 0;
  fr.ib_offset = 0;

  // Update convenience pointers
  UpdateCurrentFramePointers();
}

void D3D11RendererImpl::UpdateCurrentFramePointers() {
  auto& fr = frame_resources_[current_frame_index_];
  dynamic_vb = fr.dynamic_vb;
  dynamic_ib = fr.dynamic_ib;
  dynamic_vb_offset = fr.vb_offset;
  dynamic_ib_offset = fr.ib_offset;
}

void* D3D11RendererImpl::AllocateDynamicVB(size_t bytes, UINT& out_offset) {
  auto& fr = frame_resources_[current_frame_index_];

  // Use DISCARD on first allocation of frame, NO_OVERWRITE after that
  D3D11_MAP map_type = (fr.vb_offset == 0) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;

  if (fr.vb_offset + bytes > dynamic_vb_capacity) {
    // Buffer full within a single frame - this shouldn't happen with proper buffer sizing
    SPDLOG_ERROR("Dynamic VB overflow! Frame {} used {} + {} > {} bytes", current_frame_index_, fr.vb_offset, bytes, dynamic_vb_capacity);
    return nullptr;
  }

  D3D11_MAPPED_SUBRESOURCE mapped;
  if (FAILED(TrackedMap(context, static_cast<ID3D11Resource*>(fr.dynamic_vb), 0, map_type, 0, &mapped, "AllocateDynamicVB:dynamic_vb"))) {
    return nullptr;
  }

  out_offset = static_cast<UINT>(fr.vb_offset);
  void* ptr = static_cast<char*>(mapped.pData) + fr.vb_offset;
  fr.vb_offset += bytes;

  TrackedUnmap(context, static_cast<ID3D11Resource*>(fr.dynamic_vb), 0, "AllocateDynamicVB:dynamic_vb");

  // Update cached offset
  dynamic_vb_offset = fr.vb_offset;

  return ptr;
}

uint16_t* D3D11RendererImpl::AllocateDynamicIB(size_t count, UINT& out_offset) {
  auto& fr = frame_resources_[current_frame_index_];

  // Use DISCARD on first allocation of frame, NO_OVERWRITE after that
  D3D11_MAP map_type = (fr.ib_offset == 0) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;

  if (fr.ib_offset + count > dynamic_ib_capacity) {
    // Buffer full within a single frame - this shouldn't happen with proper buffer sizing
    SPDLOG_ERROR("Dynamic IB overflow! Frame {} used {} + {} > {} indices", current_frame_index_, fr.ib_offset, count, dynamic_ib_capacity);
    return nullptr;
  }

  D3D11_MAPPED_SUBRESOURCE mapped;
  if (FAILED(TrackedMap(context, static_cast<ID3D11Resource*>(fr.dynamic_ib), 0, map_type, 0, &mapped, "AllocateDynamicIB:dynamic_ib"))) {
    return nullptr;
  }

  out_offset = static_cast<UINT>(fr.ib_offset);
  uint16_t* ptr = reinterpret_cast<uint16_t*>(static_cast<char*>(mapped.pData) + fr.ib_offset * sizeof(uint16_t));
  fr.ib_offset += count;

  TrackedUnmap(context, static_cast<ID3D11Resource*>(fr.dynamic_ib), 0, "AllocateDynamicIB:dynamic_ib");

  // Update cached offset
  dynamic_ib_offset = fr.ib_offset;

  return ptr;
}

void D3D11RendererImpl::FlushBatch() {
  if (batch_.vertex_count == 0) {
    return;
  }

  auto& fr = frame_resources_[current_frame_index_];

  // Calculate required buffer space
  size_t vb_bytes = batch_.vertex_count * sizeof(VertexRHW);

  // Determine map type: DISCARD on first use of frame, NO_OVERWRITE after
  D3D11_MAP vb_map_type = (fr.vb_offset == 0) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;
  D3D11_MAP ib_map_type = (fr.ib_offset == 0) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;

  // Check for overflow (shouldn't happen with proper buffer sizing)
  if (fr.vb_offset + vb_bytes > dynamic_vb_capacity) {
    SPDLOG_ERROR("FlushBatch VB overflow");
    batch_.Reset();
    return;
  }
  if (fr.ib_offset + batch_.index_count > dynamic_ib_capacity) {
    SPDLOG_ERROR("FlushBatch IB overflow");
    batch_.Reset();
    return;
  }

  // Upload batch vertices
  size_t vb_start_offset = fr.vb_offset;
  D3D11_MAPPED_SUBRESOURCE mapped;
  if (SUCCEEDED(TrackedMap(context, static_cast<ID3D11Resource*>(fr.dynamic_vb), 0, vb_map_type, 0, &mapped, "FlushBatch:dynamic_vb"))) {
    memcpy(static_cast<uint8_t*>(mapped.pData) + vb_start_offset, batch_.vertices, vb_bytes);
    TrackedUnmap(context, static_cast<ID3D11Resource*>(fr.dynamic_vb), 0, "FlushBatch:dynamic_vb");
    fr.vb_offset += vb_bytes;
  }

  // Upload batch indices
  size_t ib_start_offset = fr.ib_offset;
  if (SUCCEEDED(TrackedMap(context, static_cast<ID3D11Resource*>(fr.dynamic_ib), 0, ib_map_type, 0, &mapped, "FlushBatch:dynamic_ib"))) {
    memcpy(static_cast<uint8_t*>(mapped.pData) + ib_start_offset * sizeof(uint16_t), batch_.indices, batch_.index_count * sizeof(uint16_t));
    TrackedUnmap(context, static_cast<ID3D11Resource*>(fr.dynamic_ib), 0, "FlushBatch:dynamic_ib");
    fr.ib_offset += batch_.index_count;
  }

  // Update cached offsets
  dynamic_vb_offset = fr.vb_offset;
  dynamic_ib_offset = fr.ib_offset;

  // Set render state for 2D/UI rendering.
  // Blend state is already set by SetBlendDesc() from the front-end.
  // We only need to configure depth/rasterizer for UI rendering.
  // RHW vertices use ScreenSize in shader, so we need fullscreen viewport.
  // Save and restore the current viewport so 3D rendering isn't affected.
  int saved_vp_x = current_viewport_x, saved_vp_y = current_viewport_y;
  int saved_vp_w = current_viewport_w, saved_vp_h = current_viewport_h;
  SetViewport(0, 0, screen_width, screen_height);
  SetDepthStencilState(dss_no_depth, 0);
  SetRasterizerState(rs_no_cull);

  // Set up pipeline
  SetInputLayout(layout_rhw);
  SetShaders(vs_rhw, ps_rhw);
  ApplyPrebuiltSamplerState(0);
  BindVSConstantBuffer(3, cb_screen);

  UINT stride = sizeof(VertexRHW);
  UINT vb_offset = static_cast<UINT>(vb_start_offset);
  BindVertexBuffer(0, fr.dynamic_vb, stride, vb_offset);
  BindIndexBuffer(fr.dynamic_ib, DXGI_FORMAT_R16_UINT, 0);
  SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // Draw
#ifndef NDEBUG
  {
    const std::string srv0_name = GetDebugObjectName(bound_srvs[0]);
    const std::string srv1_name = GetDebugObjectName(bound_srvs[1]);
    const int s0_white = (bound_srvs[0] == white_srv) ? 1 : 0;
    const int s0_black = (bound_srvs[0] == black_srv) ? 1 : 0;
    const int s1_white = (bound_srvs[1] == white_srv) ? 1 : 0;
    const int s1_black = (bound_srvs[1] == black_srv) ? 1 : 0;
    char rdoc_label_big[512];
    std::snprintf(rdoc_label_big, sizeof(rdoc_label_big),
                  "DrawIndexed(UIBatch) idx=%zu s0=%p white0=%d black0=%d name0='%s' s1=%p white1=%d black1=%d name1='%s'", batch_.index_count,
                  static_cast<const void*>(bound_srvs[0]), s0_white, s0_black, srv0_name.c_str(), static_cast<const void*>(bound_srvs[1]), s1_white,
                  s1_black, srv1_name.c_str());
    ScopedGpuEvent rdoc_event(user_annotation, rdoc_label_big);
    context->DrawIndexed(static_cast<UINT>(batch_.index_count), static_cast<UINT>(ib_start_offset), 0);
  }
#else
  context->DrawIndexed(static_cast<UINT>(batch_.index_count), static_cast<UINT>(ib_start_offset), 0);
#endif

  // Restore the viewport for subsequent 3D rendering (e.g., inventory items)
  SetViewport(saved_vp_x, saved_vp_y, saved_vp_w, saved_vp_h);
  batch_.Reset();
}

void D3D11RendererImpl::DrawTrianglesRHW(const VertexRHW* vertices, int count) {
  if (count <= 0)
    return;

  auto& fr = frame_resources_[current_frame_index_];
  size_t vb_bytes = count * sizeof(VertexRHW);

  // Determine map type: DISCARD on first use of frame, NO_OVERWRITE after
  D3D11_MAP vb_map_type = (fr.vb_offset == 0) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;

  if (fr.vb_offset + vb_bytes > dynamic_vb_capacity) {
    SPDLOG_ERROR("DrawTrianglesRHW VB overflow");
    return;
  }

  // Upload vertices
  size_t vb_start_offset = fr.vb_offset;
  D3D11_MAPPED_SUBRESOURCE mapped;
  if (SUCCEEDED(TrackedMap(context, static_cast<ID3D11Resource*>(fr.dynamic_vb), 0, vb_map_type, 0, &mapped, "DrawTrianglesRHW:dynamic_vb"))) {
    memcpy(static_cast<uint8_t*>(mapped.pData) + vb_start_offset, vertices, vb_bytes);
    TrackedUnmap(context, static_cast<ID3D11Resource*>(fr.dynamic_vb), 0, "DrawTrianglesRHW:dynamic_vb");
    fr.vb_offset += vb_bytes;
    dynamic_vb_offset = fr.vb_offset;
  }

  // Set render state for 2D/UI rendering.
  // Blend state is already set by SetBlendDesc() from the front-end.
  // We only need to configure depth/rasterizer for UI rendering.
  // RHW vertices use ScreenSize in shader, so we need fullscreen viewport.
  // Save and restore the current viewport so 3D rendering isn't affected.
  int saved_vp_x = current_viewport_x, saved_vp_y = current_viewport_y;
  int saved_vp_w = current_viewport_w, saved_vp_h = current_viewport_h;
  SetViewport(0, 0, screen_width, screen_height);
  SetDepthStencilState(dss_no_depth, 0);
  SetRasterizerState(rs_no_cull);

  SetInputLayout(layout_rhw);
  SetShaders(vs_rhw, ps_rhw);
  ApplyPrebuiltSamplerState(0);
  BindVSConstantBuffer(3, cb_screen);

  UINT stride = sizeof(VertexRHW);
  UINT vb_offset = static_cast<UINT>(vb_start_offset);
  BindVertexBuffer(0, fr.dynamic_vb, stride, vb_offset);
  SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  context->Draw(count, 0);

  // Restore the viewport for subsequent 3D rendering
  SetViewport(saved_vp_x, saved_vp_y, saved_vp_w, saved_vp_h);
}

void D3D11RendererImpl::DrawTriangleFan(const VertexRHW* vertices, int count, int z_func) {
  if (count < 3)
    return;

  auto& fr = frame_resources_[current_frame_index_];

  // Convert triangle fan to triangle list
  int tri_count = count - 2;
  int index_count = tri_count * 3;

  size_t vb_bytes = count * sizeof(VertexRHW);
  size_t ib_bytes = index_count * sizeof(uint16_t);

  // Determine map type: DISCARD on first use of frame, NO_OVERWRITE after
  D3D11_MAP vb_map_type = (fr.vb_offset == 0) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;
  D3D11_MAP ib_map_type = (fr.ib_offset == 0) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;

  if (fr.vb_offset + vb_bytes > dynamic_vb_capacity || fr.ib_offset + index_count > dynamic_ib_capacity) {
    SPDLOG_ERROR("DrawTriangleFan buffer overflow");
    return;
  }

  // Upload vertices
  size_t vb_start_offset = fr.vb_offset;
  D3D11_MAPPED_SUBRESOURCE mapped;
  if (SUCCEEDED(TrackedMap(context, static_cast<ID3D11Resource*>(fr.dynamic_vb), 0, vb_map_type, 0, &mapped, "DrawTriangleFan:dynamic_vb"))) {
    memcpy(static_cast<uint8_t*>(mapped.pData) + vb_start_offset, vertices, vb_bytes);
    TrackedUnmap(context, static_cast<ID3D11Resource*>(fr.dynamic_vb), 0, "DrawTriangleFan:dynamic_vb");
    fr.vb_offset += vb_bytes;
  }

  // Generate indices for triangle fan -> triangle list
  uint16_t indices[256 * 3];  // Max fan size
  int idx = 0;
  for (int i = 0; i < tri_count; i++) {
    indices[idx++] = 0;
    indices[idx++] = static_cast<uint16_t>(i + 1);
    indices[idx++] = static_cast<uint16_t>(i + 2);
  }

  // Upload indices
  size_t ib_start_offset = fr.ib_offset;
  if (SUCCEEDED(TrackedMap(context, static_cast<ID3D11Resource*>(fr.dynamic_ib), 0, ib_map_type, 0, &mapped, "DrawTriangleFan:dynamic_ib"))) {
    memcpy(static_cast<uint8_t*>(mapped.pData) + ib_start_offset * sizeof(uint16_t), indices, ib_bytes);
    TrackedUnmap(context, static_cast<ID3D11Resource*>(fr.dynamic_ib), 0, "DrawTriangleFan:dynamic_ib");
    fr.ib_offset += index_count;
  }

  // Update cached offsets
  dynamic_vb_offset = fr.vb_offset;
  dynamic_ib_offset = fr.ib_offset;

  // Set render state for alpha poly rendering.
  // Blend state is already set by the front-end via SetBlendDesc() when
  // SetAlphaBlendFuncImmed() calls ApplyRenderState(D3DRS_SRCBLEND/DESTBLEND).
  // We only need to configure depth/rasterizer state here.

  // Select depth stencil state based on z_func
  switch (z_func) {
    case 0:  // ALWAYS
    case 1:  // NEVER
      SetDepthStencilState(dss_no_depth, 0);
      break;
    case 2:  // LESS
      SetDepthStencilState(dss_alpha_less, 0);
      break;
    case 3:  // LESS_EQUAL
    default:
      SetDepthStencilState(dss_alpha, 0);
      break;
  }

  SetRasterizerState(rs_no_cull);
  // RHW vertices use ScreenSize in shader, so we need fullscreen viewport.
  // Save and restore the current viewport so 3D rendering isn't affected.
  int saved_vp_x = current_viewport_x, saved_vp_y = current_viewport_y;
  int saved_vp_w = current_viewport_w, saved_vp_h = current_viewport_h;
  SetViewport(0, 0, screen_width, screen_height);

  SetInputLayout(layout_rhw);
  if (bound_srvs[0] != nullptr) {
    // Update/bind stage-state CB for alpha polys (COLOROP/ALPHAOP).
    // Alpha test for alpha polys is driven by the front-end (SetAlphaBlendFuncImmed)
    // via impl_->SetAlphaTest. Mirror that state into the AlphaTestCB so the
    // alpha-poly shader can emulate fixed-function alpha testing.
    alpha_test_data.alpha_test_enabled = material_data.alpha_test_enabled;
    alpha_test_data.alpha_ref = ComputeEffectiveAlphaRef(material_data.alpha_ref);
    alpha_test_data.alpha_blend_func = IsAdditiveBlendForAlphaPoly() ? 1.0f : 0.0f;
    alpha_test_data.uv_source0 = PackUvSourceAndTexGen(stage_uv_source_[0], stage_texgen_mode_[0]);
    alpha_test_data.uv_source1 = PackUvSourceAndTexGen(stage_uv_source_[1], stage_texgen_mode_[1]);
    alpha_test_data.color_op0 = CombineOpToFloat(stage_color_op_[0]);
    alpha_test_data.color_op1 = CombineOpToFloat(stage_color_op_[1]);
    // AlphaOp is CPU-normalized for the alpha-poly shader paths.
    const CombineOp alpha_op = stage_alpha_op_[0];
    if (alpha_op == CombineOp::kSelectArg1) {
      alpha_test_data.alpha_op = 1.0f;
    } else if (alpha_op == CombineOp::kSelectArg2) {
      alpha_test_data.alpha_op = 2.0f;
    } else {
      alpha_test_data.alpha_op = 0.0f;
    }
    D3D11_MAPPED_SUBRESOURCE mapped_at;
    UpdateAlphaTestCB();
    BindPSConstantBuffer(4, cb_alpha_test);

    SetShaders(vs_rhw, ps_rhw_alpha);
  } else {
    SetShaders(vs_rhw, ps_vertex_color);
  }
  ApplyPrebuiltSamplerState(0);
  BindVSConstantBuffer(3, cb_screen);

  UINT stride = sizeof(VertexRHW);
  UINT vb_offset = static_cast<UINT>(vb_start_offset);
  BindVertexBuffer(0, fr.dynamic_vb, stride, vb_offset);
  BindIndexBuffer(fr.dynamic_ib, DXGI_FORMAT_R16_UINT, 0);
  SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
#ifndef NDEBUG
  {
    const std::string srv0_name = GetDebugObjectName(bound_srvs[0]);
    const std::string srv1_name = GetDebugObjectName(bound_srvs[1]);
    const int s0_white = (bound_srvs[0] == white_srv) ? 1 : 0;
    const int s0_black = (bound_srvs[0] == black_srv) ? 1 : 0;
    const int s1_white = (bound_srvs[1] == white_srv) ? 1 : 0;
    const int s1_black = (bound_srvs[1] == black_srv) ? 1 : 0;
    char rdoc_label_big[512];
    std::snprintf(rdoc_label_big, sizeof(rdoc_label_big),
                  "DrawIndexed(AlphaFan) idx=%d zf=%d s0=%p white0=%d black0=%d name0='%s' s1=%p white1=%d black1=%d name1='%s'", index_count, z_func,
                  static_cast<const void*>(bound_srvs[0]), s0_white, s0_black, srv0_name.c_str(), static_cast<const void*>(bound_srvs[1]), s1_white,
                  s1_black, srv1_name.c_str());
    ScopedGpuEvent rdoc_event(user_annotation, rdoc_label_big);
    context->DrawIndexed(index_count, static_cast<UINT>(ib_start_offset), 0);
  }
#else
  context->DrawIndexed(index_count, static_cast<UINT>(ib_start_offset), 0);
#endif
  // Restore the viewport for subsequent 3D rendering
  SetViewport(saved_vp_x, saved_vp_y, saved_vp_w, saved_vp_h);
}

void D3D11RendererImpl::DrawBatchRHW(const VertexRHW* vertices, int vertex_count, const uint16_t* indices, int index_count, int z_func) {
  if (vertex_count <= 0 || index_count <= 0)
    return;

  auto& fr = frame_resources_[current_frame_index_];

  size_t vb_bytes = vertex_count * sizeof(VertexRHW);
  size_t ib_bytes = index_count * sizeof(uint16_t);

  // Determine map type: DISCARD on first use of frame, NO_OVERWRITE after
  D3D11_MAP vb_map_type = (fr.vb_offset == 0) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;
  D3D11_MAP ib_map_type = (fr.ib_offset == 0) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;

  if (fr.vb_offset + vb_bytes > dynamic_vb_capacity || fr.ib_offset + index_count > dynamic_ib_capacity) {
    SPDLOG_ERROR("DrawBatchRHW buffer overflow");
    return;
  }

  // Upload vertices
  size_t vb_start_offset = fr.vb_offset;
  D3D11_MAPPED_SUBRESOURCE mapped;
  if (SUCCEEDED(TrackedMap(context, static_cast<ID3D11Resource*>(fr.dynamic_vb), 0, vb_map_type, 0, &mapped, "DrawBatchRHW:dynamic_vb"))) {
    memcpy(static_cast<uint8_t*>(mapped.pData) + vb_start_offset, vertices, vb_bytes);
    TrackedUnmap(context, static_cast<ID3D11Resource*>(fr.dynamic_vb), 0, "DrawBatchRHW:dynamic_vb");
    fr.vb_offset += vb_bytes;
  }

  // Upload indices
  size_t ib_start_offset = fr.ib_offset;
  if (SUCCEEDED(TrackedMap(context, static_cast<ID3D11Resource*>(fr.dynamic_ib), 0, ib_map_type, 0, &mapped, "DrawBatchRHW:dynamic_ib"))) {
    memcpy(static_cast<uint8_t*>(mapped.pData) + ib_start_offset * sizeof(uint16_t), indices, ib_bytes);
    TrackedUnmap(context, static_cast<ID3D11Resource*>(fr.dynamic_ib), 0, "DrawBatchRHW:dynamic_ib");
    fr.ib_offset += index_count;
  }

  // Update cached offsets
  dynamic_vb_offset = fr.vb_offset;
  dynamic_ib_offset = fr.ib_offset;

  // Select depth stencil state based on z_func
  switch (z_func) {
    case 0:  // ALWAYS
    case 1:  // NEVER
      SetDepthStencilState(dss_no_depth, 0);
      break;
    case 2:  // LESS
      SetDepthStencilState(dss_alpha_less, 0);
      break;
    case 3:  // LESS_EQUAL
    default:
      SetDepthStencilState(dss_alpha, 0);
      break;
  }

  SetRasterizerState(rs_no_cull);
  // RHW vertices use ScreenSize in shader, so we need fullscreen viewport.
  // Save and restore the current viewport so 3D rendering isn't affected.
  int saved_vp_x = current_viewport_x, saved_vp_y = current_viewport_y;
  int saved_vp_w = current_viewport_w, saved_vp_h = current_viewport_h;
  SetViewport(0, 0, screen_width, screen_height);

  SetInputLayout(layout_rhw);
  if (bound_srvs[0] != nullptr) {
    alpha_test_data.alpha_test_enabled = material_data.alpha_test_enabled;
    alpha_test_data.alpha_ref = ComputeEffectiveAlphaRef(material_data.alpha_ref);
    alpha_test_data.alpha_blend_func = IsAdditiveBlendForAlphaPoly() ? 1.0f : 0.0f;
    alpha_test_data.uv_source0 = PackUvSourceAndTexGen(stage_uv_source_[0], stage_texgen_mode_[0]);
    alpha_test_data.uv_source1 = PackUvSourceAndTexGen(stage_uv_source_[1], stage_texgen_mode_[1]);
    alpha_test_data.color_op0 = CombineOpToFloat(stage_color_op_[0]);
    alpha_test_data.color_op1 = CombineOpToFloat(stage_color_op_[1]);
    const CombineOp alpha_op = stage_alpha_op_[0];
    if (alpha_op == CombineOp::kSelectArg1) {
      alpha_test_data.alpha_op = 1.0f;
    } else if (alpha_op == CombineOp::kSelectArg2) {
      alpha_test_data.alpha_op = 2.0f;
    } else {
      alpha_test_data.alpha_op = 0.0f;
    }
    UpdateAlphaTestCB();
    BindPSConstantBuffer(4, cb_alpha_test);

    SetShaders(vs_rhw, ps_rhw_alpha);
  } else {
    SetShaders(vs_rhw, ps_vertex_color);
  }
  ApplyPrebuiltSamplerState(0);
  BindVSConstantBuffer(3, cb_screen);

  UINT stride = sizeof(VertexRHW);
  UINT vb_offset = static_cast<UINT>(vb_start_offset);
  BindVertexBuffer(0, fr.dynamic_vb, stride, vb_offset);
  BindIndexBuffer(fr.dynamic_ib, DXGI_FORMAT_R16_UINT, 0);
  SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
#ifndef NDEBUG
  {
    const std::string srv0_name = GetDebugObjectName(bound_srvs[0]);
    const std::string srv1_name = GetDebugObjectName(bound_srvs[1]);
    const int s0_white = (bound_srvs[0] == white_srv) ? 1 : 0;
    const int s0_black = (bound_srvs[0] == black_srv) ? 1 : 0;
    const int s1_white = (bound_srvs[1] == white_srv) ? 1 : 0;
    const int s1_black = (bound_srvs[1] == black_srv) ? 1 : 0;
    char rdoc_label_big[512];
    std::snprintf(rdoc_label_big, sizeof(rdoc_label_big),
                  "DrawIndexed(AlphaBatchRHW) idx=%d zf=%d s0=%p white0=%d black0=%d name0='%s' s1=%p white1=%d black1=%d name1='%s'", index_count,
                  z_func, static_cast<const void*>(bound_srvs[0]), s0_white, s0_black, srv0_name.c_str(), static_cast<const void*>(bound_srvs[1]),
                  s1_white, s1_black, srv1_name.c_str());
    ScopedGpuEvent rdoc_event(user_annotation, rdoc_label_big);
    context->DrawIndexed(index_count, static_cast<UINT>(ib_start_offset), 0);
  }
#else
  context->DrawIndexed(index_count, static_cast<UINT>(ib_start_offset), 0);
#endif
  // Restore the viewport for subsequent 3D rendering
  SetViewport(saved_vp_x, saved_vp_y, saved_vp_w, saved_vp_h);
}

void D3D11RendererImpl::DrawTriangles(const Vertex3D* vertices, int count) {
  if (count <= 0)
    return;

  auto& fr = frame_resources_[current_frame_index_];
  size_t vb_bytes = count * sizeof(Vertex3D);

  D3D11_MAP vb_map_type = (fr.vb_offset == 0) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;

  if (fr.vb_offset + vb_bytes > dynamic_vb_capacity) {
    SPDLOG_ERROR("DrawTriangles VB overflow");
    return;
  }

  // Upload vertices
  size_t vb_start_offset = fr.vb_offset;
  D3D11_MAPPED_SUBRESOURCE mapped;
  if (SUCCEEDED(TrackedMap(context, static_cast<ID3D11Resource*>(fr.dynamic_vb), 0, vb_map_type, 0, &mapped, "DrawTriangles:dynamic_vb"))) {
    memcpy(static_cast<uint8_t*>(mapped.pData) + vb_start_offset, vertices, vb_bytes);
    TrackedUnmap(context, static_cast<ID3D11Resource*>(fr.dynamic_vb), 0, "DrawTriangles:dynamic_vb");
    fr.vb_offset += vb_bytes;
    dynamic_vb_offset = fr.vb_offset;
  }

  SetInputLayout(layout_3d);
  SetShaders(vs_basic, ps_basic);

  UpdateTransformCB();
  BindVSConstantBuffer(0, cb_transform);

  UpdateFogCB();
  BindPSConstantBuffer(2, cb_fog);

  alpha_test_data.alpha_test_enabled = 0.0f;
  D3D11_MAPPED_SUBRESOURCE mapped_at;
  UpdateAlphaTestCB();
  BindPSConstantBuffer(4, cb_alpha_test);
  BindVSConstantBuffer(4, cb_alpha_test);

  UINT stride = sizeof(Vertex3D);
  UINT vb_offset = static_cast<UINT>(vb_start_offset);
  BindVertexBuffer(0, fr.dynamic_vb, stride, vb_offset);
  SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  context->Draw(count, 0);
}

void D3D11RendererImpl::DrawTriangleFan2(const VertexRHW2* vertices, int count) {
  (void)vertices;
  (void)count;
}

void D3D11RendererImpl::DrawLine(float x1, float y1, float x2, float y2, unsigned long color) {
  VertexRHW verts[2] = {
      {x1, y1, 0.0f, 1.0f, color, 0.0f, 0.0f},
      {x2, y2, 0.0f, 1.0f, color, 0.0f, 0.0f},
  };

  auto& fr = frame_resources_[current_frame_index_];
  size_t vb_bytes = sizeof(verts);

  D3D11_MAP vb_map_type = (fr.vb_offset == 0) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;

  if (fr.vb_offset + vb_bytes > dynamic_vb_capacity) {
    SPDLOG_ERROR("DrawLine VB overflow");
    return;
  }

  // Upload vertices
  size_t vb_start_offset = fr.vb_offset;
  D3D11_MAPPED_SUBRESOURCE mapped;
  if (SUCCEEDED(TrackedMap(context, static_cast<ID3D11Resource*>(fr.dynamic_vb), 0, vb_map_type, 0, &mapped, "DrawLine:dynamic_vb"))) {
    memcpy(static_cast<uint8_t*>(mapped.pData) + vb_start_offset, verts, vb_bytes);
    TrackedUnmap(context, static_cast<ID3D11Resource*>(fr.dynamic_vb), 0, "DrawLine:dynamic_vb");
    fr.vb_offset += vb_bytes;
    dynamic_vb_offset = fr.vb_offset;
  }

  // Set render state for 2D line rendering
  SetBlendState(bs_alpha_blend);
  SetDepthStencilState(dss_no_depth, 0);
  SetRasterizerState(rs_no_cull);

  SetInputLayout(layout_rhw);
  SetShaders(vs_rhw, ps_vertex_color);
  BindVSConstantBuffer(3, cb_screen);

  UINT stride = sizeof(VertexRHW);
  UINT vb_offset_uint = static_cast<UINT>(vb_start_offset);
  BindVertexBuffer(0, fr.dynamic_vb, stride, vb_offset_uint);
  SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
  context->Draw(2, 0);
}

bool D3D11RendererImpl::DrawVertexBuffer(ID3D11Buffer* vertex_buffer, UINT stride, D3D11_PRIMITIVE_TOPOLOGY topology, UINT start_vertex,
                                         UINT vertex_count, const unsigned short* indices, UINT index_count, int vertex_type, int alpha_blend_func) {
  if (!vertex_buffer)
    return false;

  // Select input layout and shaders based on vertex type:
  // 0 = UT_UL (Untransformed, Unlit) - 3D pipeline with normals (stride=36)
  // 1 = UT_L (Untransformed, Lit) - 3D pipeline, pre-lit (stride=24)
  // 2 = T_L (Transformed, Lit) - screen-space RHW pipeline
  // alpha_blend_func: 0=MAT_DEFAULT, 1=NONE, 2=BLEND, 3=ADD, 4=SUB, 5=MUL, 6=MUL2, 7=TEST, 8=BLEND_TEST
  switch (vertex_type) {
    case 0:  // UT_UL - Untransformed, Unlit (with normals)
      if (!lighting_enabled_) {
        if (stride >= 36 && layout_3d_color && vs_basic_color_unlit) {
          SetInputLayout(layout_3d_color);
          SetShaders(vs_basic_color_unlit, ps_basic);
        } else {
          SetInputLayout(layout_3d);
          SetShaders(vs_basic_unlit ? vs_basic_unlit : vs_basic, ps_basic);
        }
      } else {
        if (stride >= 36 && layout_3d_color && vs_basic_color) {
          SetInputLayout(layout_3d_color);
          SetShaders(vs_basic_color, ps_basic);
        } else {
          SetInputLayout(layout_3d);
          SetShaders(vs_basic, ps_basic);
        }
      }
      // Apply blend state based on alpha_blend_func
      if (alpha_blend_func == 2) {  // BLEND - no Z-write
        SetBlendState(bs_alpha_blend);
        SetDepthStencilState(dss_alpha, 0);
        SetRasterizerState(rs_no_cull);
      } else if (alpha_blend_func == 3) {  // ADD - no Z-write
        SetBlendState(bs_alpha_add);
        SetDepthStencilState(dss_alpha, 0);
        SetRasterizerState(rs_no_cull);
      } else if (alpha_blend_func == 5) {  // MUL - no Z-write
        SetBlendState(bs_alpha_mul);
        SetDepthStencilState(dss_alpha, 0);
        SetRasterizerState(rs_no_cull);
      } else if (alpha_blend_func == 6) {  // MUL2 - no Z-write
        SetBlendState(bs_alpha_mul2);
        SetDepthStencilState(dss_alpha, 0);
        SetRasterizerState(rs_no_cull);
      } else if (alpha_blend_func == 7) {  // TEST - write Z!
        // Alpha-tested geometry is effectively opaque where it passes the test
        SetBlendState(bs_opaque);
        SetDepthStencilState(dss_default, 0);
        // Respect the engine's cull mode (set via SetRasterDesc) and bind it explicitly.
        // IMPORTANT: do NOT use requested_rasterizer_state_ here because it may reflect
        // our own overrides from unrelated draws (e.g. forcing rs_no_cull for blends),
        // which can cause foliage to alternate between culled/unculled and appear to flicker.
        if (has_engine_raster_desc_) {
          SetRasterizerState(GetOrCreateRasterizerState(engine_raster_desc_));
        } else {
          SetRasterizerState(rs_default);
        }
      } else if (alpha_blend_func == 8) {  // BLEND_TEST - alpha test + alpha blend, still write Z
        SetBlendState(bs_alpha_blend);
        SetDepthStencilState(dss_default, 0);
        // Same rationale as TEST above.
        if (has_engine_raster_desc_) {
          SetRasterizerState(GetOrCreateRasterizerState(engine_raster_desc_));
        } else {
          SetRasterizerState(rs_default);
        }
      } else {
        SetBlendState(bs_opaque);
        SetDepthStencilState(dss_default, 0);
        SetRasterizerState(rs_default);
      }
      ApplyPrebuiltSamplerState(0);
      break;

    case 1: {  // UT_L - Untransformed, Lit (pre-lit)
      // Select layout based on stride:
      // stride=24: pos+color+uv (no normal) - use layout_lit
      // stride=32: pos+color+2xuv - use layout_lit_2uv
      // stride=36: pos+normal+color+uv - use layout_lit_normal_singleuv
      // stride=44: pos+normal+color+2xuv (water format) - use layout_lit_normal
      if (stride >= 44) {
        SetInputLayout(layout_lit_normal);
        // If lightmap is active, use lightmap shader
        // Otherwise, if ADD blend and has 2 UVs, use dual-add for water
        if (has_lightmap_) {
          SetShaders(vs_lit_normal, ps_lightmap);
        } else if (alpha_blend_func == 3) {
          // Water with environment map
          SetShaders(vs_lit_normal, ps_dual_add);
        } else {
          SetShaders(vs_lit_normal, ps_basic);
        }
      } else if (stride >= 32 && stride < 36 && layout_lit_2uv && vs_lit_2uv) {
        SetInputLayout(layout_lit_2uv);
        if (has_lightmap_) {
          SetShaders(vs_lit_2uv, ps_lightmap);
        } else {
          SetShaders(vs_lit_2uv, ps_basic);
        }
      } else {
        if (stride >= 36 && layout_lit_normal_singleuv && vs_lit_normal_singleuv) {
          SetInputLayout(layout_lit_normal_singleuv);
          SetShaders(vs_lit_normal_singleuv, ps_basic);
        } else {
          SetInputLayout(layout_lit);
          SetShaders(vs_lit, ps_basic);
        }
      }

      // Apply blend state based on alpha_blend_func
      // alpha_blend_func: 0=MAT_DEFAULT, 1=NONE, 2=BLEND, 3=ADD, 4=SUB, 5=MUL, 6=MUL2, 7=TEST, 8=BLEND_TEST
      if (alpha_blend_func == 2) {  // BLEND - no Z-write
        SetBlendState(bs_alpha_blend);
        SetDepthStencilState(dss_alpha, 0);
        SetRasterizerState(rs_no_cull);
      } else if (alpha_blend_func == 3) {  // ADD - no Z-write
        SetBlendState(bs_alpha_add);
        SetDepthStencilState(dss_alpha, 0);
        SetRasterizerState(rs_no_cull);
      } else if (alpha_blend_func == 5) {  // MUL - no Z-write
        SetBlendState(bs_alpha_mul);
        SetDepthStencilState(dss_alpha, 0);
        SetRasterizerState(rs_no_cull);
      } else if (alpha_blend_func == 6) {  // MUL2 - no Z-write
        SetBlendState(bs_alpha_mul2);
        SetDepthStencilState(dss_alpha, 0);
        SetRasterizerState(rs_no_cull);
      } else if (alpha_blend_func == 7) {  // TEST - write Z!
        SetBlendState(bs_opaque);
        SetDepthStencilState(dss_default, 0);
        SetRasterizerState(rs_no_cull);
      } else if (alpha_blend_func == 8) {  // BLEND_TEST - alpha test + alpha blend, still write Z
        SetBlendState(bs_alpha_blend);
        SetDepthStencilState(dss_default, 0);
        SetRasterizerState(rs_no_cull);
      } else {
        SetBlendState(bs_opaque);
        SetDepthStencilState(dss_default, 0);
        SetRasterizerState(rs_default);
      }
      // Bind samplers for the active PS.
      // Important: dual-texture shaders use s0 for tex0 and s1 for tex1.
      // NOTE: We must respect ZenGin/D3D9 sampler states; forcing linear+wrap here
      // causes visible water/env-map shimmer when rotating the camera.

      ApplyPrebuiltSamplerState(0);
      if (stride >= 44 || (stride >= 32 && stride < 36)) {
        if (current_ps == ps_lightmap) {
          // Lightmaps should not wrap; wrapping causes obvious indoor artifacts.
          SetSamplerState(1, ss_linear_clamp);
        } else if (current_ps == ps_dual_add) {
          // Water/env-map: respect sampler state from ZenGin (wrap/clamp/filter/mips).
          ApplyPrebuiltSamplerState(1);
        }
      }
      break;
    }

    case 2:  // T_L - Transformed, Lit (already in screen space)
    default:
      // Screen-space vertices need RHW pipeline
      SetInputLayout(layout_rhw);
      SetShaders(vs_rhw, ps_rhw);
      SetBlendState(bs_alpha_blend);
      SetDepthStencilState(dss_no_depth, 0);
      SetRasterizerState(rs_no_cull);
      ApplyPrebuiltSamplerState(0);  // Respect current filter mode
      break;
  }

#ifndef NDEBUG
  // RenderDoc/PIX marker for this draw.
  const char* ps_name_short = "ps?";
  if (current_ps == ps_basic)
    ps_name_short = "ps_basic";
  else if (current_ps == ps_lightmap)
    ps_name_short = "ps_lightmap";
  else if (current_ps == ps_dual_add)
    ps_name_short = "ps_dual_add";
  else if (current_ps == ps_rhw)
    ps_name_short = "ps_rhw";
  else if (current_ps == ps_rhw_alpha)
    ps_name_short = "ps_rhw_alpha";
  else if (current_ps == ps_vertex_color)
    ps_name_short = "ps_vertex_color";

  // Include SRV debug names (if present) so RenderDoc captures are self-describing.
  const std::string srv0_name = GetDebugObjectName(bound_srvs[0]);
  const std::string srv1_name = GetDebugObjectName(bound_srvs[1]);
  const int s0_white = (bound_srvs[0] == white_srv) ? 1 : 0;
  const int s1_white = (bound_srvs[1] == white_srv) ? 1 : 0;

  char rdoc_label_big[1024];
  // Include viewport and projection diagonal for debugging viewport-related issues
  DirectX::XMFLOAT4X4 proj_float;
  DirectX::XMStoreFloat4x4(&proj_float, transform_data.projection);
  std::snprintf(rdoc_label_big, sizeof(rdoc_label_big),
                "DrawVB vtype=%d stride=%u PS=%s has_lm=%d a=%d uv0=%d gen0=%d uv1=%d gen1=%d "
                "s0=%p white0=%d name0='%s' s1=%p white1=%d name1='%s' "
                "VP=(%d,%d,%dx%d) PROJ=(%.2f,%.2f,%.2f)",
                vertex_type, stride, ps_name_short, has_lightmap_ ? 1 : 0, alpha_blend_func, static_cast<int>(stage_uv_source_[0]),
                static_cast<int>(stage_texgen_mode_[0]), static_cast<int>(stage_uv_source_[1]), static_cast<int>(stage_texgen_mode_[1]),
                static_cast<const void*>(bound_srvs[0]), s0_white, srv0_name.c_str(), static_cast<const void*>(bound_srvs[1]), s1_white,
                srv1_name.c_str(), current_viewport_x, current_viewport_y, current_viewport_w, current_viewport_h, proj_float._11, proj_float._22,
                proj_float._33);
  ScopedGpuEvent rdoc_event(user_annotation, rdoc_label_big);
#endif

  // Update transform CB for world rendering
  UpdateTransformCB();
  BindVSConstantBuffer(0, cb_transform);

  // Update and bind fog CB
  UpdateFogCB();
  BindPSConstantBuffer(2, cb_fog);
  BindVSConstantBuffer(2, cb_fog);

  // Update and bind material CB (VS uses MatDiffuse, PS uses TextureEnabled/AlphaTestEnabled)
  UpdateMaterialCB();
  BindVSConstantBuffer(1, cb_material);
  BindPSConstantBuffer(1, cb_material);

  // Update alpha test CB based on blend function
  // TEST (7) and BLEND_TEST (8) use alpha testing
  // Also pass an additive-blend flag to shader for correct fog behavior (true additive needs black fog)
  // And tex coord index so shader knows which UV set to sample
  // And alpha op so shader knows how to combine alpha (MODULATE vs SELECTARG2)
  // Shader only needs a boolean: should fog fade to black (true additive) or to fog color.
  // Use the draw's blend function selector (which controls the actual D3D11 blend state)
  // instead of tracked SRC/DST blend state, which can be stale.
  alpha_test_data.alpha_blend_func = (alpha_blend_func == 3) ? 1.0f : 0.0f;

  // Pass both stage UV indices so shaders can match D3D9 fixed-function behavior.
  alpha_test_data.uv_source0 = PackUvSourceAndTexGen(stage_uv_source_[0], stage_texgen_mode_[0]);
  alpha_test_data.uv_source1 = PackUvSourceAndTexGen(stage_uv_source_[1], stage_texgen_mode_[1]);

  // Pass both stage COLOROP values so shaders can emulate D3D9 fixed-function combines.
  alpha_test_data.color_op0 = CombineOpToFloat(stage_color_op_[0]);
  alpha_test_data.color_op1 = CombineOpToFloat(stage_color_op_[1]);

  // Semantic base-stage hints (only meaningful for the lightmap shader).
  alpha_test_data.base_rgb_gen = static_cast<float>(semantic_base_rgb_gen_);
  alpha_test_data.base_factor_r = semantic_base_factor_rgb_.x;
  alpha_test_data.base_factor_g = semantic_base_factor_rgb_.y;
  alpha_test_data.base_factor_b = semantic_base_factor_rgb_.z;

  // Semantic 2-stage fixed-function emulation (only meaningful for the lightmap shader).
  alpha_test_data.sem_ffp_enabled = semantic_ffp_enabled_ ? 1.0f : 0.0f;
  alpha_test_data.sem_s0_colorop = static_cast<float>(semantic_s0_colorop_);
  alpha_test_data.sem_s0_colorarg1 = static_cast<float>(semantic_s0_colorarg1_);
  alpha_test_data.sem_s0_colorarg2 = static_cast<float>(semantic_s0_colorarg2_);
  alpha_test_data.sem_s1_colorop = static_cast<float>(semantic_s1_colorop_);
  alpha_test_data.sem_s1_colorarg1 = static_cast<float>(semantic_s1_colorarg1_);
  alpha_test_data.sem_s1_colorarg2 = static_cast<float>(semantic_s1_colorarg2_);

  // D3DRS_TEXTUREFACTOR expanded to RGBA.
  alpha_test_data.tex_factor_r = texture_factor_rgba_.x;
  alpha_test_data.tex_factor_g = texture_factor_rgba_.y;
  alpha_test_data.tex_factor_b = texture_factor_rgba_.z;
  alpha_test_data.tex_factor_a = texture_factor_rgba_.w;

  // Stage0 ALPHAARG1/2 (zRND_TA_*).
  alpha_test_data.alpha_arg1 = CombineArgToFloat(stage_alpha_arg1_[0]);
  alpha_test_data.alpha_arg2 = CombineArgToFloat(stage_alpha_arg2_[0]);

  // Texture transform flags + matrices (used by water/env-map and UV scrolling).
  // Non-zero flag means transform is enabled.
  // OPTIMIZATION: Only copy matrices when transforms are actually enabled.
  // When disabled, use identity matrix so memcmp succeeds more often.
  const bool tex0_enabled = stage_tex_transform_enabled_[0];
  const bool tex1_enabled = stage_tex_transform_enabled_[1];
  alpha_test_data.tex_transform_enabled0 = tex0_enabled ? 1.0f : 0.0f;
  alpha_test_data.tex_transform_enabled1 = tex1_enabled ? 1.0f : 0.0f;

  static const DirectX::XMMATRIX kIdentity = DirectX::XMMatrixIdentity();
  const auto SelectTexTransform = [&](int stage) -> DirectX::XMMATRIX {
    // Use raw engine-provided matrix for texgen modes, transposed matrix otherwise.
    if (stage_texgen_mode_[stage] != TexGenMode::kNone) {
      return tex_transform_matrix_raw_[stage];
    }
    return tex_transform_matrix_[stage];
  };
  alpha_test_data.tex_transform0 = tex0_enabled ? SelectTexTransform(0) : kIdentity;
  alpha_test_data.tex_transform1 = tex1_enabled ? SelectTexTransform(1) : kIdentity;

  // Convert Gothic's zRND_TOP_ to shader-friendly values:
  // zRND_TOP_SELECTARG1 = 1 (texture alpha only)
  // zRND_TOP_SELECTARG2 = 2 (vertex alpha only)
  // zRND_TOP_MODULATE = 3 (texture * vertex)
  // Shader expects: 0=MODULATE, 1=SELECTARG1, 2=SELECTARG2
  const CombineOp alpha_op = stage_alpha_op_[0];
  if (alpha_op == CombineOp::kSelectArg1) {
    alpha_test_data.alpha_op = 1.0f;
  } else if (alpha_op == CombineOp::kSelectArg2) {
    alpha_test_data.alpha_op = 2.0f;
  } else {
    alpha_test_data.alpha_op = 0.0f;
  }

  D3D11_MAPPED_SUBRESOURCE mapped_at;
  UpdateAlphaTestCB();
  BindPSConstantBuffer(4, cb_alpha_test);
  BindVSConstantBuffer(4, cb_alpha_test);

  // Update and bind light buffer for lit geometry
  UpdateLightBuffer();
  BindVSConstantBuffer(5, cb_light);

  // Fix for ADD blend with white fallback texture:
  // For additive blending, adding white (1,1,1,1) creates bright artifacts.
  // If we're using the white fallback and ADD blend, swap to black fallback.
  // Black (0,0,0,0) is the identity for addition - it adds nothing.
  if (alpha_blend_func == 3 && bound_srvs[0] == white_srv) {
    SetTexture(0, black_srv);
  }

  UINT offset = 0;
  BindVertexBuffer(0, vertex_buffer, stride, offset);
  SetPrimitiveTopology(topology);

  if (indices && index_count > 0) {
    auto& fr = frame_resources_[current_frame_index_];
    size_t ib_bytes = index_count * sizeof(uint16_t);

    D3D11_MAP ib_map_type = (fr.ib_offset == 0) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;

    if (fr.ib_offset + index_count > dynamic_ib_capacity) {
      SPDLOG_ERROR("DrawVertexBuffer IB overflow");
      return false;
    }

    // Upload indices
    size_t ib_start_offset = fr.ib_offset;
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(TrackedMap(context, static_cast<ID3D11Resource*>(fr.dynamic_ib), 0, ib_map_type, 0, &mapped, "DrawVertexBuffer:dynamic_ib"))) {
      memcpy(static_cast<uint8_t*>(mapped.pData) + ib_start_offset * sizeof(uint16_t), indices, ib_bytes);
      TrackedUnmap(context, static_cast<ID3D11Resource*>(fr.dynamic_ib), 0, "DrawVertexBuffer:dynamic_ib");
      fr.ib_offset += index_count;
      dynamic_ib_offset = fr.ib_offset;
    }
    BindIndexBuffer(fr.dynamic_ib, DXGI_FORMAT_R16_UINT, 0);
    context->DrawIndexed(index_count, static_cast<UINT>(ib_start_offset), start_vertex);
  } else {
    context->Draw(vertex_count, start_vertex);
  }

  return true;
}

bool D3D11RendererImpl::DrawAlphaBatch(const VertexRHW* vertices, size_t vertex_count, const uint16_t* indices, size_t index_count, bool has_texture,
                                       int z_func) {
  if (!vertices || vertex_count == 0)
    return false;

  auto& fr = frame_resources_[current_frame_index_];

  size_t vb_bytes = vertex_count * sizeof(VertexRHW);
  size_t ib_count = (indices && index_count > 0) ? index_count : 0;

  // Determine map type: DISCARD on first use of frame, NO_OVERWRITE after
  D3D11_MAP vb_map_type = (fr.vb_offset == 0) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;
  D3D11_MAP ib_map_type = (fr.ib_offset == 0) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;

  if (fr.vb_offset + vb_bytes > dynamic_vb_capacity) {
    SPDLOG_ERROR("DrawAlphaBatch VB overflow");
    return false;
  }
  if (ib_count > 0 && fr.ib_offset + ib_count > dynamic_ib_capacity) {
    SPDLOG_ERROR("DrawAlphaBatch IB overflow");
    return false;
  }

  // Upload vertices
  size_t vb_start_offset = fr.vb_offset;
  D3D11_MAPPED_SUBRESOURCE mapped;
  if (SUCCEEDED(TrackedMap(context, static_cast<ID3D11Resource*>(fr.dynamic_vb), 0, vb_map_type, 0, &mapped, "DrawAlphaBatch:dynamic_vb"))) {
    memcpy(static_cast<uint8_t*>(mapped.pData) + vb_start_offset, vertices, vb_bytes);
    TrackedUnmap(context, static_cast<ID3D11Resource*>(fr.dynamic_vb), 0, "DrawAlphaBatch:dynamic_vb");
    fr.vb_offset += vb_bytes;
  }

  // Upload indices (if present)
  size_t ib_start_offset = fr.ib_offset;
  if (ib_count > 0) {
    if (SUCCEEDED(TrackedMap(context, static_cast<ID3D11Resource*>(fr.dynamic_ib), 0, ib_map_type, 0, &mapped, "DrawAlphaBatch:dynamic_ib"))) {
      memcpy(static_cast<uint8_t*>(mapped.pData) + ib_start_offset * sizeof(uint16_t), indices, ib_count * sizeof(uint16_t));
      TrackedUnmap(context, static_cast<ID3D11Resource*>(fr.dynamic_ib), 0, "DrawAlphaBatch:dynamic_ib");
      fr.ib_offset += ib_count;
    }
  }

  // Update cached offsets
  dynamic_vb_offset = fr.vb_offset;
  dynamic_ib_offset = fr.ib_offset;

  // Set render state for alpha poly rendering.
  // Blend state is already set by the front-end via SetBlendDesc() when
  // SetAlphaBlendFuncImmed() calls ApplyRenderState(D3DRS_SRCBLEND/DESTBLEND).
  // We only need to configure depth/rasterizer state here.

  // Select depth stencil state based on z_func
  switch (z_func) {
    case 0:  // ALWAYS
    case 1:  // NEVER
      SetDepthStencilState(dss_no_depth, 0);
      break;
    case 2:  // LESS
      SetDepthStencilState(dss_alpha_less, 0);
      break;
    case 3:  // LESS_EQUAL
    default:
      SetDepthStencilState(dss_alpha, 0);
      break;
  }

  SetRasterizerState(rs_no_cull);

  SetInputLayout(layout_rhw);
  if (has_texture) {
    // Update/bind stage-state CB for alpha polys (COLOROP/ALPHAOP).
    alpha_test_data.alpha_test_enabled = material_data.alpha_test_enabled;
    alpha_test_data.alpha_ref = ComputeEffectiveAlphaRef(material_data.alpha_ref);
    alpha_test_data.alpha_blend_func = IsAdditiveBlendForAlphaPoly() ? 1.0f : 0.0f;
    alpha_test_data.uv_source0 = PackUvSourceAndTexGen(stage_uv_source_[0], stage_texgen_mode_[0]);
    alpha_test_data.uv_source1 = PackUvSourceAndTexGen(stage_uv_source_[1], stage_texgen_mode_[1]);
    alpha_test_data.color_op0 = CombineOpToFloat(stage_color_op_[0]);
    alpha_test_data.color_op1 = CombineOpToFloat(stage_color_op_[1]);
    // AlphaOp is CPU-normalized for the alpha-poly shader paths.
    const CombineOp alpha_op = stage_alpha_op_[0];
    if (alpha_op == CombineOp::kSelectArg1) {
      alpha_test_data.alpha_op = 1.0f;
    } else if (alpha_op == CombineOp::kSelectArg2) {
      alpha_test_data.alpha_op = 2.0f;
    } else {
      alpha_test_data.alpha_op = 0.0f;
    }
    D3D11_MAPPED_SUBRESOURCE mapped_at;
    UpdateAlphaTestCB();
    BindPSConstantBuffer(4, cb_alpha_test);

    SetShaders(vs_rhw, ps_rhw_alpha);
  } else {
    SetShaders(vs_rhw, ps_vertex_color);
  }
  ApplyPrebuiltSamplerState(0);
  BindVSConstantBuffer(3, cb_screen);

  UINT stride = sizeof(VertexRHW);
  UINT vb_offset = static_cast<UINT>(vb_start_offset);
  BindVertexBuffer(0, fr.dynamic_vb, stride, vb_offset);
  SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  if (ib_count > 0) {
    BindIndexBuffer(fr.dynamic_ib, DXGI_FORMAT_R16_UINT, 0);
#ifndef NDEBUG
    {
      const std::string srv0_name = GetDebugObjectName(bound_srvs[0]);
      const std::string srv1_name = GetDebugObjectName(bound_srvs[1]);
      const int s0_white = (bound_srvs[0] == white_srv) ? 1 : 0;
      const int s0_black = (bound_srvs[0] == black_srv) ? 1 : 0;
      const int s1_white = (bound_srvs[1] == white_srv) ? 1 : 0;
      const int s1_black = (bound_srvs[1] == black_srv) ? 1 : 0;
      char rdoc_label_big[512];
      std::snprintf(rdoc_label_big, sizeof(rdoc_label_big),
                    "DrawIndexed(AlphaBatch) idx=%zu zf=%d has_tex=%d s0=%p white0=%d black0=%d name0='%s' s1=%p white1=%d black1=%d name1='%s'",
                    ib_count, z_func, has_texture ? 1 : 0, static_cast<const void*>(bound_srvs[0]), s0_white, s0_black, srv0_name.c_str(),
                    static_cast<const void*>(bound_srvs[1]), s1_white, s1_black, srv1_name.c_str());
      ScopedGpuEvent rdoc_event(user_annotation, rdoc_label_big);
      context->DrawIndexed(static_cast<UINT>(ib_count), static_cast<UINT>(ib_start_offset), 0);
    }
#else
    context->DrawIndexed(static_cast<UINT>(ib_count), static_cast<UINT>(ib_start_offset), 0);
#endif
  } else {
    context->Draw(static_cast<UINT>(vertex_count), 0);
  }

  return true;
}

void D3D11RendererImpl::BatchTriangleFan(const VertexRHW* vertices, int count, void* texture) {
  if (count < 3)
    return;

  int tri_count = count - 2;
  int needed_verts = count;
  int needed_indices = tri_count * 3;

  // Check if we need to flush due to state change or capacity
  if (batch_.current_texture != texture || !batch_.HasRoom(needed_verts, needed_indices)) {
    FlushBatch();
    batch_.current_texture = texture;
  }

  // Add vertices
  size_t base_vertex = batch_.vertex_count;
  memcpy(&batch_.vertices[batch_.vertex_count], vertices, count * sizeof(VertexRHW));
  batch_.vertex_count += count;

  // Add indices (fan -> triangles)
  for (int i = 0; i < tri_count; i++) {
    batch_.indices[batch_.index_count++] = static_cast<uint16_t>(base_vertex);
    batch_.indices[batch_.index_count++] = static_cast<uint16_t>(base_vertex + i + 1);
    batch_.indices[batch_.index_count++] = static_cast<uint16_t>(base_vertex + i + 2);
  }

  batch_.is_active = true;
}

void D3D11RendererImpl::SetWorldMatrix(const float* matrix) {
  memcpy(&transform_data.world, matrix, 16 * sizeof(float));
  transform_dirty = true;
}

void D3D11RendererImpl::SetViewMatrix(const float* matrix) {
  memcpy(&transform_data.view, matrix, 16 * sizeof(float));
  transform_dirty = true;
}

void D3D11RendererImpl::SetProjectionMatrix(const float* matrix) {
  memcpy(&transform_data.projection, matrix, 16 * sizeof(float));
  transform_dirty = true;
}

void D3D11RendererImpl::SetTextureTransformMatrix(unsigned long stage, const float* matrix) {
  if (stage >= 8 || matrix == nullptr) {
    return;
  }
  DirectX::XMFLOAT4X4 m = {};
  memcpy(&m, matrix, 16 * sizeof(float));
  const DirectX::XMMATRIX raw = DirectX::XMLoadFloat4x4(&m);
  tex_transform_matrix_raw_[stage] = raw;

  // Keep the existing (transposed) variant as the default for non-reflection paths.
  // This preserves current behavior for regular UV transforms.
  tex_transform_matrix_[stage] = DirectX::XMMatrixTranspose(raw);
}

void D3D11RendererImpl::UpdateTransformCB() {
  if (!transform_dirty)
    return;

  // Compute world-view-projection
  DirectX::XMMATRIX wvp = DirectX::XMMatrixMultiply(DirectX::XMMatrixMultiply(transform_data.world, transform_data.view), transform_data.projection);
  transform_data.world_view_proj = wvp;

  if (has_last_transform_data_ && memcmp(&transform_data, &last_transform_data_, sizeof(TransformCB)) == 0) {
    transform_dirty = false;
    return;
  }

  D3D11_MAPPED_SUBRESOURCE mapped;
  if (SUCCEEDED(context->Map(cb_transform, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
    memcpy(mapped.pData, &transform_data, sizeof(TransformCB));
    context->Unmap(cb_transform, 0);
  }

  last_transform_data_ = transform_data;
  has_last_transform_data_ = true;

  transform_dirty = false;
}

void D3D11RendererImpl::SetMaterial(const MaterialCB& material) {
  material_data = material;
  material_dirty = true;
}

void D3D11RendererImpl::UpdateMaterialCB() {
  if (!material_dirty)
    return;

  MaterialCB sanitized = material_data;
  sanitized._pad[0] = 0.0f;

  if (has_last_material_data_ && memcmp(&sanitized, &last_material_data_, sizeof(MaterialCB)) == 0) {
    material_dirty = false;
    return;
  }

  D3D11_MAPPED_SUBRESOURCE mapped;
  if (SUCCEEDED(context->Map(cb_material, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
    memcpy(mapped.pData, &sanitized, sizeof(MaterialCB));
    context->Unmap(cb_material, 0);
  }

  last_material_data_ = sanitized;
  has_last_material_data_ = true;

  material_dirty = false;
}

void D3D11RendererImpl::SetFog(bool enable, unsigned long color, float start, float end, bool range_enabled) {
  fog_data.enabled = enable ? 1.0f : 0.0f;
  fog_data.color.x = ((color >> 16) & 0xFF) / 255.0f;
  fog_data.color.y = ((color >> 8) & 0xFF) / 255.0f;
  fog_data.color.z = (color & 0xFF) / 255.0f;
  fog_data.color.w = 1.0f;
  fog_data.start = start;
  fog_data.end = end;
  fog_data.range_enabled = range_enabled ? 1.0f : 0.0f;
  fog_dirty = true;
}

void D3D11RendererImpl::UpdateFogCB() {
  if (!fog_dirty)
    return;

  FogCB sanitized = fog_data;
  sanitized._pad[0] = 0.0f;
  sanitized._pad[1] = 0.0f;
  sanitized._pad[2] = 0.0f;

  if (has_last_fog_data_ && memcmp(&sanitized, &last_fog_data_, sizeof(FogCB)) == 0) {
    fog_dirty = false;
    return;
  }

  D3D11_MAPPED_SUBRESOURCE mapped;
  if (SUCCEEDED(context->Map(cb_fog, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
    memcpy(mapped.pData, &sanitized, sizeof(FogCB));
    context->Unmap(cb_fog, 0);
  }

  last_fog_data_ = sanitized;
  has_last_fog_data_ = true;

  fog_dirty = false;
}

void D3D11RendererImpl::UpdateAlphaTestCB() {
  if (!context || !cb_alpha_test) {
    return;
  }
  if (has_last_alpha_test_data_ && memcmp(&alpha_test_data, &last_alpha_test_data_, sizeof(AlphaTestCB)) == 0) {
    return;
  }
  D3D11_MAPPED_SUBRESOURCE mapped;
  if (SUCCEEDED(context->Map(cb_alpha_test, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
    memcpy(mapped.pData, &alpha_test_data, sizeof(AlphaTestCB));
    context->Unmap(cb_alpha_test, 0);
  }
  last_alpha_test_data_ = alpha_test_data;
  has_last_alpha_test_data_ = true;
}

void D3D11RendererImpl::SetTextureWrap(int stage, bool enable) {
  if (stage < 0 || stage >= static_cast<int>(kMaxTextureStages))
    return;

  // Update native tracking
  const AddressMode mode = enable ? AddressMode::kWrap : AddressMode::kClamp;
  stage_sampler_desc_[stage].address_u = mode;
  stage_sampler_desc_[stage].address_v = mode;

  // Apply via prebuilt sampler for efficiency
  ApplyPrebuiltSamplerState(stage);
}

void D3D11RendererImpl::SetTextureFilter(int stage, int filter) {
  if (stage < 0 || stage >= static_cast<int>(kMaxTextureStages))
    return;

  // filter: 0 = point, 1 = linear, 2 = anisotropic
  FilterMode mode;
  switch (filter) {
    case 0:
      mode = FilterMode::kPoint;
      break;
    case 2:
      mode = FilterMode::kAnisotropic;
      break;
    case 1:
    default:
      mode = FilterMode::kLinear;
      break;
  }

  // Update native tracking
  stage_sampler_desc_[stage].filter = mode;
  stage_sampler_desc_[stage].max_anisotropy = static_cast<std::uint8_t>(max_anisotropy);

  // Apply via prebuilt sampler for efficiency
  ApplyPrebuiltSamplerState(stage);
}

void D3D11RendererImpl::ApplyPrebuiltSamplerState(int stage) {
  if (stage < 0 || stage >= static_cast<int>(kMaxTextureStages))
    return;

  const SamplerDesc& desc = stage_sampler_desc_[stage];
  const bool wrap_u = (desc.address_u == AddressMode::kWrap || desc.address_u == AddressMode::kMirror);
  const bool wrap_v = (desc.address_v == AddressMode::kWrap || desc.address_v == AddressMode::kMirror);

  ID3D11SamplerState* sampler = nullptr;

  if (desc.filter == FilterMode::kAnisotropic) {
    if (wrap_u && wrap_v) {
      sampler = ss_aniso_wrap;
    } else if (!wrap_u && !wrap_v) {
      sampler = ss_aniso_clamp;
    } else if (wrap_u && !wrap_v) {
      sampler = ss_aniso_wrap_clamp;
    } else {
      sampler = ss_aniso_clamp_wrap;
    }
  } else if (desc.filter == FilterMode::kPoint) {
    if (wrap_u && wrap_v) {
      sampler = ss_point_wrap;
    } else if (!wrap_u && !wrap_v) {
      sampler = ss_point_clamp;
    } else if (wrap_u && !wrap_v) {
      sampler = ss_point_wrap_clamp;
    } else {
      sampler = ss_point_clamp_wrap;
    }
  } else {
    // Linear (default)
    if (!desc.mip_linear) {
      if (wrap_u && wrap_v) {
        sampler = ss_linear_mip_point_wrap;
      } else if (!wrap_u && !wrap_v) {
        sampler = ss_linear_mip_point_clamp;
      } else if (wrap_u && !wrap_v) {
        sampler = ss_linear_mip_point_wrap_clamp;
      } else {
        sampler = ss_linear_mip_point_clamp_wrap;
      }
    } else {
      if (wrap_u && wrap_v) {
        sampler = ss_linear_wrap;
      } else if (!wrap_u && !wrap_v) {
        sampler = ss_linear_clamp;
      } else if (wrap_u && !wrap_v) {
        sampler = ss_linear_wrap_clamp;
      } else {
        sampler = ss_linear_clamp_wrap;
      }
    }
  }

  if (sampler) {
    SetSamplerState(stage, sampler);
  }
}

void D3D11RendererImpl::SetSemanticBaseRgbGen(int rgb_gen) {
  semantic_base_rgb_gen_ = rgb_gen;
}

void D3D11RendererImpl::SetSemanticBaseColorFactor(unsigned long argb) {
  const float r = static_cast<float>((argb >> 16) & 0xFF) / 255.0f;
  const float g = static_cast<float>((argb >> 8) & 0xFF) / 255.0f;
  const float b = static_cast<float>(argb & 0xFF) / 255.0f;
  semantic_base_factor_rgb_ = {r, g, b};
}

void D3D11RendererImpl::SetSemanticLightmapFfp(bool enabled, unsigned long s0_colorop, unsigned long s0_colorarg1, unsigned long s0_colorarg2,
                                               unsigned long s1_colorop, unsigned long s1_colorarg1, unsigned long s1_colorarg2) {
  semantic_ffp_enabled_ = enabled;
  semantic_s0_colorop_ = s0_colorop;
  semantic_s0_colorarg1_ = s0_colorarg1;
  semantic_s0_colorarg2_ = s0_colorarg2;
  semantic_s1_colorop_ = s1_colorop;
  semantic_s1_colorarg1_ = s1_colorarg1;
  semantic_s1_colorarg2_ = s1_colorarg2;
}

void D3D11RendererImpl::SetFillMode(unsigned long mode) {
  // Map D3D9-style fill modes to prebuilt rasterizer states
  switch (mode) {
    case 2:  // D3DFILL_WIREFRAME
      SetRasterizerState(rs_wireframe);
      break;
    default:  // Solid or any other
      SetRasterizerState(rs_default);
      break;
  }
}

void D3D11RendererImpl::SetZBias(float depth_bias, float slope_scaled_depth_bias) {
  raster_depth_bias_ = depth_bias;
  raster_slope_scaled_depth_bias_ = slope_scaled_depth_bias;
  raster_depth_bias_clamp_ = 0.0f;

  // Re-apply the currently requested rasterizer state so the new bias takes effect.
  // If nothing has been requested yet, default to rs_default.
  SetRasterizerState(requested_rasterizer_state_ ? requested_rasterizer_state_ : rs_default);
}

void D3D11RendererImpl::SetColorWrite(bool /*r*/, bool /*g*/, bool /*b*/, bool /*a*/) {
  // Color write masks not wired yet; keep current blend state
}

void D3D11RendererImpl::SetLightingEnabled(bool enable) {
  lighting_enabled_ = enable;
}

size_t D3D11RendererImpl::GetAvailableTextureMem() const {
  // No direct query in D3D11; return a conservative estimate
  return 512ull * 1024ull * 1024ull;
}

void D3D11RendererImpl::SetLight(unsigned long index, const Light& light) {
  if (index >= kMaxLights) {
    return;
  }

  auto& gpu_light = light_data.lights[index];

  // Position and range
  gpu_light.position = {light.position_x, light.position_y, light.position_z, light.range};

  // Direction and type (0=off, 1=point, 2=spot, 3=directional)
  gpu_light.direction = {light.direction_x, light.direction_y, light.direction_z, static_cast<float>(static_cast<std::uint8_t>(light.type))};

  // Diffuse color (normalized [0,1] by adapter boundary)
  gpu_light.diffuse = {light.diffuse_r, light.diffuse_g, light.diffuse_b, light_enabled[index] ? 1.0f : 0.0f};

  // Attenuation
  gpu_light.attenuation = {light.attenuation0, light.attenuation1, light.attenuation2, light.spot_outer_angle_rad};

  light_dirty = true;
}

void D3D11RendererImpl::LightEnable(unsigned long index, bool enable) {
  if (index >= kMaxLights) {
    return;
  }

  light_enabled[index] = enable;
  light_data.lights[index].diffuse.w = enable ? 1.0f : 0.0f;
  light_dirty = true;
}

void D3D11RendererImpl::SetAmbientLight(unsigned long color) {
  // D3DCOLOR is ARGB: 0xAARRGGBB
  float r = static_cast<float>((color >> 16) & 0xFF) / 255.0f;
  float g = static_cast<float>((color >> 8) & 0xFF) / 255.0f;
  float b = static_cast<float>(color & 0xFF) / 255.0f;
  float a = static_cast<float>((color >> 24) & 0xFF) / 255.0f;

  light_data.ambient = {r, g, b, a};
  light_dirty = true;
}

void D3D11RendererImpl::UpdateLightBuffer() {
  if (!light_dirty || !context || !cb_light) {
    return;
  }

  // Count active lights
  int active_count = 0;
  for (int i = 0; i < kMaxLights; ++i) {
    if (light_enabled[i]) {
      ++active_count;
    }
  }
  light_data.num_active_lights = active_count;

  if (has_last_light_data_ && memcmp(&light_data, &last_light_data_, sizeof(LightCB)) == 0) {
    light_dirty = false;
    return;
  }

  UpdateDynamicCB(context, cb_light, light_data);

  last_light_data_ = light_data;
  has_last_light_data_ = true;

  light_dirty = false;
}

}  // namespace gmp::renderer::d3d11
