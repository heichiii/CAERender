#pragma once
#include <QOpenGLShaderProgram>
#include <QOpenGLFunctions_4_5_Core>
#include <memory>
#include <string>


class ShaderProgram 
{
public:
    ShaderProgram();
    ~ShaderProgram() = default;
    bool createFromFiles(const std::string& vertex_path, const std::string& fragment_path);
    bool createFromSource(const std::string& vertex_source, const std::string& fragment_source);
    void bind();
    void release();

    QOpenGLShaderProgram* getProgram() const { return program_.get(); }

private:
    std::unique_ptr<QOpenGLShaderProgram> program_;
    std::string readShaderFile(const std::string& file_path);
};
