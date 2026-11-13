#pragma once
// Spatial grid: buckets creatures by cell, rebuilt every tick. "Who is in
// cell c?" is then a contiguous slice.
//
// On a sparse map it costs O(N) a tick, not O(cells): creatures are put in
// cell order with a two-pass radix sort (stable, so within a cell they stay in
// creature order), and only the cells that held a body last tick are reset
// (14.5 item 1). On a crowded map one counting pass over the cells is
// cheaper, and gives the same order. An
// empty cell's slice is empty; a slice says nothing about where an empty cell
// would sit, so a scan over a row of cells walks them one by one.

#include <cstdint>
#include <span>
#include <vector>

namespace luca {

class SpatialGrid {
public:
    void build(const std::vector<int>& cell_of, int num_cells) {
        const std::size_t cells = static_cast<std::size_t>(num_cells);
        if (first_.size() != cells) {
            first_.assign(cells, 0);
            last_.assign(cells, 0);
            occupied_.clear();
            int bits = 1;
            while ((std::size_t{1} << bits) < cells) ++bits;
            lo_bits_ = (bits + 1) / 2;
            hi_bits_ = bits - lo_bits_;
        }
        for (int c : occupied_) first_[c] = last_[c] = 0;
        occupied_.clear();
        const std::size_t n = cell_of.size();
        idx_.resize(n);
        if (cells <= COUNT_SORT_RATIO * n + 4096) {
            // A crowded map: one counting pass over every cell is cheaper
            // than two radix passes. Same stable order either way.
            count_.assign(cells + 1, 0);
            for (int c : cell_of) ++count_[static_cast<std::size_t>(c) + 1];
            for (std::size_t c = 0; c < cells; ++c) {
                const uint32_t k = count_[c + 1];
                count_[c + 1] = count_[c] + k;
                if (k != 0) {
                    first_[c] = count_[c];
                    last_[c] = count_[c + 1];
                    occupied_.push_back(static_cast<int>(c));
                }
            }
            for (uint32_t i = 0; i < n; ++i) idx_[count_[static_cast<std::size_t>(cell_of[i])]++] = i;
            return;
        }

        // A sparse map: stable LSD radix sort by cell, low bits then high.
        tmp_.resize(n);
        key_.resize(n);
        const uint32_t lo_mask = (1u << lo_bits_) - 1u;
        count_.assign((std::size_t{1} << lo_bits_) + 1, 0);
        for (int c : cell_of) ++count_[(static_cast<uint32_t>(c) & lo_mask) + 1];
        for (std::size_t b = 1; b < count_.size(); ++b) count_[b] += count_[b - 1];
        for (uint32_t i = 0; i < n; ++i) tmp_[count_[static_cast<uint32_t>(cell_of[i]) & lo_mask]++] = i;
        count_.assign((std::size_t{1} << hi_bits_) + 1, 0);
        for (int c : cell_of) ++count_[(static_cast<uint32_t>(c) >> lo_bits_) + 1];
        for (std::size_t b = 1; b < count_.size(); ++b) count_[b] += count_[b - 1];
        for (uint32_t i : tmp_) {
            const uint32_t s = count_[static_cast<uint32_t>(cell_of[i]) >> lo_bits_]++;
            idx_[s] = i;
            key_[s] = cell_of[i];
        }

        for (uint32_t s = 0; s < n; ++s) {
            const int c = key_[s];
            if (s == 0 || key_[s - 1] != c) {
                first_[c] = s;
                occupied_.push_back(c);
            }
            last_[c] = s + 1;
        }
    }

    // Indices of creatures in cell c.
    std::span<const uint32_t> in_cell(int c) const {
        return {idx_.data() + first_[c], idx_.data() + last_[c]};
    }

    uint32_t count(int c) const { return last_[c] - first_[c]; }

    // Slots: positions in the cell-sorted order. Cell c owns slots
    // [begin(c), end(c)), and creature_at(slot) says who sits there. Copying
    // a few fields into slot order makes a neighbourhood scan read memory in
    // a straight line instead of hopping around the creature arrays.
    uint32_t begin(int c) const { return first_[c]; }
    uint32_t end(int c) const { return last_[c]; }
    uint32_t creature_at(uint32_t slot) const { return idx_[slot]; }
    std::size_t slots() const { return idx_.size(); }

    // The cells that hold at least one body, in ascending order.
    const std::vector<int>& occupied() const { return occupied_; }

private:
    std::vector<uint32_t> first_, last_;   // per cell: its slice (0, 0 when empty)
    std::vector<uint32_t> idx_, tmp_;
    std::vector<uint32_t> count_;
    std::vector<int>      occupied_, key_;
    static constexpr std::size_t COUNT_SORT_RATIO = 8;   // cells per body below which counting wins
    int lo_bits_ = 0, hi_bits_ = 0;
};

}  // namespace luca
