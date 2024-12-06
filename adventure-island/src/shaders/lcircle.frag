#ifdef GL_ES
precision mediump float;
#endif

uniform vec2    u_resolution;
uniform float   u_time;

#include "lygia/space/ratio.glsl"
#include "lygia/math/decimate.glsl"
#include "lygia/draw/circle.glsl"

void main(void) {
    vec3 color = vec3(0.0);
    vec2 st = gl_FragCoord.xy/u_resolution.xy;
    st = ratio(st, u_resolution);
    
    color = vec3(st.x,st.y,abs(sin(u_time)));
    color = decimate(color, 20.);
    color += circle(st, .15, .02);
    
    gl_FragColor = vec4(color, 1.0);
}
