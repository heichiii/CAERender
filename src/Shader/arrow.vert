#version 450 core

layout(location = 0) in vec3 a_pos;           // 矢量起始位置
layout(location = 1) in vec3 a_direction;    // 矢量方向 (已归一化)
layout(location = 2) in float a_magnitude;   // 矢量幅值

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;
uniform float u_arrow_scale;  // 箭头尺度缩放因子

out VS_OUT {
    vec3 frag_pos;
    vec3 direction;
    float magnitude;
} vs_out;

void main()
{
    // 顶点着色器只是传递数据给几何着色器
    vs_out.frag_pos = vec3(u_model * vec4(a_pos, 1.0));
    vs_out.direction = normalize(mat3(u_model) * a_direction);
    vs_out.magnitude = a_magnitude;
    
    gl_Position = u_projection * u_view * vec4(vs_out.frag_pos, 1.0);
}
