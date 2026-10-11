# FTXUI 排序可视化

一个基于 FTXUI 的终端排序演示程序。支持冒泡、鸡尾酒、选择、插入、希尔、快速、归并、堆、计数和基数排序，并可暂停、单步、重播或重新打乱序列。

## 构建

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## 运行

```bash
./build/sort_visualizer
```

界面支持鼠标和键盘。参数页可以点击或拖动滑块，也可以在输入框中直接键入数值；随机按钮会生成新的随机种子。可视化页面可使用 `Space` 暂停/继续，`N`/`P` 单步前后移动，`R` 从头播放，`S` 重新打乱，`Esc` 返回，`Q` 退出。
