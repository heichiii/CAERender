#version 450 core

layout(points) in;
layout(triangle_strip, max_vertices = 18) out;

in VS_OUT {
    vec3 frag_pos;
    vec3 direction;
    float magnitude;
} gs_in[];

uniform mat4 u_view;
uniform mat4 u_projection;
uniform float u_arrow_scale;
uniform float u_vector_magnitude_min;
uniform float u_vector_magnitude_max;

out vec3 v_frag_pos;
out vec3 v_normal;
out float v_magnitude_normalized;

vec3 get_perpendicular(vec3 v)
{
    // 找一个垂直于v的向量
    if (abs(v.x) < 0.9)
        return normalize(cross(v, vec3(1.0, 0.0, 0.0)));
    else
        return normalize(cross(v, vec3(0.0, 1.0, 0.0)));
}

void emit_arrow_cone()
{
    vec3 base_pos = gs_in[0].frag_pos;
    vec3 direction = normalize(gs_in[0].direction);
    float magnitude = gs_in[0].magnitude;
    
    // 归一化幅值到[0, 1]
    v_magnitude_normalized = clamp((magnitude - u_vector_magnitude_min) / 
                                    max(u_vector_magnitude_max - u_vector_magnitude_min, 0.001), 
                                    0.0, 1.0);
    
    // 箭头长度和宽度
    float arrow_length = u_arrow_scale * 0.8;
    float arrow_tip_pos = arrow_length;
    float base_radius = u_arrow_scale * 0.1;
    
    // 生成三个垂直向量作为圆锥的基圆
    vec3 perp1 = get_perpendicular(direction);
    vec3 perp2 = cross(direction, perp1);
    
    // 圆锥底部的圆圈顶点数
    int segments = 8;
    
    // 1. 生成圆锥体的尖端
    vec3 tip_pos = base_pos + direction * arrow_tip_pos;
    
    // 2. 生成圆锥的侧面（三角形带）
    for (int i = 0; i <= segments; ++i)
    {
        float angle = 2.0 * 3.14159265 * float(i) / float(segments);
        float cos_angle = cos(angle);
        float sin_angle = sin(angle);
        
        // 基圆上的点
        vec3 base_circle_point = base_pos + (perp1 * cos_angle + perp2 * sin_angle) * base_radius;
        
        // 计算法线（从尖端指向基圆外侧）
        vec3 to_circle = normalize(base_circle_point - tip_pos);
        vec3 to_next_angle = normalize((perp1 * cos(angle + 0.1) + perp2 * sin(angle + 0.1)));
        vec3 normal = normalize(cross(direction, to_next_angle));
        
        // 发出尖端顶点
        gl_Position = u_projection * u_view * vec4(tip_pos, 1.0);
        v_frag_pos = tip_pos;
        v_normal = normal;
        EmitVertex();
        
        // 发出基圆顶点
        gl_Position = u_projection * u_view * vec4(base_circle_point, 1.0);
        v_frag_pos = base_circle_point;
        v_normal = normal;
        EmitVertex();
    }
    
    EndPrimitive();
    
    // 3. 生成圆形底面
    for (int i = 0; i <= segments; ++i)
    {
        float angle = 2.0 * 3.14159265 * float(i) / float(segments);
        float cos_angle = cos(angle);
        float sin_angle = sin(angle);
        
        vec3 base_circle_point = base_pos + (perp1 * cos_angle + perp2 * sin_angle) * base_radius;
        
        // 底面法线指向负方向
        vec3 bottom_normal = -direction;
        
        // 发出中心点
        gl_Position = u_projection * u_view * vec4(base_pos, 1.0);
        v_frag_pos = base_pos;
        v_normal = bottom_normal;
        EmitVertex();
        
        // 发出底面圆周点
        gl_Position = u_projection * u_view * vec4(base_circle_point, 1.0);
        v_frag_pos = base_circle_point;
        v_normal = bottom_normal;
        EmitVertex();
    }
    
    EndPrimitive();
}

void main()
{
    if (length(gs_in[0].direction) > 0.001)
    {
        emit_arrow_cone();
    }
}
