struct Uniforms {
  viewMatrix : mat4x4<f32>,
  projectionMatrix : mat4x4<f32>,
  modelMatrix : mat4x4<f32>,
};

@group(0) @binding(0) var<uniform> uniforms : Uniforms;

@group(1) @binding(0) var textureSampler : sampler;
@group(1) @binding(1) var diffuseTexture : texture_2d<f32>;

struct VertexInput {
  @location(0) position : vec3<f32>,
  @location(1) uv : vec2<f32>,
  @location(2) normal : vec3<f32>,
};

struct VertexOutput {
  @builtin(position) position : vec4<f32>,
  @location(0) uv : vec2<f32>,
  @location(1) normal : vec3<f32>,
};

@vertex
fn vertexMain(input : VertexInput) -> VertexOutput {
  var output : VertexOutput;
  output.position = uniforms.projectionMatrix * uniforms.viewMatrix * uniforms.modelMatrix * vec4<f32>(input.position, 1.0);
  output.uv = input.uv;
  // Transform normal to world space (simplified, should ideally use normal matrix)
  output.normal = (uniforms.modelMatrix * vec4<f32>(input.normal, 0.0)).xyz;
  return output;
}

@fragment
fn fragmentMain(input : VertexOutput) -> @location(0) vec4<f32> {
  let lightDir = normalize(vec3<f32>(0.5, 1.0, 0.5));
  let diffuse = max(dot(normalize(input.normal), lightDir), 0.2);
  let texColor = textureSample(diffuseTexture, textureSampler, input.uv);
  return vec4<f32>(texColor.rgb * diffuse, texColor.a);
}
