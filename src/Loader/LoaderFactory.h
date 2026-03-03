#pragma once
#include "Loader.h"
#include "VTKLoader.h"
#include "PLYLoader.h"
#include <memory>
#include <string>

class LoaderFactory
{


public:
    static std::unique_ptr<Loader> createLoader(const std::string& filename)
    {
        // 简单根据文件扩展名选择加载器
        if (endsWith(filename, ".vtk") || endsWith(filename, ".vtu") || endsWith(filename, ".vtp"))
        {
            return std::make_unique<VTK::VTKLoader>(filename);
        }
        else if (endsWith(filename, ".ply"))
        {
            return std::make_unique<PLYLoader>(filename);
        }
        // 可以添加更多格式的支持
        return nullptr; // 不支持的格式
    }


private:
    static bool endsWith(const std::string& str, const std::string& suffix)
    {
        return str.size() >= suffix.size() &&
               str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
    }
};
