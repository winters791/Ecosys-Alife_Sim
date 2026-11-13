#pragma once
// Footprints (LIGHT_FOOTPRINT, a law; CLAUDE.md 14.2.1): which cells a body's
// outline covers, and how much of it lies in each. A body catches light over
// those cells in proportion, and competes for it by canopy height in each,
// so a big body is no longer held to one cell's light, and it shades its
// neighbours' cells.
//
// A fixed stencil chosen by radius, as if the body sat at the middle of its
// cell: no per-tick geometry. The weights are the share of a disc of that
// radius, centred on a cell's middle, that falls in each cell around it,
// worked out once by sampling and kept by radius in steps of STEP. They are
// symmetric, so one quadrant is kept: weight(|dx|, |dy|).

#include <algorithm>
#include <cmath>
#include <vector>

namespace luca {

class Footprints {
public:
    static constexpr float STEP = 0.02f;
    static constexpr int   MAX_RING = 5;   // covers radius up to 5.5 cells (MAX_RADIUS is 5)

    struct Stencil {
        int ring = 0;                                    // cells out from the middle: (2 ring + 1)^2 cells
        float w[MAX_RING + 1][MAX_RING + 1] = {};        // w[|dx|][|dy|]
    };

    // The stencil for a disc of `radius` cells.
    static const Stencil& of(float radius) {
        static const Footprints table;
        const int b = std::clamp(static_cast<int>(std::lround(radius / STEP)), 0, static_cast<int>(table.s_.size()) - 1);
        return table.s_[static_cast<std::size_t>(b)];
    }

private:
    std::vector<Stencil> s_;

    Footprints() {
        constexpr int SAMPLES = 24;   // per cell side
        const int buckets = static_cast<int>(std::lround((MAX_RING + 0.5f) / STEP)) + 1;
        s_.resize(static_cast<std::size_t>(buckets));
        for (int b = 0; b < buckets; ++b) {
            Stencil& st = s_[static_cast<std::size_t>(b)];
            const float r = std::max(static_cast<float>(b) * STEP, 1e-3f);
            st.ring = std::min(static_cast<int>(std::ceil(r - 0.5f)), MAX_RING);
            if (st.ring < 0) st.ring = 0;
            double total = 0.0;
            double w[MAX_RING + 1][MAX_RING + 1] = {};
            for (int a = 0; a <= st.ring; ++a)
                for (int c = 0; c <= st.ring; ++c) {
                    int in = 0;
                    for (int u = 0; u < SAMPLES; ++u)
                        for (int v = 0; v < SAMPLES; ++v) {
                            // Offset from the disc's centre (the middle of cell 0, 0).
                            const float x = static_cast<float>(a) - 0.5f + (static_cast<float>(u) + 0.5f) / SAMPLES;
                            const float y = static_cast<float>(c) - 0.5f + (static_cast<float>(v) + 0.5f) / SAMPLES;
                            in += x * x + y * y <= r * r;
                        }
                    w[a][c] = in;
                    total += in * (a == 0 ? 1 : 2) * (c == 0 ? 1 : 2);
                }
            for (int a = 0; a <= st.ring; ++a)
                for (int c = 0; c <= st.ring; ++c) st.w[a][c] = total > 0.0 ? static_cast<float>(w[a][c] / total) : (a == 0 && c == 0);
            if (total <= 0.0) st.w[0][0] = 1.0f;
        }
    }
};

}  // namespace luca
