# Odin + WebGPU — Graphics-from-scratch learning track

A parallel, hands-on track to learn real-time rendering at the raw level, to deepen
the graphics understanding behind the Godot "minecraft" work. Not a game — a ladder
of tiny runnable programs, each one building on the last. Rule: **don't advance until
the step runs AND you can explain every line.**

Goal: understand the modern explicit GPU model (the same model under Vulkan/Metal/
D3D12 and under Godot's RenderingServer/RenderingDevice) by implementing it.

---

## 0 · Setup  (one-time)

- [ ] Install Odin (grab the latest release or build from source; `odin version` works).
- [ ] Pick the WebGPU native backend: **wgpu-native** (Rust/Firefox, simplest to link)
      or **Dawn** (Google/Chrome). Start with wgpu-native.
- [ ] Get Odin's `vendor:wgpu` bindings building (ships with Odin) OR link wgpu-native
      manually. Confirm you can `import "vendor:wgpu"`.
- [ ] Window + surface: use `vendor:glfw` (or sokol_app) for the OS window; WebGPU only
      does the GPU, not the window.
- [ ] Build a one-file "hello" that opens a window and clears it to a color (see step 1).
- [ ] Decide the repo layout (suggestion below) and `git init`.

Suggested layout:
```
odin-webgpu-learn/
  01_clear/        02_triangle/     03_texture/      04_cube/
  05_lit/          06_offscreen/    07_bloom_dof/    08_shadows/
  09_mini_renderer/
  common/          # window, wgpu init, math, file loading — grows as you go
  assets/          # textures, glTF models
  TODO.md          # this file
  NOTES.md         # what each concept means, in your own words
```

---

## 1 · Ladder  (each step = a runnable program)

### [ ] 1. Clear screen
Open a window, init WebGPU (instance → adapter → device → queue), create the surface
+ swapchain, and clear to a solid color every frame.
- Concepts: instance/adapter/device/queue, surface config, command encoder, render
  pass (load=clear), `queue.submit`, `surface.present`.
- Done when: a colored window that resizes cleanly.

### [ ] 2. Triangle
Draw one hard-coded triangle.
- Concepts: WGSL vertex + fragment shader, `RenderPipeline`, shader modules, vertex
  buffer + vertex layout, draw call.
- Done when: gradient triangle on screen; you can explain the pipeline object.

### [ ] 3. Textured quad
Two triangles, sample a texture on them.
- Concepts: index buffer, loading an image (`vendor:stb/image`), `Texture` + `TextureView`
  + `Sampler`, **bind groups** and bind-group layouts, UV coords.
- Done when: an image displayed; you understand bind groups (the WebGPU version of
  descriptor sets).

### [ ] 4. Spinning 3D cube + camera
- Concepts: uniform buffers, MVP matrices (model/view/projection), **depth buffer**,
  per-frame uniform updates, back-face culling, winding order.
- Done when: a depth-correct rotating cube with a perspective camera you can move.

### [ ] 5. Lit mesh (Lambert → Blinn-Phong)
Light the cube (then a sphere) with one directional light.
- Concepts: normals + normal matrix, world-space lighting, N·L diffuse, specular.
  (You already know this math from the folio `mesh_default` shader — now do it raw.)
- Done when: a shaded sphere; core-shadow term matches your mental model.

### [ ] 6. Render to texture (offscreen pass)
Render the scene into an offscreen texture, then draw that texture fullscreen.
- Concepts: render targets / framebuffers, multi-pass rendering, fullscreen triangle,
  sampling the previous pass. **This is the foundation of ALL post-processing.**
- Done when: scene renders through an intermediate texture with no visible difference.

### [ ] 7. Bloom + tilt-shift DOF  ⭐ highest leverage
Reimplement the two effects you just learned in Godot, at the raw level.
- Bloom: bright-pass (threshold) → downsample+blur (mips) → additive composite.
- cheapDOF: the exact tilt-shift port (strength by screen-Y, golden-angle hash blur,
  mix). Port your `cheap_dof.gdshader` logic to WGSL.
- Concepts: ping-pong buffers, mip chains, separable blur, compositing passes.
- Done when: your own bloom + tilt-shift matches the Godot look. Biggest "click" moment.

### [ ] 8. Shadow mapping
Render depth from the light's POV, then compare in the main pass.
- Concepts: depth-only pass, light view/proj, shadow sampling + bias, PCF softening.
  (Demystifies the `ATTENUATION` you used in the folio `light()` function.)
- Done when: a cast shadow on the ground; you understand shadow acne + bias.

### [ ] 9. Mini renderer
Load a glTF model, a few lights, a simple forward (or forward+) path, the post stack.
- Concepts: glTF parsing (`vendor:cgltf`), material/texture binding, multiple lights,
  a tiny frame graph, basic frustum culling.
- Done when: a lit, post-processed model scene running at solid FPS.

---

## Depth track  (in parallel, once the ladder is moving)

- [ ] GPU hardware model: warps/wavefronts, occupancy, why memory bandwidth is the real
      bottleneck, cost of branches + texture fetches. (Matters for your mobile target.)
- [ ] sRGB vs linear color, gamma, tonemapping — get this right early (bit us in the port).
- [ ] Alpha blending + transparency sorting.
- [ ] Instancing (draw 10k objects in one call) — the "why RenderingServer exists" lesson.
- [ ] Compute shaders (GPU particles / culling).
- [ ] Read a real renderer's source: wgpu examples, sokol samples, and Godot's
      `servers/rendering/` — recognize global uniforms, the glow pass, forward+.

---

## Resources

- webgpufundamentals.org — best modern WebGPU intro (JS, concepts port directly).
- "Learn Wgpu" (sotrh) — Rust, but the WebGPU concepts are 1:1.
- LearnOpenGL — old API, still THE best free explainer of the *concepts*
  (lighting, shadow maps, deferred, post-processing). Read for theory.
- Odin: odin-lang.org docs + the `vendor:wgpu` examples in the Odin repo.
- The "best darn grid shader" article (Ben Golus) — you already used its method.
- Books: Real-Time Rendering (reference, skim), Physically Based Rendering (free
  online, deep), Ray Tracing in One Weekend (short, satisfying weekend win).

---

## Working rules

- One concept per program; keep them small and self-contained.
- After each step, write a short plain-English note in NOTES.md ("what a bind group is,
  in my words"). Teaching it back is how depth sticks.
- Reimplement things you already understand from Godot before inventing new ones — the
  familiarity lets you focus on the plumbing.
- Keep this as a side track; don't stall the Godot game. Each concept learned here loops
  back into better shaders / smarter RenderingServer use / sane mobile perf calls there.
