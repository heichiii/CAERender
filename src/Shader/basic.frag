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
uniform int u_color_scheme;      // 0=彩虹, 1=热力图, 2=冷暖, 3=灰度, 4=蓝白红
uniform bool u_use_field_coloring; // 是否使用场量着色

// 彩虹配色方案 (Jet)
vec3 rainbow_colormap(float t)
{
    t = clamp(t, 0.0, 1.0);
    vec3 color;
    
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

// 热力图配色方案 (黑->红->橙->黄->白)
vec3 heatmap_colormap(float t)
{
    t = clamp(t, 0.0, 1.0);
    vec3 color;
    
    if (t < 0.25) {
        // 黑色到红色
        color = vec3(t * 4.0, 0.0, 0.0);
    } else if (t < 0.5) {
        // 红色到橙色
        color = vec3(1.0, (t - 0.25) * 4.0, 0.0);
    } else if (t < 0.75) {
        // 橙色到黄色
        color = vec3(1.0, 1.0, (t - 0.5) * 4.0);
    } else {
        // 黄色到白色
        float blend = (t - 0.75) * 4.0;
        color = vec3(1.0, 1.0, 1.0);
    }
    
    return color;
}

// 冷暖配色方案 (蓝色->白色->红色)
vec3 cool_warm_colormap(float t)
{
    t = clamp(t, 0.0, 1.0);
    
    if (t < 0.5) {
        // 蓝色到白色
        float s = t * 2.0;
        return mix(vec3(0.0, 0.0, 1.0), vec3(1.0, 1.0, 1.0), s);
    } else {
        // 白色到红色
        float s = (t - 0.5) * 2.0;
        return mix(vec3(1.0, 1.0, 1.0), vec3(1.0, 0.0, 0.0), s);
    }
}

// 灰度配色方案
vec3 grayscale_colormap(float t)
{
    t = clamp(t, 0.0, 1.0);
    return vec3(t, t, t);
}

// 蓝白红配色方案（发散配色）
vec3 blue_white_red_colormap(float t)
{
    t = clamp(t, 0.0, 1.0);
    
    if (t < 0.5) {
        // 深蓝到白色
        float s = t * 2.0;
        return mix(vec3(0.0, 0.0, 0.5), vec3(1.0, 1.0, 1.0), s);
    } else {
        // 白色到深红
        float s = (t - 0.5) * 2.0;
        return mix(vec3(1.0, 1.0, 1.0), vec3(0.5, 0.0, 0.0), s);
    }
}

// 根据配色方案选择
vec3 apply_colormap(float t, int scheme)
{
    if (scheme == 0) {
        return rainbow_colormap(t);
    } else if (scheme == 1) {
        return heatmap_colormap(t);
    } else if (scheme == 2) {
        return cool_warm_colormap(t);
    } else if (scheme == 3) {
        return grayscale_colormap(t);
    } else if (scheme == 4) {
        return blue_white_red_colormap(t);
    } else {
        return rainbow_colormap(t); // 默认彩虹
    }
}

void main()
{
    // 双面光照：处理背面法线方向
    vec3 norm = gl_FrontFacing ? normalize(v_normal) : -normalize(v_normal);
    
    // 确定基础颜色
    vec3 base_color;
    if (u_use_field_coloring) {
        // 使用场量着色
        float normalized_scalar = (v_scalar_fields - u_scalar_min) / (u_scalar_max - u_scalar_min);
        base_color = apply_colormap(normalized_scalar, u_color_scheme);
    } else {
        // 使用固定颜色
        base_color = u_object_color;
    }
    
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