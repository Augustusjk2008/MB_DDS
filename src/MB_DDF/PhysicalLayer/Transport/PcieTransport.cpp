#include "MB_DDF/PhysicalLayer/Transport/PcieTransport.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <string>

namespace MB_DDF {
namespace PhysicalLayer {
namespace Transport {

static std::string format_bdf_path(uint16_t domain, uint8_t bus, uint8_t device, uint8_t function) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "/sys/bus/pci/devices/%04x:%02x:%02x.%1u/", domain, bus, device, function);
    return std::string(buf);
}

std::string PcieAddress::to_resource_path() const {
    if (!resource_path.empty()) {
        return resource_path;
    }
    std::string base = format_bdf_path(domain, bus, device, function);
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%sresource%d", base.c_str(), bar_index);
    return std::string(buf);
}

bool PcieTransport::open(const TransportConfig& cfg) {
    close();
    cfg_ = cfg;

    if (cfg.backend != Backend::PCIE) {
        return false;
    }

    // 映射 BAR
    if (!map_bar_via_sysfs(cfg.pcie)) {
        close();
        return false;
    }
    return true;
}

void PcieTransport::close() {
    if (mapped_base_) {
        munmap(mapped_base_, mapped_len_);
        mapped_base_ = nullptr;
        mapped_len_ = 0;
    }
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool PcieTransport::readReg32(uint64_t offset, uint32_t& val) const {
    if (!mapped_base_ || offset + 4 > mapped_len_) {
        return false;
    }
    volatile uint32_t* reg = reinterpret_cast<volatile uint32_t*>(static_cast<uint8_t*>(mapped_base_) + offset);
    val = *reg;
    return true;
}

bool PcieTransport::writeReg32(uint64_t offset, uint32_t val) {
    if (!mapped_base_ || offset + 4 > mapped_len_) {
        return false;
    }
    volatile uint32_t* reg = reinterpret_cast<volatile uint32_t*>(static_cast<uint8_t*>(mapped_base_) + offset);
    *reg = val;
    // 可选：写后读确保刷新
    // uint32_t dummy = *reg; (void)dummy;
    return true;
}

int PcieTransport::waitEvent(uint32_t /*timeout_ms*/) {
    // 基于纯 sysfs resourceN 映射无法等待中断事件，返回 0 表示超时/未实现
    return 0;
}

bool PcieTransport::map_bar_via_sysfs(const PcieAddress& addr) {
    // 解析 resourceN 文件路径
    std::string res_path = addr.to_resource_path();

    // 打开 resourceN
    fd_ = ::open(res_path.c_str(), O_RDWR | O_SYNC);
    if (fd_ < 0) {
        return false;
    }

    // 计算映射长度
    size_t bar_size = addr.bar_size;
    if (bar_size == 0) {
        if (!get_bar_size_from_resource_table(addr, bar_size)) {
            // 退回到使用 fstat 获取文件大小（某些系统 resourceN 不可直接 stat 出 size）
            struct stat st {};
            if (::fstat(fd_, &st) == 0 && st.st_size > 0) {
                bar_size = static_cast<size_t>(st.st_size);
            } else {
                // 默认映射 4KB，避免失败；建议调用方提供 bar_size
                bar_size = 0x1000;
            }
        }
    }

    // mmap 映射 BAR
    void* base = ::mmap(nullptr, bar_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (base == MAP_FAILED) {
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    mapped_base_ = base;
    mapped_len_  = bar_size;
    return true;
}

bool PcieTransport::get_bar_size_from_resource_table(const PcieAddress& addr, size_t& out_size) {
    out_size = 0;
    // 读取 /sys/bus/pci/devices/.../resource 文件，解析每行的 start-end-flags
    std::string base = format_bdf_path(addr.domain, addr.bus, addr.device, addr.function);
    std::string table = base + "resource"; // 不带序号

    FILE* fp = std::fopen(table.c_str(), "r");
    if (!fp) {
        return false;
    }

    char line[256];
    int index = 0;
    while (std::fgets(line, sizeof(line), fp)) {
        // 每行格式："%llx %llx %x" => start end flags
        unsigned long long start = 0, end = 0;
        unsigned int flags = 0;
        if (std::sscanf(line, "%llx %llx %x", &start, &end, &flags) == 3) {
            if (index == addr.bar_index) {
                if (end > start) {
                    out_size = static_cast<size_t>(end - start + 1);
                }
                break;
            }
        }
        ++index;
    }
    std::fclose(fp);
    return out_size != 0;
}

} // namespace Transport
} // namespace PhysicalLayer
} // namespace MB_DDF