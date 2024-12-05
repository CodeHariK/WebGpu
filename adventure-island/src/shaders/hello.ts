import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js'



// Vertex Shader
const vertexShader0 = `
void main() {
    gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
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


const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(75, window.innerWidth / window.innerHeight, 0.1, 1000);
camera.position.z = 1;

const renderer = new THREE.WebGLRenderer();
renderer.setSize(window.innerWidth, window.innerHeight);
document.body.appendChild(renderer.domElement);




// Orbit Controls
const controls = new OrbitControls(camera, renderer.domElement);
controls.enableDamping = true; // Smooth movement
controls.dampingFactor = 0.05; // Adjust damping strength
controls.screenSpacePanning = false; // Prevent panning in screen space
controls.minDistance = 0.5; // Minimum zoom distance
controls.maxDistance = 10; // Maximum zoom distance





// Create a plane geometry
const geometry = new THREE.PlaneGeometry(2, 2);
// Shader Material
const shaderMaterial = new THREE.ShaderMaterial({
    uniforms: {
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


    vertexShader: vertexShader0,
    fragmentShader: fragmentShader6,


});

const plane = new THREE.Mesh(geometry, shaderMaterial);
scene.add(plane);

// Animation loop
const clock = new THREE.Clock();
function animate() {
    shaderMaterial.uniforms.u_time.value = clock.getElapsedTime();
    controls.update();
    renderer.render(scene, camera);
    requestAnimationFrame(animate);
}

animate();




window.addEventListener('resize', () => {
    const width = window.innerWidth;
    const height = window.innerHeight;
    renderer.setSize(width, height);
    camera.aspect = width / height;
    camera.updateProjectionMatrix();
    shaderMaterial.uniforms.u_resolution.value.set(width, height);
});

window.addEventListener('mousemove', (event) => {
    shaderMaterial.uniforms.u_mouse.value.set(event.clientX, window.innerHeight - event.clientY);
});




