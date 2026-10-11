#include "sort_engine.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>

int main() {
    assert(GetAlgorithms().size() == 10);
    const auto input = GenerateShuffledSequence(32, 2026);
    assert(input.size() == 32);
    assert(!std::is_sorted(input.begin(), input.end()));
    assert(input == GenerateShuffledSequence(32, 2026));

    for (const auto& algorithm : GetAlgorithms()) {
        const auto frames = BuildSortFrames(algorithm.id, input);
        assert(frames.size() > 1);
        assert(frames.front().values == input);
        assert(std::is_sorted(frames.back().values.begin(), frames.back().values.end()));
        assert(frames.back().sorted_indices.size() == input.size());
        assert(frames.back().action == "排序完成");
        std::cout << algorithm.name << ": " << frames.size() << " frames\n";
    }
}
