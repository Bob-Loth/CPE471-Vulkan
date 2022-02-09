#include "utils/utils.h"
#include <string>
bool isCIS(UniformDataLayoutPtr ptr) {
    std::string dataType(typeid(*ptr.get()).name());
    if (dataType == "class UniformStructDataLayout<struct TextureSamplerData,16>") {
        return true;
    }
    return false;
}