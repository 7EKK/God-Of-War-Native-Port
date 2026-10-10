// GOW-Port: render-target GS renderer, see include/runtime/gs/gs_hw_backend.h.
#include "runtime/gs/gs_hw_backend.h"
#include "runtime/gs/gs_cpu_backend.h"
#include "runtime/gs/gs_swizzle.h"
#include "gs_gl.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    using namespace GSGL;

    constexpr GLenum kArrayBuffer = 0x8892;
    constexpr GLenum kFloat = 0x1406;
    constexpr GLenum kColorAttachment0 = 0x8CE0;
    constexpr GLenum kDepthAttachment = 0x8D00;
    constexpr GLenum kFramebufferComplete = 0x8CD5;
    constexpr GLenum kDepthComponent32F = 0x8CAC;
    constexpr GLenum kDepthComponent = 0x1902;
    constexpr GLenum kBlend = 0x0BE2;
    constexpr GLenum kDepthTest = 0x0B71;
    constexpr GLenum kScissorTest = 0x0C11;
    constexpr GLenum kCullFace = 0x0B44;
    constexpr GLenum kOne = 1, kZero = 0;
    constexpr GLenum kSrc1Alpha = 0x8589, kOneMinusSrc1Alpha = 0x88FB;
    constexpr GLenum kConstantColor = 0x8001, kOneMinusConstantColor = 0x8002;
    constexpr GLenum kFuncAdd = 0x8006, kFuncReverseSubtract = 0x800B;
    constexpr GLbitfield kColorBufferBit = 0x4000, kDepthBufferBit = 0x100;
    constexpr GLenum kNever = 0x200, kGreater = 0x204, kGEqual = 0x206, kAlways = 0x207;
    constexpr GLenum kTexture0 = 0x84C0;
    constexpr GLint kRepeat = 0x2901;
    constexpr GLenum kUnpackAlignment = 0x0CF5, kPackAlignment = 0x0D05;
    constexpr GLenum kFalse = 0, kTrue = 1;

    const char *kVsSource = R"(#version 430 core
layout(location = 0) in vec2 aPos;     // PS2 pixels, centre-adjusted
layout(location = 1) in float aZ;      // 0..1
layout(location = 2) in vec4 aCol;     // 0..255
layout(location = 3) in vec3 aSTQ;
layout(location = 4) in vec2 aUV;      // texels
uniform vec2 uInvSize;
noperspective out vec4 vCol;
flat out vec4 vColFlat;
noperspective out vec3 vSTQ;
noperspective out vec2 vUV;
void main()
{
    gl_Position = vec4(aPos * uInvSize * 2.0 - 1.0, aZ * 2.0 - 1.0, 1.0);
    vCol = aCol;
    vColFlat = aCol;
    vSTQ = aSTQ;
    vUV = aUV;
}
)";

    const char *kFsSource = R"(#version 430 core
noperspective in vec4 vCol;
flat in vec4 vColFlat;
noperspective in vec3 vSTQ;
noperspective in vec2 vUV;
layout(location = 0, index = 0) out vec4 oColor;
layout(location = 0, index = 1) out vec4 oBlend;
uniform sampler2D uTex;
uniform vec2 uTexSize;   // size the sampled image is normalised by (render targets: their own size)
uniform vec2 uTexDim;    // TEX0 TW/TH: size STQ coordinates are scaled by

// Reinterpreting fetch: the texture lives in the memory a render target was drawn into, but with another format / width /
// layout (16 bit view of a 32 bit target, T8H = alpha plane, other pitch...). The texel's GS memory address is computed
// from the texture format, mapped back to the render-target pixel that owns it and the bits are re-extracted: pure address
// math on the GPU, no memory image.
uniform int uReinterp;
uniform int uDate, uDatm;   // destination alpha test (reads the target itself)
uniform sampler2D uDest;
uniform int uZTex;     // 16/24/32: sampling a depth buffer (Z16/Z24/Z32 texture)
uniform float uZMax;
uniform usampler2D uTables;
uniform sampler2D uRtColor;
uniform sampler2D uPalette;
uniform ivec4 uTexFmt;   // pageShiftX, pageShiftY, pixelsPerPageShift, unpackedShift
uniform ivec4 uTexFmt2;  // bitOffset, texelBits, tableOffset, kind (0 CT32, 1 CT24, 2 CT16, 3 indexed)
uniform ivec4 uRtFmt;    // pageShiftX, pageShiftY, pixelsPerPageShift, bits per pixel
uniform ivec4 uRtFmt2;   // inverseTableOffset, fbp (pages), fbw (64 px), 0
uniform ivec4 uTex0;     // tbp, tbw, width, height
uniform vec2 uRtPx;      // render target size in PS2 pixels

uint tab(uint i) { return texelFetch(uTables, ivec2(int(i & 2047u), int(i >> 11)), 0).r; }

vec4 reinterpretTexel(ivec2 p)
{
    uint x = uint(p.x), y = uint(p.y);
    uint psx = uint(uTexFmt.x), psy = uint(uTexFmt.y);
    uint pagesPerRow = (uint(uTex0.y) * 64u) >> psx;
    uint page = (uint(uTex0.x) >> 5) + (y >> psy) * pagesPerRow + (x >> psx);
    uint entry = ((uint(uTex0.x) & 31u) << (psx + psy)) + ((y & ((1u << psy) - 1u)) << psx) + (x & ((1u << psx) - 1u));
    uint pixelIndex = (page << uint(uTexFmt.z)) + tab(uint(uTexFmt2.z) + entry);
    uint bitpos = (pixelIndex << uint(uTexFmt.w)) + uint(uTexFmt2.x);
    uint B = uint(uRtFmt.w);
    uint rtIdx = bitpos / B;
    uint within = bitpos % B;
    uint rtBase = uint(uRtFmt2.y) << uint(uRtFmt.z);
    if (rtIdx < rtBase) return vec4(1.0, 0.0, 1.0, 1.0);
    uint rel = rtIdx - rtBase;
    uint rpage = rel >> uint(uRtFmt.z);
    uint inpage = rel & ((1u << uint(uRtFmt.z)) - 1u);
    uint e = tab(uint(uRtFmt2.x) + inpage);
    uint rpsx = uint(uRtFmt.x), rpsy = uint(uRtFmt.y);
    uint fbw = uint(uRtFmt2.z);
    uint px = (rpage % fbw) * (1u << rpsx) + (e & ((1u << rpsx) - 1u));
    uint py = (rpage / fbw) * (1u << rpsy) + (e >> rpsx);
    vec4 c = texture(uRtColor, (vec2(float(px), float(py)) + 0.5) / uRtPx) * 255.0;
    c = floor(c + 0.5);
    uint word;
    if (B == 32u)
        word = uint(c.r) | (uint(c.g) << 8) | (uint(c.b) << 16) | (uint(c.a) << 24);
    else
        word = (c.a >= 128.0 ? 0x8000u : 0u) | ((uint(c.b) >> 3) << 10) | ((uint(c.g) >> 3) << 5) | (uint(c.r) >> 3);
    uint bits = uint(uTexFmt2.y);
    uint value = (bits >= 32u) ? (word >> within) : ((word >> within) & ((1u << bits) - 1u));
    int kind = uTexFmt2.w;
    if (kind == 0) return vec4(float(value & 255u), float((value >> 8) & 255u), float((value >> 16) & 255u), float((value >> 24) & 255u)) / 255.0;
    if (kind == 1) return vec4(float(value & 255u), float((value >> 8) & 255u), float((value >> 16) & 255u), 128.0) / 255.0; // alpha via uRtTex24 path
    if (kind == 2)
    {
        vec3 rgb = vec3(float(value & 31u), float((value >> 5) & 31u), float((value >> 10) & 31u)) * 8.0;
        return vec4(rgb, ((value & 0x8000u) != 0u) ? 128.0 : 0.0) / 255.0;
    }
    return texelFetch(uPalette, ivec2(int(value & 255u), 0), 0);
}
uniform int uTme, uFst, uTfx, uTcc, uIip;
uniform int uRtTex24; // sampling a render target as PSMCT24 (1) or PSMCT16 (2): alpha comes from TEXA
uniform float uTa0;
uniform float uTa1;
uniform int uAem;
uniform int uAte, uAtst, uAref;
uniform int uPass; // 0 = no alpha test, 1 = keep passing fragments, 2 = keep failing fragments
uniform float uBlendScale; // 1/128 style: alpha factor = min(a * scale, 1)

float clamp255(float v) { return clamp(floor(v), 0.0, 255.0); }

bool alphaPasses(float a)
{
    int ai = int(a);
    switch (uAtst)
    {
    case 0: return false;
    case 1: return true;
    case 2: return ai < uAref;
    case 3: return ai <= uAref;
    case 4: return ai == uAref;
    case 5: return ai >= uAref;
    case 6: return ai > uAref;
    default: return ai != uAref;
    }
}

void main()
{
    if (uDate != 0)
    {
        bool bit = texelFetch(uDest, ivec2(gl_FragCoord.xy), 0).a >= 0.5;
        if (bit != (uDatm != 0)) discard;
    }
    vec4 v = (uIip != 0) ? vCol : vColFlat;
    vec3 rgb = v.rgb;
    float a = v.a;
    if (uTme != 0)
    {
        vec2 uv = (uFst != 0) ? vUV : (vSTQ.xy / abs(vSTQ.z)) * uTexDim;
        vec4 t;
        if (uReinterp != 0)
        {
            ivec2 ip = ivec2(floor(uv));
            ip = ivec2(clamp(ip.x, 0, uTex0.z - 1), clamp(ip.y, 0, uTex0.w - 1));
            t = reinterpretTexel(ip) * 255.0;
        }
        else if (uZTex != 0)
        {
            float zf = texture(uTex, uv / uTexSize).r;
            uint zi = uint(clamp(zf, 0.0, 1.0) * uZMax + 0.5);
            if (uZTex == 16)
            {
                vec3 rgb = vec3(float(zi & 31u), float((zi >> 5) & 31u), float((zi >> 10) & 31u)) * 8.0;
                t = vec4(rgb, ((zi & 0x8000u) != 0u) ? 128.0 : 0.0);
            }
            else
                t = vec4(float(zi & 255u), float((zi >> 8) & 255u), float((zi >> 16) & 255u), 128.0);
        }
        else
            t = texture(uTex, uv / uTexSize) * 255.0;
        t = floor(t + 0.5);
        if (uRtTex24 == 1)
            t.a = (uAem != 0 && t.r + t.g + t.b == 0.0) ? 0.0 : uTa0;
        else if (uRtTex24 == 2)
            t.a = (uAem != 0 && t.r + t.g + t.b == 0.0) ? 0.0 : ((t.a >= 128.0) ? uTa1 : uTa0);
        float ta = (uTcc != 0) ? t.a : v.a;
        if (uTfx == 0)
        {
            rgb = vec3(clamp255(t.r * v.r / 128.0), clamp255(t.g * v.g / 128.0), clamp255(t.b * v.b / 128.0));
            a = (uTcc != 0) ? clamp255(t.a * v.a / 128.0) : v.a;
        }
        else if (uTfx == 1)
        {
            rgb = t.rgb;
            a = ta;
        }
        else if (uTfx == 2)
        {
            rgb = vec3(clamp255(t.r * v.r / 128.0 + v.a), clamp255(t.g * v.g / 128.0 + v.a), clamp255(t.b * v.b / 128.0 + v.a));
            a = (uTcc != 0) ? clamp255(t.a + v.a) : v.a;
        }
        else
        {
            rgb = vec3(clamp255(t.r * v.r / 128.0 + v.a), clamp255(t.g * v.g / 128.0 + v.a), clamp255(t.b * v.b / 128.0 + v.a));
            a = ta;
        }
    }
    if (uPass != 0)
    {
        bool pass = (uAte == 0) || alphaPasses(a);
        if (uPass == 1 && !pass) discard;
        if (uPass == 2 && pass) discard;
    }
    oColor = vec4(rgb / 255.0, a / 255.0);
    float f = min(a * uBlendScale, 1.0);
    oBlend = vec4(f);
}
)";

    // Colour -> depth: the game draws "colour" into the memory its Z buffer uses (a CT24 target at the Z buffer's base) to write
    // depth values; the written colour words are the Z words. Resolved into the depth attachment of the target that uses them.
    const char *kResolveVs = R"(#version 430 core
out vec2 vUv;
void main()
{
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    vUv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";
    const char *kResolveFs = R"(#version 430 core
in vec2 vUv;
uniform sampler2D uSrc;
uniform float uDiv;
void main()
{
    vec4 c = floor(texture(uSrc, vUv) * 255.0 + 0.5);
    float z = c.r + c.g * 256.0 + c.b * 65536.0;
    gl_FragDepth = clamp(z / uDiv, 0.0, 1.0);
}
)";

    // GS vertex as sent to the GPU.
    struct HwVertex
    {
        float x, y, z;
        float r, g, b, a;
        float s, t, q;
        float u, v;
    };

    struct Texture
    {
        GLuint id = 0;
        uint32_t width = 0, height = 0;
        uint32_t blockLo = 0, blockHi = 0;
        bool valid = false;
        uint64_t lastUse = 0;
    };

    struct RenderTarget
    {
        GLuint color = 0, depth = 0, fbo = 0;
        uint32_t fbp = 0, fbw = 1; // PS2 page base and width in 64 px units
        uint8_t psm = 0;
        uint32_t zbp = ~0u;
        uint32_t pxW = 0, pxH = 0; // size in PS2 pixels
        uint32_t glW = 0, glH = 0;
        bool depthCleared = false;
        uint64_t version = 1, syncedVersion = 0, lastDrawSeq = 0, depthResolvedVersion = 0;
        uint32_t usedW = 0, usedH = 0; // PS2 pixels touched by draws (scissor extents)
    };

    uint32_t pageRowsFor(uint32_t psm)
    {
        switch (psm & 0x3F)
        {
        case 0x13: case 0x1B: case 0x24: case 0x2C: return 64u;  // T8 / T8H
        case 0x14: case 0x2C + 1: return 128u;                    // T4
        case 0x02: case 0x0A: case 0x32: case 0x3A: return 64u;  // 16 bit
        default: return 32u;
        }
    }

    // conservative block (256 B) span of `rows` rows of a buffer `bw` (64 px units) wide
    uint32_t spanBlocks(uint32_t psm, uint32_t bw, uint32_t rows)
    {
        const uint32_t pagesDown = (rows + pageRowsFor(psm) - 1u) / pageRowsFor(psm);
        return std::max<uint32_t>(1u, pagesDown) * std::max<uint32_t>(1u, bw) * 32u;
    }
}

struct GSHwBackend::Impl
{
    GSCpuBackend cpu;
    GSGL::Context context;
    bool initialized = false, failed = false;
    uint8_t *vram = nullptr;
    uint32_t vramSize = 0;

    float scale = 2.0f;
    GLuint program = 0, vao = 0, vbo = 0;
    GLint locInvSize = -1, locTex = -1, locTexSize = -1, locTexDim = -1, locTme = -1, locFst = -1, locTfx = -1, locTcc = -1, locIip = -1,
          locAte = -1, locAtst = -1, locAref = -1, locPass = -1, locBlendScale = -1, locRtTex24 = -1, locTa0 = -1, locTa1 = -1, locReinterp = -1, locDate = -1, locDatm = -1, locDest = -1, locZTex = -1, locZMax = -1, locTables = -1, locRtColor = -1, locPalette = -1, locTexFmt = -1, locTexFmt2 = -1,
          locRtFmt = -1, locRtFmt2 = -1, locTex0 = -1, locRtPx = -1, locAem = -1;

    GLuint tableTex = 0, paletteTex = 0, resolveProgram = 0, resolveVao = 0;
    GLint locResolveSrc = -1, locResolveDiv = -1;
    std::unordered_map<const uint16_t *, uint32_t> tableOffset;
    std::unordered_map<uint32_t, uint32_t> inverseOffset; // psm -> offset of the inverse page table
    std::vector<HwVertex> pending;
    GSDrawState pendingState{};
    bool havePending = false;
    double firstRawZ = 0.0;

    std::unordered_map<uint64_t, Texture> textures;
    std::vector<RenderTarget> targets;
    uint64_t useCounter = 0, writeSeq = 0;
    struct UploadRecord { uint32_t lo, hi; uint64_t seq; };
    std::vector<UploadRecord> uploadLog;
    uint64_t clutHash = 0;

    struct Stats
    {
        uint64_t draws = 0, batches = 0, skippedPsm = 0, unsupportedBlend = 0, texDecodes = 0, texHits = 0, skippedPrim = 0;
        std::chrono::steady_clock::time_point last = std::chrono::steady_clock::now();
    } stats;
    bool statsEnabled = std::getenv("GOW_GS_HW_STATS") != nullptr;

    const Api &gl() const { return context.gl(); }

    GLuint compile(GLenum type, const char *src)
    {
        const Api &g = gl();
        const GLuint sh = g.CreateShader(type);
        g.ShaderSource(sh, 1, &src, nullptr);
        g.CompileShader(sh);
        GLint ok = 0;
        g.GetShaderiv(sh, kCompileStatus, &ok);
        if (!ok)
        {
            char log[2048];
            GLsizei n = 0;
            g.GetShaderInfoLog(sh, sizeof(log), &n, log);
            std::fprintf(stderr, "[gs-hw] shader error: %s\n", log);
            return 0;
        }
        return sh;
    }

    bool ensure()
    {
        if (initialized)
            return true;
        if (failed)
            return false;
        std::string error;
        if (!context.Create(error) || !gl().hwRaster)
        {
            std::fprintf(stderr, "[gs-hw] GL unavailable: %s\n", error.c_str());
            failed = true;
            return false;
        }
        const Api &g = gl();
        const GLuint v = compile(GSGL::kVertexShader, kVsSource);
        const GLuint f = compile(GSGL::kFragmentShader, kFsSource);
        if (!v || !f)
        {
            failed = true;
            return false;
        }
        program = g.CreateProgram();
        g.AttachShader(program, v);
        g.AttachShader(program, f);
        g.LinkProgram(program);
        GLint ok = 0;
        g.GetProgramiv(program, kLinkStatus, &ok);
        if (!ok)
        {
            char log[2048];
            GLsizei n = 0;
            g.GetProgramInfoLog(program, sizeof(log), &n, log);
            std::fprintf(stderr, "[gs-hw] link error: %s\n", log);
            failed = true;
            return false;
        }
        const auto loc = [&](const char *name) { return g.GetUniformLocation(program, name); };
        locInvSize = loc("uInvSize"); locTex = loc("uTex"); locTexSize = loc("uTexSize"); locTexDim = loc("uTexDim"); locTme = loc("uTme");
        locFst = loc("uFst"); locTfx = loc("uTfx"); locTcc = loc("uTcc"); locIip = loc("uIip"); locAte = loc("uAte");
        locAtst = loc("uAtst"); locAref = loc("uAref"); locPass = loc("uPass"); locBlendScale = loc("uBlendScale");
        locReinterp = loc("uReinterp"); locDate = loc("uDate"); locDatm = loc("uDatm"); locDest = loc("uDest"); locZTex = loc("uZTex"); locZMax = loc("uZMax"); locTables = loc("uTables"); locRtColor = loc("uRtColor"); locPalette = loc("uPalette");
        locTexFmt = loc("uTexFmt"); locTexFmt2 = loc("uTexFmt2"); locRtFmt = loc("uRtFmt"); locRtFmt2 = loc("uRtFmt2");
        locTex0 = loc("uTex0"); locRtPx = loc("uRtPx");
        locRtTex24 = loc("uRtTex24"); locTa0 = loc("uTa0"); locTa1 = loc("uTa1"); locAem = loc("uAem");

        g.GenVertexArrays(1, &vao);
        g.BindVertexArray(vao);
        g.GenBuffers(1, &vbo);
        g.BindBuffer(kArrayBuffer, vbo);
        const GLsizei stride = sizeof(HwVertex);
        g.VertexAttribPointer(0, 2, kFloat, kFalse, stride, reinterpret_cast<const void *>(offsetof(HwVertex, x)));
        g.VertexAttribPointer(1, 1, kFloat, kFalse, stride, reinterpret_cast<const void *>(offsetof(HwVertex, z)));
        g.VertexAttribPointer(2, 4, kFloat, kFalse, stride, reinterpret_cast<const void *>(offsetof(HwVertex, r)));
        g.VertexAttribPointer(3, 3, kFloat, kFalse, stride, reinterpret_cast<const void *>(offsetof(HwVertex, s)));
        g.VertexAttribPointer(4, 2, kFloat, kFalse, stride, reinterpret_cast<const void *>(offsetof(HwVertex, u)));
        for (GLuint i = 0; i < 5; ++i)
            g.EnableVertexAttribArray(i);
        buildTables();
        {
            const GLuint rv = compile(GSGL::kVertexShader, kResolveVs), rf = compile(GSGL::kFragmentShader, kResolveFs);
            if (rv && rf)
            {
                resolveProgram = g.CreateProgram();
                g.AttachShader(resolveProgram, rv);
                g.AttachShader(resolveProgram, rf);
                g.LinkProgram(resolveProgram);
                locResolveSrc = g.GetUniformLocation(resolveProgram, "uSrc");
                locResolveDiv = g.GetUniformLocation(resolveProgram, "uDiv");
                g.GenVertexArrays(1, &resolveVao);
            }
        }
        if (const char *s = std::getenv("PS2X_GS_HW_SCALE"))
            scale = std::max(1.0f, static_cast<float>(std::atof(s)));
        std::fprintf(stderr, "[gs-hw] render-target renderer ready, scale %.2f (%s)\n", scale,
                     reinterpret_cast<const char *>(g.GetString(kRenderer)));
        initialized = true;
        return true;
    }

    // forward page tables of every texture format + inverse tables (in-page pixel index -> in-page x/y) of the render-target
    // formats, in one R16UI texture (2048 entries per row)
    void buildTables()
    {
        const Api &g = gl();
        std::vector<uint16_t> data;
        const uint32_t psms[] = {0x00, 0x01, 0x02, 0x0A, 0x13, 0x14, 0x1B, 0x24, 0x2C};
        for (const uint32_t psm : psms)
        {
            const GSSwizzle::Format &f = GSSwizzle::GetFormat(psm);
            if (!f.table || tableOffset.count(f.table))
                continue;
            const uint32_t size = 32u << (f.pageShiftX + f.pageShiftY);
            tableOffset[f.table] = static_cast<uint32_t>(data.size());
            data.insert(data.end(), f.table, f.table + size);
        }
        for (const uint32_t psm : {0x00u, 0x01u, 0x02u, 0x0Au})
        {
            const GSSwizzle::Format &f = GSSwizzle::GetFormat(psm);
            const uint32_t pageSize = 1u << (f.pageShiftX + f.pageShiftY);
            inverseOffset[psm] = static_cast<uint32_t>(data.size());
            data.resize(data.size() + pageSize, 0);
            uint16_t *inv = &data[inverseOffset[psm]];
            for (uint32_t e = 0; e < pageSize; ++e)
                inv[f.table[e]] = static_cast<uint16_t>(e);
        }
        const uint32_t rows = static_cast<uint32_t>((data.size() + 2047u) / 2048u);
        data.resize(static_cast<size_t>(rows) * 2048u, 0);
        g.GenTextures(1, &tableTex);
        g.BindTexture(kTexture2D, tableTex);
        g.PixelStorei(kUnpackAlignment, 1);
        g.TexImage2D(kTexture2D, 0, 0x8234 /*R16UI*/, 2048, rows, 0, 0x8D94 /*RED_INTEGER*/, 0x1403 /*UNSIGNED_SHORT*/, data.data());
        g.TexParameteri(kTexture2D, kTextureMinFilter, kNearest);
        g.TexParameteri(kTexture2D, kTextureMagFilter, kNearest);
        g.GenTextures(1, &paletteTex);
        g.BindTexture(kTexture2D, paletteTex);
        g.TexImage2D(kTexture2D, 0, static_cast<GLint>(kRgba8), 256, 1, 0, kRgba, kUnsignedByte, nullptr);
        g.TexParameteri(kTexture2D, kTextureMinFilter, kNearest);
        g.TexParameteri(kTexture2D, kTextureMagFilter, kNearest);
    }

    // ---- render targets -------------------------------------------------------------------------------------------
    RenderTarget &target(uint32_t fbp, uint32_t fbw)
    {
        for (RenderTarget &t : targets)
            if (t.fbp == fbp && t.fbw >= fbw)
                return t;
        for (RenderTarget &t : targets) // grow an existing one
            if (t.fbp == fbp)
            {
                destroy(t);
                createTarget(t, fbp, fbw);
                return t;
            }
        targets.emplace_back();
        createTarget(targets.back(), fbp, fbw);
        return targets.back();
    }

    void destroy(RenderTarget &t)
    {
        const Api &g = gl();
        if (t.fbo) g.DeleteFramebuffers(1, &t.fbo);
        if (t.color) g.DeleteTextures(1, &t.color);
        if (t.depth) g.DeleteTextures(1, &t.depth);
        t = {};
    }

    void createTarget(RenderTarget &t, uint32_t fbp, uint32_t fbw)
    {
        const Api &g = gl();
        t.fbp = fbp;
        t.psm = 0;
        t.fbw = std::max<uint32_t>(fbw, 1u);
        t.pxW = t.fbw * 64u;
        t.pxH = 512u;
        t.glW = static_cast<uint32_t>(std::ceil(t.pxW * scale));
        t.glH = static_cast<uint32_t>(std::ceil(t.pxH * scale));
        g.GenTextures(1, &t.color);
        g.BindTexture(kTexture2D, t.color);
        g.TexImage2D(kTexture2D, 0, static_cast<GLint>(kRgba8), t.glW, t.glH, 0, kRgba, kUnsignedByte, nullptr);
        g.TexParameteri(kTexture2D, kTextureMinFilter, kLinear);
        g.TexParameteri(kTexture2D, kTextureMagFilter, kLinear);
        g.GenFramebuffers(1, &t.fbo);
        g.BindFramebuffer(kFramebuffer, t.fbo);
        g.FramebufferTexture2D(kFramebuffer, kColorAttachment0, kTexture2D, t.color, 0);
        g.ClearColor(0.f, 0.f, 0.f, 0.f);
        g.Clear(kColorBufferBit);
        std::fprintf(stderr, "[gs-hw] render target fbp=%u fbw=%u -> %ux%u\n", fbp, t.fbw, t.glW, t.glH);
    }

    void attachDepth(RenderTarget &t, uint32_t zbp)
    {
        const Api &g = gl();
        if (!t.depth)
        {
            g.GenTextures(1, &t.depth);
            g.BindTexture(kTexture2D, t.depth);
            g.TexImage2D(kTexture2D, 0, static_cast<GLint>(kDepthComponent32F), t.glW, t.glH, 0, kDepthComponent, kFloat, nullptr);
            g.TexParameteri(kTexture2D, kTextureMinFilter, kNearest);
            g.TexParameteri(kTexture2D, kTextureMagFilter, kNearest);
            g.BindFramebuffer(kFramebuffer, t.fbo);
            g.FramebufferTexture2D(kFramebuffer, kDepthAttachment, kTexture2D, t.depth, 0);
            g.ClearDepth(0.0);
            g.Clear(kDepthBufferBit);
        }
        t.zbp = zbp;
    }

    // ---- textures -------------------------------------------------------------------------------------------------
    static bool isIndexed(uint8_t psm)
    {
        return psm == GS_PSM_T8 || psm == GS_PSM_T8H || psm == GS_PSM_T4 || psm == GS_PSM_T4HL || psm == GS_PSM_T4HH;
    }

    Texture &textureFor(const GSDrawState &s)
    {
        const GSTex0Reg &t = s.context.tex0;
        uint64_t key = t.tbp0 | (static_cast<uint64_t>(t.tbw) << 14) | (static_cast<uint64_t>(t.psm & 63) << 20) |
                       (static_cast<uint64_t>(s.textureWidth) << 26) | (static_cast<uint64_t>(s.textureHeight) << 38);
        key = key * 1099511628211ull ^ (static_cast<uint64_t>(s.texa.ta0) | (static_cast<uint64_t>(s.texa.ta1) << 8) | (static_cast<uint64_t>(s.texa.aem) << 16));
        if (isIndexed(t.psm))
        {
            key = key * 1099511628211ull ^ clutHash;
            key = key * 1099511628211ull ^ (t.cpsm | (t.csm << 8) | (t.csa << 16));
        }
        Texture &tex = textures[key];
        tex.lastUse = ++useCounter;
        if (tex.valid)
        {
            ++stats.texHits;
            return tex;
        }
        ++stats.texDecodes;
        const Api &g = gl();
        std::vector<uint32_t> pixels;
        cpu.DecodeTextureRgba(s, pixels);
        static const int texDumpLimit = std::getenv("GOW_GS_HW_TEXDUMP") ? std::atoi(std::getenv("GOW_GS_HW_TEXDUMP")) : 0;
        static int texDumped = 0;
        if (texDumped < texDumpLimit && s.textureWidth > 16)
        {
            char name[96];
            std::snprintf(name, sizeof(name), "gs_hw_tex_%02d_tbp%x_psm%u_%ux%u.ppm", texDumped++, t.tbp0, t.psm, s.textureWidth, s.textureHeight);
            if (FILE *f = std::fopen(name, "wb"))
            {
                std::fprintf(f, "P6 %u %u 255 ", s.textureWidth, s.textureHeight);
                for (uint32_t px : pixels)
                {
                    const uint8_t rgb[3] = {static_cast<uint8_t>(px), static_cast<uint8_t>(px >> 8), static_cast<uint8_t>(px >> 16)};
                    std::fwrite(rgb, 1, 3, f);
                }
                std::fclose(f);
            }
        }
        tex.width = std::max<uint32_t>(1u, s.textureWidth);
        tex.height = std::max<uint32_t>(1u, s.textureHeight);
        tex.blockLo = t.tbp0;
        tex.blockHi = t.tbp0 + spanBlocks(t.psm, t.tbw, tex.height);
        if (!tex.id)
            g.GenTextures(1, &tex.id);
        g.BindTexture(kTexture2D, tex.id);
        g.PixelStorei(kUnpackAlignment, 4);
        g.TexImage2D(kTexture2D, 0, static_cast<GLint>(kRgba8), tex.width, tex.height, 0, kRgba, kUnsignedByte, pixels.data());
        tex.valid = true;
        if (textures.size() > 6000u)
            evictOld();
        return tex;
    }

    void evictOld()
    {
        const Api &g = gl();
        for (auto it = textures.begin(); it != textures.end();)
        {
            if (it->second.lastUse + 3000u < useCounter)
            {
                if (it->second.id)
                    g.DeleteTextures(1, &it->second.id);
                it = textures.erase(it);
            }
            else
                ++it;
        }
    }

    void invalidateBlocks(uint32_t lo, uint32_t hi)
    {
        for (auto &kv : textures)
        {
            Texture &t = kv.second;
            if (t.valid && t.blockLo < hi && lo < t.blockHi)
                t.valid = false;
        }
    }

    // ---- render target -> shadow VRAM ------------------------------------------------------------------------------
    // Textures / CLUTs that address a render target with another layout (other format, other width, inside it) are decoded from
    // the shadow VRAM, so the target is written back there (downsampled to PS2 resolution, swizzled by the CPU backend).
    void syncTargetToVram(RenderTarget &t)
    {
        if (t.syncedVersion == t.version || t.usedW == 0u || t.usedH == 0u)
            return;
        const Api &g = gl();
        std::vector<uint8_t> px(static_cast<size_t>(t.glW) * t.glH * 4u);
        g.BindFramebuffer(kFramebuffer, t.fbo);
        g.PixelStorei(kPackAlignment, 1);
        g.ReadPixels(0, 0, t.glW, t.glH, kRgba, kUnsignedByte, px.data());
        const uint32_t base = t.fbp * 32u;
        const bool sixteen = t.psm == GS_PSM_CT16 || t.psm == GS_PSM_CT16S;
        for (uint32_t y = 0; y < t.usedH; ++y)
        {
            const uint32_t gy = std::min<uint32_t>(static_cast<uint32_t>((y + 0.5f) * scale), t.glH - 1u);
            for (uint32_t x = 0; x < t.usedW; ++x)
            {
                const uint32_t gx = std::min<uint32_t>(static_cast<uint32_t>((x + 0.5f) * scale), t.glW - 1u);
                const uint8_t *p = &px[(static_cast<size_t>(gy) * t.glW + gx) * 4u];
                uint32_t value = p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24);
                if (sixteen)
                    value = ((p[3] >> 7) << 15) | ((p[2] >> 3) << 10) | ((p[1] >> 3) << 5) | (p[0] >> 3);
                cpu.WriteVram(t.psm, base, t.fbw, x, y, value);
            }
        }
        t.syncedVersion = t.version;
        invalidateBlocks(base, base + spanBlocks(t.psm, t.fbw, t.usedH));
    }

    // does the block range [lo, hi) overlap a render target? then bring that target's pixels into the shadow VRAM first
    std::unordered_map<std::string, int> syncCounts;
    uint32_t probeTbw = 0, probePsm = 0, probeRows = 0, probeFbp = 0, probeFbw = 0, probeFpsm = 0, probeTme = 0;
    size_t probeVerts = 0;
    void syncOverlapping(uint32_t lo, uint32_t hi, const RenderTarget *except)
    {
        for (RenderTarget &t : targets)
        {
            if (&t == except || t.usedW == 0u)
                continue;
            const uint32_t tlo = t.fbp * 32u, thi = tlo + spanBlocks(t.psm, t.fbw, t.usedH);
            if (lo < thi && tlo < hi)
            {
                // VRAM is reused over time: when an upload hit this range after the target was last drawn, the data is the
                // upload's (the target is dead there), not the target's
                bool overwritten = false;
                for (const UploadRecord &u : uploadLog)
                    if (u.seq > t.lastDrawSeq && u.lo < hi && lo < u.hi)
                    {
                        overwritten = true;
                        break;
                    }
                if (overwritten)
                    continue;
                if (statsEnabled)
                {
                    static std::unordered_map<std::string, int> &seen = syncCounts;
                    char key[200];
                    std::snprintf(key, sizeof(key), "tex tbp=%x tbw=%u psm=%u (%u rows) vs RT fbp=%u(blk %x) fbw=%u psm=%u used=%ux%u", lo, probeTbw, probePsm, probeRows, t.fbp, tlo, t.fbw, t.psm, t.usedW, t.usedH);
                    if (seen[key]++ < 0)
                        std::fprintf(stderr, "[gs-hw-reinterp] %s | draw into fbp=%u fbw=%u psm=%u tme=%d verts=%zu\n", key, probeFbp, probeFbw, probeFpsm, probeTme, probeVerts);
                }
                syncTargetToVram(t);
            }
        }
    }

    // The render target whose memory range holds the texture's first block (and was not overwritten by an upload since).
    RenderTarget *findReinterpretSource(uint32_t tbp)
    {
        for (RenderTarget &t : targets)
        {
            if (t.usedW == 0u)
                continue;
            const uint32_t tlo = t.fbp * 32u, thi = tlo + spanBlocks(t.psm, t.fbw, t.usedH);
            if (tbp < tlo || tbp >= thi)
                continue;
            bool overwritten = false;
            for (const UploadRecord &u : uploadLog)
                if (u.seq > t.lastDrawSeq && u.lo <= tbp && tbp < u.hi)
                {
                    overwritten = true;
                    break;
                }
            if (!overwritten)
                return &t;
        }
        return nullptr;
    }

    void setupReinterpret(const GSDrawState &s, RenderTarget &rt)
    {
        const Api &g = gl();
        const GSContext &c = s.context;
        const GSSwizzle::Format &tf = GSSwizzle::GetFormat(c.tex0.psm);
        const GSSwizzle::Format &rf = GSSwizzle::GetFormat(rt.psm);
        const auto offsetOf = [&](const uint16_t *table) { const auto it = tableOffset.find(table); return it == tableOffset.end() ? 0u : it->second; };
        int kind = 3, texelBits = 8;
        switch (tf.kind)
        {
        case GSSwizzle::Kind::Bits32: kind = 0; texelBits = 32; break;
        case GSSwizzle::Kind::Bits24: kind = 1; texelBits = 24; break;
        case GSSwizzle::Kind::Bits16: kind = 2; texelBits = 16; break;
        case GSSwizzle::Kind::Bits8: kind = 3; texelBits = 8; break;
        default: kind = 3; texelBits = 4; break;
        }
        if (kind == 3)
        {
            uint32_t palette[256];
            cpu.BuildPalette(s, palette);
            g.ActiveTexture(kTexture0 + 3);
            g.BindTexture(kTexture2D, paletteTex);
            g.PixelStorei(kUnpackAlignment, 4);
            g.TexSubImage2D(kTexture2D, 0, 0, 0, 256, 1, kRgba, kUnsignedByte, palette);
        }
        g.ActiveTexture(kTexture0 + 1);
        g.BindTexture(kTexture2D, rt.color);
        g.TexParameteri(kTexture2D, kTextureMinFilter, kNearest);
        g.TexParameteri(kTexture2D, kTextureMagFilter, kNearest);
        g.ActiveTexture(kTexture0 + 2);
        g.BindTexture(kTexture2D, tableTex);
        g.ActiveTexture(kTexture0);
        g.Uniform1i(locRtColor, 1);
        g.Uniform1i(locTables, 2);
        g.Uniform1i(locPalette, 3);
        g.Uniform4i(locTexFmt, tf.pageShiftX, tf.pageShiftY, tf.pixelsPerPageShift, tf.unpackedShift);
        g.Uniform4i(locTexFmt2, tf.bitOffset, texelBits, static_cast<GLint>(offsetOf(tf.table)), kind);
        const auto inv = inverseOffset.find(rt.psm);
        g.Uniform4i(locRtFmt, rf.pageShiftX, rf.pageShiftY, rf.pixelsPerPageShift, 1 << rf.unpackedShift);
        g.Uniform4i(locRtFmt2, inv == inverseOffset.end() ? 0 : static_cast<GLint>(inv->second), static_cast<GLint>(rt.fbp), static_cast<GLint>(rt.fbw), 0);
        g.Uniform4i(locTex0, static_cast<GLint>(c.tex0.tbp0), c.tex0.tbw, s.textureWidth, s.textureHeight);
        g.Uniform2f(locRtPx, static_cast<float>(rt.pxW), static_cast<float>(rt.pxH));
    }

    void resolveColorToDepth(RenderTarget &src, RenderTarget &dst)
    {
        if (!resolveProgram || !dst.depth || src.usedW == 0u)
            return;
        const Api &g = gl();
        g.BindFramebuffer(kFramebuffer, dst.fbo);
        g.Viewport(0, 0, dst.glW, dst.glH);
        g.Enable(kScissorTest);
        g.Scissor(0, 0, static_cast<GLsizei>(std::ceil(src.usedW * scale)), static_cast<GLsizei>(std::ceil(src.usedH * scale)));
        g.Disable(kBlend);
        g.ColorMask(kFalse, kFalse, kFalse, kFalse);
        g.Enable(kDepthTest);
        g.DepthFunc(kAlways);
        g.DepthMask(kTrue);
        g.UseProgram(resolveProgram);
        g.ActiveTexture(kTexture0);
        g.BindTexture(kTexture2D, src.color);
        g.TexParameteri(kTexture2D, kTextureMinFilter, kNearest);
        g.TexParameteri(kTexture2D, kTextureMagFilter, kNearest);
        g.Uniform1i(locResolveSrc, 0);
        g.Uniform1f(locResolveDiv, 16777215.0f);
        g.BindVertexArray(resolveVao);
        g.DrawArrays(kTriangles, 0, 3);
        g.TexParameteri(kTexture2D, kTextureMinFilter, kLinear);
        g.TexParameteri(kTexture2D, kTextureMagFilter, kLinear);
        g.Disable(kScissorTest);
        g.ColorMask(kTrue, kTrue, kTrue, kTrue);
        g.BindVertexArray(vao);
        src.depthResolvedVersion = src.version;
    }

    // ---- drawing --------------------------------------------------------------------------------------------------
    static bool sameState(const GSDrawState &a, const GSDrawState &b)
    {
        return std::memcmp(&a, &b, sizeof(GSDrawState)) == 0;
    }

    void addVertex(const GSDrawState &s, const GSVertex &v, float u, float tv, float zmax)
    {
        const GSContext &c = s.context;
        if (pending.empty())
            firstRawZ = v.z;
        HwVertex h;
        h.x = v.x - static_cast<float>(c.xyoffset.ofx) / 16.0f + 0.5f;
        h.y = v.y - static_cast<float>(c.xyoffset.ofy) / 16.0f + 0.5f;
        // the Z buffer keeps only its low bits (Z24: 24, Z16: 16): a vertex Z of 1<<24 is stored as 0
        double zq = std::max(v.z, 0.0);
        if (zmax < 4294967295.0f)
            zq = static_cast<double>(static_cast<uint64_t>(zq) & (zmax < 70000.0f ? 0xFFFFull : 0xFFFFFFull));
        h.z = static_cast<float>(std::min(zq / zmax, 1.0));
        h.r = v.r; h.g = v.g; h.b = v.b; h.a = v.a;
        h.s = v.s; h.t = v.t; h.q = v.q;
        h.u = u; h.v = tv;
        pending.push_back(h);
    }

    static bool family16(uint8_t psm) { return psm == GS_PSM_CT16 || psm == GS_PSM_CT16S; }

    static float zMaxFor(uint8_t zpsm)
    {
        switch (zpsm)
        {
        case 0x30: return 4294967295.0f;
        case 0x32: case 0x3A: return 65535.0f;
        default: return 16777215.0f;
        }
    }

    void submit(const GSPrimitiveBatch &b)
    {
        const GSDrawState &s = b.state;
        const GSPrimType type = s.prim.type;
        const bool tri = type == GS_PRIM_TRIANGLE || type == GS_PRIM_TRISTRIP || type == GS_PRIM_TRIFAN;
        const bool sprite = type == GS_PRIM_SPRITE;
        if (!tri && !sprite)
        {
            ++stats.skippedPrim;
            return;
        }
        const uint8_t fpsm = s.context.frame.psm;
        if (fpsm != GS_PSM_CT32 && fpsm != GS_PSM_CT24 && fpsm != GS_PSM_CT16 && fpsm != GS_PSM_CT16S)
        {
            ++stats.skippedPsm;
            return;
        }
        // a draw through a different-format view (CT16) of memory that a CT32/CT24 target already owns writes bit fields of its
        // pixels; it cannot be rasterised into the RGBA target at the same coordinates
        for (const RenderTarget &t : targets)
            if (t.fbp == s.context.frame.fbp && t.usedW != 0u && family16(t.psm) != family16(fpsm))
            {
                ++stats.skippedPsm;
                return;
            }
        {
            // draws that only fill a single colour plane (e.g. FBMSK=ffff00ff: G only) build an auxiliary mask that the game
            // reads back through a differently laid out view; that readback is not reproduced yet, so keep it out of the image
            const uint32_t m = s.context.frame.fbmsk;
            const int open = ((m & 0xFFu) != 0xFFu) + (((m >> 8) & 0xFFu) != 0xFFu) + (((m >> 16) & 0xFFu) != 0xFFu);
            if (open == 1 && (m >> 24) == 0xFFu && s.context.frame.fbmsk != 0xFFFFFFFFu)
            {
                ++stats.skippedPrim;
                return;
            }
        }
        if (havePending && !sameState(pendingState, s))
            flushPending();
        if (!havePending)
        {
            pendingState = s;
            havePending = true;
        }
        const float zmax = zMaxFor(s.context.zbuf.psm);
        const float texW = static_cast<float>(std::max<uint16_t>(s.textureWidth, 1)), texH = static_cast<float>(std::max<uint16_t>(s.textureHeight, 1));
        const auto texelUV = [&](const GSVertex &v, float &u, float &t)
        {
            if (s.prim.fst)
            {
                u = static_cast<float>(v.u) / 16.0f;
                t = static_cast<float>(v.v) / 16.0f;
            }
            else
            {
                u = 0.0f;
                t = 0.0f;
            }
        };
        if (tri)
        {
            for (int i = 0; i < 3; ++i)
            {
                float u, t;
                texelUV(b.vertices[i], u, t);
                addVertex(s, b.vertices[i], u, t, zmax);
            }
        }
        else
        {
            const GSVertex &a = b.vertices[0];
            const GSVertex &c = b.vertices[1];
            float u0, v0, u1, v1;
            if (s.prim.fst)
            {
                u0 = a.u / 16.0f; v0 = a.v / 16.0f; u1 = c.u / 16.0f; v1 = c.v / 16.0f;
            }
            else
            {
                const float q0 = std::max(std::fabs(a.q), 1e-20f), q1 = std::max(std::fabs(c.q), 1e-20f);
                u0 = a.s / q0 * texW; v0 = a.t / q0 * texH; u1 = c.s / q1 * texW; v1 = c.t / q1 * texH;
            }
            GSDrawState sp = s; // sprites carry texel UVs, never STQ
            (void)sp;
            GSVertex corner[4] = {a, a, a, a};
            const float xs[2] = {a.x, c.x}, ys[2] = {a.y, c.y};
            const float us[2] = {u0, u1}, vs[2] = {v0, v1};
            const int order[6][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 0}, {1, 1}, {0, 1}};
            for (const auto &o : order)
            {
                GSVertex v = c; // colour, z, fog come from the second vertex
                v.x = xs[o[0]];
                v.y = ys[o[1]];
                addVertex(s, v, us[o[0]], vs[o[1]], zmax);
                pending.back().s = us[o[0]] / texW;
                pending.back().t = vs[o[1]] / texH;
                pending.back().q = 1.0f;
            }
            (void)corner;
        }
        ++stats.draws;
    }

    void flushPending()
    {
        if (!havePending || pending.empty())
        {
            havePending = false;
            pending.clear();
            return;
        }
        havePending = false;
        if (!ensure())
        {
            pending.clear();
            return;
        }
        draw(pendingState, pending);
        pending.clear();
    }

    void blendState(const GSDrawState &s, bool &enabled, GLenum &sf, GLenum &df, float &fixScale, float &constant, GLenum &equation)
    {
        enabled = false;
        equation = kFuncAdd;
        sf = kOne; df = kZero;
        fixScale = 1.0f / 128.0f;
        constant = 0.0f;
        if (!s.prim.abe)
            return;
        const uint64_t alpha = s.context.alpha;
        const uint32_t a = alpha & 3, b = (alpha >> 2) & 3, c = (alpha >> 4) & 3, d = (alpha >> 6) & 3;
        const uint32_t fix = static_cast<uint32_t>((alpha >> 32) & 0xFF);
        GLenum cs, cd; // factor C and 1-C
        if (c == 0) { cs = kSrc1Alpha; cd = kOneMinusSrc1Alpha; }
        else if (c == 2) { cs = kConstantColor; cd = kOneMinusConstantColor; constant = std::min(fix / 128.0f, 1.0f); }
        else { ++stats.unsupportedBlend; cs = kSrc1Alpha; cd = kOneMinusSrc1Alpha; }
        if (a == b)
        {
            // (A-A)*C + D = D
            if (d == 0) return;            // Cs: plain write
            enabled = true; sf = kZero; df = kOne; return; // Cd: destination unchanged
        }
        enabled = true;
        if (a == 0 && b == 1 && d == 1) { sf = cs; df = cd; }            // Cs*C + Cd*(1-C)
        else if (a == 0 && b == 2 && d == 1) { sf = cs; df = kOne; }     // Cs*C + Cd
        else if (a == 2 && b == 1 && d == 1) { sf = kZero; df = cd; }    // Cd*(1-C)
        else if (a == 0 && b == 2 && d == 2) { sf = cs; df = kZero; }    // Cs*C
        else if (a == 2 && b == 0 && d == 2) { sf = kZero; df = kZero; } // 0
        else if (a == 2 && b == 0 && d == 1) { sf = cs; df = kOne; equation = kFuncReverseSubtract; } // Cd - Cs*C
        else { ++stats.unsupportedBlend; sf = cs; df = cd; }
    }

    int dumpPresents = std::getenv("GOW_GS_HW_DUMP") ? std::atoi(std::getenv("GOW_GS_HW_DUMP")) : -1;
    int presentCount = 0, dumped = 0, dumpedAt = 0;

    int onlyLo = std::getenv("GOW_GS_HW_ONLY") ? std::atoi(std::getenv("GOW_GS_HW_ONLY")) : -1;
    int onlyHi = std::getenv("GOW_GS_HW_ONLY") && std::strchr(std::getenv("GOW_GS_HW_ONLY"), '-') ? std::atoi(std::strchr(std::getenv("GOW_GS_HW_ONLY"), '-') + 1) : 1 << 30;
    int frameBatch = 0;

    void draw(const GSDrawState &s, const std::vector<HwVertex> &verts)
    {
        const Api &g = gl();
        ++frameBatch;
        if (onlyLo >= 0 && (frameBatch < onlyLo || frameBatch > onlyHi) && s.context.frame.fbp == 0 && s.context.frame.fbw == 8 && !(s.prim.tme && s.context.tex0.tbp0 == 0))
            return;
        const GSContext &c = s.context;
        static const double dumpAtSec = std::getenv("GOW_GS_HW_DUMPAT") ? std::atof(std::getenv("GOW_GS_HW_DUMPAT")) : -1.0;
        static const auto t0 = std::chrono::steady_clock::now();
        const double nowSec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        if (dumpAtSec >= 0.0 && nowSec >= dumpAtSec && dumpedAt == 0 && frameBatch == 1)
            dumpedAt = 1;
        if (dumpedAt == 1 && dumped < 800 && dumpPresents < 0)
            dumpPresents = 0, presentCount = 0;
        if (dumpPresents >= 0 && presentCount >= dumpPresents && presentCount < dumpPresents + 3 && dumped < 800 && (c.frame.fbp == 208 || c.frame.fbp == 320 || dumped < 40))
        {
            ++dumped;
            const HwVertex &v0 = verts[0];
            std::fprintf(stderr,
                         "[gs-hw-dump] #%d verts=%zu fbp=%u fbw=%u fpsm=%u zbp=%x zpsm=%x zmsk=%d tme=%d tbp=%x tbw=%u tpsm=%u %ux%u cbp=%x test=%llx alpha=%llx abe=%d fbmsk=%x rawz=%.0f v0=(%.1f,%.1f z=%.3f col=%.0f,%.0f,%.0f,%.0f uv=%.1f,%.1f) scis=%u-%u,%u-%u\n",
                         dumped, verts.size(), c.frame.fbp, c.frame.fbw, c.frame.psm, c.zbuf.zbp, c.zbuf.psm, c.zbuf.zmask ? 1 : 0, s.prim.tme ? 1 : 0,
                         c.tex0.tbp0, c.tex0.tbw, c.tex0.psm, s.textureWidth, s.textureHeight, c.tex0.cbp, (unsigned long long)c.test,
                         (unsigned long long)c.alpha, s.prim.abe ? 1 : 0, c.frame.fbmsk, firstRawZ, v0.x, v0.y, v0.z, v0.r, v0.g, v0.b, v0.a, v0.u, v0.v,
                         c.scissor.x0, c.scissor.x1, c.scissor.y0, c.scissor.y1);
        }
        RenderTarget &rt = target(c.frame.fbp, c.frame.fbw);
        attachDepth(rt, c.zbuf.zbp);
        for (RenderTarget &z : targets)
            if (&z != &rt && z.fbp == c.zbuf.zbp && z.version != z.depthResolvedVersion && (z.psm == GS_PSM_CT24 || z.psm == GS_PSM_CT32))
                resolveColorToDepth(z, rt);
        rt.psm = c.frame.psm;
        ++rt.version;
        rt.lastDrawSeq = ++writeSeq;
        {
            // real extent of what was drawn (vertices clipped to the scissor), not the scissor itself
            float maxX = 0.0f, maxY = 0.0f;
            for (const HwVertex &v : verts)
            {
                maxX = std::max(maxX, v.x);
                maxY = std::max(maxY, v.y);
            }
            const uint32_t bx = static_cast<uint32_t>(std::min<float>(std::max(maxX, 0.0f), static_cast<float>(c.scissor.x1 + 1u)));
            const uint32_t by = static_cast<uint32_t>(std::min<float>(std::max(maxY, 0.0f), static_cast<float>(c.scissor.y1 + 1u)));
            rt.usedW = std::max<uint32_t>(rt.usedW, std::min<uint32_t>(bx, rt.pxW));
            rt.usedH = std::max<uint32_t>(rt.usedH, std::min<uint32_t>(by, rt.pxH));
        }
        g.BindFramebuffer(kFramebuffer, rt.fbo);
        g.Viewport(0, 0, rt.glW, rt.glH);
        g.Enable(kScissorTest);
        const int sx0 = c.scissor.x0, sy0 = c.scissor.y0;
        const int sw = std::max(0, static_cast<int>(c.scissor.x1) - sx0 + 1), sh = std::max(0, static_cast<int>(c.scissor.y1) - sy0 + 1);
        g.Scissor(static_cast<GLint>(std::floor(sx0 * scale)), static_cast<GLint>(std::floor(sy0 * scale)),
                  static_cast<GLsizei>(std::ceil(sw * scale)), static_cast<GLsizei>(std::ceil(sh * scale)));
        g.Disable(kCullFace);

        // depth
        const uint32_t ztst = static_cast<uint32_t>((c.test >> 17) & 3u);
        static const bool noDepth = std::getenv("GOW_GS_HW_NODEPTH") != nullptr;
        if (noDepth) g.Disable(kDepthTest); else g.Enable(kDepthTest);
        g.DepthFunc(ztst == 0 ? kNever : ztst == 1 ? kAlways : ztst == 2 ? kGEqual : kGreater);
        const bool zwrite = !c.zbuf.zmask;

        // colour mask
        const uint32_t fbmsk = c.frame.fbmsk;
        const GLboolean mr = (fbmsk & 0xFF) == 0xFF ? kFalse : kTrue, mg = (fbmsk & 0xFF00) == 0xFF00 ? kFalse : kTrue,
                        mb = (fbmsk & 0xFF0000) == 0xFF0000 ? kFalse : kTrue, ma = (fbmsk & 0xFF000000u) == 0xFF000000u ? kFalse : kTrue;

        bool blend = false;
        GLenum sf, df, equation;
        float fixScale, constant;
        blendState(s, blend, sf, df, fixScale, constant, equation);
        if (blend)
        {
            g.Enable(kBlend);
            g.BlendEquationSeparate(equation, kFuncAdd);
            g.BlendFuncSeparate(sf, df, kOne, kZero);
            g.BlendColor(constant, constant, constant, constant);
        }
        else
            g.Disable(kBlend);

        g.UseProgram(program);
        {
            const bool date = ((c.test >> 14) & 1u) != 0u && c.frame.psm != GS_PSM_CT24;
            g.Uniform1i(locDate, date ? 1 : 0);
            g.Uniform1i(locDatm, ((c.test >> 15) & 1u) != 0u ? 1 : 0);
            if (date)
            {
                g.ActiveTexture(kTexture0 + 4);
                g.BindTexture(kTexture2D, rt.color);
                g.Uniform1i(locDest, 4);
                g.ActiveTexture(kTexture0);
                g.TextureBarrier();
            }
        }
        g.Uniform2f(locInvSize, 1.0f / rt.pxW, 1.0f / rt.pxH);
        g.Uniform1i(locTme, s.prim.tme ? 1 : 0);
        g.Uniform1i(locFst, s.prim.fst ? 1 : 0);
        g.Uniform1i(locIip, s.prim.iip ? 1 : 0);
        g.Uniform1i(locTfx, c.tex0.tfx);
        g.Uniform1i(locTcc, c.tex0.tcc);
        g.Uniform1f(locBlendScale, 1.0f / 128.0f);
        if (s.prim.tme)
        {
            // a texture that lives in a render target of the same format is sampled straight from it
            RenderTarget *src = nullptr;
            const auto family = [](uint8_t psm) { return (psm == GS_PSM_CT16 || psm == GS_PSM_CT16S) ? 16 : (psm == GS_PSM_CT32 || psm == GS_PSM_CT24) ? 32 : 0; };
            if (family(c.tex0.psm) != 0 && (c.tex0.tbp0 % 32u) == 0u)
                if (RenderTarget *candidate = findTarget(c.tex0.tbp0 / 32u))
                    if (family(candidate->psm) == family(c.tex0.psm))
                        src = candidate;
            if (src == &rt)
                g.TextureBarrier();
            Texture dummy;
            RenderTarget *zsrc = nullptr;
            const bool zFormat = c.tex0.psm == 0x30 || c.tex0.psm == 0x31 || c.tex0.psm == 0x32 || c.tex0.psm == 0x3A;
            if (zFormat && (c.tex0.tbp0 % 32u) == 0u)
                for (RenderTarget &t : targets)
                    if (t.depth && t.zbp == c.tex0.tbp0 / 32u)
                        zsrc = &t;
            if (zsrc)
                src = nullptr;
            RenderTarget *rsrc = nullptr;
            if (!src && !zsrc)
                rsrc = findReinterpretSource(c.tex0.tbp0);
            if (rsrc == &rt)
                g.TextureBarrier();
            Texture &tex = (src || rsrc || zsrc) ? dummy : textureFor(s);
            g.ActiveTexture(kTexture0);
            g.BindTexture(kTexture2D, zsrc ? zsrc->depth : src ? src->color : rsrc ? rsrc->color : tex.id);
            g.Uniform1i(locZTex, !zsrc ? 0 : c.tex0.psm == 0x32 || c.tex0.psm == 0x3A ? 16 : c.tex0.psm == 0x31 ? 24 : 32);
            g.Uniform1f(locZMax, zsrc ? zMaxFor(static_cast<uint8_t>(c.tex0.psm)) : 1.0f);
            g.Uniform1i(locRtTex24, !(src || rsrc) ? 0 : c.tex0.psm == GS_PSM_CT24 ? 1 : family16(c.tex0.psm) ? 2 : 0);
            g.Uniform1i(locReinterp, rsrc ? 1 : 0);
            if (rsrc)
                setupReinterpret(s, *rsrc);
            g.Uniform1f(locTa0, static_cast<float>(s.texa.ta0));
            g.Uniform1f(locTa1, static_cast<float>(s.texa.ta1));
            g.Uniform1i(locAem, s.texa.aem ? 1 : 0);
            const GLint filter = s.linearFilter ? kLinear : kNearest;
            g.TexParameteri(kTexture2D, kTextureMinFilter, filter);
            g.TexParameteri(kTexture2D, kTextureMagFilter, filter);
            const uint64_t clamp = c.clamp;
            const GLint wu = (clamp & 3u) == 0u ? kRepeat : kClampToEdge, wv = ((clamp >> 2) & 3u) == 0u ? kRepeat : kClampToEdge;
            g.TexParameteri(kTexture2D, kTextureWrapS, wu);
            g.TexParameteri(kTexture2D, kTextureWrapT, wv);
            g.Uniform1i(locTex, 0);
            g.Uniform2f(locTexDim, static_cast<float>(std::max<uint16_t>(s.textureWidth, 1)), static_cast<float>(std::max<uint16_t>(s.textureHeight, 1)));
            if (zsrc)
            {
                g.Uniform2f(locTexSize, static_cast<float>(zsrc->pxW), static_cast<float>(zsrc->pxH));
                g.TexParameteri(kTexture2D, kTextureMinFilter, kNearest);
                g.TexParameteri(kTexture2D, kTextureMagFilter, kNearest);
            }
            else if (src)
                g.Uniform2f(locTexSize, static_cast<float>(src->pxW), static_cast<float>(src->pxH));
            else
                g.Uniform2f(locTexSize, static_cast<float>(tex.width), static_cast<float>(tex.height));
        }
        g.BindVertexArray(vao);
        g.BindBuffer(kArrayBuffer, vbo);
        g.BufferData(kArrayBuffer, static_cast<GLsizeiptr>(verts.size() * sizeof(HwVertex)), verts.data(), kStreamDraw);

        const bool ate = (c.test & 1u) != 0u;
        const uint32_t atst = static_cast<uint32_t>((c.test >> 1) & 7u), aref = static_cast<uint32_t>((c.test >> 4) & 0xFFu);
        const uint32_t afail = static_cast<uint32_t>((c.test >> 12) & 3u);
        g.Uniform1i(locAte, ate ? 1 : 0);
        g.Uniform1i(locAtst, static_cast<GLint>(atst));
        g.Uniform1i(locAref, static_cast<GLint>(aref));
        static const bool forceNoTest = std::getenv("GOW_GS_HW_NOATEST") != nullptr;
        g.Uniform1i(locPass, (ate && !forceNoTest) ? 1 : 0);
        static const int traceAt = std::getenv("GOW_GS_HW_DEPTHTRACE") ? std::atoi(std::getenv("GOW_GS_HW_DEPTHTRACE")) : -1;
        const bool tracing = traceAt >= 0 && presentCount >= traceAt && presentCount < traceAt + 1 && rt.depth && rt.fbp == 0;
        const auto depthStats = [&](const char *when)
        {
            std::vector<float> dz(static_cast<size_t>(rt.glW) * rt.glH);
            g.BindFramebuffer(kFramebuffer, rt.fbo);
            g.ReadPixels(0, 0, rt.glW, rt.glH, kDepthComponent, kFloat, dz.data());
            double mx = 0.0;
            size_t nz = 0;
            for (float v : dz) { mx = std::max<double>(mx, v); nz += v > 0.0f; }
            std::fprintf(stderr, "[gs-hw-depth] batch %d %s: max=%.3f nonzero=%zu/%zu (this batch z=%.3f zmsk=%d ztst=%u verts=%zu tbp=%x)\n", frameBatch, when, mx, nz, dz.size(),
                         verts[0].z, c.zbuf.zmask ? 1 : 0, ztst, verts.size(), c.tex0.tbp0);
        };
        if (tracing) depthStats("before");
        g.ColorMask(mr, mg, mb, ma);
        g.DepthMask(zwrite ? kTrue : kFalse);
        g.DrawArrays(kTriangles, 0, static_cast<GLsizei>(verts.size()));
        static const bool noFailPass = std::getenv("GOW_GS_HW_NOFAIL") != nullptr;
        if (ate && afail != 0u && !noFailPass)
        {
            // failing fragments: 1 FB_ONLY (colour+alpha, no Z), 2 ZB_ONLY (Z only), 3 RGB_ONLY (colour, no alpha, no Z)
            g.Uniform1i(locPass, 2);
            if (afail == 1u) { g.DepthMask(kFalse); }
            else if (afail == 2u) { g.ColorMask(kFalse, kFalse, kFalse, kFalse); }
            else { g.DepthMask(kFalse); g.ColorMask(mr, mg, mb, kFalse); }
            g.DrawArrays(kTriangles, 0, static_cast<GLsizei>(verts.size()));
        }
        ++stats.batches;
        g.Disable(kScissorTest);
        static const int rtDumpAt = std::getenv("GOW_GS_HW_RTDUMP") ? std::atoi(std::getenv("GOW_GS_HW_RTDUMP")) : -1;
        static const double rtDumpSec = std::getenv("GOW_GS_HW_RTDUMPAT") ? std::atof(std::getenv("GOW_GS_HW_RTDUMPAT")) : -1.0;
        static const auto rtT0 = std::chrono::steady_clock::now();
        static int rtDumpPresent = -1;
        if (rtDumpSec >= 0.0 && rtDumpPresent < 0 && frameBatch == 1 && std::chrono::duration<double>(std::chrono::steady_clock::now() - rtT0).count() >= rtDumpSec)
            rtDumpPresent = presentCount;
        if (((rtDumpAt >= 0 && presentCount == rtDumpAt) || (rtDumpPresent >= 0 && presentCount == rtDumpPresent)) && (rt.fbp == 0 || rt.fbp == 208) && frameBatch >= 26 && frameBatch <= 46)
        {
            std::vector<uint8_t> px(static_cast<size_t>(rt.glW) * rt.glH * 4u);
            g.BindFramebuffer(kFramebuffer, rt.fbo);
            g.PixelStorei(kPackAlignment, 1);
            g.ReadPixels(0, 0, rt.glW, rt.glH, kRgba, kUnsignedByte, px.data());
            char name[64];
            std::snprintf(name, sizeof(name), "gs_hw_rt%u_b%03d.ppm", rt.fbp, frameBatch);
            if (FILE *f = std::fopen(name, "wb"))
            {
                std::fprintf(f, "P6 %u %u 255 ", rt.glW, rt.glH);
                for (size_t i = 0; i + 3 < px.size(); i += 4)
                    std::fwrite(&px[i], 1, 3, f);
                std::fclose(f);
            }
        }

        static int errorsShown = 0;
        if (errorsShown < 8)
            if (const GLenum err = g.GetError())
                std::fprintf(stderr, "[gs-hw] GL error 0x%x after batch %llu\n", err, (unsigned long long)stats.batches), ++errorsShown;
    }

    void reportStats()
    {
        if (!statsEnabled)
            return;
        const auto now = std::chrono::steady_clock::now();
        if (now - stats.last < std::chrono::seconds(5))
            return;
        stats.last = now;
        std::fprintf(stderr, "[gs-hw] draws=%llu batches=%llu texDecodes=%llu texHits=%llu skippedPsm=%llu skippedPrim=%llu unsupportedBlend=%llu textures=%zu\n",
                     (unsigned long long)stats.draws, (unsigned long long)stats.batches, (unsigned long long)stats.texDecodes,
                     (unsigned long long)stats.texHits, (unsigned long long)stats.skippedPsm, (unsigned long long)stats.skippedPrim,
                     (unsigned long long)stats.unsupportedBlend, textures.size());
        for (const auto &kv : syncCounts)
            if (kv.second > 5)
                std::fprintf(stderr, "[gs-hw-sync] x%d %s\n", kv.second, kv.first.c_str());
        syncCounts.clear();
        stats.draws = stats.batches = stats.texDecodes = stats.texHits = stats.skippedPsm = stats.skippedPrim = stats.unsupportedBlend = 0;
    }

    RenderTarget *findTarget(uint32_t fbp)
    {
        for (RenderTarget &t : targets)
            if (t.fbp == fbp)
                return &t;
        return nullptr;
    }
};

GSHwBackend::GSHwBackend() : m_impl(std::make_unique<Impl>()) {}
GSHwBackend::~GSHwBackend() = default;

void GSHwBackend::Initialize(uint8_t *vram, uint32_t vramSize)
{
    m_impl->vram = vram;
    m_impl->vramSize = vramSize;
    m_impl->cpu.Initialize(vram, vramSize);
}

void GSHwBackend::Reset()
{
    Impl &d = *m_impl;
    d.flushPending();
    d.cpu.Reset();
    for (auto &kv : d.textures)
        kv.second.valid = false;
}

void GSHwBackend::Submit(const GSPrimitiveBatch &batch)
{
    if (batch.vertexCount == 0u)
        return;
    Impl &d = *m_impl;
    if (!d.ensure())
        return;
    d.submit(batch);
}

void GSHwBackend::LoadClut(const GSTex0Reg &tex0, const GSTexClutReg &texclut)
{
    Impl &d = *m_impl;
    d.flushPending();
    if (d.initialized && (tex0.psm == GS_PSM_T8 || tex0.psm == GS_PSM_T8H || tex0.psm == GS_PSM_T4 || tex0.psm == GS_PSM_T4HL || tex0.psm == GS_PSM_T4HH))
        d.syncOverlapping(tex0.cbp, tex0.cbp + 64u, nullptr);
    d.cpu.LoadClut(tex0, texclut);
    d.clutHash = d.cpu.ClutHash();
}

void GSHwBackend::BeginTransfer(const GSTransferCommand &command)
{
    Impl &d = *m_impl;
    d.flushPending();
    d.cpu.BeginTransfer(command);
    if (command.direction == 0u || command.direction == 2u)
    {
        const uint32_t rows = static_cast<uint32_t>(command.trxpos.dsay) + command.trxreg.rrh;
        const uint32_t lo = command.bitbltbuf.dbp, hi = lo + spanBlocks(command.bitbltbuf.dpsm, command.bitbltbuf.dbw, rows);
        d.invalidateBlocks(lo, hi);
        if (d.uploadLog.size() > 4096u)
            d.uploadLog.erase(d.uploadLog.begin(), d.uploadLog.begin() + 2048);
        d.uploadLog.push_back({lo, hi, ++d.writeSeq});
    }
}

void GSHwBackend::UploadImage(const uint8_t *data, uint32_t sizeBytes)
{
    m_impl->cpu.UploadImage(data, sizeBytes);
}

void GSHwBackend::Flush()
{
    m_impl->flushPending();
    m_impl->cpu.Flush();
}

void GSHwBackend::TextureFlush()
{
    m_impl->flushPending();
    m_impl->cpu.TextureFlush();
}

void GSHwBackend::Sync(GSSyncReason reason)
{
    m_impl->flushPending();
    m_impl->cpu.Sync(reason);
}

bool GSHwBackend::ClearFramebuffer(const GSContext &context, uint32_t rgba)
{
    Impl &d = *m_impl;
    if (!d.ensure() || context.frame.fbw == 0u)
        return false;
    d.flushPending();
    const Api &g = d.gl();
    RenderTarget &rt = d.target(context.frame.fbp, context.frame.fbw);
    g.BindFramebuffer(kFramebuffer, rt.fbo);
    g.Enable(kScissorTest);
    const int sx0 = context.scissor.x0, sy0 = context.scissor.y0;
    g.Scissor(static_cast<GLint>(std::floor(sx0 * d.scale)), static_cast<GLint>(std::floor(sy0 * d.scale)),
              static_cast<GLsizei>(std::ceil((context.scissor.x1 - sx0 + 1) * d.scale)),
              static_cast<GLsizei>(std::ceil((context.scissor.y1 - sy0 + 1) * d.scale)));
    g.ColorMask(kTrue, kTrue, kTrue, kTrue);
    g.ClearColor((rgba & 0xFF) / 255.0f, ((rgba >> 8) & 0xFF) / 255.0f, ((rgba >> 16) & 0xFF) / 255.0f, ((rgba >> 24) & 0xFF) / 255.0f);
    g.Clear(kColorBufferBit);
    g.Disable(kScissorTest);
    return true;
}

uint32_t GSHwBackend::ConsumeLocalToHostBytes(uint8_t *dst, uint32_t maxBytes)
{
    return m_impl->cpu.ConsumeLocalToHostBytes(dst, maxBytes);
}

uint32_t GSHwBackend::ReadVram(uint32_t psm, uint32_t base, uint32_t bw, uint32_t x, uint32_t y) const
{
    return m_impl->cpu.ReadVram(psm, base, bw, x, y);
}

void GSHwBackend::WriteVram(uint32_t psm, uint32_t base, uint32_t bw, uint32_t x, uint32_t y, uint32_t value)
{
    m_impl->cpu.WriteVram(psm, base, bw, x, y, value);
}

void GSHwBackend::SnapshotVram(std::vector<uint8_t> &out) const
{
    m_impl->cpu.SnapshotVram(out);
}

GSTransferSnapshot GSHwBackend::GetTransferSnapshot() const
{
    return m_impl->cpu.GetTransferSnapshot();
}

PresentationFrame GSHwBackend::Present(const GSPresentationRequest &request)
{
    Impl &d = *m_impl;
    PresentationFrame frame;
    if (!d.ensure())
        return frame;
    d.flushPending();
    ++d.presentCount;
    d.frameBatch = 0;
    d.reportStats();
    const bool enable1 = (request.pmode & 1ull) != 0ull, enable2 = (request.pmode & 2ull) != 0ull;
    const auto decode = [&](bool enabled, uint64_t dispfb, uint64_t display, uint32_t &fbp, uint32_t &fbw, uint32_t &dbx, uint32_t &dby,
                            uint32_t &w, uint32_t &h)
    {
        fbp = static_cast<uint32_t>(dispfb & 0x1FFu);
        fbw = std::max<uint32_t>(1u, static_cast<uint32_t>((dispfb >> 9) & 0x3Fu));
        dbx = static_cast<uint32_t>((dispfb >> 32) & 0x7FFu);
        dby = static_cast<uint32_t>((dispfb >> 43) & 0x7FFu);
        const uint32_t hdiv = static_cast<uint32_t>((display >> 23) & 0xFu) + 1u;
        const uint32_t vdiv = static_cast<uint32_t>((display >> 27) & 0x3u) + 1u;
        w = (static_cast<uint32_t>((display >> 32) & 0xFFFu) + 1u) / hdiv;
        h = (static_cast<uint32_t>((display >> 44) & 0x7FFu) + 1u) / vdiv;
        return enabled && display != 0u && w != 0u && h != 0u;
    };
    if (d.statsEnabled)
    {
        static int shown = 0;
        if (shown++ < 6)
            std::fprintf(stderr, "[gs-hw] present pmode=%llx smode2=%llx dispfb1=%llx display1=%llx dispfb2=%llx display2=%llx\n",
                         (unsigned long long)request.pmode, (unsigned long long)request.smode2, (unsigned long long)request.dispfb1,
                         (unsigned long long)request.display1, (unsigned long long)request.dispfb2, (unsigned long long)request.display2);
    }
    uint32_t fbp, fbw, dbx, dby, w, h;
    bool ok = enable1 && decode(true, request.dispfb1, request.display1, fbp, fbw, dbx, dby, w, h);
    if (!ok)
        ok = enable2 && decode(true, request.dispfb2, request.display2, fbp, fbw, dbx, dby, w, h);
    if (!ok)
        return frame;
    RenderTarget *rt = d.findTarget(fbp);
    frame.displayFbp = fbp;
    frame.sourceFbp = fbp;
    const uint32_t outW = static_cast<uint32_t>(std::ceil(w * d.scale)), outH = static_cast<uint32_t>(std::ceil(h * d.scale));
    frame.width = outW;
    frame.height = outH;
    frame.pixels.assign(static_cast<size_t>(outW) * outH * 4u, 0u);
    if (!rt)
        return frame;
    const Api &g = d.gl();
    g.BindFramebuffer(kFramebuffer, rt->fbo);
    g.PixelStorei(kPackAlignment, 1);
    static const int depthDumpAt = std::getenv("GOW_GS_HW_DEPTHDUMP") ? std::atoi(std::getenv("GOW_GS_HW_DEPTHDUMP")) : -1;
    if (depthDumpAt >= 0 && d.presentCount == depthDumpAt)
    {
        RenderTarget *main = d.findTarget(0);
        if (main && main->depth)
        {
            std::vector<float> depth(static_cast<size_t>(main->glW) * main->glH);
            g.BindFramebuffer(kFramebuffer, main->fbo);
            g.ReadPixels(0, 0, main->glW, main->glH, kDepthComponent, kFloat, depth.data());
            if (FILE *f = std::fopen("gs_hw_depth.pgm", "wb"))
            {
                std::fprintf(f, "P5 %u %u 255 ", main->glW, main->glH);
                for (float v : depth)
                {
                    const uint8_t b = static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, v * 255.0f)));
                    std::fwrite(&b, 1, 1, f);
                }
                std::fclose(f);
            }
            g.BindFramebuffer(kFramebuffer, rt->fbo);
        }
    }

    const uint32_t sx = static_cast<uint32_t>(dbx * d.scale), sy = static_cast<uint32_t>(dby * d.scale);
    const uint32_t cw = std::min(outW, rt->glW > sx ? rt->glW - sx : 0u), ch = std::min(outH, rt->glH > sy ? rt->glH - sy : 0u);
    std::vector<uint8_t> tmp(static_cast<size_t>(cw) * ch * 4u);
    g.ReadPixels(sx, sy, cw, ch, kRgba, kUnsignedByte, tmp.data());
    for (uint32_t y = 0; y < ch; ++y)
        std::memcpy(frame.pixels.data() + static_cast<size_t>(y) * outW * 4u, tmp.data() + static_cast<size_t>(y) * cw * 4u, static_cast<size_t>(cw) * 4u);
    for (size_t i = 3; i < frame.pixels.size(); i += 4)
        frame.pixels[i] = 255;
    // the frontend packs presentation rows with a fixed 640 pixel stride
    if (frame.width < 640u)
    {
        std::vector<uint8_t> rows(static_cast<size_t>(640u) * frame.height * 4u, 0u);
        for (uint32_t y = 0; y < frame.height; ++y)
            std::memcpy(rows.data() + static_cast<size_t>(y) * 640u * 4u, frame.pixels.data() + static_cast<size_t>(y) * frame.width * 4u,
                        static_cast<size_t>(frame.width) * 4u);
        frame.pixels = std::move(rows);
    }
    return frame;
}
