#include "ftxui/component/animation.hpp"  // for ElasticOut, Linear
#include "ftxui/component/component.hpp"  // for Menu, Horizontal, Renderer, Vertical, Button
#include "ftxui/component/component_base.hpp"     // for ComponentBase
#include "ftxui/component/component_options.hpp"  // for MenuOption, EntryState, MenuEntryOption, AnimatedColorOption, AnimatedColorsOption, UnderlineOption, ButtonOption
#include "ftxui/component/screen_interactive.hpp"  // for Component, ScreenInteractive
#include "ftxui/dom/elements.hpp"  // for separator, operator|, Element, text, bgcolor, hbox, bold, color, filler, border, vbox, borderDouble, dim, flex, hcenter, yframe, center, yscroll_indicator
#include "ftxui/screen/color.hpp"  // for Color, Color::Red, Color::Black, Color::Yellow, Color::Blue, Color::Default, Color::White, Color::Green

#include "MB_DDF/DDS/DDSCore.h"
#include "MB_DDF/Debug/Logger.h"
#include "MB_DDF/Monitor/DDSMonitor.h"

#include <vector>
#include <string>
#include <mutex>

using namespace ftxui;

// 文本浏览器类，支持颜色控制和线程安全
class TextBrowser {
private:
    std::vector<std::pair<std::string, Color>> lines_;
    std::mutex mutex_;
    size_t max_lines_ = 1000;  // 最大行数限制

public:
    // 追加文本，可指定颜色
    void append(const std::string& text, Color color = Color::White) {
        std::lock_guard<std::mutex> lock(mutex_);
        lines_.push_back({text, color});
        
        // 限制行数，避免内存无限增长
        if (lines_.size() > max_lines_) {
            lines_.erase(lines_.begin());
        }
    }
    
    // 清空内容
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        lines_.clear();
    }
    
    // 获取渲染元素
    Element render() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Element> elements;
        
        for (const auto& line : lines_) {
            elements.push_back(text(line.first) | color(line.second));
        }
        
        if (elements.empty()) {
            return text("日志输出区域") | dim | center;
        }
        
        // 使用yframe来确保正确的滚动行为
        return vbox(std::move(elements)) | yframe | flex;
    }
};

Component HMenu(std::vector<std::string>* entries, int* selected) {
    auto option = MenuOption::HorizontalAnimated();
    option.underline.SetAnimation(std::chrono::milliseconds(1500),
                                animation::easing::ElasticOut);
    option.entries_option.transform = [](EntryState state) {
        Element e = text(state.label) | hcenter | flex;
        if (state.active && state.focused) {
        e = e | bold;
        }
        if (!state.focused && !state.active) {
        e = e | dim;
        }
        return e;
    };
    option.underline.color_inactive = Color::Default;
    option.underline.color_active = Color::Red;
    return Menu(entries, selected, option);
}

int main() {
    // dds monitor    
    // 设置日志输出级别
    LOG_SET_LEVEL_ERROR();

    // 初始化DDS核心，分配共享内存
    auto& dds = MB_DDF::DDS::DDSCore::instance();
    dds.initialize(128 * 1024 * 1024);
        
    // 创建监控器实例，1秒扫描间隔，3秒活跃超时
    MB_DDF::Monitor::DDSMonitor monitor(1000, 3000);  
          
    // 初始化监控器
    if (!monitor.initialize(dds)) {
        LOG_ERROR << "Failed to initialize DDS monitor";
        return -1;
    }

    // ftxui
    // 初始化FTXUI屏幕
    auto screen = ScreenInteractive::TerminalOutput();

    // 创建测试项菜单
    std::vector<std::string> test_entries{
      "DDS综合测试", "实时性测试", "串口测试", "CANFD测试", "图像测试", "舵机测试",
    };
    int selected = 0;

    // 创建文本浏览器实例
    TextBrowser text_browser;
    
    // 监控状态
    bool monitoring_active = false;
    
    // 创建横向菜单组件
    auto hmenu = HMenu(&test_entries, &selected);
    
    // 创建DDS测试组件（带监控按钮）

    auto style = ButtonOption::Animated(Color::Default, Color::GrayDark,
                                      Color::Blue, Color::White);
    auto start_monitor_btn = Button("开始监控", [&] {
        monitoring_active = true;
        text_browser.append("开始监控DDS状态...", Color::Green);
        // 这里可以添加实际的监控逻辑
    }, style);
    auto stop_monitor_btn = Button("停止监控", [&] {
        monitoring_active = false;
        text_browser.append("停止监控DDS状态", Color::Red);
        // 这里可以添加实际的停止监控逻辑
    }, style);
    
    // 创建按钮容器，确保按钮可以被点击
    auto button_container = Container::Horizontal({
        start_monitor_btn,
        stop_monitor_btn,
    });
    
    // 创建其他测试组件
    auto realtime_test_component = Renderer([] {
        return vbox({
                filler(),
                text("实时性测试内容") | center,
                filler(),
            }) | flex;
    });
    
    auto serial_test_component = Renderer([] {
        return vbox({
                filler(),
                text("串口测试内容") | center,
                filler(),
            }) | flex;
    });
    
    auto canfd_test_component = Renderer([] {
        return vbox({
                filler(),
                text("CANFD测试内容") | center,
                filler(),
            }) | flex;
    });
    
    auto image_test_component = Renderer([] {
        return vbox({
                filler(),
                text("图像测试内容") | center,
                filler(),
            }) | flex;
    });
    
    auto servo_test_component = Renderer([] {
        return vbox({
                filler(),
                text("舵机测试内容") | center,
                filler(),
            }) | flex;
    });

    // 创建主容器 - 包含所有可交互组件
    auto container = Container::Vertical({
      hmenu,
      button_container,  // 添加按钮容器以确保按钮可点击
    });

    // 创建渲染器
    auto renderer = Renderer(container, [&] {
        // 创建标题 - 加粗蓝色，靠左
        auto title = text(L"  弹载系统测试程序  ") | bold | color(Color::Blue) | hcenter;
        
        // 根据选择的测试项创建对应的组件
        Element test_component;
        switch (selected) {
            case 0: { // DDS综合测试
                test_component = vbox({
                    button_container->Render(),  // 使用按钮容器
                    filler(),
                    text("DDS综合测试内容") | center,
                    filler(),
                }) | flex;
                break;
            }
            case 1: // 实时性测试
                test_component = realtime_test_component->Render();
                break;
            case 2: // 串口测试
                test_component = serial_test_component->Render();
                break;
            case 3: // CANFD测试
                test_component = canfd_test_component->Render();
                break;
            case 4: // 图像测试
                test_component = image_test_component->Render();
                break;
            case 5: // 舵机测试
                test_component = servo_test_component->Render();
                break;
            default:
                test_component = text("未知测试项") | center;
        }
        
        // 创建测试窗口
        auto test_window = window(
            text(L"测试窗口"),
            test_component
        ) | flex;
        
        // 创建文本浏览器窗口（占1/4空间）
        auto text_browser_window = window(
            text(L"日志输出"),
            text_browser.render()
        ) | size(WIDTH, EQUAL, 30);  // 大约占1/4屏幕宽度
        
        return vbox({
            title,
            separator(),
            hmenu->Render() | flex,  // 横向菜单撑满整个宽度
            separator(),
            hbox({
                test_window | flex,      // 测试窗口占3/4宽度
                text_browser_window,     // 文本浏览器占1/4宽度
            }) | flex,
        });
    });

    // 添加一些初始日志
    text_browser.append("系统启动完成", Color::Green);
    text_browser.append("DDS核心已初始化", Color::Blue);
    text_browser.append("监控器已就绪", Color::Blue);
    text_browser.append("XXXX Test XXXX", Color::Red);
    text_browser.append("XXXX Test XXXX", Color::Red);
    text_browser.append("XXXX Test XXXX", Color::Red);
    text_browser.append("XXXX Test XXXX", Color::Red);
    text_browser.append("XXXX Test XXXX", Color::Red);
    text_browser.append("XXXX Test XXXX", Color::Red);
    text_browser.append("XXXX Test XXXX", Color::Red);
    text_browser.append("XXXX Test XXXX", Color::Red);
    text_browser.append("XXXX Test XXXX", Color::Red);
    text_browser.append("XXXX Test XXXX", Color::Red);
    text_browser.append("XXXX Test XXXX", Color::Red);
    text_browser.append("XXXX Test XXXX", Color::Red);
    text_browser.append("XXXX Test XXXX", Color::Red);
    text_browser.append("XXXX Test XXXX", Color::Red);
    text_browser.append("XXXX Test XXXX", Color::Red);
    text_browser.append("XXXX Test XXXX", Color::Red);
    text_browser.append("XXXX Test XXXX", Color::Red);
    text_browser.append("XXXX Test XXXX", Color::Red);
    text_browser.append("XXXX Test XXXX", Color::Red);
    text_browser.append("XXXX Test XXXX", Color::Red);
    text_browser.append("XXXX Test XXXX111111111111111111111111111", Color::Red);

    // 进入主循环，不会退出
    screen.Loop(renderer);
    return 0;
}
