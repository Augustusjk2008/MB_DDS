/**
 * @file TestCML.cpp
 * @brief CML测试
 */
#include <cstdint>
#include <unistd.h>
#include <string>
#include <math.h>
#include <cstring>

#include "MB_DDF/Debug/Logger.h"
#include "MB_DDF/Debug/LoggerExtensions.h"
#include "MB_DDF/DDS/DDSCore.h"
#include "MB_DDF/PhysicalLayer/Factory/HardwareFactory.h"

// --- 主函数 ---
int main() {
    LOG_SET_LEVEL_INFO();
    LOG_DISABLE_TIMESTAMP();
    LOG_DISABLE_FUNCTION_LINE();

    LOG_DOUBLE_SEPARATOR();
    LOG_TITLE("Starting CML Test");
    LOG_DOUBLE_SEPARATOR();
    LOG_BLANK_LINE();    

    auto& dds = MB_DDF::DDS::DDSCore::instance();
    dds.initialize(128 * 1024 * 1024);
    auto cml = MB_DDF::PhysicalLayer::Factory::HardwareFactory::create("ddr");
    MB_DDF::DDS::PubAndSub cml_operator(cml, [](const void* data, size_t size, uint64_t timestamp) {
        LOG_INFO << "CML data received: " << size << " bytes at " << timestamp;
    });

    auto dyt = MB_DDF::PhysicalLayer::Factory::HardwareFactory::create("dyt");
    MB_DDF::DDS::PubAndSub dyt_rs422(dyt, [](const void* data, size_t size, uint64_t timestamp) {
        LOG_INFO << "DYT Rs422 data received: " << size << " bytes at " << timestamp;
    });

    // 导引头串口发数据
    const char* dyt_data = "DYT_RS422_TEST";
    dyt_rs422.write(dyt_data, strlen(dyt_data));

    const uint32_t sleep_us = 500000;
    while(1) {     
        usleep(sleep_us);
    }

    LOG_DOUBLE_SEPARATOR();
    LOG_TITLE("CML Test Finished");
    LOG_DOUBLE_SEPARATOR();

    return 0;
}