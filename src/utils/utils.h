#pragma once
#include "data/UniformBuffer.h"
//given a UniformDataLayoutPtr, returns true if the pointer encompasses a TextureImageSampler struct.
bool isCIS(UniformDataLayoutPtr ptr);

