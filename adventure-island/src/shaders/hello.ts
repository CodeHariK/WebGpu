import * as THREE from 'three';
import lcircleFrag from "./lcircle.frag";
import nearestFrag from "./nearest.frag";
import graphFrag from "./graph.frag";
import voronoiseFrag from "./voronoise.frag";


// Vertex Shader
const vertexShader0 = `
varying vec3 vNormal;
varying vec3 worldSpacePosition;

void main() {
    vNormal = normalMatrix * normal; // Transform normal into world space
    worldSpacePosition = vec3(modelViewMatrix * vec4(position, 1.0)); // Compute position in view space
    gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); // Final position
}
`;

// Fragment Shader
const fragmentShader0 = `
#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;
uniform vec2 u_mouse;
uniform float u_time;

void main() {
    vec2 st = gl_FragCoord.xy / u_resolution;
    gl_FragColor = vec4(st.x, st.y, 0.0, 1.0);
}
`;

const fragmentShader1 = `
#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;
uniform vec2 u_mouse;
uniform float u_time;

void main() {
    vec2 st = gl_FragCoord.xy/u_resolution.xy;
    st.x *= u_resolution.x/u_resolution.y;

    vec3 color = vec3(0.);
    color = vec3(st.x,st.y,abs(sin(u_time)));

    gl_FragColor = vec4(color,1.0);
}
`

const fragmentShader2 = `
#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;
uniform vec2 u_mouse;
uniform float u_time;

// Plot a line on Y using a value between 0.0-1.0
float plot(vec2 st) {    
    return smoothstep(0.02, 0.0, abs(st.y - st.x));
}

void main() {
	vec2 st = gl_FragCoord.xy/u_resolution;

    float y = st.x;

    vec3 color = vec3(y);

    // Plot a line
    float pct = plot(st);
    color = (1.0-pct)*color+pct*vec3(0.0,1.0,0.0);

	gl_FragColor = vec4(color,1.0);
}
`


const fragmentShader3 = `
#ifdef GL_ES
precision mediump float;
#endif

#define PI 3.14159265359

uniform vec2 u_resolution;
uniform vec2 u_mouse;
uniform float u_time;

vec3 colorA = vec3(0.149,0.141,0.912);
vec3 colorB = vec3(1.000,0.833,0.224);

float plot (vec2 st, float pct){
    return  smoothstep( pct-0.01, pct, st.y) -
    smoothstep( pct, pct+0.01, st.y);
}

void main() {
    vec2 st = gl_FragCoord.xy/u_resolution.xy;
    vec3 color = vec3(0.0);

    vec3 pct = vec3(st.x);

    pct.r = smoothstep(0.0,1.0, st.x);
    pct.g = sin(st.x*PI);
    pct.b = pow(st.x,0.5);

    color = mix(colorA, colorB, pct);

    // Plot transition lines for each channel
    color = mix(color,vec3(1.0,0.0,0.0),plot(st,pct.r));
    color = mix(color,vec3(0.0,1.0,0.0),plot(st,pct.g));
    color = mix(color,vec3(0.0,0.0,1.0),plot(st,pct.b));

    gl_FragColor = vec4(color,1.0);
}
`

const fragmentShader4 = `
#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;
uniform vec2 u_mouse;
uniform float u_time;

void main(){
    vec2 st = gl_FragCoord.xy/u_resolution.xy;
    st.x *= u_resolution.x/u_resolution.y;
    vec3 color = vec3(0.0);
    float d = 0.0;

    // Remap the space to -1. to 1.
    st = st *2.-1.;

    // Make the distance field
    d = length( abs(st)-.3 );
    // d = length( min(abs(st)-.3,0.) );
    // d = length( max(abs(st)-.3,0.) );

    // Visualize the distance field
    gl_FragColor = vec4(vec3(fract(d*10.0)),1.0);

    // Drawing with the distance field
    // gl_FragColor = vec4(vec3( step(.3,d) ),1.0);
    // gl_FragColor = vec4(vec3( step(.3,d) * step(d,.4)),1.0);
    // gl_FragColor = vec4(vec3( smoothstep(.3,.4,d)* smoothstep(.6,.5,d)) ,1.0);
}
`

const fragmentShader5 = `
#ifdef GL_ES
precision mediump float;
#endif

#define PI 3.14159265359
#define TWO_PI 6.28318530718

uniform vec2 u_resolution;
uniform vec2 u_mouse;
uniform float u_time;

// Reference to
// http://thndl.com/square-shaped-shaders.html

void main(){
    vec2 st = gl_FragCoord.xy/u_resolution.xy;
    st.x *= u_resolution.x/u_resolution.y;
    vec3 color = vec3(0.0);
    float d = 0.0;

    // Remap the space to -1. to 1.
    st = st *2.-1.;

    // Number of sides of your shape
    int N = 3;

    // Angle and radius from the current pixel
    float a = atan(st.x,st.y)+PI;
    float r = TWO_PI/float(N);

    // Shaping function that modulate the distance
    d = cos(floor(.5+a/r)*r-a)*length(st);

    color = vec3(1.0-smoothstep(.4,0.530,d));
    // color = vec3(d);

    gl_FragColor = vec4(color,1.0);
}
`


const fragmentShader6 = `
#ifdef GL_ES
precision mediump float;
#endif

#define PI 3.14159265359

uniform vec2 u_resolution;
uniform float u_time;

mat2 rotate2d(float _angle){
    return mat2(cos(_angle),-sin(_angle),
    sin(_angle),cos(_angle));
}

float box(in vec2 _st, in vec2 _size){
    _size = vec2(0.5) - _size*0.5;
    vec2 uv = smoothstep(_size,
    _size+vec2(0.001),
    _st);
    uv *= smoothstep(_size,
    _size+vec2(0.001),
    vec2(1.0)-_st);
    return uv.x*uv.y;
}

float crossBox(in vec2 _st, float _size){
    return  box(_st, vec2(_size,_size/4.)) +
    box(_st, vec2(_size/4.,_size));
}

void main(){
    vec2 st = gl_FragCoord.xy/u_resolution.xy;
    vec3 color = vec3(0.0);

    // move space from the center to the vec2(0.0)
    st -= vec2(0.5);
    // rotate the space
    st = rotate2d( sin(u_time)*PI ) * st;
    // move it back to the original place
    st += vec2(0.5);

    // Show the coordinates of the space on the background
    // color = vec3(st.x,st.y,0.0);

    // Add the shape on the foreground
    color += vec3(crossBox(st,0.4));

    gl_FragColor = vec4(color,1.0);
}
`
const fragmentShader7 = `
#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;
uniform float u_time;

vec2 brickTile(vec2 _st, float _zoom){
    _st *= _zoom;

    // Here is where the offset is happening
    _st.x += step(1., mod(_st.y,2.0)) * 0.5;

    return fract(_st);
}

float box(vec2 _st, vec2 _size){
    _size = vec2(0.5)-_size*0.5;
    vec2 uv = smoothstep(_size,_size+vec2(1e-4),_st);
    uv *= smoothstep(_size,_size+vec2(1e-4),vec2(1.0)-_st);
    return uv.x*uv.y;
}

void main(void){
    vec2 st = gl_FragCoord.xy/u_resolution.xy;
    vec3 color = vec3(0.0);

    // st /= vec2(2.15,0.65)/1.5;

    // Apply the brick tiling
    st = brickTile(st,5.0);

    color = vec3(box(st,vec2(0.9)));

    // Uncomment to see the space coordinates
    // color = vec3(st,0.0);

    gl_FragColor = vec4(color,1.0);
}
`

const fragmentShader8 = `
#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;
uniform vec2 u_mouse;
uniform float u_time;

varying vec3 vNormal;  // Normal from the vertex shader
varying vec3 worldSpacePosition; // Position from the vertex shader

void main() {

    vec2 st = gl_FragCoord.xy / u_resolution;

    // gl_FragColor = vec4(st.x, st.y, 0.0, 1.0);

    // gl_FragColor = vec4(vNormal.xy, 0.0, 1.0);

    gl_FragColor = vec4(worldSpacePosition.xy, 0.0, 1.0);
}
`;


const textureLoader = new THREE.TextureLoader();
const normalMap = textureLoader.load('src/shaders/img.jpg');
normalMap.wrapS = normalMap.wrapT = THREE.RepeatWrapping;


// Shader Material
export const Hello_Shader = new THREE.ShaderMaterial({
    uniforms: {
        u_tex0: { value: normalMap },
        u_tex0Resolution: { value: new THREE.Vector2(640, 640) },
        u_resolution: { value: new THREE.Vector2(window.innerWidth, window.innerHeight) },
        u_mouse: { value: new THREE.Vector2(0, 0) },
        u_time: { value: 0.0 },
    },

    // vertexShader: vertexShader0,
    // fragmentShader: fragmentShader0,


    // vertexShader: vertexShader0,
    // fragmentShader: fragmentShader1,


    // vertexShader: vertexShader0,
    // fragmentShader: fragmentShader2,


    // vertexShader: vertexShader0,
    // fragmentShader: fragmentShader3,


    // vertexShader: vertexShader0,
    // fragmentShader: fragmentShader4,


    // vertexShader: vertexShader0,
    // fragmentShader: fragmentShader5,


    // vertexShader: vertexShader0,
    // fragmentShader: fragmentShader6,


    // vertexShader: vertexShader0,
    // fragmentShader: fragmentShader7,


    vertexShader: vertexShader0,
    fragmentShader: fragmentShader8,


    // vertexShader: vertexShader0,
    // fragmentShader: lcircleFrag,


    // vertexShader: vertexShader0,
    // fragmentShader: nearestFrag,


    // vertexShader: vertexShader0,
    // fragmentShader: graphFrag,


    // vertexShader: vertexShader0,
    // fragmentShader: voronoiseFrag,

});




