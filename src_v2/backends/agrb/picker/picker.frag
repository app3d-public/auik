#version 460

layout(location = 0) flat in uint in_widget_id;
layout(location = 1) flat in uint in_tag_id;
layout(location = 2) flat in uint in_clip_and_flags;
layout(location = 3) in vec2 in_pixel_pos;
layout(location = 4) in vec2 in_local_pos;
layout(location = 5) flat in vec2 in_rect_size;
layout(location = 0) out uvec2 out_id;

layout(std430, set = 0, binding = 1) readonly buffer ClipRectsBuffer { vec4 clip_rects[]; };

const float AUIK_HITBOX_PAD = 2.0;

float sd_box(vec2 p, vec2 half_size)
{
    vec2 d = abs(p) - half_size;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

void main()
{
    const uint clip_id = in_clip_and_flags & 0xFFFFu;
    const bool is_hitbox = (((in_clip_and_flags >> 16u) & 0x1u) != 0u);
    uvec2 out_value = uvec2(in_widget_id, in_tag_id);

    if (is_hitbox)
    {
        const vec2 half_size = in_rect_size * 0.5;
        const float dist = sd_box(in_local_pos, half_size);
        if (dist > AUIK_HITBOX_PAD) discard;
        if (abs(dist) <= AUIK_HITBOX_PAD) out_value.y = uint(0xBF9B2277u);
    }
    else
    {
        vec4 clip_rect = clip_rects[clip_id];
        vec2 clip_min = clip_rect.xy;
        vec2 clip_max = clip_rect.xy + clip_rect.zw;
        if (in_pixel_pos.x < clip_min.x || in_pixel_pos.y < clip_min.y || in_pixel_pos.x >= clip_max.x ||
            in_pixel_pos.y >= clip_max.y)
            discard;
    }

    out_id = out_value;
}
