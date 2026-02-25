#include "ShaderProgram.h"
#include <fstream>
#include <stdexcept>

ShaderProgram::ShaderProgram() : program_(std::make_unique<QOpenGLShaderProgram>())
{
}

std::string ShaderProgram::readShaderFile(const std::string& file_path)
{
    std::ifstream file(file_path);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open shader file: " + file_path);
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return content;
}

bool ShaderProgram::createFromSource(const std::string& vertex_source, const std::string& fragment_source)
{
    if (!program_->addShaderFromSourceCode(QOpenGLShader::Vertex, vertex_source.c_str()))
    {
        return false;
    }
    if (!program_->addShaderFromSourceCode(QOpenGLShader::Fragment, fragment_source.c_str()))
    {
        return false;
    }
    return program_->link();
}

bool ShaderProgram::createFromFiles(const std::string& vertex_path, const std::string& fragment_path)
{
    try
    {
        std::string vertex_source = readShaderFile(vertex_path);
        std::string fragment_source = readShaderFile(fragment_path);
        return createFromSource(vertex_source, fragment_source);
    }
    catch (const std::exception& e)
    {
        qDebug() << "Error creating shader program: " << e.what();
        return false;
    }
}

void ShaderProgram::bind()
{
    program_->bind();
}

void ShaderProgram::release()
{
    program_->release();
}

