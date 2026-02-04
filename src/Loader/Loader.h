#pragma once
#include <string>
#include "Data/MeshPart.h"
class Loader
{
public:
    explicit Loader(const std::string& filename) : filename_(filename) {}
    virtual MeshPart load() = 0;

protected:
    std::string filename_;
};