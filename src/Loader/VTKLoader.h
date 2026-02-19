#pragma once
#include "Loader.h"
namespace VTK
{
    class VTKLoader : public Loader
    {
    public:
        VTKLoader(const std::string& filename) : Loader(filename) {};
        ~VTKLoader() override = default;
        MeshPart load() override;
    };
} // namespace VTK