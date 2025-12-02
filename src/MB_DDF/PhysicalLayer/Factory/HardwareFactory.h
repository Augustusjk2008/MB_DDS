#pragma once
#include <memory>
#include <string>
#include <cstdint>

namespace MB_DDF {
namespace PhysicalLayer {
namespace Factory {

class HardwareFactory {
public:
    class Handle {
    public:
        virtual ~Handle() = default;
        virtual bool send(const uint8_t* data, uint32_t len) = 0;
        virtual int32_t receive(uint8_t* buf, uint32_t buf_size) = 0;
        virtual int32_t receive(uint8_t* buf, uint32_t buf_size, uint32_t timeout_us) = 0;
        virtual uint16_t getMTU() const = 0;
    };

    static std::unique_ptr<Handle> create(const std::string& name, void* param = nullptr);
};

} // namespace PhysicalLayer
} // namespace MB_DDF
} // namespace Factory
