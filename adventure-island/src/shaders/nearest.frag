#ifdef GL_ES
precision mediump float;
#endif

uniform sampler2D   u_tex0; // /imgs/noise.png
uniform vec2        u_tex0Resolution;
uniform vec2        u_resolution;

#include "lygia/sample/nearest.glsl"

void main (void) {
    vec4 color = vec4(0.0, 0.0, 0.0, 1.0);
    vec2 pixel = 1.0/u_resolution.xy;
    vec2 st = gl_FragCoord.xy * pixel;
    vec2 uv = st * 0.5;

    color.rgb = mix(texture2D(u_tex0, uv).rgb, 
                    sampleNearest(u_tex0, uv, u_tex0Resolution).rgb, 
                    step(0.5, st.x));
        
    gl_FragColor = color;
}
