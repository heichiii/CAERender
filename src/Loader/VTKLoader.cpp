#include "VTKLoader.h"
#include "Data/MeshPart.h"
#include "TestTool/Profiler.h"
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

MeshPart VTK::VTKLoader::load()
{
    PROFILE_CODE

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
            dataset_ = reader->GetOutput();
        }
        else
        {
            auto reader = vtkSmartPointer<vtkXMLPolyDataReader>::New();
            reader->SetFileName(filename_.c_str());
            reader->Update();
            dataset_ = reader->GetOutput();
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
            dataset_ = poly_reader->GetOutput();
        }
        else
        {
            // 默认尝试UnstructuredGrid
            auto reader = vtkSmartPointer<vtkUnstructuredGridReader>::New();
            reader->SetFileName(filename_.c_str());
            reader->Update();


            dataset_ = reader->GetOutput();
        }
    }

    if (!dataset_)
    {
        throw std::runtime_error("Failed to read VTK dataset: " + filename_);
    }
    points_ = dataset_->GetPoints();
    if (!points_)
    {
        throw std::runtime_error("Failed to read VTK points: " + filename_);
    }
    point_data_ = dataset_->GetPointData();
    num_cells_ = dataset_->GetNumberOfCells();
    cell_data_ = dataset_->GetCellData();
    int num_arrays;
    // 读取点数据数组
    num_arrays = point_data_->GetNumberOfArrays();
    point_fields_.reserve(num_arrays);
    for (int i = 0; i < num_arrays; ++i)
    {
        vtkDataArray* data_array = point_data_->GetArray(i);
        FieldData field;
        field.name_ = data_array->GetName();
        field.location_ = Location::POINT;
        // field.type_ = Type::SCALAR; // 简化处理，假设为标量
        field.num_components_ = data_array->GetNumberOfComponents();
        if (field.num_components_ == 1)
            field.type_ = Type::SCALAR;
        else if (field.num_components_ == 3)
            field.type_ = Type::VECTOR;
        else if (field.num_components_ == 9)
            field.type_ = Type::TENSOR;
        else
            field.type_ = Type::OTHER;
        field.num_tuples_ = data_array->GetNumberOfTuples();
        field.pdata_ = data_array;
        point_fields_.push_back(field);
    }

    // 读取单元数据数组
    num_arrays = cell_data_->GetNumberOfArrays();
    cell_fields_.reserve(num_arrays);
    for (int i = 0; i < num_arrays; ++i)
    {
        vtkDataArray* data_array = cell_data_->GetArray(i);
        FieldData field;
        field.name_ = data_array->GetName();
        field.location_ = Location::CELL;
        // field.type_ = Type::SCALAR; // 简化处理，假设为标量
        field.num_components_ = data_array->GetNumberOfComponents();
        if (field.num_components_ == 1)
            field.type_ = Type::SCALAR;
        else if (field.num_components_ == 3)
            field.type_ = Type::VECTOR;
        else if (field.num_components_ == 9)
            field.type_ = Type::TENSOR;
        else
            field.type_ = Type::OTHER;
        field.num_tuples_ = data_array->GetNumberOfTuples();
        field.pdata_ = data_array;
        cell_fields_.push_back(field);
    }

    MeshPart mesh_part;
    mesh_part.name_ = filename_;
    // 1-提取顶点坐标
    mesh_part.vertices_.reserve(points_->GetNumberOfPoints() * 3);
    for (vtkIdType i = 0; i < points_->GetNumberOfPoints(); ++i)
    {
        double p[3];
        points_->GetPoint(i, p);
        mesh_part.vertices_.push_back(static_cast<float>(p[0]));
        mesh_part.vertices_.push_back(static_cast<float>(p[1]));
        mesh_part.vertices_.push_back(static_cast<float>(p[2]));
    }
    // 2-提取点数据
    for (const auto& field : point_fields_)
    {
        Field f;
        f.name_ = field.name_;
        f.location_ = field.location_;
        f.type_ = field.type_;
        f.num_components_ = field.num_components_;
        f.num_tuples_ = field.num_tuples_;
        f.data.reserve(f.num_tuples_ * f.num_components_);
        for (vtkIdType i = 0; i < field.num_tuples_; ++i)
        {
            for (int j = 0; j < field.num_components_; ++j)
            {
                double val = field.pdata_->GetComponent(i, j);
                f.data.push_back(static_cast<float>(val));
            }
        }
        if(f.type_ == Type::SCALAR)
        {
            f.computeRange();
        }
        qInfo() << "Loaded point field: " << QString::fromStdString(f.name_)
                << " components: " << f.num_components_
                << " tuples: " << f.num_tuples_
                << " range: [" << f.min_value << ", " << f.max_value << "]";
        mesh_part.point_fields_.push_back(std::move(f));
        
    }
    // 3-提取单元数据
    for (const auto& field : cell_fields_)
    {
        Field f;
        f.name_ = field.name_;
        f.location_ = field.location_;
        f.type_ = field.type_;
        f.num_components_ = field.num_components_;
        f.num_tuples_ = field.num_tuples_;
        f.data.reserve(f.num_tuples_ * f.num_components_);
        for (vtkIdType i = 0; i < field.num_tuples_; ++i)
        {
            for (int j = 0; j < field.num_components_; ++j)
            {
                double val = field.pdata_->GetComponent(i, j);
                f.data.push_back(static_cast<float>(val));
            }
        }
        if(f.type_ == Type::SCALAR)
        {
            f.computeRange();
        }
         qInfo() << "Loaded cell field: " << QString::fromStdString(f.name_)
                << " components: " << f.num_components_
                << " tuples: " << f.num_tuples_
                << " range: [" << f.min_value << ", " << f.max_value << "]";
        mesh_part.cell_fields_.push_back(std::move(f));
    }
    // 4-从单元提取所有面
    // TODO:面提取VTK API
    mesh_part.faces_.reserve(num_cells_ * 6); // 粗略估计每个单元平均6个面
    for(vtkIdType cell_id = 0; cell_id < num_cells_; ++cell_id)
    {
        vtkCell* cell = dataset_->GetCell(cell_id);
        int cell_type = cell->GetCellType();
        vtkIdList* point_ids = cell->GetPointIds();
        vtkIdType num_points = point_ids->GetNumberOfIds();
#if 1
        auto requirePoints = [&](vtkIdType required, const char* type_name)
        {
            if (num_points < required)
            {
                std::cerr << "Unsupported cell (too few points): " << type_name
                          << " points=" << num_points
                          << " cell ID: " << cell_id << std::endl;
                return false;
            }
            return true;
        };
#endif
        #define IDX(k) static_cast<uint32_t>(point_ids->GetId(k))
        Face face;
        switch (cell_type)
        {
            case VTK_TRIANGLE:
                if (!requirePoints(3, "VTK_TRIANGLE"))
                    break;
                face.set3(IDX(0), IDX(1), IDX(2));
                face.cell_id = static_cast<uint32_t>(cell_id);
                mesh_part.faces_.push_back(face);
                break;

            case VTK_QUAD:
                if (!requirePoints(4, "VTK_QUAD"))
                    break;
                face.set4(IDX(0), IDX(1), IDX(2), IDX(3));
                face.cell_id = static_cast<uint32_t>(cell_id);
                mesh_part.faces_.push_back(face);
                break;
            case VTK_TETRA:
                if (!requirePoints(4, "VTK_TETRA"))
                    break;
                face.set3(IDX(0), IDX(1), IDX(3));
                face.cell_id = static_cast<uint32_t>(cell_id);
                mesh_part.faces_.push_back(face);
                face.set3(IDX(0), IDX(2), IDX(3));
                face.cell_id = static_cast<uint32_t>(cell_id);
                mesh_part.faces_.push_back(face);
                face.set3(IDX(1), IDX(2), IDX(3));
                face.cell_id = static_cast<uint32_t>(cell_id);
                mesh_part.faces_.push_back(face);
                face.set3(IDX(0), IDX(1), IDX(2));
                face.cell_id = static_cast<uint32_t>(cell_id);
                mesh_part.faces_.push_back(face);
                break;
            case VTK_HEXAHEDRON:
                if (!requirePoints(8, "VTK_HEXAHEDRON"))
                    break;
                face.set4(IDX(0), IDX(1), IDX(2), IDX(3));
                face.cell_id = static_cast<uint32_t>(cell_id);
                mesh_part.faces_.push_back(face);
                face.set4(IDX(4), IDX(5), IDX(6), IDX(7));
                face.cell_id = static_cast<uint32_t>(cell_id);
                mesh_part.faces_.push_back(face);
                face.set4(IDX(0), IDX(1), IDX(5), IDX(4));
                face.cell_id = static_cast<uint32_t>(cell_id);
                mesh_part.faces_.push_back(face);
                face.set4(IDX(1), IDX(2), IDX(6), IDX(5));
                face.cell_id = static_cast<uint32_t>(cell_id);
                mesh_part.faces_.push_back(face);
                face.set4(IDX(2), IDX(3), IDX(7), IDX(6));
                face.cell_id = static_cast<uint32_t>(cell_id);
                mesh_part.faces_.push_back(face);
                face.set4(IDX(3), IDX(0), IDX(4), IDX(7));
                face.cell_id = static_cast<uint32_t>(cell_id);
                mesh_part.faces_.push_back(face);
                break;
         case VTK_WEDGE:
             if (!requirePoints(6, "VTK_WEDGE"))
                break;
                face.set3(IDX(0), IDX(1), IDX(2));
                face.cell_id = static_cast<uint32_t>(cell_id);
                mesh_part.faces_.push_back(face);
                face.set3(IDX(3), IDX(4), IDX(5));
                face.cell_id = static_cast<uint32_t>(cell_id);
                mesh_part.faces_.push_back(face);
                face.set4(IDX(0), IDX(1), IDX(4), IDX(3));
                face.cell_id = static_cast<uint32_t>(cell_id);
                mesh_part.faces_.push_back(face);
                face.set4(IDX(1), IDX(2), IDX(5), IDX(4));
                face.cell_id = static_cast<uint32_t>(cell_id);
                mesh_part.faces_.push_back(face);
                face.set4(IDX(2), IDX(0), IDX(3), IDX(5));
                face.cell_id = static_cast<uint32_t>(cell_id);
                mesh_part.faces_.push_back(face);
                break;
            case VTK_PYRAMID:
                if (!requirePoints(5, "VTK_PYRAMID"))
                    break;
                face.set4(IDX(0), IDX(1), IDX(2), IDX(3));
                face.cell_id = static_cast<uint32_t>(cell_id);
                mesh_part.faces_.push_back(face);
                face.set3(IDX(0), IDX(1), IDX(4));
                face.cell_id = static_cast<uint32_t>(cell_id);
                mesh_part.faces_.push_back(face);
                face.set3(IDX(1), IDX(2), IDX(4));
                face.cell_id = static_cast<uint32_t>(cell_id);
                mesh_part.faces_.push_back(face);
                face.set3(IDX(2), IDX(3), IDX(4));
                face.cell_id = static_cast<uint32_t>(cell_id);
                mesh_part.faces_.push_back(face);
                face.set3(IDX(3), IDX(0), IDX(4));
                face.cell_id = static_cast<uint32_t>(cell_id);
                mesh_part.faces_.push_back(face);
                break;
            default:
                // 对于不支持的单元类型，可以选择跳过或抛出异常
                std::cerr << "Unsupported cell type: " << cell_type << " for cell ID: " << cell_id << std::endl;
                break;
        }
        #undef IDX
    }
    return mesh_part;
}
