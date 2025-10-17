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

# 显示帮助信息
show_help() {
    echo -e "${GREEN}用法: $0 [选项]${NC}"
    echo "选项:"
    echo "  -r, --release    构建 Release 版本（默认: Debug）"
    echo "  -c, --clean      清理后重新构建"
    echo "  -h, --help       显示帮助信息"
    echo ""
    echo "示例:"
    echo "  $0                # 构建 Debug 版本"
    echo "  $0 -r             # 构建 Release 版本"
    echo "  $0 -c             # 清理构建"
    echo ""
    echo "构建完成后，可执行文件位于 build/ 目录下，如："
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
if ! cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE; then
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

# 显示可用的可执行文件
echo -e "${GREEN}可用的测试程序：${NC}"
for exe in build/Test*; do
    if [ -x "$exe" ]; then
        echo "  $exe"
    fi
done

echo -e "${YELLOW}提示：选择一个测试程序运行，例如：${NC}"
echo "  ./build/TestMonitor"