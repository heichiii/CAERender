#include "VTKLoader.h"
#include "Data/MeshPart.h"
#include "Test/Profiler.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vtkPolyData.h>
#include <vtkPolyDataReader.h>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>
#include <vtkUnstructuredGridReader.h>
#include <vtkXMLPolyDataReader.h>
#include <vtkXMLUnstructuredGridReader.h>

MeshPart VTKLoader::load()
{
    PROFILE_CODE

    MeshPart mesh_part;
    std::ifstream file_stream(filename_, std::ios::in);
    if (!file_stream.is_open())
    {
        throw std::runtime_error("Failed to open VTK file: " + filename_);
    }
    file_stream.close();

    // 判断文件类型
    bool is_XML = (filename_.find(".vtu") != std::string::npos ||
                   filename_.find(".vtp") != std::string::npos);

    if (is_XML)
    {
        // XML格式
        if (filename_.find(".vtu") != std::string::npos)
        {
            auto reader = vtkSmartPointer<vtkXMLUnstructuredGridReader>::New();
            reader->SetFileName(filename_.c_str());
            reader->Update();
            mesh_part.dataset_ = reader->GetOutput();
        }
        else
        {
            auto reader = vtkSmartPointer<vtkXMLPolyDataReader>::New();
            reader->SetFileName(filename_.c_str());
            reader->Update();
            mesh_part.dataset_ = reader->GetOutput();
        }
    }
    else
    {
        // Legacy格式 - 先读取文件头检测类型
        std::ifstream file(filename_, std::ios::binary);
        // 读取文件头查找DATASET类型
        std::string line;
        std::string dataset_type;
        bool found_dataset = false;

        while (std::getline(file, line) && !found_dataset)
        {
            if (line.find("DATASET") != std::string::npos)
            {
                // 解析DATASET类型
                size_t pos = line.find("DATASET");
                if (pos != std::string::npos)
                {
                    pos += 8; // "DATASET " 的长度
                    while (pos < line.length() && std::isspace(line[pos]))
                        pos++;

                    size_t end_pos = pos;
                    while (end_pos < line.length() && !std::isspace(line[end_pos]))
                        end_pos++;

                    dataset_type = line.substr(pos, end_pos - pos);
                    found_dataset = true;
                }
            }
        }
        file.close();

        // 根据类型选择合适的reader
        if (dataset_type == "POLYDATA")
        {
            auto poly_reader = vtkSmartPointer<vtkPolyDataReader>::New();
            poly_reader->SetFileName(filename_.c_str());
            poly_reader->Update();
            mesh_part.dataset_ = poly_reader->GetOutput();
        }
        else
        {
            // 默认尝试UnstructuredGrid
            auto reader = vtkSmartPointer<vtkUnstructuredGridReader>::New();
            reader->SetFileName(filename_.c_str());
            reader->Update();


            mesh_part.dataset_ = reader->GetOutput();
        }
    }
    mesh_part.name_ = filename_;
    mesh_part.points_ = mesh_part.dataset_->GetPoints();
    mesh_part.point_data_ = mesh_part.dataset_->GetPointData();
    mesh_part.num_cells_ = mesh_part.dataset_->GetNumberOfCells();
    mesh_part.cell_data_ = mesh_part.dataset_->GetCellData();
    int num_arrays;
    // 读取点数据数组
    num_arrays = mesh_part.point_data_->GetNumberOfArrays();
    mesh_part.point_fields_.reserve(num_arrays);
    for (int i = 0; i < num_arrays; ++i)
    {
        vtkDataArray* data_array = mesh_part.point_data_->GetArray(i);
        Field field;
        field.name_ = data_array->GetName();
        field.location_ = Location::POINT;
        // field.type_ = Type::SCALAR; // 简化处理，假设为标量
        field.num_components_ = data_array->GetNumberOfComponents();
        if( field.num_components_ == 1 )
            field.type_ = Type::SCALAR;
        else if( field.num_components_ == 3 )
            field.type_ = Type::VECTOR;
        else if( field.num_components_ == 9 )
            field.type_ = Type::TENSOR;
        else
            field.type_ = Type::OTHER;
        field.num_tuples_ = data_array->GetNumberOfTuples();
        field.pdata_ = data_array;
        mesh_part.point_fields_.push_back(field);
    }

    // 读取单元数据数组
    num_arrays = mesh_part.cell_data_->GetNumberOfArrays();
    mesh_part.cell_fields_.reserve(num_arrays);
    for (int i = 0; i < num_arrays; ++i)
    {
        vtkDataArray* data_array = mesh_part.cell_data_->GetArray(i);
        Field field;
        field.name_ = data_array->GetName();
        field.location_ = Location::CELL;
        // field.type_ = Type::SCALAR; // 简化处理，假设为标量
        field.num_components_ = data_array->GetNumberOfComponents();
        if( field.num_components_ == 1 )
            field.type_ = Type::SCALAR;
        else if( field.num_components_ == 3 )
            field.type_ = Type::VECTOR;
        else if( field.num_components_ == 9 )
            field.type_ = Type::TENSOR;
        else
            field.type_ = Type::OTHER;
        field.num_tuples_ = data_array->GetNumberOfTuples();
        field.pdata_ = data_array;
        mesh_part.cell_fields_.push_back(field);
    }
    
    return mesh_part;
}
