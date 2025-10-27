#!/bin/bash

# 简化的 CMake 构建脚本
# 支持 Debug/Release 构建、交叉编译、本地编译，以及同时构建两者

# 颜色定义
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

# 默认配置
BUILD_TYPE="Debug"
CLEAN_BUILD=false
RUN_ALL_TESTS=false
CROSS_BUILD=false
BOTH_BUILD=false

# 构建输出根目录与子目录
BUILD_ROOT="build"
HOST_SUBDIR="host"
CROSS_SUBDIR="aarch64"
HOST_BUILD_DIR="${BUILD_ROOT}/${HOST_SUBDIR}"
CROSS_BUILD_DIR="${BUILD_ROOT}/${CROSS_SUBDIR}"

# 交叉编译默认配置
CROSS_SDK_DEFAULT="/opt/wanghuo/v2.0.0-rc4"
CROSS_ENV_SCRIPT_DEFAULT="$CROSS_SDK_DEFAULT/environment-setup-armv8a-ucas-linux"
CROSS_SYSROOT_DEFAULT="$CROSS_SDK_DEFAULT/sysroots/armv8a-ucas-linux"
CROSS_C_COMPILER_DEFAULT="aarch64-ucas-linux-gcc"
CROSS_CXX_COMPILER_DEFAULT="aarch64-ucas-linux-g++"
ARM64_LIBS_PREFIX_DEFAULT="/opt/arm64-libs"

# 显示帮助信息
show_help() {
    echo -e "${GREEN}用法: $0 [选项]${NC}"
    echo "选项:"
    echo "  -r, --release    构建 Release 版本（默认: Debug）"
    echo "  -c, --clean      清理后重新构建（清理 ${BUILD_ROOT}/）"
    echo "  -t, --test       构建完成后自动运行所有本机测试程序"
    echo "  --cross          使用 ARM 交叉编译工具链构建 (aarch64)"
    echo "  --both           同时构建本机与交叉两个产物"
    echo "  -h, --help       显示帮助信息"
    echo ""
    echo "输出目录:"
    echo "  本机构建:      ${HOST_BUILD_DIR}/"
    echo "  交叉构建(aarch64): ${CROSS_BUILD_DIR}/"
    echo ""
    echo "示例:"
    echo "  $0                  # 本机 Debug 构建（输出到 ${HOST_BUILD_DIR}/）"
    echo "  $0 -r               # 本机 Release 构建"
    echo "  $0 --cross          # 交叉编译 Debug（输出到 ${CROSS_BUILD_DIR}/）"
    echo "  $0 -r --cross       # 交叉编译 Release"
    echo "  $0 --both           # 同时构建本机与交叉"
    echo "  $0 -c --both -r     # 清理后同时构建 Release（本机+交叉）"
    echo ""
    echo "构建完成后，静态库与测试程序分别位于对应子目录，如："
    echo "  ${HOST_BUILD_DIR}/libMB_DDF_CORE.a, ${HOST_BUILD_DIR}/TestMonitor"
    echo "  ${CROSS_BUILD_DIR}/libMB_DDF_CORE.a, ${CROSS_BUILD_DIR}/TestMonitor"
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
        --cross)
            CROSS_BUILD=true
            shift
            ;;
        --both)
            BOTH_BUILD=true
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
    echo -e "${YELLOW}清理构建目录 ${BUILD_ROOT}/...${NC}"
    rm -rf "$BUILD_ROOT"
fi

# 工具函数：字节转换为人类可读（KB/MB）
human_size() {
    local size_bytes="$1"
    awk -v s="$size_bytes" 'BEGIN { if (s >= 1048576) printf "%.2f MB", s/1048576; else printf "%.2f KB", s/1024; }'
}

# 展示一个目录下的静态库与测试程序
show_artifacts() {
    local dir="$1"
    local label="$2"
    echo -e "${GREEN}[${label}] 生成的静态库：${NC}"
    local lib_rows=()
    local lib_maxlen=0
    for lib in "$dir"/*.a; do
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

    echo -e "${GREEN}[${label}] 可用的测试程序：${NC}"
    local test_rows=()
    local test_maxlen=0
    for exe in "$dir"/Test*; do
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
}

# 在指定目录运行所有测试程序（后台运行）
run_tests_in_dir() {
    local dir="$1"
    echo -e "${YELLOW}开始运行所有测试程序（${dir}）...${NC}"
    echo ""
    local test_programs=("${dir}"/Test*)
    echo -e "${YELLOW}发现 ${#test_programs[@]} 个测试程序${NC}"
    for exe in "${test_programs[@]}"; do
        if [ -x "$exe" ]; then
            echo -e "${GREEN}启动 $exe...${NC}"
            echo "----------------------------------------"
            "$exe" &
            test_pid=$!
            if kill -0 $test_pid 2>/dev/null; then
                echo -e "${GREEN}程序已成功启动，PID: $test_pid${NC}"
            else
                echo -e "${RED}程序启动失败或立即退出${NC}"
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
}

# 单次构建（在子shell中执行，以隔离交叉环境）
build_one() {
    local dir="$1"         # 目标构建目录
    local label="$2"       # 标签: host/aarch64
    local is_cross="$3"    # true/false

    echo -e "${YELLOW}配置 CMake (${label}, ${BUILD_TYPE})...${NC}"
    mkdir -p "$dir"
    (
        set -e
        cd "$dir"
        local CMAKE_ARGS="-DCMAKE_BUILD_TYPE=$BUILD_TYPE"
        if [ "$is_cross" = true ]; then
            echo -e "${YELLOW}设置交叉编译环境（ARM aarch64）...${NC}"
            CROSS_SDK="${CROSS_SDK:-$CROSS_SDK_DEFAULT}"
            CROSS_ENV_SCRIPT="${CROSS_ENV_SCRIPT:-$CROSS_ENV_SCRIPT_DEFAULT}"
            CROSS_SYSROOT="${CROSS_SYSROOT:-$CROSS_SYSROOT_DEFAULT}"
            CROSS_C_COMPILER="${CROSS_C_COMPILER:-$CROSS_C_COMPILER_DEFAULT}"
            CROSS_CXX_COMPILER="${CROSS_CXX_COMPILER:-$CROSS_CXX_COMPILER_DEFAULT}"
            ARM64_LIBS_PREFIX="${ARM64_LIBS_PREFIX:-$ARM64_LIBS_PREFIX_DEFAULT}"
            if [ ! -f "$CROSS_ENV_SCRIPT" ]; then
                echo -e "${RED}未找到交叉编译环境脚本: $CROSS_ENV_SCRIPT${NC}"
                exit 1
            fi
            # 注入 Yocto/UCAS 交叉环境
            source "$CROSS_ENV_SCRIPT"
            CMAKE_ARGS="$CMAKE_ARGS -DCROSS_COMPILE=ON"
            CMAKE_ARGS="$CMAKE_ARGS -DARM64_LIBS_PREFIX=$ARM64_LIBS_PREFIX"
            if [ -d "$ARM64_LIBS_PREFIX" ]; then
                echo -e "${YELLOW}使用 ARM64 第三方库前缀: $ARM64_LIBS_PREFIX${NC}"
                if [ -d "$ARM64_LIBS_PREFIX/lib" ]; then
                    echo -e "${YELLOW}提示: 若链接失败，可将 ${ARM64_LIBS_PREFIX}/lib 添加到目标板运行时库路径${NC}"
                fi
            else
                echo -e "${RED}警告: ARM64_LIBS_PREFIX 路径不存在: $ARM64_LIBS_PREFIX${NC}"
            fi
            OE_TC_FILE="${OECORE_CMAKE_TOOLCHAIN_FILE:-$CMAKE_TOOLCHAIN_FILE}"
            if [ -n "$OE_TC_FILE" ] && [ -f "$OE_TC_FILE" ]; then
                echo -e "${YELLOW}检测到 Yocto 工具链文件: $OE_TC_FILE${NC}"
                CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=$OE_TC_FILE"
                if [ -n "$SDKTARGETSYSROOT" ]; then
                    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_SYSROOT=$SDKTARGETSYSROOT -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY -DCMAKE_EXE_LINKER_FLAGS=--sysroot=$SDKTARGETSYSROOT -DCMAKE_SHARED_LINKER_FLAGS=--sysroot=$SDKTARGETSYSROOT"
                fi
            else
                CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 -DCMAKE_SYSROOT=$CROSS_SYSROOT -DCMAKE_C_COMPILER=$CROSS_C_COMPILER -DCMAKE_CXX_COMPILER=$CROSS_CXX_COMPILER -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY -DCMAKE_EXE_LINKER_FLAGS=--sysroot=$CROSS_SYSROOT -DCMAKE_SHARED_LINKER_FLAGS=--sysroot=$CROSS_SYSROOT"
            fi
        fi
        if ! cmake -S ../.. -B . $CMAKE_ARGS; then
            echo -e "${RED}CMake 配置失败（${label}）${NC}"
            exit 1
        fi
        echo -e "${YELLOW}编译项目（${label}）...${NC}"
        if ! cmake --build . --parallel $(nproc); then
            echo -e "${RED}编译失败（${label}）${NC}"
            exit 1
        fi
        echo -e "${GREEN}构建成功（${label} / ${BUILD_TYPE}）${NC}"
    )
}

# 构建流程
mkdir -p "$BUILD_ROOT"

if [ "$BOTH_BUILD" = true ]; then
    # 先构建本机
    build_one "$HOST_BUILD_DIR" "host" false
    # 同步 compile_commands.json 到根目录（供 clangd 使用）
    ln -sf "${HOST_BUILD_DIR}/compile_commands.json" "${BUILD_ROOT}/compile_commands.json"
    ln -sf "${HOST_BUILD_DIR}/compile_commands.json" "compile_commands.json"
    show_artifacts "$HOST_BUILD_DIR" "host"
    if [ "$RUN_ALL_TESTS" = true ]; then
        run_tests_in_dir "$HOST_BUILD_DIR"
    else
        echo -e "${YELLOW}提示：选择一个测试程序运行，例如：${NC}"
        echo "  ./${HOST_BUILD_DIR}/TestMonitor"
        echo -e "${YELLOW}或者使用 -t 选项自动运行所有测试程序：${NC}"
        echo "  $0 -t"
    fi

    # 再构建交叉
    build_one "$CROSS_BUILD_DIR" "aarch64" true
    # 提供切换交叉数据库的软链（需要时可改）
    ln -sf "${CROSS_BUILD_DIR}/compile_commands.json" "${BUILD_ROOT}/compile_commands.aarch64.json"
    show_artifacts "$CROSS_BUILD_DIR" "aarch64"
    if [ "$RUN_ALL_TESTS" = true ]; then
        echo -e "${YELLOW}交叉编译产物为 ARM，可执行文件无法在本机运行，已跳过 -t 测试启动${NC}"
    fi
else
    # 单一构建：根据 --cross 决定
    if [ "$CROSS_BUILD" = true ]; then
        build_one "$CROSS_BUILD_DIR" "aarch64" true
        ln -sf "${CROSS_BUILD_DIR}/compile_commands.json" "${BUILD_ROOT}/compile_commands.json"
        ln -sf "${CROSS_BUILD_DIR}/compile_commands.json" "compile_commands.json"
        show_artifacts "$CROSS_BUILD_DIR" "aarch64"
        if [ "$RUN_ALL_TESTS" = true ]; then
            echo -e "${YELLOW}交叉编译产物为 ARM，可执行文件无法在本机运行，已跳过 -t 测试启动${NC}"
        else
            echo -e "${YELLOW}提示：选择一个测试程序运行（在目标板上），例如：${NC}"
            echo "  ./TestMonitor"
        fi
    else
        build_one "$HOST_BUILD_DIR" "host" false
        ln -sf "${HOST_BUILD_DIR}/compile_commands.json" "${BUILD_ROOT}/compile_commands.json"
        ln -sf "${HOST_BUILD_DIR}/compile_commands.json" "compile_commands.json"
        show_artifacts "$HOST_BUILD_DIR" "host"
        if [ "$RUN_ALL_TESTS" = true ]; then
            run_tests_in_dir "$HOST_BUILD_DIR"
        else
            echo -e "${YELLOW}提示：选择一个测试程序运行，例如：${NC}"
            echo "  ./${HOST_BUILD_DIR}/TestMonitor"
            echo -e "${YELLOW}或者使用 -t 选项自动运行所有测试程序：${NC}"
            echo "  $0 -t"
        fi
    fi
fi