#pragma once
#include "Loader.h"

class VTKLoader : public Loader
{
public:
    VTKLoader(const std::string& filename) : Loader(filename) {};
    MeshPart load() override;
};