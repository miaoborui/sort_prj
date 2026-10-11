#include "sort_engine.hpp"

#include <algorithm>
#include <functional>
#include <numeric>
#include <random>
#include <utility>

namespace {

class Recorder {
   public:
    explicit Recorder(std::vector<int> input) : values_(std::move(input)) {
        Capture({}, {}, "序列已生成并打乱");
    }

    void Compare(int first, int second, const std::vector<int>& sorted = {},
                 std::string action = "比较两个元素") {
        ++comparisons_;
        Capture({first, second}, sorted, std::move(action));
    }

    void Swap(int first, int second, const std::vector<int>& sorted = {},
              std::string action = "交换元素") {
        std::swap(values_[first], values_[second]);
        writes_ += 2;
        Capture({first, second}, sorted, std::move(action));
    }

    void Write(int index, int value, const std::vector<int>& active,
               const std::vector<int>& sorted = {}, std::string action = "写入元素") {
        values_[index] = value;
        ++writes_;
        Capture(active, sorted, std::move(action));
    }

    void Mark(const std::vector<int>& active, const std::vector<int>& sorted, std::string action) {
        Capture(active, sorted, std::move(action));
    }

    std::vector<int>& Values() { return values_; }

    std::vector<SortFrame> Finish() {
        std::vector<int> all(values_.size());
        std::iota(all.begin(), all.end(), 0);
        Capture({}, all, "排序完成");
        return std::move(frames_);
    }

   private:
    void Capture(std::vector<int> active, std::vector<int> sorted, std::string action) {
        frames_.push_back({values_, std::move(active), std::move(sorted), std::move(action),
                           comparisons_, writes_});
    }

    std::vector<int> values_;
    std::vector<SortFrame> frames_;
    std::size_t comparisons_ = 0;
    std::size_t writes_ = 0;
};

std::vector<int> InclusiveRange(int first, int last) {
    if (first > last) {
        return {};
    }
    std::vector<int> result(static_cast<std::size_t>(last - first + 1));
    std::iota(result.begin(), result.end(), first);
    return result;
}

std::vector<SortFrame> BubbleSort(std::vector<int> input) {
    Recorder recorder(std::move(input));
    auto& values = recorder.Values();
    const int size = static_cast<int>(values.size());

    for (int end = size - 1; end > 0; --end) {
        bool changed = false;
        const auto sorted = InclusiveRange(end + 1, size - 1);
        for (int index = 0; index < end; ++index) {
            recorder.Compare(index, index + 1, sorted);
            if (values[index] > values[index + 1]) {
                recorder.Swap(index, index + 1, sorted);
                changed = true;
            }
        }
        recorder.Mark({}, InclusiveRange(end, size - 1), "最大元素已移动到末端");
        if (!changed) {
            break;
        }
    }
    return recorder.Finish();
}

std::vector<SortFrame> CocktailSort(std::vector<int> input) {
    Recorder recorder(std::move(input));
    auto& values = recorder.Values();
    const int size = static_cast<int>(values.size());
    int left = 0;
    int right = size - 1;

    while (left < right) {
        bool changed = false;
        for (int index = left; index < right; ++index) {
            recorder.Compare(index, index + 1, {}, "从左向右扫描");
            if (values[index] > values[index + 1]) {
                recorder.Swap(index, index + 1, {}, "较大元素向右移动");
                changed = true;
            }
        }
        recorder.Mark({}, InclusiveRange(right, size - 1), "右端位置已经确定");
        --right;
        if (!changed) {
            break;
        }

        changed = false;
        for (int index = right; index > left; --index) {
            recorder.Compare(index - 1, index, InclusiveRange(right + 1, size - 1), "从右向左扫描");
            if (values[index - 1] > values[index]) {
                recorder.Swap(index - 1, index, InclusiveRange(right + 1, size - 1),
                              "较小元素向左移动");
                changed = true;
            }
        }
        recorder.Mark({}, InclusiveRange(0, left), "左端位置已经确定");
        ++left;
        if (!changed) {
            break;
        }
    }
    return recorder.Finish();
}

std::vector<SortFrame> SelectionSort(std::vector<int> input) {
    Recorder recorder(std::move(input));
    auto& values = recorder.Values();
    const int size = static_cast<int>(values.size());

    for (int index = 0; index + 1 < size; ++index) {
        int minimum = index;
        const auto sorted = InclusiveRange(0, index - 1);
        for (int scan = index + 1; scan < size; ++scan) {
            recorder.Compare(minimum, scan, sorted, "寻找未排序区间的最小值");
            if (values[scan] < values[minimum]) {
                minimum = scan;
                recorder.Mark({minimum}, sorted, "更新当前最小值");
            }
        }
        if (minimum != index) {
            recorder.Swap(index, minimum, sorted, "将最小值放到前端");
        }
        recorder.Mark({}, InclusiveRange(0, index), "一个位置已经确定");
    }
    return recorder.Finish();
}

std::vector<SortFrame> InsertionSort(std::vector<int> input) {
    Recorder recorder(std::move(input));
    auto& values = recorder.Values();
    const int size = static_cast<int>(values.size());

    for (int index = 1; index < size; ++index) {
        const int key = values[index];
        int scan = index - 1;
        while (scan >= 0) {
            recorder.Compare(scan, scan + 1, InclusiveRange(0, index - 1),
                             "将当前元素插入有序前缀");
            if (values[scan] <= key) {
                break;
            }
            recorder.Write(scan + 1, values[scan], {scan, scan + 1}, {}, "向右移动较大的元素");
            --scan;
        }
        recorder.Write(scan + 1, key, {scan + 1}, InclusiveRange(0, index), "当前元素插入完成");
    }
    return recorder.Finish();
}

std::vector<SortFrame> ShellSort(std::vector<int> input) {
    Recorder recorder(std::move(input));
    auto& values = recorder.Values();
    const int size = static_cast<int>(values.size());

    for (int gap = size / 2; gap > 0; gap /= 2) {
        recorder.Mark({}, {}, "开始处理间隔 gap = " + std::to_string(gap));
        for (int index = gap; index < size; ++index) {
            const int value = values[index];
            int scan = index;
            while (scan >= gap) {
                recorder.Compare(scan - gap, scan, {}, "比较同一间隔分组中的元素");
                if (values[scan - gap] <= value) {
                    break;
                }
                recorder.Write(scan, values[scan - gap], {scan - gap, scan}, {},
                               "分组元素向后移动");
                scan -= gap;
            }
            recorder.Write(scan, value, {scan}, {}, "元素插入当前间隔分组");
        }
        recorder.Mark({}, gap == 1 ? InclusiveRange(0, size - 1) : std::vector<int>{},
                      "完成当前间隔的分组插入");
    }
    return recorder.Finish();
}

std::vector<SortFrame> QuickSort(std::vector<int> input) {
    Recorder recorder(std::move(input));
    auto& values = recorder.Values();
    std::vector<int> sorted;

    std::function<void(int, int)> sort = [&](int low, int high) {
        if (low > high) {
            return;
        }
        if (low == high) {
            sorted.push_back(low);
            recorder.Mark({}, sorted, "单元素区间已经有序");
            return;
        }

        const int pivot_value = values[high];
        int boundary = low;
        recorder.Mark({high}, sorted, "选择末端元素作为基准");
        for (int scan = low; scan < high; ++scan) {
            recorder.Compare(scan, high, sorted, "与基准元素比较");
            if (values[scan] < pivot_value) {
                if (boundary != scan) {
                    recorder.Swap(boundary, scan, sorted, "元素移入基准左侧");
                }
                ++boundary;
            }
        }
        if (boundary != high) {
            recorder.Swap(boundary, high, sorted, "基准元素移动到最终位置");
        }
        sorted.push_back(boundary);
        recorder.Mark({}, sorted, "基准位置已经确定");
        sort(low, boundary - 1);
        sort(boundary + 1, high);
    };

    sort(0, static_cast<int>(values.size()) - 1);
    return recorder.Finish();
}

std::vector<SortFrame> MergeSort(std::vector<int> input) {
    Recorder recorder(std::move(input));
    auto& values = recorder.Values();

    std::function<void(int, int)> sort = [&](int left, int right) {
        if (left >= right) {
            return;
        }
        const int middle = left + (right - left) / 2;
        sort(left, middle);
        sort(middle + 1, right);

        std::vector<int> merged;
        merged.reserve(static_cast<std::size_t>(right - left + 1));
        int first = left;
        int second = middle + 1;
        while (first <= middle && second <= right) {
            recorder.Compare(first, second, {}, "比较两个有序区间的头部");
            if (values[first] <= values[second]) {
                merged.push_back(values[first++]);
            } else {
                merged.push_back(values[second++]);
            }
        }
        while (first <= middle) {
            merged.push_back(values[first++]);
        }
        while (second <= right) {
            merged.push_back(values[second++]);
        }
        for (int offset = 0; offset < static_cast<int>(merged.size()); ++offset) {
            recorder.Write(left + offset, merged[static_cast<std::size_t>(offset)], {left + offset},
                           {}, "合并区间并写回序列");
        }
        recorder.Mark(InclusiveRange(left, right), {}, "当前区间合并完成");
    };

    sort(0, static_cast<int>(values.size()) - 1);
    return recorder.Finish();
}

std::vector<SortFrame> HeapSort(std::vector<int> input) {
    Recorder recorder(std::move(input));
    auto& values = recorder.Values();
    const int size = static_cast<int>(values.size());

    std::function<void(int, int, const std::vector<int>&)> sift_down =
        [&](int root, int heap_size, const std::vector<int>& sorted) {
            while (true) {
                int largest = root;
                const int left = root * 2 + 1;
                const int right = root * 2 + 2;
                if (left < heap_size) {
                    recorder.Compare(largest, left, sorted, "比较堆节点与左子节点");
                    if (values[left] > values[largest]) {
                        largest = left;
                    }
                }
                if (right < heap_size) {
                    recorder.Compare(largest, right, sorted, "比较堆节点与右子节点");
                    if (values[right] > values[largest]) {
                        largest = right;
                    }
                }
                if (largest == root) {
                    return;
                }
                recorder.Swap(root, largest, sorted, "恢复最大堆结构");
                root = largest;
            }
        };

    for (int root = size / 2 - 1; root >= 0; --root) {
        sift_down(root, size, {});
    }
    recorder.Mark({}, {}, "最大堆构建完成");
    for (int end = size - 1; end > 0; --end) {
        const auto sorted = InclusiveRange(end + 1, size - 1);
        recorder.Swap(0, end, sorted, "堆顶最大值移动到末端");
        const auto new_sorted = InclusiveRange(end, size - 1);
        recorder.Mark({}, new_sorted, "一个位置已经确定");
        sift_down(0, end, new_sorted);
    }
    return recorder.Finish();
}

std::vector<SortFrame> CountingSort(std::vector<int> input) {
    Recorder recorder(std::move(input));
    auto& values = recorder.Values();
    const int maximum = *std::max_element(values.begin(), values.end());
    std::vector<int> counts(static_cast<std::size_t>(maximum + 1));

    for (int index = 0; index < static_cast<int>(values.size()); ++index) {
        ++counts[static_cast<std::size_t>(values[index])];
        recorder.Mark({index}, {}, "统计数值 " + std::to_string(values[index]) + " 的出现次数");
    }

    int output = 0;
    for (int value = 0; value <= maximum; ++value) {
        while (counts[static_cast<std::size_t>(value)]-- > 0) {
            recorder.Write(output, value, {output}, InclusiveRange(0, output - 1),
                           "按计数结果写回数值 " + std::to_string(value));
            ++output;
        }
    }
    return recorder.Finish();
}

std::vector<SortFrame> RadixSort(std::vector<int> input) {
    Recorder recorder(std::move(input));
    auto& values = recorder.Values();
    const int maximum = *std::max_element(values.begin(), values.end());
    std::vector<int> output(values.size());

    for (int exponent = 1; maximum / exponent > 0; exponent *= 10) {
        std::vector<int> counts(10);
        recorder.Mark({}, {}, "按个位权 " + std::to_string(exponent) + " 进行分配");
        for (int index = 0; index < static_cast<int>(values.size()); ++index) {
            const int digit = values[index] / exponent % 10;
            ++counts[static_cast<std::size_t>(digit)];
            recorder.Mark({index}, {}, "读取当前位数字 " + std::to_string(digit));
        }
        for (int digit = 1; digit < 10; ++digit) {
            counts[static_cast<std::size_t>(digit)] += counts[static_cast<std::size_t>(digit - 1)];
        }
        for (int index = static_cast<int>(values.size()) - 1; index >= 0; --index) {
            const int digit = values[index] / exponent % 10;
            output[static_cast<std::size_t>(--counts[static_cast<std::size_t>(digit)])] =
                values[index];
        }
        for (int index = 0; index < static_cast<int>(values.size()); ++index) {
            recorder.Write(index, output[static_cast<std::size_t>(index)], {index}, {},
                           "按当前位顺序写回序列");
        }
        recorder.Mark({}, {}, "当前位排序完成");
        if (exponent > maximum / 10) {
            break;
        }
    }
    return recorder.Finish();
}

}  // namespace

const std::vector<AlgorithmInfo>& GetAlgorithms() {
    static const std::vector<AlgorithmInfo> algorithms = {
        {SortAlgorithm::Bubble, "冒泡排序", "反复比较相邻元素，让较大值逐步上浮", "O(n²) / 稳定"},
        {SortAlgorithm::Cocktail, "鸡尾酒排序", "交替进行左右方向的相邻元素扫描", "O(n²) / 稳定"},
        {SortAlgorithm::Selection, "选择排序", "每轮选择最小值放入已排序前缀", "O(n²) / 原地"},
        {SortAlgorithm::Insertion, "插入排序", "将元素逐个插入前方的有序区间", "O(n²) / 稳定"},
        {SortAlgorithm::Shell, "希尔排序", "按递减间隔执行分组插入排序", "约 O(n^1.5) / 原地"},
        {SortAlgorithm::Quick, "快速排序", "围绕基准划分区间并递归处理", "平均 O(n log n)"},
        {SortAlgorithm::Merge, "归并排序", "拆分序列，再合并相邻有序区间", "O(n log n) / 稳定"},
        {SortAlgorithm::Heap, "堆排序", "构建最大堆并依次取出堆顶", "O(n log n) / 原地"},
        {SortAlgorithm::Counting, "计数排序", "统计每个数值的出现次数后依次写回",
         "O(n + k) / 非比较"},
        {SortAlgorithm::Radix, "基数排序", "从低位到高位进行稳定的逐位排序", "O(d(n + k)) / 稳定"},
    };
    return algorithms;
}

std::vector<int> GenerateShuffledSequence(int count, std::uint32_t seed) {
    count = std::max(2, count);
    std::vector<int> values(static_cast<std::size_t>(count));
    std::iota(values.begin(), values.end(), 1);
    std::mt19937 generator(seed);
    std::shuffle(values.begin(), values.end(), generator);
    if (std::is_sorted(values.begin(), values.end())) {
        std::rotate(values.begin(), values.begin() + 1, values.end());
    }
    return values;
}

std::vector<SortFrame> BuildSortFrames(SortAlgorithm algorithm, const std::vector<int>& input) {
    switch (algorithm) {
        case SortAlgorithm::Bubble:
            return BubbleSort(input);
        case SortAlgorithm::Cocktail:
            return CocktailSort(input);
        case SortAlgorithm::Selection:
            return SelectionSort(input);
        case SortAlgorithm::Insertion:
            return InsertionSort(input);
        case SortAlgorithm::Shell:
            return ShellSort(input);
        case SortAlgorithm::Quick:
            return QuickSort(input);
        case SortAlgorithm::Merge:
            return MergeSort(input);
        case SortAlgorithm::Heap:
            return HeapSort(input);
        case SortAlgorithm::Counting:
            return CountingSort(input);
        case SortAlgorithm::Radix:
            return RadixSort(input);
    }
    return {};
}
