#pragma once
#include "Loader.h"
#include <string>
#include <vtkDataArray.h>
#include <vtkSmartPointer.h>
#include <vtkDataSet.h>
#include <vtkCellData.h>
#include <vtkPointData.h>

namespace VTK
{
    class FieldData
    {
    public:
        std::string name_;
        Location location_;
        Type type_;
        int num_components_;
        vtkIdType num_tuples_;
        vtkSmartPointer<vtkDataArray> pdata_; // 实际数据指针
    };

    class VTKLoader : public Loader
    {
    public:
        VTKLoader(const std::string& filename) : Loader(filename) {};
        ~VTKLoader() override = default;
        MeshPart load() override;

    private:
        vtkSmartPointer<vtkDataSet> dataset_;

        vtkPoints* points_ = nullptr;
        vtkPointData* point_data_ = nullptr;

        vtkIdType num_cells_ = 0;
        vtkCellData* cell_data_ = nullptr;


        std::vector<FieldData> point_fields_; // 点上物理量集合
        std::vector<FieldData> cell_fields_;  // 单元上物理量集合
    };

} // namespace VTK