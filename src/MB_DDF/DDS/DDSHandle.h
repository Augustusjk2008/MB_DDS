#pragma once
#include <cstdint>

namespace MB_DDF {
namespace DDS {

class Handle {
public:
    virtual ~Handle() = default;
    virtual bool send(const uint8_t* data, uint32_t len) = 0;
    virtual int32_t receive(uint8_t* buf, uint32_t buf_size) = 0;
    virtual int32_t receive(uint8_t* buf, uint32_t buf_size, uint32_t timeout_us) = 0;
    virtual uint32_t getMTU() const = 0;
};

} // namespace DDS
} // namespace MB_DDF
