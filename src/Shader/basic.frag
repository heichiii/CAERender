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
    vec3 norm = normalize(v_normal);
    vec3 light_dir = normalize(u_light_pos - v_frag_pos);
    float diff = max(dot(norm, light_dir), 0.0);
    vec3 diffuse = diff * u_object_color;
    // Specular
    float specular_strength = 0.5;
    vec3 view_dir = normalize(u_view_pos - v_frag_pos);
    vec3 reflect_dir = reflect(-light_dir, norm);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32);
    vec3 specular = specular_strength * spec * vec3(1.0); // 白色高光
    vec3 result = ambient + diffuse + specular;
    frag_color = vec4(result, 1.0);

    
}