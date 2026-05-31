void generate_chunk_mesh(PrismChunk3D *chunk, Vector3 chunk_world_pos) {
	const int VISUAL_SIZE = 6;
	const int PRISMS_PER_ROW = VISUAL_SIZE * 2; // 12 triangles per row

	// 1. Loop Y (Up) [0 to 5]
	for (int y = 0; y < VISUAL_SIZE; y++) {
		// 2. Loop Z (Depth) [0 to 5] - Positive mapping!
		for (int z = 0; z < VISUAL_SIZE; z++) {
			// 3. Loop X (Width) [0 to 11]
			for (int cx = 0; cx < PRISMS_PER_ROW; cx++) {
				int vx = cx / 2;
				int v0_x, v0_z, v1_x, v1_z, v2_x, v2_z;

				// --- 2D Row Stagger Logic ---
				if (z % 2 == 0) {
					if (cx % 2 == 0) { // Pointing Down (∇)
						v0_x = vx;
						v0_z = z;
						v1_x = vx + 1;
						v1_z = z;
						v2_x = vx;
						v2_z = z + 1;
					} else { // Pointing Up (Δ)
						v0_x = vx + 1;
						v0_z = z;
						v1_x = vx;
						v1_z = z + 1;
						v2_x = vx + 1;
						v2_z = z + 1;
					}
				} else {
					if (cx % 2 == 0) { // Pointing Down (∇)
						v0_x = vx;
						v0_z = z;
						v1_x = vx + 1;
						v1_z = z;
						v2_x = vx + 1;
						v2_z = z + 1;
					} else { // Pointing Up (Δ)
						v0_x = vx;
						v0_z = z;
						v1_x = vx;
						v1_z = z + 1;
						v2_x = vx + 1;
						v2_z = z + 1;
					}
				}

				// --- EXTRACT 6-BIT CONFIGURATION ---
				uint8_t config_idx = 0;

				// Bottom Layer (Bits 0, 1, 2)
				if (chunk->get_vertex(v0_x, y, v0_z))
					config_idx |= (1 << 0);
				if (chunk->get_vertex(v1_x, y, v1_z))
					config_idx |= (1 << 1);
				if (chunk->get_vertex(v2_x, y, v2_z))
					config_idx |= (1 << 2);

				// Top Layer (Bits 3, 4, 5) -> Exact same XZ, just Y+1
				if (chunk->get_vertex(v0_x, y + 1, v0_z))
					config_idx |= (1 << 3);
				if (chunk->get_vertex(v1_x, y + 1, v1_z))
					config_idx |= (1 << 4);
				if (chunk->get_vertex(v2_x, y + 1, v2_z))
					config_idx |= (1 << 5);

				// Skip if empty air (0) or fully buried (63)
				if (config_idx == 0 || config_idx == 63)
					continue;

				// --- CALCULATE PHYSICAL CENTROID ---
				// Everything is purely positive now.
				float stagger = (z % 2 != 0) ? 0.5f : 0.0f;
				float physical_x = (vx + stagger);
				float physical_z = (z * 0.866025f); // Positive Z spacing!
				float physical_y = y * 1.0f; // Positive Y spacing!

				// The local centroid of this specific prism
				Vector3 local_centroid = Vector3(physical_x, physical_y, physical_z);

				// Final World Transform
				Transform3D instance_transform;
				// 1. Get rotation from your LUT based on config_idx
				// instance_transform.basis = ...

				// 2. Set origin (World chunk position + local offset)
				instance_transform.origin = chunk_world_pos + local_centroid;

				// 3. Add instance_transform to the correct MultiMesh buffer
			}
		}
	}
}
