#version 460

struct QuadsInstanceData
{
    vec2 position;
    vec2 size;
    vec4 background_color;
    vec4 border_color;
    float border_radius;
    float border_thickness;
    float z_order;
    uint mask;
};

layout(std430, set = 0, binding = 0) readonly buffer QuadsBuffer { QuadsInstanceData instances[]; };

layout(push_constant) uniform Push { uvec2 window_size; };

layout(location = 0) out vec2 out_local_pos;
layout(location = 1) flat out vec2 out_size;
layout(location = 2) flat out vec4 out_background_color;
layout(location = 3) flat out vec4 out_border_color;
layout(location = 4) flat out float out_border_radius;
layout(location = 5) flat out float out_border_thickness;
layout(location = 6) flat out uint out_corner_mask;
layout(location = 7) flat out uint out_flags;

vec2 get_quad_uv(uint vertex_index)
{
    const vec2 uv[6] =
        vec2[6](vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0));
    return uv[vertex_index];
}

void main()
{
    QuadsInstanceData instance = instances[gl_InstanceIndex];
    vec2 uv = get_quad_uv(uint(gl_VertexIndex));

    vec2 pixel_pos = instance.position + uv * instance.size;
    vec2 window = vec2(window_size);
    vec2 ndc = vec2((pixel_pos.x / window.x) * 2.0 - 1.0, 1.0 - (pixel_pos.y / window.y) * 2.0);

    gl_Position = vec4(ndc, instance.z_order, 1.0);

    out_local_pos = (uv - 0.5) * instance.size;
    out_size = instance.size;
    out_background_color = instance.background_color;
    out_border_color = instance.border_color;
    out_border_radius = instance.border_radius;
    out_border_thickness = instance.border_thickness;
    out_corner_mask = instance.mask & 0xFu;
    out_flags = instance.mask >> 20u;
}
