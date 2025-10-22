#include "MB_DDF/Timer/SystemTimer.h"
#include "MB_DDF/Timer/ChronoHelper.h"

// 测试实时定时器
using namespace MB_DDF::Timer;

// 测试回调函数，记录当前时间
void my_callback() {
    ChronoHelper::record(0);
}

// 测试主函数，设置实时定时器
int main() {
    SystemTimerOptions opt;
    opt.use_new_thread = true;                 // 使用独立线程
    opt.sched_policy = SCHED_FIFO;             // 新线程调度策略：FIFO
    opt.priority = sched_get_priority_max(SCHED_FIFO); // 优先级设为最高
    opt.cpu = 6;                               // 绑核
    opt.signal_no = SIGRTMIN;                  // 使用实时信号

    auto my_timer 
    = SystemTimer::start("250us", my_callback, opt);

    std::cout << "press enter to stop" << std::endl;
    std::cin.get();
    my_timer->stop();

    return 0;
}