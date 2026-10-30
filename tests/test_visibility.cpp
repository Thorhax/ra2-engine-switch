#include "doctest.h"
#include "visibility.hpp"
using namespace ra2engine;

static int disc_count(int sight) {
    // Count cells in a fresh large grid so no clipping occurs (center at 20,20).
    const int gw = 41, gh = 41;
    std::vector<std::uint8_t> e(gw * gh, 0), v(gw * gh, 0);
    return reveal_disc(e, v, gw, gh, 20, 20, sight);
}

TEST_CASE("visibility: cell_in_sight is near-circular dx2+dy2<=sight2 [RE: shroud.md §3]") {
    CHECK(cell_in_sight(0, 0, 5));
    CHECK(cell_in_sight(3, 4, 5));    // 9+16=25 == 25, inclusive
    CHECK_FALSE(cell_in_sight(4, 4, 5));  // 32 > 25
    CHECK(cell_in_sight(0, 0, 0));    // sight 0 = source cell only
    CHECK_FALSE(cell_in_sight(1, 0, 0));
}

TEST_CASE("visibility: reveal_disc marks both grids, sticky explored [RE: shroud.md §3/§4]") {
    const int gw = 41, gh = 41;
    std::vector<std::uint8_t> e(gw * gh, 0), v(gw * gh, 0);
    const int n = reveal_disc(e, v, gw, gh, 20, 20, 5);
    CHECK(n > 0);
    CHECK(v[20 * gw + 20] == 1);      // center visible
    CHECK(e[20 * gw + 20] == 1);      // and explored
    CHECK(v[20 * gw + 23] == 1);      // (3,0): 9<=25 visible
    // (dx=5,dy=3): 25+9=34>25 -> not visible
    CHECK(v[(20 + 3) * gw + (20 + 5)] == 0);
    // Simulate a tick: caller clears visible_, explored_ stays (sticky).
    std::fill(v.begin(), v.end(), (std::uint8_t)0);
    CHECK(e[20 * gw + 20] == 1);      // explored persists (fog no-regrow)
    CHECK(v[20 * gw + 20] == 0);
}

TEST_CASE("visibility: reveal_disc counts match known radii [RE: shroud.md §3]") {
    // Exact near-circular cell counts (dx2+dy2<=r2, integer centers).
    CHECK(disc_count(4) == 49);
    CHECK(disc_count(5) == 81);
    CHECK(disc_count(8) == 197);
    CHECK(disc_count(9) == 253);
    CHECK(disc_count(10) == 317);
    CHECK(disc_count(0) == 1);        // source cell only
}

TEST_CASE("visibility: reveal_disc clips at grid edge [RE: shroud.md §3]") {
    const int gw = 6, gh = 6;
    std::vector<std::uint8_t> e(gw * gh, 0), v(gw * gh, 0);
    const int n = reveal_disc(e, v, gw, gh, 0, 0, 5);   // corner source
    CHECK(n > 0);
    CHECK(n < 81);                    // clipped to the in-bounds quarter
    CHECK(v[0] == 0 + 1);            // (0,0) set
    CHECK(v[5 * gw + 5] == 0);       // (5,5): 25+25=50>25 not in range anyway
}

TEST_CASE("visibility: reveal_rect equals a reveal_disc per footprint cell") {
    const int gw = 30, gh = 24;
    struct Case { int cx, cy, fw, fh, sight; };
    const Case cases[] = {{10, 10, 1, 1, 5}, {10, 8, 4, 3, 6}, {0, 0, 3, 3, 7}, {27, 21, 3, 3, 4},
                          {-2, 5, 4, 2, 3}, {12, 12, 2, 5, 0}, {5, 5, 3, 3, -1}, {14, 10, 6, 6, 10}};
    for (const Case& c : cases) {
        std::vector<std::uint8_t> e1(gw * gh, 0), v1(gw * gh, 0), e2(gw * gh, 0), v2(gw * gh, 0);
        for (int fy = 0; fy < c.fh; ++fy)
            for (int fx = 0; fx < c.fw; ++fx) reveal_disc(e1, v1, gw, gh, c.cx + fx, c.cy + fy, c.sight);
        const int n = reveal_rect(e2, v2, gw, gh, c.cx, c.cy, c.fw, c.fh, c.sight);
        CHECK(v1 == v2);
        CHECK(e1 == e2);
        int set = 0;
        for (std::uint8_t b : v2) set += b;
        CHECK(n == set);
    }
}
