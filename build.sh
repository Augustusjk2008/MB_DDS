#!/bin/bash

# 简化的 CMake 构建脚本
# 支持 Debug/Release 构建和基本调试功能

# 颜色定义
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

# 默认配置
BUILD_TYPE="Debug"
CLEAN_BUILD=false
RUN_ALL_TESTS=false

# 显示帮助信息
show_help() {
    echo -e "${GREEN}用法: $0 [选项]${NC}"
    echo "选项:"
    echo "  -r, --release    构建 Release 版本（默认: Debug）"
    echo "  -c, --clean      清理后重新构建"
    echo "  -t, --test       构建完成后自动运行所有测试程序"
    echo "  -h, --help       显示帮助信息"
    echo ""
    echo "示例:"
    echo "  $0                # 构建 Debug 版本（库+测试程序）"
    echo "  $0 -r             # 构建 Release 版本（库+测试程序）"
    echo "  $0 -c             # 清理构建"
    echo "  $0 -t             # 构建并运行所有测试程序"
    echo "  $0 -r -t          # 构建 Release 版本并运行所有测试程序"
    echo ""
    echo "构建完成后，生成的静态库位于 build/ 目录："
    echo "  ./build/libMB_DDF_CORE.a"
    echo "  ./build/libMB_DDF_PHYSICAL.a"
    echo "  ./build/libMB_DDF_TIMER.a"
    echo "并且可执行文件位于 build/ 目录，如："
    echo "构建完成后，生成的静态库位于 build/ 目录："
    echo "  ./build/libMB_DDF_CORE.a"
    echo "  ./build/libMB_DDF_PHYSICAL.a"
    echo "并且可执行文件位于 build/ 目录，如："
    echo "  ./build/TestMonitor"
    exit 0
}

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -r|--release)
            BUILD_TYPE="Release"
            shift
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -t|--test)
            RUN_ALL_TESTS=true
            shift
            ;;
        -h|--help)
            show_help
            ;;
        *)
            echo -e "${RED}未知选项: $1${NC}"
            echo "使用 -h 查看帮助信息"
            exit 1
            ;;
    esac
done

# 清理构建目录
if [ "$CLEAN_BUILD" = true ]; then
    echo -e "${YELLOW}清理构建目录...${NC}"
    rm -rf build
fi

# 创建并进入构建目录
mkdir -p build
cd build || exit 1

# CMake 配置
echo -e "${YELLOW}配置 CMake ($BUILD_TYPE)...${NC}"
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=$BUILD_TYPE"

if ! cmake .. $CMAKE_ARGS; then
    echo -e "${RED}CMake 配置失败${NC}"
    exit 1
fi

# 编译
echo -e "${YELLOW}编译项目...${NC}"
if ! make -j$(nproc); then
    echo -e "${RED}编译失败${NC}"
    exit 1
fi

echo -e "${GREEN}构建成功 ($BUILD_TYPE)${NC}"

# 返回项目根目录
cd ..

# 工具函数：字节转换为人类可读（KB/MB）
human_size() {
    local size_bytes="$1"
    awk -v s="$size_bytes" 'BEGIN { if (s >= 1048576) printf "%.2f MB", s/1048576; else printf "%.2f KB", s/1024; }'
}

# 显示生成的静态库（相对路径 + 对齐的大小）
echo -e "${GREEN}生成的静态库：${NC}"
lib_rows=()
lib_maxlen=0
for lib in build/*.a; do
    if [ -f "$lib" ]; then
        size=$(stat -c %s "$lib" 2>/dev/null || stat -f %z "$lib")
        hsize=$(human_size "$size")
        lib_rows+=("$lib|$hsize")
        (( ${#lib} > lib_maxlen )) && lib_maxlen=${#lib}
    fi
done
if [ ${#lib_rows[@]} -gt 0 ]; then
    for row in "${lib_rows[@]}"; do
        path="${row%%|*}"
        hsize="${row##*|}"
        printf "  %-*s  %12s\n" "$lib_maxlen" "$path" "$hsize"
    done
else
    echo "  (无)"
fi

# 显示可用的可执行文件（相对路径 + 对齐的大小）
echo -e "${GREEN}可用的测试程序：${NC}"
test_rows=()
test_maxlen=0
for exe in build/Test*; do
    if [ -x "$exe" ]; then
        size=$(stat -c %s "$exe" 2>/dev/null || stat -f %z "$exe")
        hsize=$(human_size "$size")
        test_rows+=("$exe|$hsize")
        (( ${#exe} > test_maxlen )) && test_maxlen=${#exe}
    fi
done
if [ ${#test_rows[@]} -gt 0 ]; then
    for row in "${test_rows[@]}"; do
        path="${row%%|*}"
        hsize="${row##*|}"
        printf "  %-*s  %12s\n" "$test_maxlen" "$path" "$hsize"
    done
else
    echo "  (无)"
fi

# 运行所有测试程序（如果指定了-t选项）
if [ "$RUN_ALL_TESTS" = true ]; then
    echo -e "${YELLOW}开始运行所有测试程序...${NC}"
    echo ""
    
    # 获取所有测试程序
    test_programs=(build/Test*)
    
    echo -e "${YELLOW}发现 ${#test_programs[@]} 个测试程序${NC}"
    
    for exe in "${test_programs[@]}"; do
        if [ -x "$exe" ]; then
            echo -e "${GREEN}启动 $exe...${NC}"
            echo "----------------------------------------"
            
            # 在后台运行测试程序，不设置超时，让程序持续运行
            "$exe" &
            test_pid=$!
            
            # 等待一小段时间检查程序是否成功启动
            # sleep 0.1
            
            # 检查进程是否还在运行
            if kill -0 $test_pid 2>/dev/null; then
                echo -e "${GREEN}程序已成功启动，PID: $test_pid${NC}"
            else
                echo -e "${RED}程序启动失败或立即退出${NC}"
                # 尝试获取退出状态
                wait $test_pid 2>/dev/null
                exit_code=$?
                echo -e "${RED}退出码: $exit_code${NC}"
            fi
            
            echo "----------------------------------------"
            echo ""
        else
            echo -e "${RED}文件 $exe 不可执行或不存在${NC}"
        fi
    done
    
    echo -e "${GREEN}所有测试程序已启动完成${NC}"
    echo -e "${YELLOW}注意：所有程序都在后台运行，使用以下命令查看运行状态：${NC}"
    echo "  ps aux | grep Test"
    echo -e "${YELLOW}要停止所有测试程序，可以使用：${NC}"
    echo "  pkill -f Test"
else
    echo -e "${YELLOW}提示：选择一个测试程序运行，例如：${NC}"
    echo "  ./build/TestMonitor"
    echo -e "${YELLOW}或者使用 -t 选项自动运行所有测试程序：${NC}"
    echo "  $0 -t"
fi