#pragma once
#include <vector>
#include "voxel_raster.hpp"

namespace ra2engine {

// Cached-facings renderer strategy (spec: "cached facings"): the original
// quantizes vehicle orientation to kVehicleFacings pre-rendered views
// (docs/re/voxel-render.md §2); we rasterize each once at load and blit.
constexpr int kVehicleFacings = 32;  // [RE: voxel-render.md §2]

struct FacingSet {
    std::vector<VoxelSprite> sprites;  // size n_facings; index = facing_to_index()
};

// Rasterizes model at every quantized facing. Pure; deterministic.
FacingSet build_facing_set(const ra2assets::VxlModel& m, const ra2assets::HvaModel& hva,
                           int n_facings, const ra2assets::Vpl& vpl,
                           const ra2assets::Palette& pal);

// build_facing_set for several palettes at once (house colours): each facing
// is rasterized once and coloured per palette. Facings [first, last) only, so
// callers can split the work; `out` holds one set per palette, each sized
// n_facings (resized here when empty).
void build_facing_sets(const ra2assets::VxlModel& m, const ra2assets::HvaModel& hva, int n_facings,
                       const ra2assets::Vpl& vpl, const ra2assets::Palette* pals, int n_pals,
                       FacingSet* out, int first, int last);

}  // namespace ra2engine
