The Infinite Track Algorithm (World Logic)
Right now, you have hand-drawn a few Curve3D splines in the editor. To make the game truly infinite, we need to write a procedural pathfinding algorithm that dynamically adds new Vector3 points to the RoadSpline just ahead of the player, twisting and turning forever.

## TODO

Searched for "terraspline"
Searched for "terraspline"
Viewed tr_deformer.cpp:1-350
Viewed terraspline.h:1-249
Searched for "ProceduralSpline3D"
Viewed procedural_spline3d.h:1-437

Your proposal to triangulate the spline corridor and use barycentric coordinate interpolation is a classic and highly effective technique in computer graphics and terrain generation (often referred to as **ribbon rasterization** or **vector-to-raster extrusion**). 

Here is a detailed breakdown of the trade-offs, advantages, and implementation challenges of this approach compared to the current analytical segment-distance approach in **TerraSpline**:

---

### 1. The Core Differences

| Feature | Current Approach (Analytical SDF / Segment Search) | Proposed Approach (Mesh Triangulation & Barycentric Interpolation) |
| :--- | :--- | :--- |
| **Logic** | For each heightmap pixel, find the closest point on the nearest spline segment, calculate the distance, and sample curves. | Generate a mesh of triangles along the spline (center to outer falloff edges). For each pixel inside a triangle, linearly interpolate height using barycentric coordinates. |
| **Math Complexity** | High. Solves projection equations (dot products, clamping, and distance checks) per pixel. | Very Low. Basic linear interpolation (barycentric weights) per pixel. |
| **Traversal** | Iterates over bounding-box tiles of pixels and queries the spline segments. | Rasterizes the triangles directly onto the heightmap grid (updating only the pixels covered by the triangles). |

---

### 2. Pros (Why this is a great idea)

1. **Massive Performance Boost on High-Res Heightmaps:**
   * In the current implementation, if you have a $1024 \times 1024$ heightmap ($1,048,576$ pixels) and a detailed spline, you are doing a large number of projection checks. 
   * With the triangle approach, you evaluate the spline equations **only at the mesh vertices** (e.g., a few hundred control points). The heightmap pixels are then updated using ultra-fast scanline rasterization and simple float multiplication.
2. **Strictly Bounded Work (No Redundant Pixels):**
   * By rasterizing the triangles (using standard 2D triangle-bounding-box rasterization), you only touch pixels that are guaranteed to lie within the spline's width/falloff corridor. You never perform dummy calculations on pixels that are far away.
3. **Perfect Control over Profile Shape:**
   * You can explicitly define vertices at the "Road Center", "Road Edge", and "Falloff Limit". This makes it very easy to create complex cross-sections (like gutters, curbs, or double-humped terrain profiles) just by setting vertex offsets and heights.

---

### 3. Cons & Implementation Challenges

1. **Junctions and Self-Intersections (The Overlap Problem):**
   * When a spline loops over itself or sharp bends cause triangles to overlap, rasterizing them directly can lead to artifacts:
     * *If using Add/Subtract blend modes:* Overlapping triangles will double-apply the deformation.
     * *If using Replace/Max/Min:* You must decide which triangle has precedence.
   * **Solution:** Typically, you rasterize into a temporary stencil buffer or write the maximum depth/weight to resolve overlapping triangles before writing back to the heightmap.
2. **Faceted Terrain (Linear Interpolation Artifacts):**
   * Barycentric interpolation is linear. If your spline has a high curvature or your triangles are too large, the terrain might look "faceted" (sharp ridges where triangle edges meet) instead of showing a smooth, curved falloff. 
   * **Solution:** You would need a sufficiently high tessellation rate (low `bake_interval`) along the curve to keep the transitions smooth.
3. **Closed Spline Interiors (Crater/Plateau Fill):**
   * The current system has a `fill_interior` mode for closed loops (useful for creating flat plateaus or lakes). 
   * Triangulating the arbitrary 2D shape of a closed loop's interior (which can be concave or self-intersecting) requires a robust triangulation library (like Ear Clipping or Delaunay triangulation), which adds significant architectural complexity to the C++ side.

---

### Conclusion & Recommendation

* **Use the current SDF approach if:** You need robust support for overlapping splines, arbitrary closed-loop interiors (plateaus/craters), and simple C++ code without managing triangle topology.
* **Use the Triangulation/Barycentric approach if:** You are hitting CPU performance bottlenecks during real-time deformation of high-resolution heightmaps, or if you want to implement advanced cross-sectional profiles (like ditches or sloped embankments) that are hard to model with a single analytical distance curve.
