/*
contributors: Patricio Gonzalez Vivo, Anton Marini
description: Porter Duff Source In Compositing
use: <float4> compositeSourceIn(<float4> src, <float4> dst)
license:
    - Copyright (c) 2021 Patricio Gonzalez Vivo under Prosperity License - https://prosperitylicense.com/versions/3.0.0
    - Copyright (c) 2021 Patricio Gonzalez Vivo under Patron License - https://lygia.xyz/license
*/

#ifndef FNC_COMPOSITE_SRC_IN
#define FNC_COMPOSITE_SRC_IN

float compositeSourceIn(float src, float dst) {
    return src * dst;
}

float3 compositeSourceIn(float3 srcColor, float3 dstColor, float srcAlpha, float dstAlpha) {
    return srcColor * dstAlpha;
}

float4 compositeSourceIn(float4 srcColor, float4 dstColor) {
    float4 result;

    result.rgb = compositeSourceIn(srcColor.rgb, dstColor.rgb, srcColor.a, dstColor.a);
    result.a = compositeSourceIn(srcColor.a, dstColor.a);

    return result;
}

#endif