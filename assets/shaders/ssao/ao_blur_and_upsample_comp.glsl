// Based on SSAO in Microsoft's MiniEngine (https://github.com/Microsoft/DirectX-Graphics-Samples/tree/master/MiniEngine)
// Original Copyright (c) 2013-2015 Microsoft (MIT Licence)
// Transliterated to GLSL compute in 2017 by https://github.com/ddiakopoulos

#version 450

#ifdef COMBINE_LOWER_RESOLUTIONS
    uniform sampler2D LoResAO2;
#endif

#ifdef BLEND_WITH_HIGHER_RESOLUTION
    uniform sampler2D HiResAO;
#endif

layout (binding = 0, r32f) writeonly uniform image2D AoResult;

uniform sampler2D LoResDB;
uniform sampler2D HiResDB;
uniform sampler2D LoResAO1;

uniform vec2  InvLowResolution;
uniform vec2  InvHighResolution;
uniform float NoiseFilterStrength;
uniform float StepSize;
uniform float kBlurTolerance;
uniform float kUpsampleTolerance;

shared float DepthCache[256];
shared float AOCache1[256];
shared float AOCache2[256];

void GroupMemoryBarrierWithGroupSync()
{
    groupMemoryBarrier();
    barrier();
}

void PrefetchData(uint index, vec2 uv)
{
    vec4 AO1 = textureGather(LoResAO1, uv, 0);

#ifdef COMBINE_LOWER_RESOLUTIONS
    AO1 = min(AO1, textureGather(LoResAO2, uv, 0));
#endif

    AOCache1[index] = AO1.w;
    AOCache1[index + 1] = AO1.z;
    AOCache1[index + 16] = AO1.x;
    AOCache1[index + 17] = AO1.y;

    vec4 ID = vec4(1.0) / textureGather(LoResDB, uv, 0);

    DepthCache[index] = ID.w;
    DepthCache[index + 1] = ID.z;
    DepthCache[index + 16] = ID.x;
    DepthCache[index + 17] = ID.y;
}

float SmartBlur(float a, float b, float c, float d, float e, bool Left, bool Middle, bool Right)
{
    b = (Left || Middle) ? b : c;
    a = Left ? a : b;
    d = (Right || Middle) ? d : c;
    e = Right ? e : d;
    return ((a + e) / 2.0 + b + c + d) / 4.0;
}

bool CompareDeltas(float d1, float d2, float l1, float l2)
{
    float temp = d1 * d2 + StepSize;
    return temp * temp > l1 * l2 * kBlurTolerance;
}

void BlurHorizontally(uint leftMostIndex)
{
    float a0 = AOCache1[leftMostIndex];
    float a1 = AOCache1[leftMostIndex + 1];
    float a2 = AOCache1[leftMostIndex + 2];
    float a3 = AOCache1[leftMostIndex + 3];
    float a4 = AOCache1[leftMostIndex + 4];
    float a5 = AOCache1[leftMostIndex + 5];
    float a6 = AOCache1[leftMostIndex + 6];

    float d0 = DepthCache[leftMostIndex];
    float d1 = DepthCache[leftMostIndex + 1];
    float d2 = DepthCache[leftMostIndex + 2];
    float d3 = DepthCache[leftMostIndex + 3];
    float d4 = DepthCache[leftMostIndex + 4];
    float d5 = DepthCache[leftMostIndex + 5];
    float d6 = DepthCache[leftMostIndex + 6];

    float d01 = d1 - d0;
    float d12 = d2 - d1;
    float d23 = d3 - d2;
    float d34 = d4 - d3;
    float d45 = d5 - d4;
    float d56 = d6 - d5;

    float l01 = d01 * d01 + StepSize;
    float l12 = d12 * d12 + StepSize;
    float l23 = d23 * d23 + StepSize;
    float l34 = d34 * d34 + StepSize;
    float l45 = d45 * d45 + StepSize;
    float l56 = d56 * d56 + StepSize;

    bool c02 = CompareDeltas(d01, d12, l01, l12);
    bool c13 = CompareDeltas(d12, d23, l12, l23);
    bool c24 = CompareDeltas(d23, d34, l23, l34);
    bool c35 = CompareDeltas(d34, d45, l34, l45);
    bool c46 = CompareDeltas(d45, d56, l45, l56);

    AOCache2[leftMostIndex] = SmartBlur(a0, a1, a2, a3, a4, c02, c13, c24);
    AOCache2[leftMostIndex + 1] = SmartBlur(a1, a2, a3, a4, a5, c13, c24, c35);
    AOCache2[leftMostIndex + 2] = SmartBlur(a2, a3, a4, a5, a6, c24, c35, c46);
}

void BlurVertically(uint topMostIndex)
{
    float a0 = AOCache2[topMostIndex];
    float a1 = AOCache2[topMostIndex + 16];
    float a2 = AOCache2[topMostIndex + 32];
    float a3 = AOCache2[topMostIndex + 48];
    float a4 = AOCache2[topMostIndex + 64];
    float a5 = AOCache2[topMostIndex + 80];

    float d0 = DepthCache[topMostIndex + 2];
    float d1 = DepthCache[topMostIndex + 18];
    float d2 = DepthCache[topMostIndex + 34];
    float d3 = DepthCache[topMostIndex + 50];
    float d4 = DepthCache[topMostIndex + 66];
    float d5 = DepthCache[topMostIndex + 82];

    float d01 = d1 - d0;
    float d12 = d2 - d1;
    float d23 = d3 - d2;
    float d34 = d4 - d3;
    float d45 = d5 - d4;

    float l01 = d01 * d01 + StepSize;
    float l12 = d12 * d12 + StepSize;
    float l23 = d23 * d23 + StepSize;
    float l34 = d34 * d34 + StepSize;
    float l45 = d45 * d45 + StepSize;

    bool c02 = CompareDeltas(d01, d12, l01, l12);
    bool c13 = CompareDeltas(d12, d23, l12, l23);
    bool c24 = CompareDeltas(d23, d34, l23, l34);
    bool c35 = CompareDeltas(d34, d45, l34, l45);

    float aoResult1 = SmartBlur(a0, a1, a2, a3, a4, c02, c13, c24);
    float aoResult2 = SmartBlur(a1, a2, a3, a4, a5, c13, c24, c35);

    AOCache1[topMostIndex] = aoResult1;
    AOCache1[topMostIndex + 16] = aoResult2;
}

// We essentially want 5 weights:  4 for each low-res pixel and 1 to blend in when none of the 4 really
// match.  The filter strength is 1 / DeltaZTolerance.  So a tolerance of 0.01 would yield a strength of 100.
// Note that a perfect match of low to high depths would yield a weight of 10^6, completely superceding any
// noise filtering.  The noise filter is intended to soften the effects of shimmering when the high-res depth
// buffer has a lot of small holes in it causing the low-res depth buffer to inaccurately represent it.
float BilateralUpsample(float HiDepth, float HiAO, vec4 LowDepths, vec4 LowAO)
{
    vec4 weights = vec4(9, 3, 1, 3) / (abs(vec4(HiDepth - LowDepths)) + kUpsampleTolerance);
    float TotalWeight = dot(float(weights), 1.0) + NoiseFilterStrength;
    float WeightedSum = dot(LowAO, weights) + NoiseFilterStrength;
    return HiAO * WeightedSum / TotalWeight;
}

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

void main()
{
    // Load 4 pixels per thread into LDS to fill the 16x16 LDS cache with depth and AO
    PrefetchData(gl_LocalInvocationID.x << 1 | gl_LocalInvocationID.y << 5, 
        vec2(ivec2(gl_GlobalInvocationID.xy + gl_LocalInvocationID.xy - uvec2(2))) * InvLowResolution);
   
    GroupMemoryBarrierWithGroupSync();
    
    // Goal:  End up with a 9x9 patch that is blurred so we can upsample.  Blur radius is 2 pixels, so start with 13x13 area.
    // Horizontally blur the pixels. 13x13 -> 9x13
    if (gl_LocalInvocationIndex < 39)
    {
        BlurHorizontally((gl_LocalInvocationIndex / 3) * 16 + (gl_LocalInvocationIndex % 3) * 3);
    }

    GroupMemoryBarrierWithGroupSync();
    
    // Vertically blur the pixels. 9x13 -> 9x9
    if (gl_LocalInvocationIndex < 45) 
    {
        BlurVertically((gl_LocalInvocationIndex / 9) * 32 + gl_LocalInvocationIndex % 9);
    }

    GroupMemoryBarrierWithGroupSync();

    // Bilateral upsample
    uint Idx0 = gl_LocalInvocationID.x + gl_LocalInvocationID.y * 16;
    vec4 LoSSAOs = vec4(AOCache1[Idx0 + 16], AOCache1[Idx0 + 17], AOCache1[Idx0 + 1], AOCache1[Idx0]);
   
    // We work on a quad of pixels at once because then we can gather 4 each of high and low-res depth values
    vec2 UV0 = vec2(gl_GlobalInvocationID.xy * InvLowResolution);
    vec2 UV1 = vec2(gl_GlobalInvocationID.xy * 2 * InvHighResolution);

#ifdef BLEND_WITH_HIGHER_RESOLUTION
    vec4 HiSSAOs  = textureGather(HiResAO, UV1, 0);
#else
    vec4 HiSSAOs = vec4(2.0); // (MEGA HACK WARNING) DEPTH BUFFER SCALING ISSUES - THIS SHOULD BE 1.0
#endif

    vec4 LoDepths = textureGather(LoResDB, UV0, 0);
    vec4 HiDepths = textureGather(HiResDB, UV1, 0);

    ivec2 OutST = ivec2(gl_GlobalInvocationID.xy << uvec2(1));

    imageStore(AoResult, OutST + ivec2(-1,  0), vec4(BilateralUpsample(HiDepths.x, HiSSAOs.x, LoDepths.xyzw, LoSSAOs.xyzw)));
    imageStore(AoResult, OutST + ivec2( 0,  0), vec4(BilateralUpsample(HiDepths.y, HiSSAOs.y, LoDepths.yzwx, LoSSAOs.yzwx)));
    imageStore(AoResult, OutST + ivec2( 0, -1), vec4(BilateralUpsample(HiDepths.z, HiSSAOs.z, LoDepths.zwxy, LoSSAOs.zwxy)));
    imageStore(AoResult, OutST + ivec2(-1, -1), vec4(BilateralUpsample(HiDepths.w, HiSSAOs.w, LoDepths.wxyz, LoSSAOs.wxyz)));
}
