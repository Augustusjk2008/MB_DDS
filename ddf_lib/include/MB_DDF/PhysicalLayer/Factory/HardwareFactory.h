#pragma once
#include <memory>
#include <string>
#include "MB_DDF/DDS/DDSHandle.h"

namespace MB_DDF {
namespace PhysicalLayer {
namespace Factory {

class HardwareFactory {
public:
    static std::shared_ptr<DDS::Handle> create(const std::string& name, void* param = nullptr);
};

} // namespace PhysicalLayer
} // namespace MB_DDF
} // namespace Factory
