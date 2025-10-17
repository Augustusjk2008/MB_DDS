# CMake 的最低要求版本
cmake_minimum_required(VERSION 3.10)

# 项目名称
project(MB_DDF_Demo LANGUAGES CXX)

# 生成compile_commands.json文件供clangd使用
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 设置默认构建类型（如果没有指定）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose build type: Debug or Release" FORCE)
endif()

# 设置可用构建类型
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Configurations" FORCE)

# 设置C++标准为C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 递归查找MB_DDF库源文件（排除Test文件夹）
file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS
    "src/MB_DDF/DDS/*.cpp"
    "src/MB_DDF/Debug/*.cpp"
    "src/MB_DDF/Monitor/*.cpp"
    "src/MB_DDF/PhysicalLayer/*.cpp"
)

# 递归查找所有头文件
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    "src/MB_DDF/*.h"
    "src/MB_DDF/*.hpp"
)

# 创建MB_DDF静态库（避免重复编译）
add_library(MB_DDF_LIB STATIC ${LIB_SOURCES} ${HEADERS})

# 为库添加包含目录
target_include_directories(MB_DDF_LIB PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# 为库链接必要的系统库
target_link_libraries(MB_DDF_LIB PUBLIC 
    pthread 
    rt 
)

# 查找所有Test程序
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS
    "src/MB_DDF/Test/Test*.cpp"
)

# 为每个Test程序创建可执行文件
foreach(TEST_SOURCE ${TEST_SOURCES})
    # 获取文件名（不含路径和扩展名）
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    
    # 创建可执行文件（只包含Test源文件）
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    
    # 链接MB_DDF静态库
    target_link_libraries(${TEST_NAME} PRIVATE MB_DDF_LIB)
endforeach()

# 设置不同构建类型的编译选项
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Debug 构建选项 - 应用到静态库
    target_compile_options(MB_DDF_LIB PRIVATE
        -g           # 生成调试信息
        -O0          # 禁用优化
        -Wall        # 启用所有警告
        -Wextra      # 启用额外警告
        -pedantic    # 严格标准检查
    )
    
    # 设置调试器友好选项
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -g")
    
    # 添加调试目标（以第一个Test程序为例）
    if(TEST_SOURCES)
        list(GET TEST_SOURCES 0 FIRST_TEST)
        get_filename_component(FIRST_TEST_NAME ${FIRST_TEST} NAME_WE)
        add_custom_target(debug
            COMMAND ${CMAKE_BUILD_TOOL} --build ${CMAKE_BINARY_DIR} --config Debug
            COMMAND gdb -ex 'run' ${CMAKE_BINARY_DIR}/${FIRST_TEST_NAME}
            DEPENDS ${FIRST_TEST_NAME}
            COMMENT "Building Debug version and starting GDB with ${FIRST_TEST_NAME}"
        )
    endif()
else()
    # Release 构建选项 - 应用到静态库
    target_compile_options(MB_DDF_LIB PRIVATE
        -O3          # 最大优化
        -DNDEBUG     # 禁用断言
        -Werror      # 将警告视为错误
    )
    
    # 链接时优化
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(MB_DDF_LIB PRIVATE -flto)
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto")
    endif()
endif()

# 安装规则 - 安装所有Test程序
foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    install(TARGETS ${TEST_NAME}
        DESTINATION bin
    )
endforeach()

# 添加一个显示构建信息的自定义目标
add_custom_target(info
    COMMAND ${CMAKE_COMMAND} -E echo "Build Type: ${CMAKE_BUILD_TYPE}"
    COMMAND ${CMAKE_COMMAND} -E echo "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
    COMMAND ${CMAKE_COMMAND} -E echo "Library Sources: ${LIB_SOURCES}"
    COMMAND ${CMAKE_COMMAND} -E echo "Test Programs: ${TEST_SOURCES}"
    COMMAND ${CMAKE_COMMAND} -E echo "Static Library: MB_DDF_LIB (compiled once, linked to all tests)"
    COMMENT "Displaying build information"
)