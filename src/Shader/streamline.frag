#version 450 core

in VS_OUT {
    float magnitude;
} vs_in;

out vec4 FragColor;

uniform int u_color_scheme;
uniform float u_magnitude_min;
uniform float u_magnitude_max;

// 颜色映射函数（Rainbow/Jet方案）
vec3 colorRainbow(float t)
{
    // t: [0, 1]
    float r = 0.0;
    float g = 0.0;
    float b = 0.0;

    if (t < 0.25) {
        // 蓝 -> 青
        b = 1.0;
        g = t * 4.0;
    } else if (t < 0.5) {
        // 青 -> 绿
        b = 1.0 - (t - 0.25) * 4.0;
        g = 1.0;
    } else if (t < 0.75) {
        // 绿 -> 黄
        r = (t - 0.5) * 4.0;
        g = 1.0;
    } else {
        // 黄 -> 红
        r = 1.0;
        g = 1.0 - (t - 0.75) * 4.0;
    }

    return vec3(r, g, b);
}

// 热力图方案
vec3 colorHeatmap(float t)
{
    // t: [0, 1]
    float r = 0.0;
    float g = 0.0;
    float b = 0.0;

    if (t < 0.33) {
        // 蓝
        b = 1.0 - t * 3.0;
        g = t * 3.0;
    } else if (t < 0.67) {
        // 绿 -> 黄
        r = (t - 0.33) * 3.0;
        g = 1.0;
    } else {
        // 黄 -> 红
        r = 1.0;
        g = 1.0 - (t - 0.67) * 3.0;
    }

    return vec3(r, g, b);
}

// 冷暖配色
vec3 colorCoolWarm(float t)
{
    // t: [0, 1]  0=冷(蓝) -> 1=暖(红)
    float r = t;
    float g = 1.0 - abs(t - 0.5) * 2.0;
    float b = 1.0 - t;
    return vec3(r, g, b);
}

// 蓝白红配色
vec3 colorBlueWhiteRed(float t)
{
    // t: [0, 1]  0=蓝 -> 0.5=白 -> 1=红
    if (t < 0.5) {
        float s = t * 2.0;  // [0, 1]
        return vec3(s, s, 1.0);  // 蓝->白
    } else {
        float s = (t - 0.5) * 2.0;  // [0, 1]
        return vec3(1.0, 1.0 - s, 1.0 - s);  // 白->红
    }
}

// 灰度方案
vec3 colorGrayscale(float t)
{
    return vec3(t, t, t);
}

// 应用选定的配色方案
vec3 applyColorScheme(float t, int scheme)
{
    switch (scheme) {
        case 0:  // RAINBOW
            return colorRainbow(t);
        case 1:  // HEATMAP
            return colorHeatmap(t);
        case 2:  // COOL_WARM
            return colorCoolWarm(t);
        case 3:  // GRAYSCALE
            return colorGrayscale(t);
        case 4:  // BLUE_WHITE_RED
            return colorBlueWhiteRed(t);
        default:
            return colorRainbow(t);
    }
}

void main()
{
    // 归一化幅值到[0, 1]
    float normalized = (vs_in.magnitude - u_magnitude_min) / 
                       (u_magnitude_max - u_magnitude_min + 1e-6);
    normalized = clamp(normalized, 0.0, 1.0);

    // 应用配色方案
    vec3 color = applyColorScheme(normalized, u_color_scheme);

    FragColor = vec4(color, 1.0);
}
