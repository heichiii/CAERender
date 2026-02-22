#version 450 core
// TODO: Implement 
out vec4 frag_color;
in vec3 v_frag_pos;
in vec3 v_normal;
// in float v_scalar_fields;

uniform vec3 u_light_pos;
uniform vec3 u_view_pos;
uniform vec3 u_object_color;
// uniform float u_scalar_min;
// uniform float u_scalar_max;

void main()
{
    // Ambient
    float ambient_strength = 0.1;
    vec3 ambient = ambient_strength * u_object_color;

    // Diffuse
    vec3 light_dir = normalize(u_light_pos - v_frag_pos);
    float diff = max(dot(v_normal, light_dir), 0.0);
    vec3 diffuse = diff * u_object_color;
    
    // Combine ambient and diffuse lighting
    vec3 result = ambient + diffuse;
    
    // Clamp the result to avoid overbrightening
    result = clamp(result, 0.0, 1.0);
    
    // Map scalar fields to a color range (blue to red gradient)
    float scalar_range = u_scalar_max - u_scalar_min;
    float normalized_scalar = clamp((v_scalar_fields - u_scalar_min) / scalar_range, 0.0, 1.0);
    
    // Color mapping: blue (low) -> green (mid) -> red (high)
    vec3 scalar_color;
    if (normalized_scalar < 0.5) {
        scalar_color = mix(vec3(0.0, 0.0, 1.0), vec3(0.0, 1.0, 0.0), normalized_scalar * 2.0);
    } else {
        scalar_color = mix(vec3(0.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0), (normalized_scalar - 0.5) * 2.0);
    }
    
    frag_color = vec4(result * scalar_color, 1.0);
}