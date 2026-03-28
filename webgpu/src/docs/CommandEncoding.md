# WebGPU Core Concepts

Understanding the core objects and workflow of WebGPU is essential for building high-performance graphics applications. This guide covers the three main pillars: Pipelines, Bind Groups, and Command Encoding.

---

## 1. GPURenderPipeline

A **GPURenderPipeline** is a pre-compiled state for rendering. It defines how vertices are processed, which shaders to run, and how the resulting pixels are blended.

### Key Components
- **Vertex State**: Defines the vertex shader, vertex buffers, and attributes.
- **Fragment State**: Defines the fragment shader and output color formats.
- **Primitive State**: Configures the primitive topology (triangles, lines, etc.) and culling.
- **Depth/Stencil State**: Manages depth testing and writing.

Once created, a pipeline is immutable and highly optimized for the target hardware, preventing expensive state validation at draw time.

---

## 2. GPUBindGroup

A **GPUBindGroup** defines a set of resources (buffers, textures, samplers) that are accessible by shaders during a draw call. It "binds" these resources to the binding points defined in the **GPUBindGroupLayout**.

### Why use BindGroups?
- **Efficiency**: They allow you to swap multiple resources at once.
- **Organization**: Shaders access resources through groups (e.g., `@group(0) @binding(1)`).
- **Flexibility**: You can use different BindGroups with the same pipeline to render different sets of data (e.g., different transformation matrices for multiple objects).

---

## 3. Command Encoding and Submission

WebGPU uses a recording-based approach. Instead of immediate issuance, you record batches of commands into a **Command Buffer** and then submit them as a single unit.

### The Encoding Workflow

1.  **GPUCommandEncoder**: Started from the `GPUDevice`. Records the entire set of operations for a frame.
    ```typescript
    const commandEncoder = device.createCommandEncoder();
    ```

2.  **GPUTextureView**: Defines how a texture is accessed (e.g., the current swap chain texture for the screen).
    ```typescript
    const textureView = context.getCurrentTexture().createView();
    ```

3.  **GPURenderPassDescriptor**: Configures a single render pass (attachments, clear values).
    ```typescript
    const renderPassDescriptor: GPURenderPassDescriptor = {
      colorAttachments: [{
        view: textureView,
        loadOp: "clear",
        clearValue: [0, 0, 0, 0],
        storeOp: "store",
      }],
    };
    ```

4.  **GPURenderPassEncoder**: Created via `beginRenderPass`. This is where you set the pipeline, bind groups, and issue `draw()` commands.
    ```typescript
    const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
    passEncoder.setPipeline(pipeline);
    passEncoder.setBindGroup(0, bindGroup);
    passEncoder.draw(3);
    passEncoder.end();
    ```

5.  **GPUQueue**: The final step. Finish the recording and submit the buffer to the hardware.
    ```typescript
    const commandBuffer = commandEncoder.finish();
    device.queue.submit([commandBuffer]);
    ```
