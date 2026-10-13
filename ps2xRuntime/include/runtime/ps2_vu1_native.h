// GOW-Port: native VU1 backend. Basic blocks of the game's VU1 microcode are translated ahead of time (tools/vu1/vu1n_gen.py)
// into straight-line SSE functions that use immediate, timing-free semantics: the VU interlocks on every hazard, so the
// *values* a program computes never depend on cycle timing; only the MAC/CLIP flag registers have a visible latency, and
// the generator keeps their order. The exact engine (VU1Interpreter) stays as fallback for blocks that were not translated
// and as the oracle for GOW_VU1N_VERIFY.
#ifndef PS2_VU1_NATIVE_H
#define PS2_VU1_NATIVE_H

#include "runtime/ps2_vu1.h"

#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <cstring>
#include <immintrin.h>

class PS2Memory;

struct VU1NativeCtx
{
    VU1State *s = nullptr;
    uint8_t *data = nullptr;
    uint32_t mask = 0u; // dataSize - 1
    VU1Interpreter *vu = nullptr;
    void *context = nullptr; // VU1Interpreter::StepContext *
    const uint8_t *cyc = nullptr; // issue cycle of every pair of the running block, from the exact engine's stall model
    uint64_t start = 0;           // exact-engine cycle at which the running block's pair 0 issues
};

// Return value of a native block: next pc in the low 16 bits; kEnded when the program finished; kBail when the exact
// engine must continue at the returned pc (D/T halt enabled, ...).
constexpr uint32_t kVu1nEnded = 0x80000000u;
constexpr uint32_t kVu1nBail = 0x40000000u;
using VU1NativeFn = uint32_t (*)(VU1NativeCtx &);

// Registered by the generated tables: key = FNV-1a over (pc, block words), see vu1n::blockKey.
void vu1nRegisterBlock(uint64_t key, VU1NativeFn fn, uint32_t pairs);

// Callouts into the exact engine for the rare instructions (defined in ps2_vu1_core.cpp).
struct VU1NativeAccess
{
    // Flags across block boundaries, through the exact engine's flag pipeline (cycle = start + rel): the MAC / clip value
    // a reader issued at rel sees, the clip chain value CLIP shifts (newest, in flight included), and queueing a writer
    // issued at rel (visible kFmacLatency later).
    static uint32_t macAt(VU1NativeCtx &n, uint32_t rel);
    static uint32_t clipAt(VU1NativeCtx &n, uint32_t rel);
    static uint32_t clipChain(VU1NativeCtx &n);
    static void pushMac(VU1NativeCtx &n, uint32_t rel, uint32_t mac);
    static void pushClip(VU1NativeCtx &n, uint32_t rel, uint32_t clip);
    static void retireFlagsAt(VU1Interpreter &vu, uint64_t cycle);
    static void callLower(VU1NativeCtx &n, uint32_t pc, uint32_t lower, uint32_t upper);
    static void callUpper(VU1NativeCtx &n, uint32_t pc, uint32_t upper);
    static void xgkick(VU1NativeCtx &n, uint32_t qwordAddress);
    // GOW_VU1N_RECORD / GOW_VU1N_REPLAY (bit-exact A/B of generator changes, see ps2_vu1_native.inl)
    static void recordReplay(VU1Interpreter &vu, void *context, uint32_t maxCycles);
    static uint64_t silentNativeHash(VU1Interpreter &vu, void *context);
};

struct VU1Interpreter::NativeState
{
    uint64_t generation = ~0ull;
    VU1NativeFn fn[2048];
    uint16_t pairs[2048];
    const uint8_t *cyc[2048];
    bool known[2048];
    bool entryFlags[2048]; // the block reads flags written before it (see runNative)
    bool enabled = std::getenv("GOW_VU1_NATIVE_OFF") == nullptr;
    unsigned verifyLeft = std::getenv("GOW_VU1N_VERIFY") ? static_cast<unsigned>(std::atoi(std::getenv("GOW_VU1N_VERIFY"))) : 0u;
    bool diag = std::getenv("GOW_VU1N_DIAG") != nullptr;
    unsigned long long nativePairs = 0, missBlocks = 0, nativeRuns = 0;
    std::unordered_map<uint32_t, unsigned> missPcs, missHits;
    unsigned limit = ~0u, count = 0; // verify bisection: native blocks allowed in this run
    VU1State snap;                   // state on entry of block number `limit` (verify bisection)
    std::vector<uint32_t> trace;     // pcs of the native blocks executed while bisecting
};

// The VU arithmetic must not be reassociated, contracted or assumed finite (the runtime builds with /fp:fast).
#if defined(_MSC_VER)
#pragma float_control(precise, on, push)
#pragma fp_contract(off)
#endif

#if defined(_MSC_VER)
#define VU1N_INLINE __forceinline
#define VU1N_NOINLINE __declspec(noinline)
#else
#define VU1N_INLINE inline __attribute__((always_inline))
#define VU1N_NOINLINE __attribute__((noinline))
#endif

namespace vu1n
{
    VU1N_INLINE uint64_t fnv(uint64_t h, uint32_t w)
    {
        for (int i = 0; i < 4; ++i)
            h = (h ^ ((w >> (i * 8)) & 0xFFu)) * 1099511628211ull;
        return h;
    }
    VU1N_INLINE uint64_t blockKey(uint32_t pc, const uint32_t *words, uint32_t count)
    {
        uint64_t h = fnv(1469598103934665603ull, pc);
        for (uint32_t i = 0; i < count; ++i)
            h = fnv(h, words[i]);
        return h;
    }

    // lane masks: DEST field order (x = 8) -> SSE blend/movemask order (x = 1).
    constexpr int kRev4[16] = {0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};

    VU1N_INLINE __m128 ld(const VU1State &s, int r) { return _mm_loadu_ps(s.vf[r]); }
    // Operand view of a register: Inf/NaN (exponent 255) act as +-FLT_MAX, as in the exact engine's normalizeOperand. The register
    // itself keeps its raw bits (integer data travels through VF registers).
    VU1N_INLINE __m128 nrm(__m128 v)
    {
        const __m128i b = _mm_castps_si128(v);
        const __m128i expMask = _mm_set1_epi32(0x7F800000);
        const __m128i bad = _mm_cmpeq_epi32(_mm_and_si128(b, expMask), expMask);
        const __m128i fixed = _mm_or_si128(_mm_and_si128(b, _mm_set1_epi32(static_cast<int>(0x80000000u))), _mm_set1_epi32(0x7F7FFFFF));
        return _mm_castsi128_ps(_mm_blendv_epi8(b, fixed, bad));
    }
    VU1N_INLINE __m128 ldn(const VU1State &s, int r) { return nrm(_mm_loadu_ps(s.vf[r])); }
    VU1N_INLINE __m128 ldAcc(const VU1State &s) { return _mm_loadu_ps(s.acc); }
    template <int M> // M in SSE order
    VU1N_INLINE void stf(float *p, __m128 v)
    {
        if constexpr (M == 15)
            _mm_storeu_ps(p, v);
        else if constexpr (M != 0)
            _mm_storeu_ps(p, _mm_blend_ps(_mm_loadu_ps(p), v, M));
    }
    template <int C>
    VU1N_INLINE __m128 bc(__m128 v) { return _mm_shuffle_ps(v, v, _MM_SHUFFLE(C, C, C, C)); }
    VU1N_INLINE __m128 clampv(__m128 r)
    {
        return _mm_min_ps(_mm_max_ps(r, _mm_set1_ps(-FLT_MAX)), _mm_set1_ps(FLT_MAX));
    }
    // MAC register after an FMAC: per-lane zero / sign / overflow (underflow is not tracked); lanes outside DEST are 0.
    template <int M>
    VU1N_INLINE void macFlags(VU1State &s, __m128 r)
    {
        const __m128i b = _mm_castps_si128(r);
        const __m128i expMask = _mm_set1_epi32(0x7F800000);
        const int z = _mm_movemask_ps(_mm_castsi128_ps(_mm_cmpeq_epi32(_mm_and_si128(b, _mm_set1_epi32(0x7FFFFFFF)), _mm_setzero_si128()))) & M;
        const int sg = _mm_movemask_ps(r) & M;
        const int o = _mm_movemask_ps(_mm_castsi128_ps(_mm_cmpeq_epi32(_mm_and_si128(b, expMask), expMask))) & M;
        s.mac = static_cast<uint32_t>(kRev4[z] | (kRev4[sg] << 4) | (kRev4[o] << 12));
    }
    // acc +/- a*b. The exact engine evaluates it unrounded, so a product that overflows float can still cancel against ACC:
    // when the float result is not finite the lanes are redone in double (out of line: rare, and keeping it out lets the
    // fast path inline into the generated blocks; MSVC x64 passes __m128 arguments of a real call through memory).
    template <bool Sub>
    VU1N_NOINLINE __m128 maddWide(__m128 acc, __m128 a, __m128 b)
    {
        alignas(16) float fa[4], fb[4], fc[4], out[4];
        _mm_store_ps(fa, acc);
        _mm_store_ps(fb, a);
        _mm_store_ps(fc, b);
        for (int i = 0; i < 4; ++i)
        {
            const double x = Sub ? static_cast<double>(fa[i]) - static_cast<double>(fb[i]) * static_cast<double>(fc[i])
                                 : static_cast<double>(fa[i]) + static_cast<double>(fb[i]) * static_cast<double>(fc[i]);
            out[i] = x > static_cast<double>(FLT_MAX) ? HUGE_VALF : x < -static_cast<double>(FLT_MAX) ? -HUGE_VALF : static_cast<float>(x);
        }
        return _mm_load_ps(out);
    }
    template <bool Sub>
    VU1N_INLINE __m128 madd(__m128 acc, __m128 a, __m128 b)
    {
        const __m128 p = _mm_mul_ps(a, b);
        __m128 r = Sub ? _mm_sub_ps(acc, p) : _mm_add_ps(acc, p);
        // round-toward-zero saturates an overflowing product to FLT_MAX instead of infinity: any lane at/over FLT_MAX in the product
        // or the sum needs the wide evaluation
        const __m128i lim = _mm_set1_epi32(0x7F7FFFFE);
        const __m128i hot = _mm_or_si128(_mm_cmpgt_epi32(_mm_and_si128(_mm_castps_si128(p), _mm_set1_epi32(0x7FFFFFFF)), lim),
                                         _mm_cmpgt_epi32(_mm_and_si128(_mm_castps_si128(r), _mm_set1_epi32(0x7FFFFFFF)), lim));
        if (_mm_movemask_epi8(hot) == 0)
            return r;
        return maddWide<Sub>(acc, a, b);
    }
    template <int M>
    VU1N_INLINE uint32_t macVal(__m128 r)
    {
        const __m128i b = _mm_castps_si128(r);
        const __m128i expMask = _mm_set1_epi32(0x7F800000);
        const int z = _mm_movemask_ps(_mm_castsi128_ps(_mm_cmpeq_epi32(_mm_and_si128(b, _mm_set1_epi32(0x7FFFFFFF)), _mm_setzero_si128()))) & M;
        const int sg = _mm_movemask_ps(r) & M;
        // round-toward-zero turns an overflow into the largest finite value, so that counts as overflow too
        const __m128i big = _mm_cmpeq_epi32(_mm_and_si128(b, _mm_set1_epi32(0x7FFFFFFF)), _mm_set1_epi32(0x7F7FFFFF));
        const int o = _mm_movemask_ps(_mm_castsi128_ps(_mm_or_si128(_mm_cmpeq_epi32(_mm_and_si128(b, expMask), expMask), big))) & M;
        return static_cast<uint32_t>(kRev4[z] | (kRev4[sg] << 4) | (kRev4[o] << 12));
    }
    VU1N_INLINE uint32_t clipVal(const VU1State &s, int fs, int ft, uint32_t prev)
    {
        alignas(16) float v[4], t[4];
        _mm_store_ps(v, ldn(s, fs));
        _mm_store_ps(t, ldn(s, ft));
        const float w = std::fabs(t[3]);
        uint32_t f = 0;
        if (v[0] > w) f |= 0x01u;
        if (v[0] < -w) f |= 0x02u;
        if (v[1] > w) f |= 0x04u;
        if (v[1] < -w) f |= 0x08u;
        if (v[2] > w) f |= 0x10u;
        if (v[2] < -w) f |= 0x20u;
        return ((prev << 6) | f) & 0xFFFFFFu;
    }
    VU1N_INLINE float qOperand(const VU1State &s) { return s.q; }

    // --- integer / memory helpers -------------------------------------------------------------------------------
    VU1N_INLINE uint32_t qaddr(const VU1NativeCtx &n, int32_t qw) { return (static_cast<uint32_t>(qw) * 16u) & n.mask; }
    VU1N_INLINE __m128 ldq(const VU1NativeCtx &n, uint32_t addr) { return _mm_loadu_ps(reinterpret_cast<const float *>(n.data + addr)); }
    template <int M>
    VU1N_INLINE void stq(VU1NativeCtx &n, uint32_t addr, __m128 v) { stf<M>(reinterpret_cast<float *>(n.data + addr), v); }
    VU1N_INLINE int32_t sx16(int32_t v) { return static_cast<int16_t>(v); }
    VU1N_INLINE int32_t ldWord(const VU1NativeCtx &n, uint32_t addr, int comp)
    {
        uint32_t v;
        std::memcpy(&v, n.data + addr + comp * 4, 4);
        return static_cast<int16_t>(v & 0xFFFFu);
    }
    VU1N_INLINE void stWords(VU1NativeCtx &n, uint32_t addr, int32_t value, int destMask)
    {
        const uint32_t w = static_cast<uint16_t>(value & 0xFFFF);
        for (int c = 0; c < 4; ++c)
            if (destMask & (8 >> c))
                std::memcpy(n.data + addr + c * 4, &w, 4);
    }
    VU1N_INLINE __m128 fromInt(int32_t v)
    {
        const float f = [&] { float x; std::memcpy(&x, &v, 4); return x; }();
        return _mm_set1_ps(f);
    }
    VU1N_INLINE int32_t compBits(const VU1State &s, int reg, int comp)
    {
        uint32_t v;
        std::memcpy(&v, &s.vf[reg][comp], 4);
        return static_cast<int16_t>(v & 0xFFFFu);
    }

    // --- FMAC special ops ----------------------------------------------------------------------------------------
    template <int Shift, int M>
    VU1N_INLINE void itof(VU1State &s, int fs, int ft)
    {
        const __m128 v = _mm_cvtepi32_ps(_mm_castps_si128(ld(s, fs))); // integer bits: raw
        __m128 r = v;
        if constexpr (Shift == 4) r = _mm_mul_ps(v, _mm_set1_ps(1.0f / 16.0f));
        if constexpr (Shift == 12) r = _mm_mul_ps(v, _mm_set1_ps(1.0f / 4096.0f));
        if constexpr (Shift == 15) r = _mm_mul_ps(v, _mm_set1_ps(1.0f / 32768.0f));
        stf<M>(s.vf[ft], r);
    }
    template <int Shift, int M>
    VU1N_INLINE void ftoi(VU1State &s, int fs, int ft)
    {
        __m128 v = ldn(s, fs);
        constexpr float scale = Shift == 0 ? 1.0f : Shift == 4 ? 16.0f : Shift == 12 ? 4096.0f : 32768.0f;
        v = _mm_mul_ps(clampv(v), _mm_set1_ps(scale));
        // saturate like floatToInt: >= 2^31 -> INT_MAX (cvtt gives INT_MIN for out of range)
        const __m128 big = _mm_cmpge_ps(v, _mm_set1_ps(2147483648.0f));
        __m128i i = _mm_cvttps_epi32(v);
        i = _mm_castps_si128(_mm_blendv_ps(_mm_castsi128_ps(i), _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF)), big));
        stf<M>(s.vf[ft], _mm_castsi128_ps(i));
    }
    template <int M>
    VU1N_INLINE void absOp(VU1State &s, int fs, int ft)
    {
        stf<M>(s.vf[ft], _mm_and_ps(ldn(s, fs), _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF))));
    }
    VU1N_INLINE void clipOp(VU1State &s, int fs, int ft)
    {
        const float *v = s.vf[fs];
        const float w = std::fabs(s.vf[ft][3]);
        uint32_t f = 0;
        if (v[0] > w) f |= 0x01u;
        if (v[0] < -w) f |= 0x02u;
        if (v[1] > w) f |= 0x04u;
        if (v[1] < -w) f |= 0x08u;
        if (v[2] > w) f |= 0x10u;
        if (v[2] < -w) f |= 0x20u;
        s.clip = ((s.clip << 6) | f) & 0xFFFFFFu;
    }

    // --- Q unit --------------------------------------------------------------------------------------------------
    VU1N_INLINE float qnorm(float x)
    {
        uint32_t b;
        std::memcpy(&b, &x, 4);
        const uint32_t e = (b >> 23) & 0xFFu;
        if (e == 0u) b &= 0x80000000u;
        else if (e == 255u) b = (b & 0x80000000u) | 0x7F7FFFFFu;
        std::memcpy(&x, &b, 4);
        return x;
    }
    VU1N_INLINE float qfinish(float x)
    {
        uint32_t b;
        std::memcpy(&b, &x, 4);
        const uint32_t e = (b >> 23) & 0xFFu;
        if (e == 0u) b &= 0x80000000u;
        else if (e == 255u) b = (b & 0x80000000u) | 0x7F7FFFFFu;
        std::memcpy(&x, &b, 4);
        return x;
    }
    VU1N_INLINE void divOp(VU1State &s, int fs, int fsf, int ft, int ftf)
    {
        const float num = qnorm(s.vf[fs][fsf]), den = qnorm(s.vf[ft][ftf]);
        float r;
        if (den == 0.0f)
            r = (std::signbit(num) != std::signbit(den)) ? -FLT_MAX : FLT_MAX;
        else
            r = num / den;
        s.q = qfinish(r);
    }
    VU1N_INLINE void sqrtOp(VU1State &s, int ft, int ftf) { s.q = qfinish(std::sqrt(std::fabs(qnorm(s.vf[ft][ftf])))); }
    VU1N_INLINE void rsqrtOp(VU1State &s, int fs, int fsf, int ft, int ftf)
    {
        const float num = qnorm(s.vf[fs][fsf]);
        const float den = std::sqrt(std::fabs(qnorm(s.vf[ft][ftf])));
        float r;
        if (den != 0.0f)
            r = num / den;
        else
            r = std::signbit(num) ? -FLT_MAX : FLT_MAX;
        s.q = qfinish(r);
    }

    // --- register-file-in-locals variants (generated blocks keep VF/VI/ACC/Q/I in C++ locals; same semantics as above) ---
    inline uint32_t qa(uint32_t mask, int32_t qw) { return (static_cast<uint32_t>(qw) * 16u) & mask; }
    inline __m128 ldm(const uint8_t *mem, uint32_t addr) { return _mm_loadu_ps(reinterpret_cast<const float *>(mem + addr)); }
    template <int M>
    inline void stm(uint8_t *mem, uint32_t addr, __m128 v) { stf<M>(reinterpret_cast<float *>(mem + addr), v); }
    inline int32_t ldWordm(const uint8_t *mem, uint32_t addr, int comp)
    {
        uint32_t v;
        std::memcpy(&v, mem + addr + comp * 4, 4);
        return static_cast<int16_t>(v & 0xFFFFu);
    }
    inline void stWordsm(uint8_t *mem, uint32_t addr, int32_t value, int destMask)
    {
        const uint32_t w = static_cast<uint16_t>(value & 0xFFFF);
        for (int c = 0; c < 4; ++c)
            if (destMask & (8 >> c))
                std::memcpy(mem + addr + c * 4, &w, 4);
    }
    template <int C> // raw lane (bits preserved)
    inline float lane(__m128 v)
    {
        uint32_t b = static_cast<uint32_t>(_mm_cvtsi128_si32(_mm_castps_si128(_mm_shuffle_ps(v, v, _MM_SHUFFLE(C, C, C, C)))));
        float f;
        std::memcpy(&f, &b, 4);
        return f;
    }
    template <int C>
    inline int32_t laneBits16(__m128 v)
    {
        const uint32_t b = static_cast<uint32_t>(_mm_cvtsi128_si32(_mm_castps_si128(_mm_shuffle_ps(v, v, _MM_SHUFFLE(C, C, C, C)))));
        return static_cast<int16_t>(b & 0xFFFFu);
    }
    template <int Shift>
    inline __m128 itofv(__m128 raw)
    {
        const __m128 v = _mm_cvtepi32_ps(_mm_castps_si128(raw));
        if constexpr (Shift == 4) return _mm_mul_ps(v, _mm_set1_ps(1.0f / 16.0f));
        if constexpr (Shift == 12) return _mm_mul_ps(v, _mm_set1_ps(1.0f / 4096.0f));
        if constexpr (Shift == 15) return _mm_mul_ps(v, _mm_set1_ps(1.0f / 32768.0f));
        return v;
    }
    template <int Shift>
    inline __m128 ftoiv(__m128 normalized)
    {
        constexpr float scale = Shift == 0 ? 1.0f : Shift == 4 ? 16.0f : Shift == 12 ? 4096.0f : 32768.0f;
        const __m128 v = _mm_mul_ps(clampv(normalized), _mm_set1_ps(scale));
        const __m128 big = _mm_cmpge_ps(v, _mm_set1_ps(2147483648.0f));
        const __m128i i = _mm_cvttps_epi32(v);
        return _mm_blendv_ps(_mm_castsi128_ps(i), _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF)), big);
    }
    inline __m128 absv(__m128 normalized) { return _mm_and_ps(normalized, _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF))); }
    inline uint32_t clipValv(__m128 vn, __m128 tn, uint32_t prev)
    {
        alignas(16) float v[4], t[4];
        _mm_store_ps(v, vn);
        _mm_store_ps(t, tn);
        const float w = std::fabs(t[3]);
        uint32_t f = 0;
        if (v[0] > w) f |= 0x01u;
        if (v[0] < -w) f |= 0x02u;
        if (v[1] > w) f |= 0x04u;
        if (v[1] < -w) f |= 0x08u;
        if (v[2] > w) f |= 0x10u;
        if (v[2] < -w) f |= 0x20u;
        return ((prev << 6) | f) & 0xFFFFFFu;
    }
    inline float divv(float fsRaw, float ftRaw)
    {
        const float num = qnorm(fsRaw), den = qnorm(ftRaw);
        float r;
        if (den == 0.0f)
            r = (std::signbit(num) != std::signbit(den)) ? -FLT_MAX : FLT_MAX;
        else
            r = num / den;
        return qfinish(r);
    }
    inline float sqrtv(float ftRaw) { return qfinish(std::sqrt(std::fabs(qnorm(ftRaw)))); }
    inline float rsqrtv(float fsRaw, float ftRaw)
    {
        const float num = qnorm(fsRaw);
        const float den = std::sqrt(std::fabs(qnorm(ftRaw)));
        float r;
        if (den != 0.0f)
            r = num / den;
        else
            r = std::signbit(num) ? -FLT_MAX : FLT_MAX;
        return qfinish(r);
    }
}

#if defined(_MSC_VER)
#pragma float_control(pop)
#endif

#endif
