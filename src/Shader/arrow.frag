#version 450 core

out vec4 frag_color;
in vec3 v_frag_pos;
in vec3 v_normal;
in float v_magnitude_normalized;

uniform vec3 u_light_pos;
uniform vec3 u_view_pos;
uniform int u_color_scheme;  // 0=彩虹, 1=热力图, 2=冷暖, 3=灰度, 4=蓝白红

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

// 热力图配色方案
vec3 heatmap_colormap(float t)
{
    t = clamp(t, 0.0, 1.0);
    vec3 color;
    
    if (t < 0.25) {
        color = vec3(t * 4.0, 0.0, 0.0);
    } else if (t < 0.5) {
        color = vec3(1.0, (t - 0.25) * 4.0, 0.0);
    } else if (t < 0.75) {
        color = vec3(1.0, 1.0, (t - 0.5) * 4.0);
    } else {
        color = vec3(1.0, 1.0, 1.0);
    }
    
    return color;
}

// 冷暖配色方案
vec3 cool_warm_colormap(float t)
{
    t = clamp(t, 0.0, 1.0);
    
    if (t < 0.5) {
        float s = t * 2.0;
        return mix(vec3(0.0, 0.0, 1.0), vec3(1.0, 1.0, 1.0), s);
    } else {
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

// 蓝白红配色方案
vec3 blue_white_red_colormap(float t)
{
    t = clamp(t, 0.0, 1.0);
    
    if (t < 0.5) {
        float s = t * 2.0;
        return mix(vec3(0.0, 0.0, 0.5), vec3(1.0, 1.0, 1.0), s);
    } else {
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
        return rainbow_colormap(t);
    }
}

void main()
{
    // 获取箭头颜色
    vec3 base_color = apply_colormap(v_magnitude_normalized, u_color_scheme);
    
    // 法线处理
    vec3 norm = normalize(v_normal);
    if (!gl_FrontFacing)
        norm = -norm;
    
    // Ambient光照
    float ambient_strength = 0.3;
    vec3 ambient = ambient_strength * base_color;
    
    // Diffuse光照
    vec3 light_dir = normalize(u_light_pos - v_frag_pos);
    float diff = max(dot(norm, light_dir), 0.0);
    vec3 diffuse = diff * base_color;
    
    // Specular高光
    float specular_strength = 0.5;
    vec3 view_dir = normalize(u_view_pos - v_frag_pos);
    vec3 reflect_dir = reflect(-light_dir, norm);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32.0);
    vec3 specular = specular_strength * spec * vec3(1.0);
    
    // 合成最终颜色
    vec3 result = ambient + diffuse + specular;
    frag_color = vec4(result, 1.0);
}
