#version 450 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in float a_magnitude;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

out VS_OUT {
    float magnitude;
} vs_out;

void main()
{
    gl_Position = u_projection * u_view * u_model * vec4(a_position, 1.0);
    vs_out.magnitude = a_magnitude;
}
