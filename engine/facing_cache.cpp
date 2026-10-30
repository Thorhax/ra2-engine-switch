#include "facing_cache.hpp"

namespace ra2engine {

FacingSet build_facing_set(const ra2assets::VxlModel& m, const ra2assets::HvaModel& hva,
                           int n_facings, const ra2assets::Vpl& vpl,
                           const ra2assets::Palette& pal) {
    FacingSet out;
    out.sprites.reserve(n_facings);
    for (int i = 0; i < n_facings; ++i)
        out.sprites.push_back(rasterize_model(m, hva, index_to_yaw(i, n_facings), vpl, pal));
    return out;
}

void build_facing_sets(const ra2assets::VxlModel& m, const ra2assets::HvaModel& hva, int n_facings,
                       const ra2assets::Vpl& vpl, const ra2assets::Palette* pals, int n_pals,
                       FacingSet* out, int first, int last) {
    for (int p = 0; p < n_pals; ++p)
        if (out[p].sprites.size() != static_cast<size_t>(n_facings)) out[p].sprites.resize(n_facings);
    for (int i = first; i < last && i < n_facings; ++i) {
        const VoxelIndexSprite s = rasterize_model_indexed(m, hva, index_to_yaw(i, n_facings), vpl);
        for (int p = 0; p < n_pals; ++p) out[p].sprites[i] = colorize(s, pals[p]);
    }
}

}  // namespace ra2engine
