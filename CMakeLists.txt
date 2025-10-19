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

# 添加构建选项
option(BUILD_TESTS "Build test programs" ON)
option(BUILD_LIBS "Build static libraries" ON)

# 递归查找核心库源文件（不含 PhysicalLayer）
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
    "src/MB_DDF/DDS/*.cpp"
    "src/MB_DDF/Debug/*.cpp"
    "src/MB_DDF/Monitor/*.cpp"
)

# 递归查找 Timer 库源文件
file(GLOB_RECURSE TIMER_SOURCES CONFIGURE_DEPENDS
    "src/MB_DDF/Timer/*.cpp"
)

# 递归查找物理层库源文件
file(GLOB_RECURSE PHYSICAL_SOURCES CONFIGURE_DEPENDS
    "src/MB_DDF/PhysicalLayer/*.cpp"
)

# 递归查找定时器库源文件
file(GLOB_RECURSE TIMER_SOURCES CONFIGURE_DEPENDS
    "src/MB_DDF/Timer/*.cpp"
)

# 递归查找所有头文件（用于 IDE 展示与包含路径）
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    "src/MB_DDF/*.h"
    "src/MB_DDF/*.hpp"
)

# 创建三个静态库：核心库、物理层库、定时器库（条件编译）
if(BUILD_LIBS)
    add_library(MB_DDF_CORE STATIC ${CORE_SOURCES} ${HEADERS})
    add_library(MB_DDF_PHYSICAL STATIC ${PHYSICAL_SOURCES} ${HEADERS})
    add_library(MB_DDF_TIMER STATIC ${TIMER_SOURCES} ${HEADERS})

    # 为库添加包含目录
    foreach(TGT IN ITEMS MB_DDF_CORE MB_DDF_PHYSICAL MB_DDF_TIMER)
        target_include_directories(${TGT} PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
        target_link_libraries(${TGT} PUBLIC pthread rt)
    endforeach()
endif()

# 查找所有Test程序（条件编译）
if(BUILD_TESTS)
    file(GLOB TEST_SOURCES CONFIGURE_DEPENDS
        "src/MB_DDF/Test/Test*.cpp"
    )

    # 为每个Test程序创建可执行文件并按需链接库
    foreach(TEST_SOURCE ${TEST_SOURCES})
        # 获取文件名（不含路径和扩展名）
        get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)

        # 创建可执行文件（只包含Test源文件）
        add_executable(${TEST_NAME} ${TEST_SOURCE})

        # 设置包含目录
        target_include_directories(${TEST_NAME} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/src
        )

        if(BUILD_LIBS)
            # 如果构建了库，则链接库
            if(TEST_NAME STREQUAL "TestMonitor")
                target_link_libraries(${TEST_NAME} PRIVATE MB_DDF_CORE MB_DDF_TIMER MB_DDF_PHYSICAL)
            else()
                target_link_libraries(${TEST_NAME} PRIVATE MB_DDF_CORE MB_DDF_TIMER)
            endif()
        else()
            # 如果没有构建库，则直接编译源文件
            target_sources(${TEST_NAME} PRIVATE ${CORE_SOURCES} ${TIMER_SOURCES})
            if(TEST_NAME STREQUAL "TestMonitor")
                target_sources(${TEST_NAME} PRIVATE ${PHYSICAL_SOURCES})
            endif()
            target_link_libraries(${TEST_NAME} PRIVATE pthread rt)
        endif()
    endforeach()
endif()

# 设置不同构建类型的编译选项
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Debug 构建选项 - 应用到库和测试程序
    set(DEBUG_COMPILE_OPTIONS
        -g           # 生成调试信息
        -O0          # 禁用优化
        -Wall        # 启用所有警告
        -Wextra      # 启用额外警告
        -pedantic    # 严格标准检查
    )
    
    if(BUILD_LIBS)
        foreach(TGT IN ITEMS MB_DDF_CORE MB_DDF_PHYSICAL MB_DDF_TIMER)
            target_compile_options(${TGT} PRIVATE ${DEBUG_COMPILE_OPTIONS})
        endforeach()
    endif()
    
    if(BUILD_TESTS)
        foreach(TEST_SOURCE ${TEST_SOURCES})
            get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
            target_compile_options(${TEST_NAME} PRIVATE ${DEBUG_COMPILE_OPTIONS})
        endforeach()
    endif()

    # 设置调试器友好选项
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -g")


    # 添加调试目标（以第一个Test程序为例）
    if(BUILD_TESTS AND TEST_SOURCES)
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
    # Release 构建选项 - 应用到库和测试程序
    set(RELEASE_COMPILE_OPTIONS
        -O3          # 最大优化
        -DNDEBUG     # 禁用断言
        -Werror      # 将警告视为错误
    )
    
    if(BUILD_LIBS)
        foreach(TGT IN ITEMS MB_DDF_CORE MB_DDF_PHYSICAL MB_DDF_TIMER)
            target_compile_options(${TGT} PRIVATE ${RELEASE_COMPILE_OPTIONS})
        endforeach()
    endif()
    
    if(BUILD_TESTS)
        foreach(TEST_SOURCE ${TEST_SOURCES})
            get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
            target_compile_options(${TEST_NAME} PRIVATE ${RELEASE_COMPILE_OPTIONS})
        endforeach()
    endif()

    # 链接时优化（根据编译器选择更合适的 LTO 模式）
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        set(LTO_OPTIONS -flto=auto)
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto=auto -s")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(LTO_OPTIONS -flto=thin)
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto=thin -s")
    endif()
    
    if(BUILD_LIBS AND LTO_OPTIONS)
        foreach(TGT IN ITEMS MB_DDF_CORE MB_DDF_PHYSICAL MB_DDF_TIMER)
            target_compile_options(${TGT} PRIVATE ${LTO_OPTIONS})
        endforeach()
    endif()
    
    if(BUILD_TESTS AND LTO_OPTIONS)
        foreach(TEST_SOURCE ${TEST_SOURCES})
            get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
            target_compile_options(${TEST_NAME} PRIVATE ${LTO_OPTIONS})
        endforeach()
    endif()
endif()

# 安装规则 - 安装所有Test程序（条件编译）
if(BUILD_TESTS)
    foreach(TEST_SOURCE ${TEST_SOURCES})
        get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
        install(TARGETS ${TEST_NAME}
            DESTINATION bin
        )
    endforeach()
endif()

# 添加一个显示构建信息的自定义目标
add_custom_target(info
    COMMAND ${CMAKE_COMMAND} -E echo "Build Type: ${CMAKE_BUILD_TYPE}"
    COMMAND ${CMAKE_COMMAND} -E echo "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
    COMMAND ${CMAKE_COMMAND} -E echo "Core Sources: ${CORE_SOURCES}"
    COMMAND ${CMAKE_COMMAND} -E echo "Timer Sources: ${TIMER_SOURCES}"
    COMMAND ${CMAKE_COMMAND} -E echo "Physical Sources: ${PHYSICAL_SOURCES}"
    COMMAND ${CMAKE_COMMAND} -E echo "Timer Sources: ${TIMER_SOURCES}"
    COMMAND ${CMAKE_COMMAND} -E echo "Test Programs: ${TEST_SOURCES}"
    COMMAND ${CMAKE_COMMAND} -E echo "Static Libraries: MB_DDF_CORE, MB_DDF_PHYSICAL, MB_DDF_TIMER"
    COMMENT "Displaying build information"
)