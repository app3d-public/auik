#version 460

layout(location = 0) in vec2 in_local_pos;
layout(location = 1) flat in vec2 in_size;
layout(location = 2) flat in vec4 in_background_color;
layout(location = 3) flat in vec4 in_border_color;
layout(location = 4) flat in float in_border_radius;
layout(location = 5) flat in float in_border_thickness;
layout(location = 6) flat in uint in_corner_mask;

layout(location = 0) out vec4 out_color;

float get_corner_radius(vec2 p, float radius, uint corner_mask)
{
    // bit0: top-left, bit1: top-right, bit2: bottom-right, bit3: bottom-left
    vec2 s = step(0.0, p);
    vec4 corner_weights = vec4((1.0 - s.x) * (1.0 - s.y), // TL
                               s.x * (1.0 - s.y),         // TR
                               s.x * s.y,                 // BR
                               (1.0 - s.x) * s.y          // BL
    );
    const uvec4 shift = uvec4(0, 1, 2, 3);
    uvec4 bits = (uvec4(corner_mask) >> shift) & 1u;

    return radius * dot(corner_weights, vec4(bits));
}

float sd_rounded_rect(vec2 p, vec2 half_size, float radius)
{
    vec2 q = abs(p) - half_size + vec2(radius);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

void main()
{
    vec2 half_size = 0.5 * in_size;
    float radius = clamp(in_border_radius, 0.0, min(half_size.x, half_size.y));
    float corner_radius = get_corner_radius(in_local_pos, radius, in_corner_mask);

    float dist_outer = sd_rounded_rect(in_local_pos, half_size, corner_radius);
    float aa_outer = max(fwidth(dist_outer), 1e-4);
    float fill_outer = 1.0 - smoothstep(0.0, aa_outer, dist_outer);

    if (fill_outer <= 0.0) discard;

    float thickness = max(in_border_thickness, 0.0);
    vec4 color = in_background_color;

    if (thickness > 0.0)
    {
        vec2 inner_half = max(half_size - vec2(thickness), vec2(0.0));
        float inner_radius = max(corner_radius - thickness, 0.0);
        float dist_inner = sd_rounded_rect(in_local_pos, inner_half, inner_radius);
        float aa_inner = max(fwidth(dist_inner), 1e-4);
        float fill_inner = 1.0 - smoothstep(0.0, aa_inner, dist_inner);
        color = mix(in_border_color, in_background_color, fill_inner);
    }

    color.a *= fill_outer;
    out_color = color;
}
