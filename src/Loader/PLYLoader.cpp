#include "PLYLoader.h"
#include "Data/MeshPart.h"
#include "TestTool/Profiler.h"
#include <QDebug>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace
{
bool isLittleEndianHost()
{
    uint16_t value = 0x1;
    return *reinterpret_cast<unsigned char*>(&value) == 0x1;
}

size_t plyTypeSize(const std::string& type)
{
    if (type == "char" || type == "uchar" || type == "int8" || type == "uint8")
        return 1;
    if (type == "short" || type == "ushort" || type == "int16" || type == "uint16")
        return 2;
    if (type == "int" || type == "uint" || type == "float" || type == "int32" ||
        type == "uint32" || type == "float32")
        return 4;
    if (type == "double" || type == "int64" || type == "uint64" || type == "float64")
        return 8;
    return 0;
}

template <typename T>
void byteSwapInPlace(T& value)
{
    auto* begin = reinterpret_cast<unsigned char*>(&value);
    std::reverse(begin, begin + sizeof(T));
}

double readScalarAsDouble(std::ifstream& file, const std::string& type, bool file_big_endian)
{
    const bool need_swap = (file_big_endian == isLittleEndianHost());

    if (type == "char" || type == "int8")
    {
        int8_t value = 0;
        file.read(reinterpret_cast<char*>(&value), sizeof(value));
        return static_cast<double>(value);
    }
    if (type == "uchar" || type == "uint8")
    {
        uint8_t value = 0;
        file.read(reinterpret_cast<char*>(&value), sizeof(value));
        return static_cast<double>(value);
    }
    if (type == "short" || type == "int16")
    {
        int16_t value = 0;
        file.read(reinterpret_cast<char*>(&value), sizeof(value));
        if (need_swap)
            byteSwapInPlace(value);
        return static_cast<double>(value);
    }
    if (type == "ushort" || type == "uint16")
    {
        uint16_t value = 0;
        file.read(reinterpret_cast<char*>(&value), sizeof(value));
        if (need_swap)
            byteSwapInPlace(value);
        return static_cast<double>(value);
    }
    if (type == "int" || type == "int32")
    {
        int32_t value = 0;
        file.read(reinterpret_cast<char*>(&value), sizeof(value));
        if (need_swap)
            byteSwapInPlace(value);
        return static_cast<double>(value);
    }
    if (type == "uint" || type == "uint32")
    {
        uint32_t value = 0;
        file.read(reinterpret_cast<char*>(&value), sizeof(value));
        if (need_swap)
            byteSwapInPlace(value);
        return static_cast<double>(value);
    }
    if (type == "float" || type == "float32")
    {
        float value = 0.0f;
        file.read(reinterpret_cast<char*>(&value), sizeof(value));
        if (need_swap)
            byteSwapInPlace(value);
        return static_cast<double>(value);
    }
    if (type == "double" || type == "float64")
    {
        double value = 0.0;
        file.read(reinterpret_cast<char*>(&value), sizeof(value));
        if (need_swap)
            byteSwapInPlace(value);
        return value;
    }
    if (type == "int64")
    {
        int64_t value = 0;
        file.read(reinterpret_cast<char*>(&value), sizeof(value));
        if (need_swap)
            byteSwapInPlace(value);
        return static_cast<double>(value);
    }
    if (type == "uint64")
    {
        uint64_t value = 0;
        file.read(reinterpret_cast<char*>(&value), sizeof(value));
        if (need_swap)
            byteSwapInPlace(value);
        return static_cast<double>(value);
    }

    throw std::runtime_error("Unsupported PLY property type: " + type);
}

void skipHeader(std::ifstream& file)
{
    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line == "end_header")
            break;
    }
}
} // namespace

MeshPart PLYLoader::load()
{
    PROFILE_CODE

    PLYHeader header = parsePLYHeader();
    if (header.num_vertices <= 0)
    {
        throw std::runtime_error("PLY file has no vertices: " + filename_);
    }

    std::vector<VertexData> vertices = readVertexData(header);

    MeshPart mesh_part;
    mesh_part.name_ = filename_;
    fillMeshPart(mesh_part, vertices, header);
    return mesh_part;
}

PLYLoader::PLYHeader PLYLoader::parsePLYHeader()
{
    std::ifstream file(filename_, std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open PLY file: " + filename_);
    }

    PLYHeader header;
    std::string line;

    if (!std::getline(file, line) || line != "ply")
    {
        throw std::runtime_error("Invalid PLY file: missing 'ply' magic number");
    }

    bool found_end_header = false;
    std::string current_element;

    while (std::getline(file, line) && !found_end_header)
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line == "end_header")
        {
            found_end_header = true;
            break;
        }

        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if (token == "format")
        {
            std::string format;
            std::string version;
            iss >> format >> version;
            if (format == "ascii")
            {
                header.is_binary = false;
                header.is_binary_big_endian = false;
            }
            else if (format == "binary_little_endian")
            {
                header.is_binary = true;
                header.is_binary_big_endian = false;
            }
            else if (format == "binary_big_endian")
            {
                header.is_binary = true;
                header.is_binary_big_endian = true;
            }
            else
            {
                throw std::runtime_error("Unsupported PLY format: " + format);
            }
        }
        else if (token == "element")
        {
            std::string element_type;
            iss >> element_type;
            current_element = element_type;
            if (element_type == "vertex")
            {
                iss >> header.num_vertices;
            }
            else if (element_type == "face")
            {
                iss >> header.num_faces;
            }
        }
        else if (token == "property")
        {
            if (current_element == "vertex")
            {
                PropertyDesc property;
                iss >> property.type >> property.name;
                if (property.type.empty() || property.name.empty())
                {
                    throw std::runtime_error("Invalid vertex property declaration in PLY header");
                }

                const size_t type_size = plyTypeSize(property.type);
                if (type_size == 0)
                {
                    throw std::runtime_error("Unsupported vertex property type: " + property.type);
                }

                header.vertex_byte_size += static_cast<int>(type_size);
                header.vertex_properties.push_back(property);

                if (property.name == "x")
                    header.has_x = true;
                else if (property.name == "y")
                    header.has_y = true;
                else if (property.name == "z")
                    header.has_z = true;
                else if (property.name == "red" || property.name == "r")
                    header.has_red = true;
                else if (property.name == "green" || property.name == "g")
                    header.has_green = true;
                else if (property.name == "blue" || property.name == "b")
                    header.has_blue = true;
            }
            else if (current_element == "face")
            {
                std::string kind;
                iss >> kind;
                if (kind == "list")
                {
                    PropertyDesc property;
                    property.is_list = true;
                    iss >> property.list_count_type >> property.list_value_type >> property.name;
                    if (!header.has_face_vertex_indices &&
                        (property.name == "vertex_indices" || property.name == "vertex_index" ||
                         property.name == "vertex_ids"))
                    {
                        header.has_face_vertex_indices = true;
                        header.face_vertex_indices = property;
                    }
                }
            }
        }
    }

    if (!found_end_header)
    {
        throw std::runtime_error("Invalid PLY file: missing 'end_header'");
    }

    if (!header.has_x || !header.has_y || !header.has_z)
    {
        throw std::runtime_error("PLY file missing required x, y, or z coordinates");
    }

    qDebug() << "PLYLoader::parsePLYHeader:" << filename_.c_str();
    qDebug() << "  - num_vertices:" << header.num_vertices;
    qDebug() << "  - num_faces:" << header.num_faces;
    qDebug() << "  - vertex_byte_size:" << header.vertex_byte_size;
    qDebug() << "  - format: binary=" << header.is_binary
             << ", big_endian=" << header.is_binary_big_endian;
    qDebug() << "  - has RGB:" << (header.has_red && header.has_green && header.has_blue);
    if (header.has_face_vertex_indices)
    {
        qDebug() << "  - face list:" << QString::fromStdString(header.face_vertex_indices.list_count_type)
                 << QString::fromStdString(header.face_vertex_indices.list_value_type)
                 << QString::fromStdString(header.face_vertex_indices.name);
    }

    return header;
}

std::vector<PLYLoader::VertexData> PLYLoader::readVertexData(const PLYHeader& header)
{
    std::ifstream file(filename_, std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open PLY file: " + filename_);
    }

    skipHeader(file);

    std::vector<VertexData> vertices;
    vertices.reserve(header.num_vertices);

    if (header.is_binary)
    {
        for (int i = 0; i < header.num_vertices; ++i)
        {
            VertexData vdata;
            for (const auto& property : header.vertex_properties)
            {
                const double value =
                    readScalarAsDouble(file, property.type, header.is_binary_big_endian);

                if (property.name == "x")
                    vdata.x = static_cast<float>(value);
                else if (property.name == "y")
                    vdata.y = static_cast<float>(value);
                else if (property.name == "z")
                    vdata.z = static_cast<float>(value);
                else if (property.name == "red" || property.name == "r")
                    vdata.r = static_cast<unsigned char>(std::clamp(value, 0.0, 255.0));
                else if (property.name == "green" || property.name == "g")
                    vdata.g = static_cast<unsigned char>(std::clamp(value, 0.0, 255.0));
                else if (property.name == "blue" || property.name == "b")
                    vdata.b = static_cast<unsigned char>(std::clamp(value, 0.0, 255.0));
            }
            vertices.push_back(vdata);
        }
    }
    else
    {
        std::string line;
        for (int i = 0; i < header.num_vertices; ++i)
        {
            if (!std::getline(file, line))
            {
                throw std::runtime_error("Unexpected end of file while reading vertices");
            }

            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            std::istringstream iss(line);
            VertexData vdata;
            for (const auto& property : header.vertex_properties)
            {
                double value = 0.0;
                iss >> value;
                if (property.name == "x")
                    vdata.x = static_cast<float>(value);
                else if (property.name == "y")
                    vdata.y = static_cast<float>(value);
                else if (property.name == "z")
                    vdata.z = static_cast<float>(value);
                else if (property.name == "red" || property.name == "r")
                    vdata.r = static_cast<unsigned char>(std::clamp(value, 0.0, 255.0));
                else if (property.name == "green" || property.name == "g")
                    vdata.g = static_cast<unsigned char>(std::clamp(value, 0.0, 255.0));
                else if (property.name == "blue" || property.name == "b")
                    vdata.b = static_cast<unsigned char>(std::clamp(value, 0.0, 255.0));
            }
            vertices.push_back(vdata);
        }
    }

    return vertices;
}

std::vector<Face> PLYLoader::readFaceData(const PLYHeader& header)
{
    std::vector<Face> faces;
    if (header.num_faces <= 0)
    {
        return faces;
    }

    std::ifstream file(filename_, std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open PLY file: " + filename_);
    }

    skipHeader(file);

    if (header.is_binary)
    {
        if (header.vertex_byte_size <= 0)
        {
            qWarning() << "PLYLoader: invalid vertex_byte_size=" << header.vertex_byte_size;
            return faces;
        }
        file.seekg(static_cast<std::streamoff>(header.num_vertices) * header.vertex_byte_size,
                   std::ios::cur);
    }
    else
    {
        std::string line;
        for (int i = 0; i < header.num_vertices; ++i)
        {
            if (!std::getline(file, line))
                return faces;
        }
    }

    faces.reserve(header.num_faces);

    if (header.is_binary)
    {
        if (!header.has_face_vertex_indices)
        {
            qWarning() << "PLYLoader: face element found but no supported vertex list property";
            return faces;
        }

        const auto& list_property = header.face_vertex_indices;
        for (int i = 0; i < header.num_faces; ++i)
        {
            const double count_value =
                readScalarAsDouble(file, list_property.list_count_type, header.is_binary_big_endian);
            const int vertex_count = static_cast<int>(count_value);
            if (!file.good())
            {
                break;
            }

            std::vector<uint32_t> idx;
            idx.reserve(static_cast<size_t>(std::max(vertex_count, 0)));
            for (int j = 0; j < vertex_count; ++j)
            {
                const double id_value =
                    readScalarAsDouble(file, list_property.list_value_type, header.is_binary_big_endian);
                const int64_t id_int = static_cast<int64_t>(id_value);
                if (id_int < 0)
                    idx.push_back(UINT32_MAX);
                else
                    idx.push_back(static_cast<uint32_t>(id_int));
            }

            if (vertex_count == 3)
            {
                if (idx[0] < static_cast<uint32_t>(header.num_vertices) &&
                    idx[1] < static_cast<uint32_t>(header.num_vertices) &&
                    idx[2] < static_cast<uint32_t>(header.num_vertices))
                {
                    Face face;
                    face.set3(idx[0], idx[1], idx[2]);
                    face.cell_id = static_cast<uint32_t>(i);
                    faces.push_back(face);
                }
            }
            else if (vertex_count == 4)
            {
                if (idx[0] < static_cast<uint32_t>(header.num_vertices) &&
                    idx[1] < static_cast<uint32_t>(header.num_vertices) &&
                    idx[2] < static_cast<uint32_t>(header.num_vertices) &&
                    idx[3] < static_cast<uint32_t>(header.num_vertices))
                {
                    Face face;
                    face.set4(idx[0], idx[1], idx[2], idx[3]);
                    face.cell_id = static_cast<uint32_t>(i);
                    faces.push_back(face);
                }
            }
        }
    }
    else
    {
        std::string line;
        for (int i = 0; i < header.num_faces; ++i)
        {
            if (!std::getline(file, line))
                break;
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            std::istringstream iss(line);
            int vertex_count = 0;
            iss >> vertex_count;

            if (vertex_count == 3)
            {
                uint32_t v0 = 0, v1 = 0, v2 = 0;
                iss >> v0 >> v1 >> v2;
                if (v0 < static_cast<uint32_t>(header.num_vertices) &&
                    v1 < static_cast<uint32_t>(header.num_vertices) &&
                    v2 < static_cast<uint32_t>(header.num_vertices))
                {
                    Face face;
                    face.set3(v0, v1, v2);
                    face.cell_id = static_cast<uint32_t>(i);
                    faces.push_back(face);
                }
            }
            else if (vertex_count == 4)
            {
                uint32_t v0 = 0, v1 = 0, v2 = 0, v3 = 0;
                iss >> v0 >> v1 >> v2 >> v3;
                if (v0 < static_cast<uint32_t>(header.num_vertices) &&
                    v1 < static_cast<uint32_t>(header.num_vertices) &&
                    v2 < static_cast<uint32_t>(header.num_vertices) &&
                    v3 < static_cast<uint32_t>(header.num_vertices))
                {
                    Face face;
                    face.set4(v0, v1, v2, v3);
                    face.cell_id = static_cast<uint32_t>(i);
                    faces.push_back(face);
                }
            }
        }
    }

    qDebug() << "PLYLoader: Loaded" << faces.size() << "valid faces";
    return faces;
}

void PLYLoader::fillMeshPart(MeshPart& mesh_part, const std::vector<VertexData>& vertices,
                             const PLYHeader& header)
{
    mesh_part.vertices_.reserve(vertices.size() * 3);
    for (const auto& v : vertices)
    {
        mesh_part.vertices_.push_back(v.x);
        mesh_part.vertices_.push_back(v.y);
        mesh_part.vertices_.push_back(v.z);
    }

    Field color_field;
    color_field.name_ = "RGB";
    color_field.location_ = Location::POINT;
    color_field.type_ = Type::VECTOR;
    color_field.num_components_ = 3;
    color_field.num_tuples_ = static_cast<int>(vertices.size());
    color_field.data.reserve(vertices.size() * 3);

    for (const auto& v : vertices)
    {
        color_field.data.push_back(v.r / 255.0f);
        color_field.data.push_back(v.g / 255.0f);
        color_field.data.push_back(v.b / 255.0f);
    }
    color_field.computeRange();

    mesh_part.point_fields_.push_back(color_field);
    if (!mesh_part.point_fields_.empty())
    {
        mesh_part.active_field_ = &mesh_part.point_fields_[0];
    }

    if (header.num_faces > 0)
    {
        mesh_part.faces_ = readFaceData(header);
    }

    if (mesh_part.faces_.empty())
    {
        const uint32_t num_vertices = static_cast<uint32_t>(vertices.size());
        for (uint32_t i = 0; i + 2 < num_vertices; i += 3)
        {
            Face face;
            face.set3(i, i + 1, i + 2);
            face.cell_id = i / 3;
            mesh_part.faces_.push_back(face);
        }
        qDebug() << "PLYLoader: Generated" << mesh_part.faces_.size() << "virtual faces for"
                 << num_vertices << "vertices";
    }
    else
    {
        qDebug() << "PLYLoader: Loaded" << mesh_part.faces_.size() << "faces from PLY file";
    }
}
