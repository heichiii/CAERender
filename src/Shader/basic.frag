#version 450 core

out vec4 frag_color;
in vec3 v_frag_pos;
in vec3 v_normal;
in float v_scalar_fields;

uniform vec3 u_light_pos;
uniform vec3 u_view_pos;
uniform vec3 u_object_color;
uniform float u_scalar_min;
uniform float u_scalar_max;

// Jet颜色映射函数
vec3 jet_colormap(float t)
{
    // 将[0,1]映射到Jet配色方案
    // Jet配色: 蓝色->青色->绿色->黄色->红色
    vec3 color;
    
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    
    if (t < 0.125) {
        color = vec3(0.0, 0.0, 0.5 + 0.5 * (t / 0.125));
    } else if (t < 0.375) {
        color = vec3(0.0, (t - 0.125) / 0.25, 1.0);
    } else if (t < 0.625) {
        color = vec3((t - 0.375) / 0.25, 1.0, 1.0 - (t - 0.375) / 0.25);
    } else if (t < 0.875) {
        color = vec3(1.0, 1.0 - (t - 0.625) / 0.25, 0.0);
    } else {
        color = vec3(1.0 - 0.5 * (t - 0.875) / 0.125, 0.0, 0.0);
    }
    
    return color;
}

void main()
{
    // 双面光照：处理背面法线方向
    vec3 norm = gl_FrontFacing ? normalize(v_normal) : -normalize(v_normal);
    
    // 标量值归一化到[0,1]区间
    float normalized_scalar = (v_scalar_fields - u_scalar_min) / (u_scalar_max - u_scalar_min);
    
    // 使用Jet颜色映射获取基础颜色
    vec3 base_color = jet_colormap(normalized_scalar);
    
    // Ambient光照
    float ambient_strength = 0.3;
    vec3 ambient = ambient_strength * base_color;
    
    // Diffuse光照
    vec3 light_dir = normalize(u_light_pos - v_frag_pos);
    float diff = max(abs(dot(norm, light_dir)), 0.0);  // 使用abs确保背面也有光照
    vec3 diffuse = diff * base_color;
    
    // Specular高光
    float specular_strength = 0.4;
    vec3 view_dir = normalize(u_view_pos - v_frag_pos);
    vec3 reflect_dir = reflect(-light_dir, norm);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32.0);
    vec3 specular = specular_strength * spec * vec3(1.0);
    
    // 合成最终颜色
    vec3 result = ambient + diffuse + specular;
    frag_color = vec4(result, 1.0);
}