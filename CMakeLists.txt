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
option(CROSS_COMPILE "Enable cross-compilation for ARM aarch64" OFF)

# 交叉编译配置（与 reference/make/Makefile 保持一致的默认值，可被外部覆盖）
if(CROSS_COMPILE)
    set(CROSS_SDK_DEFAULT "/opt/wanghuo/v2.0.0-rc4")
    set(CROSS_SYSROOT_DEFAULT "${CROSS_SDK_DEFAULT}/sysroots/armv8a-ucas-linux")
    set(CROSS_C_COMPILER_DEFAULT "aarch64-ucas-linux-gcc")
    set(CROSS_CXX_COMPILER_DEFAULT "aarch64-ucas-linux-g++")

    # 优先使用由环境脚本提供的工具链文件，避免与手动设置冲突
    if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{OECORE_CMAKE_TOOLCHAIN_FILE})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{OECORE_CMAKE_TOOLCHAIN_FILE}" CACHE FILEPATH "Yocto/OE toolchain file" FORCE)
    endif()

    # 若不存在工具链文件，再允许手动覆盖
    if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        set(CROSS_SDK "${CROSS_SDK}" CACHE PATH "Cross SDK root (optional)")
        set(CROSS_SYSROOT "${CROSS_SYSROOT}" CACHE PATH "Cross sysroot path (optional)")
        set(CROSS_C_COMPILER "${CROSS_C_COMPILER}" CACHE STRING "Cross C compiler executable")
        set(CROSS_CXX_COMPILER "${CROSS_CXX_COMPILER}" CACHE STRING "Cross C++ compiler executable")

        if(NOT CROSS_SYSROOT)
            set(CROSS_SYSROOT "${CROSS_SYSROOT_DEFAULT}")
        endif()
        if(NOT CROSS_C_COMPILER)
            set(CROSS_C_COMPILER "${CROSS_C_COMPILER_DEFAULT}")
        endif()
        if(NOT CROSS_CXX_COMPILER)
            set(CROSS_CXX_COMPILER "${CROSS_CXX_COMPILER_DEFAULT}")
        endif()

        # 设置目标系统
        set(CMAKE_SYSTEM_NAME Linux)
        set(CMAKE_SYSTEM_PROCESSOR aarch64)

        # 设置工具链
        set(CMAKE_SYSROOT "${CROSS_SYSROOT}")
        set(CMAKE_C_COMPILER "${CROSS_C_COMPILER}")
        set(CMAKE_CXX_COMPILER "${CROSS_CXX_COMPILER}")

        # 控制 find_root_path 行为，以 sysroot 为根查找库与头
        set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
        set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
        set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
        set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
    endif()

    add_compile_options($<$<CONFIG:Release>:-fPIC> $<$<CONFIG:Debug>:-fPIC>)

    message(STATUS "[Cross] CROSS_COMPILE=ON")
    message(STATUS "[Cross] CMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
    message(STATUS "[Cross] CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}")
    message(STATUS "[Cross] CMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}")
    message(STATUS "[Cross] CMAKE_SYSROOT=${CMAKE_SYSROOT}")
    message(STATUS "[Cross] CMAKE_C_COMPILER=${CMAKE_C_COMPILER}")
    message(STATUS "[Cross] CMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}")

    # 额外的 ARM64 第三方库前缀（用于非 sysroot 的外部安装，如 /opt/arm64-libs）
    set(ARM64_LIBS_PREFIX "$ENV{ARM64_LIBS_PREFIX}" CACHE PATH "Prefix for aarch64 third-party libs (libaio/liburing)" FORCE)
    if(NOT ARM64_LIBS_PREFIX OR ARM64_LIBS_PREFIX STREQUAL "")
        set(ARM64_LIBS_PREFIX "/opt/arm64-libs" CACHE PATH "Prefix for aarch64 third-party libs (libaio/liburing)" FORCE)
    endif()
    message(STATUS "[Cross] ARM64_LIBS_PREFIX=${ARM64_LIBS_PREFIX}")
endif()

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
    # 检测并链接 libgpiod（GPIO 中断支持，供 SpiTransport 使用）
    find_library(GPIOD_LIBRARY NAMES gpiod)
    find_path(GPIOD_INCLUDE_DIR NAMES gpiod.h)
    if((NOT GPIOD_LIBRARY OR NOT GPIOD_INCLUDE_DIR) AND DEFINED ARM64_LIBS_PREFIX)
        find_library(GPIOD_LIBRARY NAMES gpiod PATHS "${ARM64_LIBS_PREFIX}/lib" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
        find_path(GPIOD_INCLUDE_DIR NAMES gpiod.h PATHS "${ARM64_LIBS_PREFIX}/include" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
    endif()
    if(GPIOD_LIBRARY AND GPIOD_INCLUDE_DIR)
        target_include_directories(MB_DDF_PHYSICAL PUBLIC ${GPIOD_INCLUDE_DIR})
        target_link_libraries(MB_DDF_PHYSICAL PUBLIC ${GPIOD_LIBRARY})
        message(STATUS "libgpiod found: ${GPIOD_LIBRARY}; enabling GPIO event support")
    else()
        message(STATUS "libgpiod not found; GPIO event fd disabled for SpiTransport")
    endif()
    # 查找 libaio：优先 sysroot，失败后尝试 ARM64_LIBS_PREFIX
    find_library(LIBAIO_LIBRARY NAMES aio)
    find_path(LIBAIO_INCLUDE_DIR NAMES libaio.h)
    if((NOT LIBAIO_LIBRARY OR NOT LIBAIO_INCLUDE_DIR) AND DEFINED ARM64_LIBS_PREFIX)
        find_library(LIBAIO_LIBRARY NAMES aio PATHS "${ARM64_LIBS_PREFIX}/lib" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
        find_path(LIBAIO_INCLUDE_DIR NAMES libaio.h PATHS "${ARM64_LIBS_PREFIX}/include" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
    endif()
    if(LIBAIO_LIBRARY AND LIBAIO_INCLUDE_DIR)
        target_compile_definitions(MB_DDF_PHYSICAL PUBLIC MB_DDF_HAS_LIBAIO=1)
        target_include_directories(MB_DDF_PHYSICAL PUBLIC ${LIBAIO_INCLUDE_DIR})
        target_link_libraries(MB_DDF_PHYSICAL PUBLIC ${LIBAIO_LIBRARY})
        message(STATUS "libaio found: ${LIBAIO_LIBRARY}; enabling MB_DDF_HAS_LIBAIO")
    else()
        message(STATUS "libaio not found; building without async DMA support")
    endif()

    # 新增：检测并链接 liburing（优先启用 io_uring）
    find_library(LIBURING_LIBRARY NAMES uring)
    find_path(LIBURING_INCLUDE_DIR NAMES liburing.h)
    if((NOT LIBURING_LIBRARY OR NOT LIBURING_INCLUDE_DIR) AND DEFINED ARM64_LIBS_PREFIX)
        find_library(LIBURING_LIBRARY NAMES uring PATHS "${ARM64_LIBS_PREFIX}/lib" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
        find_path(LIBURING_INCLUDE_DIR NAMES liburing.h PATHS "${ARM64_LIBS_PREFIX}/include" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
    endif()
    if(LIBURING_LIBRARY AND LIBURING_INCLUDE_DIR)
        target_compile_definitions(MB_DDF_PHYSICAL PUBLIC MB_DDF_HAS_IOURING=1)
        target_include_directories(MB_DDF_PHYSICAL PUBLIC ${LIBURING_INCLUDE_DIR})
        target_link_libraries(MB_DDF_PHYSICAL PUBLIC ${LIBURING_LIBRARY})
        message(STATUS "liburing found: ${LIBURING_LIBRARY}; enabling MB_DDF_HAS_IOURING")
    else()
        message(STATUS "liburing not found; io_uring async support disabled")
    endif()

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

    foreach(TEST_SOURCE ${TEST_SOURCES})
        get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
        add_executable(${TEST_NAME} ${TEST_SOURCE})
        target_include_directories(${TEST_NAME} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/src
        )

        if(BUILD_LIBS)
            if(TEST_NAME STREQUAL "TestMonitor")
                target_link_libraries(${TEST_NAME} PRIVATE MB_DDF_CORE MB_DDF_PHYSICAL MB_DDF_TIMER)
            elseif(TEST_NAME STREQUAL "TestPublishPerf")
                target_link_libraries(${TEST_NAME} PRIVATE MB_DDF_CORE MB_DDF_PHYSICAL MB_DDF_TIMER)
            elseif(TEST_NAME STREQUAL "TestPhysicalLayer")
                target_link_libraries(${TEST_NAME} PRIVATE MB_DDF_PHYSICAL MB_DDF_CORE MB_DDF_TIMER)
            elseif(TEST_NAME STREQUAL "TestRealTime")
                target_link_libraries(${TEST_NAME} PRIVATE MB_DDF_CORE MB_DDF_PHYSICAL MB_DDF_TIMER)
            else()
                target_link_libraries(${TEST_NAME} PRIVATE MB_DDF_CORE MB_DDF_TIMER)
            endif()
        else()
            if(TEST_NAME STREQUAL "TestMonitor")
                target_sources(${TEST_NAME} PRIVATE ${PHYSICAL_SOURCES})
        endif()
        target_link_libraries(${TEST_NAME} PRIVATE pthread rt)
        # 如果直接编译 PhysicalLayer 源并且系统存在 libaio/liburing，则为测试链接
        find_library(LIBAIO_LIBRARY NAMES aio)
        find_path(LIBAIO_INCLUDE_DIR NAMES libaio.h)
            if((NOT LIBAIO_LIBRARY OR NOT LIBAIO_INCLUDE_DIR) AND DEFINED ARM64_LIBS_PREFIX)
                find_library(LIBAIO_LIBRARY NAMES aio PATHS "${ARM64_LIBS_PREFIX}/lib" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
                find_path(LIBAIO_INCLUDE_DIR NAMES libaio.h PATHS "${ARM64_LIBS_PREFIX}/include" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
            endif()
            if(LIBAIO_LIBRARY AND LIBAIO_INCLUDE_DIR)
                target_compile_definitions(${TEST_NAME} PRIVATE MB_DDF_HAS_LIBAIO=1)
                target_include_directories(${TEST_NAME} PRIVATE ${LIBAIO_INCLUDE_DIR})
                target_link_libraries(${TEST_NAME} PRIVATE ${LIBAIO_LIBRARY})
            endif()
            find_library(LIBURING_LIBRARY NAMES uring)
            find_path(LIBURING_INCLUDE_DIR NAMES liburing.h)
            if((NOT LIBURING_LIBRARY OR NOT LIBURING_INCLUDE_DIR) AND DEFINED ARM64_LIBS_PREFIX)
                find_library(LIBURING_LIBRARY NAMES uring PATHS "${ARM64_LIBS_PREFIX}/lib" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
                find_path(LIBURING_INCLUDE_DIR NAMES liburing.h PATHS "${ARM64_LIBS_PREFIX}/include" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
            endif()
            if(LIBURING_LIBRARY AND LIBURING_INCLUDE_DIR)
                target_compile_definitions(${TEST_NAME} PRIVATE MB_DDF_HAS_IOURING=1)
                target_include_directories(${TEST_NAME} PRIVATE ${LIBURING_INCLUDE_DIR})
                target_link_libraries(${TEST_NAME} PRIVATE ${LIBURING_LIBRARY})
            endif()
            # 测试程序链接 libgpiod 以支持 GPIO 事件
            find_library(GPIOD_LIBRARY NAMES gpiod)
            find_path(GPIOD_INCLUDE_DIR NAMES gpiod.h)
            if((NOT GPIOD_LIBRARY OR NOT GPIOD_INCLUDE_DIR) AND DEFINED ARM64_LIBS_PREFIX)
                find_library(GPIOD_LIBRARY NAMES gpiod PATHS "${ARM64_LIBS_PREFIX}/lib" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
                find_path(GPIOD_INCLUDE_DIR NAMES gpiod.h PATHS "${ARM64_LIBS_PREFIX}/include" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
            endif()
            if(GPIOD_LIBRARY AND GPIOD_INCLUDE_DIR)
                target_include_directories(${TEST_NAME} PRIVATE ${GPIOD_INCLUDE_DIR})
                target_link_libraries(${TEST_NAME} PRIVATE ${GPIOD_LIBRARY})
            endif()
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