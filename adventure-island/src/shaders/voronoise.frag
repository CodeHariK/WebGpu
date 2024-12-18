#ifdef GL_ES
precision mediump float;
#endif

uniform vec2    u_resolution;
uniform float   u_time;

#include "lygia/generative/voronoise.glsl"

void main(void) {
    vec4 color = vec4(vec3(0.0), 1.0);
    vec2 pixel = 1.0/u_resolution.xy;
    vec2 st = gl_FragCoord.xy * pixel;
    vec2 p = 0.5 - 0.5*cos( u_time *vec2(1.0,0.5) );
    
	p = p*p*(3.0-2.0*p);
	p = p*p*(3.0-2.0*p);
	p = p*p*(3.0-2.0*p);
	
	color += voronoise( vec3(24.0*st, u_time), p.x, 1. );

    gl_FragColor = color;
}
