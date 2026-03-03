#pragma once
#include "Loader.h"
#include "Data/Face.h"
#include <string>
#include <vector>

class PLYLoader : public Loader
{
public:
    PLYLoader(const std::string& filename) : Loader(filename) {}
    ~PLYLoader() override = default;
    MeshPart load() override;

private:
    struct PropertyDesc
    {
        std::string name;
        std::string type;
        bool is_list = false;
        std::string list_count_type;
        std::string list_value_type;
    };

    struct PLYHeader
    {
        int num_vertices = 0;
        int num_faces = 0;
        bool has_x = false, has_y = false, has_z = false;
        bool has_red = false, has_green = false, has_blue = false;
        bool is_binary = false;
        bool is_binary_big_endian = false;
        int vertex_byte_size = 0;
        std::vector<PropertyDesc> vertex_properties;
        bool has_face_vertex_indices = false;
        PropertyDesc face_vertex_indices;
    };

    struct VertexData
    {
        float x = 0, y = 0, z = 0;
        unsigned char r = 0, g = 0, b = 0;
        float nx = 0, ny = 0, nz = 0;
    };

    PLYHeader parsePLYHeader();
    std::vector<VertexData> readVertexData(const PLYHeader& header);
    std::vector<Face> readFaceData(const PLYHeader& header);
    void fillMeshPart(MeshPart& mesh_part, const std::vector<VertexData>& vertices,
                      const PLYHeader& header);
};
