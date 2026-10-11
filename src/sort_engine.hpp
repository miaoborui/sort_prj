#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class SortAlgorithm {
    Bubble,
    Cocktail,
    Selection,
    Insertion,
    Shell,
    Quick,
    Merge,
    Heap,
    Counting,
    Radix,
};

struct AlgorithmInfo {
    SortAlgorithm id;
    std::string name;
    std::string summary;
    std::string complexity;
};

struct SortFrame {
    std::vector<int> values;
    std::vector<int> active_indices;
    std::vector<int> sorted_indices;
    std::string action;
    std::size_t comparisons = 0;
    std::size_t writes = 0;
};

const std::vector<AlgorithmInfo>& GetAlgorithms();
std::vector<int> GenerateShuffledSequence(int count, std::uint32_t seed);
std::vector<SortFrame> BuildSortFrames(SortAlgorithm algorithm, const std::vector<int>& input);
