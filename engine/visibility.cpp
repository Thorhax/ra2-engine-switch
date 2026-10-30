#include "visibility.hpp"
#include "orig_math.hpp"   // Net L0: om::ftol (_ftol 0x7C4D00) for double->int

namespace ra2engine {

int reveal_disc(std::vector<std::uint8_t>& explored,
                std::vector<std::uint8_t>& visible,
                int gw, int gh, int cx, int cy, int sight) {
    if (gw <= 0 || gh <= 0) return 0;
    if ((int)explored.size() != gw * gh || (int)visible.size() != gw * gh) return 0;
    const int r = sight < 0 ? 0 : sight;   // negative sight -> source cell only
    int n = 0;
    for (int dy = -r; dy <= r; ++dy) {
        const int y = cy + dy;
        if (y < 0 || y >= gh) continue;
        for (int dx = -r; dx <= r; ++dx) {
            const int x = cx + dx;
            if (x < 0 || x >= gw) continue;
            if (!cell_in_sight(dx, dy, r)) continue;   // near-circular filter
            const int idx = y * gw + x;
            visible[idx] = 1;
            explored[idx] = 1;   // sticky
            ++n;
        }
    }
    return n;
}

int reveal_rect(std::vector<std::uint8_t>& explored,
                std::vector<std::uint8_t>& visible,
                int gw, int gh, int cx, int cy, int fw, int fh, int sight) {
    if (gw <= 0 || gh <= 0 || fw <= 0 || fh <= 0) return 0;
    if ((int)explored.size() != gw * gh || (int)visible.size() != gw * gh) return 0;
    const int r = sight < 0 ? 0 : sight;
    const int x1 = cx + fw - 1, y1 = cy + fh - 1;
    int n = 0;
    for (int y = cy - r; y <= y1 + r; ++y) {
        if (y < 0 || y >= gh) continue;
        const int dy = y < cy ? cy - y : (y > y1 ? y - y1 : 0);
        for (int x = cx - r; x <= x1 + r; ++x) {
            if (x < 0 || x >= gw) continue;
            const int dx = x < cx ? cx - x : (x > x1 ? x - x1 : 0);
            if (!cell_in_sight(dx, dy, r)) continue;
            const int idx = y * gw + x;
            visible[idx] = 1;
            explored[idx] = 1;
            ++n;
        }
    }
    return n;
}

}  // namespace ra2engine
