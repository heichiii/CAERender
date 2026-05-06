#pragma once
#include "Loader.h"
#include <string>
#include <vtkSmartPointer.h>
#include <vtkDataArray.h>

class vtkUnstructuredGrid;
class vtkPointData;
class vtkCellData;

namespace VTK {
    class FieldData; // Re-use from VTKLoader if possible? No, redefining is fine or I can just use a local struct.
}

class CGNSLoader : public Loader
{
public:
    CGNSLoader(const std::string& filename) : Loader(filename) {};
    ~CGNSLoader() override = default;
    MeshPart load() override;

private:
    void processGrid(vtkUnstructuredGrid* dataset, MeshPart& mesh_part, size_t& current_vertex_offset);
};
