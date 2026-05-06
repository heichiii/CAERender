#include "CGNSLoader.h"
#include "Data/MeshPart.h"
#include "TestTool/Profiler.h"
#include <iostream>
#include <stdexcept>
#include <vtkCGNSReader.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkUnstructuredGrid.h>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkCell.h>

struct FieldTempData {
    std::string name_;
    Location location_;
    Type type_;
    int num_components_;
    vtkIdType num_tuples_;
    vtkSmartPointer<vtkDataArray> pdata_;
};

MeshPart CGNSLoader::load()
{
    PROFILE_CODE

    auto reader = vtkSmartPointer<vtkCGNSReader>::New();
    reader->SetFileName(filename_.c_str());
    
    // IMPORTANT: Must call UpdateInformation before enabling arrays!
    reader->UpdateInformation();
    reader->EnableAllBases();
    for(int i=0; i<reader->GetNumberOfBaseArrays(); ++i) {
        reader->SetBaseArrayStatus(reader->GetBaseArrayName(i), 1);
    }
    reader->EnableAllFamilies();
    reader->EnableAllCellArrays();
    for(int i=0; i<reader->GetNumberOfCellArrays(); ++i) {
        reader->SetCellArrayStatus(reader->GetCellArrayName(i), 1);
    }
    reader->EnableAllPointArrays();
    for(int i=0; i<reader->GetNumberOfPointArrays(); ++i) {
        reader->SetPointArrayStatus(reader->GetPointArrayName(i), 1);
    }
    
    reader->Update();

    auto output = reader->GetOutput();
    if (!output) {
        throw std::runtime_error("Failed to read CGNS dataset: " + filename_);
    }

    MeshPart mesh_part;
    mesh_part.name_ = filename_;

    size_t current_vertex_offset = 0;

    std::vector<vtkDataObject*> queue;
    queue.push_back(output);
    while(!queue.empty()) {
        auto obj = queue.back();
        queue.pop_back();

        if (auto mb = vtkMultiBlockDataSet::SafeDownCast(obj)) {
            for(unsigned int i = 0; i < mb->GetNumberOfBlocks(); ++i) {
                if(mb->GetBlock(i)) {
                    queue.push_back(mb->GetBlock(i));
                }
            }
        } else if (auto ug = vtkUnstructuredGrid::SafeDownCast(obj)) {
            processGrid(ug, mesh_part, current_vertex_offset);
        }
    }

    return mesh_part;
}

void CGNSLoader::processGrid(vtkUnstructuredGrid* dataset, MeshPart& mesh_part, size_t& current_vertex_offset)
{
    std::vector<FieldTempData> point_fields_;
    std::vector<FieldTempData> cell_fields_;

    auto points = dataset->GetPoints();
    if (!points) return;

    auto point_data = dataset->GetPointData();
    auto cell_data = dataset->GetCellData();
    auto num_cells = dataset->GetNumberOfCells();

    // 提取顶点坐标
    size_t point_count = points->GetNumberOfPoints();
    mesh_part.vertices_.reserve(mesh_part.vertices_.size() + point_count * 3);
    for (vtkIdType i = 0; i < point_count; ++i)
    {
        double p[3];
        points->GetPoint(i, p);
        mesh_part.vertices_.push_back(static_cast<float>(p[0]));
        mesh_part.vertices_.push_back(static_cast<float>(p[1]));
        mesh_part.vertices_.push_back(static_cast<float>(p[2]));
    }

    // 处理面，单元连通性
    size_t cell_offset = mesh_part.cells_.size();
    mesh_part.cells_.resize(cell_offset + static_cast<size_t>(num_cells));

    for (vtkIdType cell_id = 0; cell_id < num_cells; ++cell_id)
    {
        vtkCell* cell = dataset->GetCell(cell_id);
        int cell_type = cell->GetCellType();
        vtkIdList* point_ids = cell->GetPointIds();
        vtkIdType num_points = point_ids->GetNumberOfIds();

#define IDX(k) static_cast<uint32_t>(point_ids->GetId(k) + current_vertex_offset)
        Face face;
        CellData& c_data = mesh_part.cells_[cell_offset + static_cast<size_t>(cell_id)];
        switch (cell_type)
        {
            case VTK_TRIANGLE:
                if (num_points >= 3) {
                    face.set3(IDX(0), IDX(1), IDX(2));
                    face.cell_id = static_cast<uint32_t>(cell_offset + cell_id);
                    mesh_part.faces_.push_back(face);
                }
                break;
            case VTK_QUAD:
                if (num_points >= 4) {
                    face.set4(IDX(0), IDX(1), IDX(2), IDX(3));
                    face.cell_id = static_cast<uint32_t>(cell_offset + cell_id);
                    mesh_part.faces_.push_back(face);
                }
                break;
            case VTK_TETRA:
                if (num_points >= 4) {
                    c_data.kind = CellKind::TETRA;
                    c_data.num_points = 4;
                    for (uint8_t i = 0; i < 4; ++i) c_data.point_ids[i] = IDX(i);
                    face.set3(IDX(0), IDX(1), IDX(3)); face.cell_id = static_cast<uint32_t>(cell_offset + cell_id); mesh_part.faces_.push_back(face);
                    face.set3(IDX(0), IDX(2), IDX(3)); face.cell_id = static_cast<uint32_t>(cell_offset + cell_id); mesh_part.faces_.push_back(face);
                    face.set3(IDX(1), IDX(2), IDX(3)); face.cell_id = static_cast<uint32_t>(cell_offset + cell_id); mesh_part.faces_.push_back(face);
                    face.set3(IDX(0), IDX(1), IDX(2)); face.cell_id = static_cast<uint32_t>(cell_offset + cell_id); mesh_part.faces_.push_back(face);
                }
                break;
            case VTK_HEXAHEDRON:
                if (num_points >= 8) {
                    c_data.kind = CellKind::HEXAHEDRON;
                    c_data.num_points = 8;
                    for (uint8_t i = 0; i < 8; ++i) c_data.point_ids[i] = IDX(i);
                    face.set4(IDX(0), IDX(1), IDX(2), IDX(3)); face.cell_id = static_cast<uint32_t>(cell_offset + cell_id); mesh_part.faces_.push_back(face);
                    face.set4(IDX(4), IDX(5), IDX(6), IDX(7)); face.cell_id = static_cast<uint32_t>(cell_offset + cell_id); mesh_part.faces_.push_back(face);
                    face.set4(IDX(0), IDX(1), IDX(5), IDX(4)); face.cell_id = static_cast<uint32_t>(cell_offset + cell_id); mesh_part.faces_.push_back(face);
                    face.set4(IDX(1), IDX(2), IDX(6), IDX(5)); face.cell_id = static_cast<uint32_t>(cell_offset + cell_id); mesh_part.faces_.push_back(face);
                    face.set4(IDX(2), IDX(3), IDX(7), IDX(6)); face.cell_id = static_cast<uint32_t>(cell_offset + cell_id); mesh_part.faces_.push_back(face);
                    face.set4(IDX(3), IDX(0), IDX(4), IDX(7)); face.cell_id = static_cast<uint32_t>(cell_offset + cell_id); mesh_part.faces_.push_back(face);
                }
                break;
            case VTK_WEDGE:
                if (num_points >= 6) {
                    face.set3(IDX(0), IDX(1), IDX(2)); face.cell_id = static_cast<uint32_t>(cell_offset + cell_id); mesh_part.faces_.push_back(face);
                    face.set3(IDX(3), IDX(4), IDX(5)); face.cell_id = static_cast<uint32_t>(cell_offset + cell_id); mesh_part.faces_.push_back(face);
                    face.set4(IDX(0), IDX(1), IDX(4), IDX(3)); face.cell_id = static_cast<uint32_t>(cell_offset + cell_id); mesh_part.faces_.push_back(face);
                    face.set4(IDX(1), IDX(2), IDX(5), IDX(4)); face.cell_id = static_cast<uint32_t>(cell_offset + cell_id); mesh_part.faces_.push_back(face);
                    face.set4(IDX(2), IDX(0), IDX(3), IDX(5)); face.cell_id = static_cast<uint32_t>(cell_offset + cell_id); mesh_part.faces_.push_back(face);
                }
                break;
            case VTK_PYRAMID:
                if (num_points >= 5) {
                    face.set4(IDX(0), IDX(1), IDX(2), IDX(3)); face.cell_id = static_cast<uint32_t>(cell_offset + cell_id); mesh_part.faces_.push_back(face);
                    face.set3(IDX(0), IDX(1), IDX(4)); face.cell_id = static_cast<uint32_t>(cell_offset + cell_id); mesh_part.faces_.push_back(face);
                    face.set3(IDX(1), IDX(2), IDX(4)); face.cell_id = static_cast<uint32_t>(cell_offset + cell_id); mesh_part.faces_.push_back(face);
                    face.set3(IDX(2), IDX(3), IDX(4)); face.cell_id = static_cast<uint32_t>(cell_offset + cell_id); mesh_part.faces_.push_back(face);
                    face.set3(IDX(3), IDX(0), IDX(4)); face.cell_id = static_cast<uint32_t>(cell_offset + cell_id); mesh_part.faces_.push_back(face);
                }
                break;
            // Polygons from NGON cells
            case VTK_POLYGON:
            case VTK_CONVEX_POINT_SET:
            {
                // Fan triangulation
                for(vtkIdType i = 1; i < num_points - 1; i++) {
                    face.set3(IDX(0), IDX(i), IDX(i+1));
                    face.cell_id = static_cast<uint32_t>(cell_offset + cell_id);
                    mesh_part.faces_.push_back(face);
                }
                break;
            }
            default:
                break;
        }
#undef IDX
    }
    
    std::cout << "[CGNSLoader] Found Point Arrays: " << dataset->GetPointData()->GetNumberOfArrays() << " empty: " << mesh_part.point_fields_.empty() << std::endl;
    std::cout << "[CGNSLoader] Found Cell Arrays: " << dataset->GetCellData()->GetNumberOfArrays() << " empty: " << mesh_part.cell_fields_.empty() << std::endl;

    // (提取点/单元数据的部分，因为可能是多区块，可以合并Field，为简便假设只有一个主数据区)
    // 为简便起见，只向mesh_part追加第一次遇到的Field
    if (dataset->GetPointData()->GetNumberOfArrays() > 0 && mesh_part.point_fields_.empty()) {
        int num_arrays = dataset->GetPointData()->GetNumberOfArrays();
        for (int i = 0; i < num_arrays; ++i) {
            vtkDataArray* data_array = dataset->GetPointData()->GetArray(i);
            Field f;
            f.name_ = std::string("POINT-") + data_array->GetName();
            f.location_ = Location::POINT;
            f.num_components_ = data_array->GetNumberOfComponents();
            if (f.num_components_ == 1) { f.type_ = Type::SCALAR; f.name_ += "-SCALAR"; }
            else if (f.num_components_ == 3) { f.type_ = Type::VECTOR; f.name_ += "-VECTOR"; }
            else if (f.num_components_ == 9) { f.type_ = Type::TENSOR; f.name_ += "-TENSOR"; }
            else { f.type_ = Type::OTHER; f.name_ += "-OTHER"; }
            
            f.num_tuples_ = data_array->GetNumberOfTuples();
            f.data.reserve(f.num_tuples_ * f.num_components_);
            for (vtkIdType t = 0; t < f.num_tuples_; ++t) {
                for (int c = 0; c < f.num_components_; ++c) {
                    f.data.push_back(static_cast<float>(data_array->GetComponent(t, c)));
                }
            }
            if (f.type_ == Type::SCALAR) f.computeRange();
            mesh_part.point_fields_.push_back(f);
        }
    }

    if (dataset->GetCellData()->GetNumberOfArrays() > 0 && mesh_part.cell_fields_.empty()) {
        int num_arrays = dataset->GetCellData()->GetNumberOfArrays();
        for (int i = 0; i < num_arrays; ++i) {
            vtkDataArray* data_array = dataset->GetCellData()->GetArray(i);
            Field f;
            f.name_ = std::string("CELL-") + data_array->GetName();
            f.location_ = Location::CELL;
            f.num_components_ = data_array->GetNumberOfComponents();
            if (f.num_components_ == 1) { f.type_ = Type::SCALAR; f.name_ += "-SCALAR"; }
            else if (f.num_components_ == 3) { f.type_ = Type::VECTOR; f.name_ += "-VECTOR"; }
            else if (f.num_components_ == 9) { f.type_ = Type::TENSOR; f.name_ += "-TENSOR"; }
            else { f.type_ = Type::OTHER; f.name_ += "-OTHER"; }
            
            f.num_tuples_ = data_array->GetNumberOfTuples();
            f.data.reserve(f.num_tuples_ * f.num_components_);
            for (vtkIdType t = 0; t < f.num_tuples_; ++t) {
                for (int c = 0; c < f.num_components_; ++c) {
                    f.data.push_back(static_cast<float>(data_array->GetComponent(t, c)));
                }
            }
            if (f.type_ == Type::SCALAR) f.computeRange();
            mesh_part.cell_fields_.push_back(f);
        }
    }

    if (!mesh_part.point_fields_.empty() && !mesh_part.active_field_) {
        mesh_part.active_field_ = &mesh_part.point_fields_[0];
    } else if (!mesh_part.cell_fields_.empty() && !mesh_part.active_field_) {
        mesh_part.active_field_ = &mesh_part.cell_fields_[0];
    }

    current_vertex_offset += point_count;
}
