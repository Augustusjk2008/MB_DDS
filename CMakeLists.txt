# CMake 的最低要求版本
cmake_minimum_required(VERSION 3.10)

# 项目名称
project(MB_DDF_Demo LANGUAGES CXX)

# 设置默认构建类型（如果没有指定）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose build type: Debug or Release" FORCE)
endif()

# 设置可用构建类型
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Configurations" FORCE)

# 设置C++标准为C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 递归查找所有源文件
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "src/MB_DDF/*.cpp"
    "src/TestMain.cpp"
)

# 递归查找所有头文件
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    "src/MB_DDF/*.h"
    "src/MB_DDF/*.hpp"
)

# 添加可执行文件
add_executable(TestMain ${SOURCES} ${HEADERS})

# 为目标添加包含目录
target_include_directories(TestMain PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 设置不同构建类型的编译选项
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Debug 构建选项
    target_compile_options(TestMain PRIVATE
        -g           # 生成调试信息
        -O0          # 禁用优化
        -Wall        # 启用所有警告
        -Wextra      # 启用额外警告
        -pedantic    # 严格标准检查
    )
    
    # 设置调试器友好选项
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -g")
    
    # 添加调试目标
    add_custom_target(debug
        COMMAND ${CMAKE_BUILD_TOOL} --build ${CMAKE_BINARY_DIR} --config Debug
        COMMAND gdb -ex 'run' ${CMAKE_BINARY_DIR}/TestMain
        DEPENDS TestMain
        COMMENT "Building Debug version and starting GDB"
    )
else()
    # Release 构建选项
    target_compile_options(TestMain PRIVATE
        -O3          # 最大优化
        -DNDEBUG     # 禁用断言
        -Werror      # 将警告视为错误
    )
    
    # 链接时优化
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(TestMain PRIVATE -flto)
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto")
    endif()
endif()

# 安装规则
install(TARGETS TestMain
    DESTINATION bin
)

# 添加一个显示构建信息的自定义目标
add_custom_target(info
    COMMAND ${CMAKE_COMMAND} -E echo "Build Type: ${CMAKE_BUILD_TYPE}"
    COMMAND ${CMAKE_COMMAND} -E echo "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
    COMMAND ${CMAKE_COMMAND} -E echo "Source Files: ${SOURCES}"
    COMMENT "Displaying build information"
)