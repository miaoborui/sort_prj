#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/canvas.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/cell.hpp>
#include <ftxui/screen/color.hpp>
#include <functional>
#include <random>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

#include "sort_engine.hpp"

namespace {

using namespace ftxui;
using Clock = std::chrono::steady_clock;

const Color kAccent = Color::RGB(76, 154, 255);
const Color kActive = Color::RGB(255, 196, 74);
const Color kSorted = Color::RGB(75, 201, 133);
const Color kMuted = Color::RGB(135, 145, 160);
const Color kDanger = Color::RGB(244, 112, 112);

bool Contains(const std::vector<int>& indices, int value) {
    return std::find(indices.begin(), indices.end(), value) != indices.end();
}

Element Swatch(Color swatch_color, const std::string& label) {
    return hbox({text("  ") | bgcolor(swatch_color), text(" " + label)});
}

Element Header(const std::string& section) {
    return hbox({text(" SORT LAB ") | bold | color(Color::White) | bgcolor(kAccent),
                 text("  " + section) | bold, filler(), text("FTXUI") | color(kMuted)});
}

Element NumberField(const Component& input, bool valid) {
    return input->Render() | color(valid ? Color::White : kDanger) |
           bgcolor(Color::RGB(38, 43, 52)) | size(WIDTH, EQUAL, 9);
}

Element BarChart(const SortFrame& frame) {
    const auto values = frame.values;
    const auto active = frame.active_indices;
    const auto sorted = frame.sorted_indices;

    return canvas([values, active, sorted](Canvas& drawing) {
               const int width = drawing.width();
               const int height = drawing.height();
               const int terminal_columns = width / 2;
               const int terminal_rows = height / 4;
               if (values.empty() || terminal_columns <= 0 || terminal_rows <= 1) {
                   return;
               }

               const int maximum = *std::max_element(values.begin(), values.end());
               const int count = static_cast<int>(values.size());
               const int gap = terminal_columns >= count * 2 - 1 ? 1 : 0;
               const int bar_width = std::max(1, (terminal_columns - gap * (count - 1)) / count);
               const int chart_width = count * bar_width + (count - 1) * gap;
               const int left_padding = std::max(0, (terminal_columns - chart_width) / 2);
               const int usable_rows = terminal_rows - 1;

               for (int index = 0; index < count; ++index) {
                   Color bar_color = kAccent;
                   if (Contains(sorted, index)) {
                       bar_color = kSorted;
                   }
                   if (Contains(active, index)) {
                       bar_color = kActive;
                   }

                   Cell bar_cell;
                   bar_cell.character = "█";
                   bar_cell.foreground_color = bar_color;
                   const int bar_height =
                       std::max(1, values[static_cast<std::size_t>(index)] * usable_rows / maximum);
                   const int top_row = terminal_rows - bar_height;
                   const int start_column = left_padding + index * (bar_width + gap);
                   for (int column = 0; column < bar_width; ++column) {
                       for (int row = top_row; row < terminal_rows; ++row) {
                           drawing.DrawCell((start_column + column) * 2, row * 4, bar_cell);
                       }
                   }
               }
           }) |
           size(HEIGHT, EQUAL, 17) | flex;
}

std::chrono::milliseconds FrameInterval(int speed) {
    static constexpr int delays[] = {650, 480, 350, 250, 180, 125, 85, 55, 35, 20};
    speed = std::clamp(speed, 1, 10);
    return std::chrono::milliseconds(delays[speed - 1]);
}

bool ParseBoundedInteger(std::string_view text, int minimum, int maximum, int& value) {
    if (text.empty()) {
        return false;
    }
    int parsed = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (error != std::errc{} || end != text.data() + text.size() || parsed < minimum ||
        parsed > maximum) {
        return false;
    }
    value = parsed;
    return true;
}

}  // namespace

int main() {
    auto screen = ScreenInteractive::Fullscreen();
    const auto& algorithms = GetAlgorithms();

    int page = 0;
    int selected_algorithm = 0;
    int element_count = 24;
    int speed = 6;
    int seed = 2026;
    std::string element_count_text = std::to_string(element_count);
    std::string speed_text = std::to_string(speed);
    std::string seed_text = std::to_string(seed);
    int element_count_cursor = static_cast<int>(element_count_text.size());
    int speed_cursor = static_cast<int>(speed_text.size());
    int seed_cursor = static_cast<int>(seed_text.size());
    bool element_count_valid = true;
    bool speed_valid = true;
    bool seed_valid = true;
    std::string settings_message;
    bool settings_message_is_error = false;
    std::size_t frame_index = 0;
    bool playing = false;
    std::atomic<bool> animation_active = false;
    std::vector<SortFrame> frames;
    auto last_tick = Clock::now();

    std::vector<std::string> algorithm_names;
    algorithm_names.reserve(algorithms.size());
    for (const auto& algorithm : algorithms) {
        algorithm_names.push_back(algorithm.name + "   " + algorithm.complexity);
    }

    auto open_settings = [&] { page = 1; };
    auto build_visualization = [&] {
        element_count_valid = ParseBoundedInteger(element_count_text, 8, 64, element_count);
        speed_valid = ParseBoundedInteger(speed_text, 1, 10, speed);
        seed_valid = ParseBoundedInteger(seed_text, 1, 9999, seed);
        if (!element_count_valid || !speed_valid || !seed_valid) {
            settings_message = "请修正红色参数：元素 8–64，速度 1–10，种子 1–9999";
            settings_message_is_error = true;
            return;
        }
        const auto input =
            GenerateShuffledSequence(element_count, static_cast<std::uint32_t>(seed));
        frames =
            BuildSortFrames(algorithms[static_cast<std::size_t>(selected_algorithm)].id, input);
        frame_index = 0;
        playing = frames.size() > 1;
        animation_active = playing;
        last_tick = Clock::now();
        page = 2;
    };
    auto replay = [&] {
        frame_index = 0;
        playing = frames.size() > 1;
        animation_active = playing;
        last_tick = Clock::now();
    };
    auto reshuffle = [&] {
        seed = seed == 9999 ? 1 : seed + 1;
        seed_text = std::to_string(seed);
        seed_cursor = static_cast<int>(seed_text.size());
        seed_valid = true;
        build_visualization();
    };
    auto toggle_playback = [&] {
        if (frames.empty()) {
            return;
        }
        if (frame_index + 1 >= frames.size()) {
            frame_index = 0;
        }
        playing = !playing;
        animation_active = playing;
        last_tick = Clock::now();
    };
    auto step_forward = [&] {
        playing = false;
        animation_active = false;
        if (frame_index + 1 < frames.size()) {
            ++frame_index;
        }
    };
    auto step_back = [&] {
        playing = false;
        animation_active = false;
        if (frame_index > 0) {
            --frame_index;
        }
    };

    MenuOption menu_options = MenuOption::VerticalAnimated();
    menu_options.on_enter = open_settings;
    menu_options.entries_option.transform = [](const EntryState& state) {
        auto entry = text((state.active ? "  > " : "    ") + state.label);
        if (state.active) {
            entry = entry | bold | color(kAccent);
        } else {
            entry = entry | color(Color::GrayLight);
        }
        return entry;
    };
    auto algorithm_menu = Menu(&algorithm_names, &selected_algorithm, menu_options);
    auto configure_button = Button("配置所选算法", open_settings, ButtonOption::Animated(kAccent));
    auto quit_button = Button("退出", screen.ExitLoopClosure(), ButtonOption::Animated(kDanger));
    auto menu_controls = Container::Vertical({algorithm_menu, configure_button, quit_button});

    auto main_page = Renderer(menu_controls, [&] {
        const auto& current = algorithms[static_cast<std::size_t>(selected_algorithm)];
        return vbox({
                   Header("选择排序算法"),
                   separator(),
                   hbox({
                       window(text(" 算法 ") | bold,
                              vbox({algorithm_menu->Render(), separatorEmpty(),
                                    hbox({configure_button->Render(), text("  "),
                                          quit_button->Render()})})) |
                           flex,
                       separator(),
                       window(text(" 概览 ") | bold,
                              vbox({text(current.name) | bold | color(kAccent), separatorEmpty(),
                                    paragraph(current.summary), separatorEmpty(),
                                    text("复杂度  " + current.complexity) | color(kActive),
                                    filler(), text("Enter 进入参数配置") | color(kMuted)})) |
                           size(WIDTH, EQUAL, 38),
                   }) | flex,
                   separator(),
                   text("↑↓ 选择   Enter 确认   Q 退出") | color(kMuted) | center,
               }) |
               border;
    });

    SliderOption<int> count_slider_options;
    count_slider_options.value = &element_count;
    count_slider_options.min = 8;
    count_slider_options.max = 64;
    count_slider_options.increment = 1;
    count_slider_options.color_active = kAccent;
    count_slider_options.on_change = [&] {
        element_count_text = std::to_string(element_count);
        element_count_cursor = static_cast<int>(element_count_text.size());
        element_count_valid = true;
        settings_message.clear();
    };
    auto count_slider = Slider(count_slider_options);

    SliderOption<int> speed_slider_options;
    speed_slider_options.value = &speed;
    speed_slider_options.min = 1;
    speed_slider_options.max = 10;
    speed_slider_options.increment = 1;
    speed_slider_options.color_active = kAccent;
    speed_slider_options.on_change = [&] {
        speed_text = std::to_string(speed);
        speed_cursor = static_cast<int>(speed_text.size());
        speed_valid = true;
        settings_message.clear();
    };
    auto speed_slider = Slider(speed_slider_options);

    SliderOption<int> seed_slider_options;
    seed_slider_options.value = &seed;
    seed_slider_options.min = 1;
    seed_slider_options.max = 9999;
    seed_slider_options.increment = 1;
    seed_slider_options.color_active = kAccent;
    seed_slider_options.on_change = [&] {
        seed_text = std::to_string(seed);
        seed_cursor = static_cast<int>(seed_text.size());
        seed_valid = true;
        settings_message.clear();
    };
    auto seed_slider = Slider(seed_slider_options);

    auto NumberInputOptions = [&](std::string& content, int& cursor, int minimum, int maximum,
                                  int& value, bool& valid) {
        InputOption options = InputOption::Default();
        options.content = &content;
        options.cursor_position = &cursor;
        options.multiline = false;
        options.on_change = [&content, minimum, maximum, &value, &valid, &settings_message] {
            valid = ParseBoundedInteger(content, minimum, maximum, value);
            settings_message.clear();
        };
        options.on_enter = build_visualization;
        return options;
    };
    auto OnlyDigits = [](Event event) {
        if (!event.is_character()) {
            return false;
        }
        const std::string characters = event.character();
        return std::any_of(characters.begin(), characters.end(),
                           [](char character) { return character < '0' || character > '9'; });
    };
    auto count_input =
        CatchEvent(Input(NumberInputOptions(element_count_text, element_count_cursor, 8, 64,
                                            element_count, element_count_valid)),
                   OnlyDigits);
    auto speed_input = CatchEvent(
        Input(NumberInputOptions(speed_text, speed_cursor, 1, 10, speed, speed_valid)), OnlyDigits);
    auto seed_input = CatchEvent(
        Input(NumberInputOptions(seed_text, seed_cursor, 1, 9999, seed, seed_valid)), OnlyDigits);

    std::random_device random_device;
    std::mt19937 seed_generator(random_device());
    std::uniform_int_distribution<int> seed_distribution(1, 9999);
    auto random_seed_button = Button(
        "随机",
        [&] {
            seed = seed_distribution(seed_generator);
            seed_text = std::to_string(seed);
            seed_cursor = static_cast<int>(seed_text.size());
            seed_valid = true;
            settings_message = "已生成新的随机种子";
            settings_message_is_error = false;
        },
        ButtonOption::Ascii());
    auto start_button =
        Button("生成序列并开始", build_visualization, ButtonOption::Animated(kSorted));
    auto settings_back_button = Button("返回", [&] { page = 0; }, ButtonOption::Animated(kMuted));
    auto count_controls = Container::Horizontal({count_slider, count_input});
    auto speed_controls = Container::Horizontal({speed_slider, speed_input});
    auto seed_controls = Container::Horizontal({seed_slider, seed_input, random_seed_button});
    auto settings_controls =
        Container::Vertical({count_controls, speed_controls, seed_controls,
                             Container::Horizontal({start_button, settings_back_button})});

    auto settings_page = Renderer(settings_controls, [&] {
        const auto& current = algorithms[static_cast<std::size_t>(selected_algorithm)];
        return vbox({
                   Header("参数设置"),
                   separator(),
                   filler(),
                   window(text(" " + current.name + " ") | bold | color(kAccent),
                          vbox({paragraph(current.summary), separatorEmpty(),
                                hbox({text("元素数量") | size(WIDTH, EQUAL, 12),
                                      count_slider->Render() | flex, text("  "),
                                      NumberField(count_input, element_count_valid),
                                      text(" 8–64") | color(kMuted) | size(WIDTH, EQUAL, 7)}),
                                separatorEmpty(),
                                hbox({text("播放速度") | size(WIDTH, EQUAL, 12),
                                      speed_slider->Render() | flex, text("  "),
                                      NumberField(speed_input, speed_valid),
                                      text(" 1–10") | color(kMuted) | size(WIDTH, EQUAL, 7)}),
                                separatorEmpty(),
                                hbox({text("随机种子") | size(WIDTH, EQUAL, 12),
                                      seed_slider->Render() | flex, text("  "),
                                      NumberField(seed_input, seed_valid), text(" "),
                                      random_seed_button->Render()}),
                                settings_message.empty()
                                    ? separatorEmpty()
                                    : text(settings_message) |
                                          color(settings_message_is_error ? kDanger : kSorted) |
                                          center,
                                separator(),
                                hbox({filler(), start_button->Render(), text("  "),
                                      settings_back_button->Render(), filler()})})) |
                       size(WIDTH, EQUAL, 72) | center,
                   filler(),
                   separator(),
                   text("鼠标点击/拖动滑块，或在输入框键入数值   Tab 切换   Esc 返回") |
                       color(kMuted) | center,
               }) |
               border;
    });

    auto play_button = Button("暂停/继续", toggle_playback, ButtonOption::Animated(kAccent));
    auto back_step_button = Button("上一步", step_back, ButtonOption::Animated(kMuted));
    auto next_step_button = Button("下一步", step_forward, ButtonOption::Animated(kMuted));
    auto replay_button = Button("重新播放", replay, ButtonOption::Animated(kActive));
    auto shuffle_button = Button("重新打乱", reshuffle, ButtonOption::Animated(kSorted));
    auto visual_back_button = Button("返回设置", [&] {
        playing = false;
        animation_active = false;
        page = 1;
    });
    auto visual_controls =
        Container::Horizontal({play_button, back_step_button, next_step_button, replay_button,
                               shuffle_button, visual_back_button});

    auto visual_page = Renderer(visual_controls, [&] {
        if (frames.empty()) {
            return vbox({Header("排序可视化"), filler(), text("尚未生成序列") | center, filler()}) |
                   border;
        }
        const auto& frame = frames[frame_index];
        const auto& algorithm = algorithms[static_cast<std::size_t>(selected_algorithm)];
        const float progress = frames.size() <= 1 ? 1.0F
                                                  : static_cast<float>(frame_index) /
                                                        static_cast<float>(frames.size() - 1);
        std::string state = "已暂停";
        Color state_color = kActive;
        if (frame_index + 1 >= frames.size()) {
            state = "已完成";
            state_color = kSorted;
        } else if (playing) {
            state = "播放中";
            state_color = kAccent;
        }

        return vbox({
                   Header(algorithm.name),
                   separator(),
                   hbox(
                       {text(" " + state + " ") | bold | color(Color::Black) | bgcolor(state_color),
                        text("  " + frame.action) | bold, filler(),
                        text("比较 " + std::to_string(frame.comparisons)) | color(kActive),
                        text("   写入 " + std::to_string(frame.writes)) | color(kAccent)}),
                   separatorEmpty(),
                   BarChart(frame),
                   separatorEmpty(),
                   hbox({Swatch(kAccent, "待处理"), text("   "), Swatch(kActive, "当前操作"),
                         text("   "), Swatch(kSorted, "已归位"), filler(),
                         text("帧 " + std::to_string(frame_index + 1) + " / " +
                              std::to_string(frames.size())) |
                             color(kMuted)}),
                   gauge(progress) | color(kSorted),
                   separator(),
                   hbox({filler(), play_button->Render(), text(" "), back_step_button->Render(),
                         text(" "), next_step_button->Render(), text(" "), replay_button->Render(),
                         text(" "), shuffle_button->Render(), text(" "),
                         visual_back_button->Render(), filler()}),
                   text("Space 播放/暂停   P/N 单步   R 重播   S 打乱   Esc 返回") | color(kMuted) |
                       center,
               }) |
               border;
    });

    auto pages = Container::Tab({main_page, settings_page, visual_page}, &page);
    auto root = CatchEvent(pages, [&](Event event) {
        if (event == Event::Custom) {
            if (page == 2 && playing && frame_index + 1 < frames.size()) {
                const auto now = Clock::now();
                if (now - last_tick >= FrameInterval(speed)) {
                    ++frame_index;
                    last_tick = now;
                    if (frame_index + 1 >= frames.size()) {
                        playing = false;
                        animation_active = false;
                    }
                }
            }
            return true;
        }
        if (page != 1 && (event == Event::Character('q') || event == Event::Character('Q'))) {
            screen.Exit();
            return true;
        }
        if (event == Event::Escape) {
            if (page == 2) {
                playing = false;
                animation_active = false;
                page = 1;
            } else if (page == 1) {
                page = 0;
            } else {
                screen.Exit();
            }
            return true;
        }
        if (page != 2) {
            return false;
        }
        if (event == Event::Character(' ')) {
            toggle_playback();
            return true;
        }
        if (event == Event::Character('n') || event == Event::Character('N')) {
            step_forward();
            return true;
        }
        if (event == Event::Character('p') || event == Event::Character('P')) {
            step_back();
            return true;
        }
        if (event == Event::Character('r') || event == Event::Character('R')) {
            replay();
            return true;
        }
        if (event == Event::Character('s') || event == Event::Character('S')) {
            reshuffle();
            return true;
        }
        return false;
    });

    std::atomic<bool> refresh_running = true;
    std::thread refresh_thread([&] {
        while (refresh_running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            if (animation_active.load()) {
                screen.PostEvent(Event::Custom);
            }
        }
    });

    screen.Loop(root);
    refresh_running = false;
    refresh_thread.join();
    return 0;
}
