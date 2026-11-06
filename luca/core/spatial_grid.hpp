#pragma once
// Spatial grid: buckets creatures by cell with a counting sort, rebuilt
// every tick in O(N). "Who is in cell c?" is then a contiguous slice.
//
// Used now for light competition. Later: contact checks and sensing look
// only at a creature's own and neighbouring cells, never at everyone.

#include <cstdint>
#include <span>
#include <vector>

namespace luca {

class SpatialGrid {
public:
    void build(const std::vector<int>& cell_of, int num_cells) {
        start_.assign(static_cast<std::size_t>(num_cells) + 1, 0);
        for (int c : cell_of) ++start_[static_cast<std::size_t>(c) + 1];
        for (int c = 0; c < num_cells; ++c) start_[c + 1] += start_[c];

        idx_.resize(cell_of.size());
        fill_ = std::vector<uint32_t>(start_.begin(), start_.end() - 1);
        for (uint32_t i = 0; i < cell_of.size(); ++i) idx_[fill_[cell_of[i]]++] = i;
    }

    // Indices of creatures in cell c.
    std::span<const uint32_t> in_cell(int c) const {
        return {idx_.data() + start_[c], idx_.data() + start_[c + 1]};
    }

    uint32_t count(int c) const { return start_[c + 1] - start_[c]; }

private:
    std::vector<uint32_t> start_;  // start_[c] .. start_[c+1] = slice for cell c
    std::vector<uint32_t> idx_;
    std::vector<uint32_t> fill_;
};

}  // namespace luca
