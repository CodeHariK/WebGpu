/**
 * @file terraspline.h
 * @brief Umbrella include for the TerraSpline module (spline-driven Terrain3D streaming).
 *
 * Include this from code outside the module (e.g. register_types.cpp). Inside the module, include
 * the specific header you need:
 *
 *   tr_heightmap.h      TerrainHeightmap        float grid for one chunk
 *   tr_chunk.h          TerrainChunk            resident chunk state (heightmap, visuals, physics)
 *   tr_deformer_job.h   DeformerJob             per-(deformer, chunk) inputs + distance field
 *   tr_deformer.h       TerrainSplineDeformer   raises/lowers terrain along a spline
 *   tr_scatter_job.h    ScatterJob              per-(scatterer, chunk) inputs + transforms
 *   tr_scatter.h        TerrainSplineScatter    places meshes along a spline
 *   tr_chunk_job.h      ChunkJob                one chunk generation (make -> math -> finalize)
 *   tr_compositor.h     TerrainSplineCompositor streams chunks into Terrain3D
 *   tr_compositor_ui.h  TerrainSplineCompositorUI, GrayscaleJob   debug preview
 */
#ifndef TERRASPLINE_H
#define TERRASPLINE_H

#include "tr_chunk.h"
#include "tr_chunk_job.h"
#include "tr_cliff.h"
#include "tr_compositor.h"
#include "tr_compositor_ui.h"
#include "tr_deformer.h"
#include "tr_deformer_job.h"
#include "tr_heightmap.h"
#include "tr_scatter.h"
#include "tr_scatter_job.h"
#include "tr_stream_map.h"

#endif // TERRASPLINE_H
