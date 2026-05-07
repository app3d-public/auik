#pragma once

#include <acul/disposal_queue.hpp>
#include <acul/enum.hpp>
#include <acul/event.hpp>
#include <acul/functional/unique_function.hpp>
#include <acul/hash/hashmap.hpp>
#include <acul/hash/hashset.hpp>
#include <acul/string/string.hpp>
#include <amal/common.hpp>
#include <amal/vector.hpp>
#include "../pending_filter.hpp"
#include "atlas.hpp"
#include "events.hpp"
#include "fwd.hpp"
#include "gpu_context.hpp"
#include "pixel_snap.hpp"

struct FT_LibraryRec_;

#define AUIK_SYNC_CLIP_RECT                 0
#define AUIK_SYNC_HIT_RECT                  1
#define AUIK_PRIMARY_QUAD_STREAM            0
#define AUIK_PRIMARY_TEXTURED_QUADS_STREAM  1
#define AUIK_PRIMARY_VERTEX_STREAM          2
#define AUIK_PRIMARY_TEXTURED_VERTEX_STREAM 3
#define AUIK_PRIMARY_OVERLAY_QUADS_STREAM   4

namespace auik::v2
{
    struct PostEffect;

    struct DirtyFlagBits
    {
        enum enum_type
        {
            none = 0x0,
            redraw = 0x1,
            layout = 0x2,
            streams = 0x4,
            hit_rect_sync = 0x8,
            clip_rect = 0x10,
            host_update = 0x20,
            hover_update = 0x40,
            hit_rect_draw = 0x80,
            hit_rect_update = 0x100,
            textures = 0x200,
            delayed_tasks = 0x400
        };
        using flag_bitmask = std::true_type;
    };

    using DirtyFlags = acul::flags<DirtyFlagBits>;

    namespace detail
    {
        struct SharedBufferSyncState
        {
            u32 master_id = 0;
            u32 master_version = 0;
            u32 stage_version = 0;
            u32 invalidation_count = 0;
            u32 *buffer_versions = nullptr;
        };

        inline void construct_shared_buffer_sync_state(SharedBufferSyncState &state, u32 frame_count)
        {
            state.master_id = 0;
            state.master_version = 0;
            state.stage_version = 0;
            state.invalidation_count = 0;
            state.buffer_versions = acul::alloc_n<u32>(frame_count);
            for (u32 i = 0; i < frame_count; ++i) state.buffer_versions[i] = 0;
        }

        inline void destroy_shared_buffer_sync_state(SharedBufferSyncState &state)
        {
            acul::release(state.buffer_versions);
            state.buffer_versions = nullptr;
        }

        struct IO
        {
            amal::vec2 mouse_pos;
            amal::vec2 display_size;
            amal::vec2 last_click_pos{0.0f, 0.0f};
            amal::vec2 last_drag_pos{0.0f, 0.0f};
            f64 last_click_time = -1.0;
            u32 click_count = 0;
            u32 click_streak = 0;
            ElementID clicked_id{};
            ElementID drag_id{};
            bool mouse_down = false;
            acul::hashmap<u64, acul::unique_function<void()>> shortcuts;
            acul::hashmap<u32, acul::vector<u64>> widget_shortcuts;
            acul::hashset<Key> active_keys;
            acul::hashset<MouseKey> active_mouse_buttons;
            KeyMode active_mods = KeyModeBits::enum_type(0);
        };

        struct FrameCache
        {
            FrameChanges changes = FrameChangesBits::none;
            u32 drag_widget_id = 0;
            amal::vec2 drag_delta{0.0f, 0.0f};
            amal::vec2 scroll_delta{0.0f, 0.0f};
            u32 char_code = 0;
            u32 char_repeat_count = 0;
        };

        struct DelayedHostTask
        {
            u64 id = 0;
            u64 owner_id = 0;
            f64 due_time = 0.0;
            acul::unique_function<void()> fn = nullptr;
        };

        struct StyleSelectorTransition
        {
            ElementID prev_id{};
            ElementID current_id{};
            u8 prev_state = 0;
            u8 current_state = 0;
        };

        extern APPLIB_API struct Context
        {
            acul::events::dispatcher *ed = nullptr;
            acul::disposal_queue disposal_queue;
            acul::vector<Widget *> widget_tree;
            acul::vector<Widget *> transient_cache;
            acul::hashmap<u32, Widget *> id_map;
            acul::hashmap<u32, Image *> image_cache;
            Tooltip *tooltip = nullptr;
            acul::vector<TextureID> textures;
            acul::hashmap<u64, u32> texture_bind_slots;
            ElementID hover_id{};
            StyleSelectorTransition style_selector{};
            detail::HitboxZone hover_hitbox_zone = detail::HitboxZoneBits::none;
            u32 active_id = 0;
            u32 focus_id = 0;
            int root_depth_counts[3] = {};
            GPUContext *gpu_ctx = nullptr;
            WindowContext *window_ctx = nullptr;
            ::FT_LibraryRec_ *ft_library = nullptr;
            IO io;
            u32 frame_id = 0;
            u32 frames_in_flight = 0;
            u32 max_textures_size = 32;
            SharedBufferSyncState shared_sync_state[2];
            AtlasState atlas_state;
            amal::vec4 main_viewport{0.0f, 0.0f, 0.0f, 0.0f};
            DirtyFlags dirty_flags = DirtyFlagBits::none;
            Theme *theme = nullptr;
            bool *host_refresh_request = nullptr;
            PendingFilter *pending_filter = nullptr;
            FrameCache frame_cache;
            acul::vector<DelayedHostTask> delayed_tasks;
            u64 next_delayed_task_id = 1;
            f64 delayed_tasks_pause_time = -1.0;
            acul::vector<PostEffect *> post_effects;
            bool raw_mouse_mode = false;
            struct
            {
                DrawStream *attached_streams = nullptr;
                u32 stream_count = 0;
                DrawStream **default_streams = nullptr;
            } streams;
        } *g_context;

        inline Context &get_context()
        {
            assert(g_context && "auik context is not initialized");
            return *g_context;
        }

        inline SharedBufferSyncState &get_clip_rects_sync_state()
        {
            return get_context().shared_sync_state[AUIK_SYNC_CLIP_RECT];
        }
        inline SharedBufferSyncState &get_hit_rects_sync_state()
        {
            return get_context().shared_sync_state[AUIK_SYNC_HIT_RECT];
        }

        inline void mark_shared_buffer_mutation(SharedBufferSyncState &state, u32 frame_id, u32 frames_in_flight)
        {
            const bool already_mutating_same_frame = (state.master_id == frame_id) &&
                                                     (state.master_version != state.stage_version) &&
                                                     (state.buffer_versions[frame_id] == state.master_version);
            if (already_mutating_same_frame) return;
            // Version must be strictly monotonic across mutations.
            // Buffers can carry different content with the same version.
            state.master_version = amal::max(state.master_version, state.stage_version) + 1;
            state.master_id = frame_id;
            assert(state.buffer_versions);
            state.buffer_versions[frame_id] = state.master_version;
            state.invalidation_count = (frames_in_flight > 0) ? (frames_in_flight - 1) : 0;
        }

        inline void mark_clip_rects_mutation()
        {
            auto &ctx = get_context();
            mark_shared_buffer_mutation(get_clip_rects_sync_state(), ctx.frame_id, ctx.frames_in_flight);
            ctx.dirty_flags |= DirtyFlagBits::clip_rect;
        }

        inline void mark_hit_rects_mutation()
        {
            auto &ctx = get_context();
            mark_shared_buffer_mutation(get_hit_rects_sync_state(), ctx.frame_id, ctx.frames_in_flight);
            ctx.dirty_flags |= DirtyFlagBits::hit_rect_sync;
        }

#ifndef NDEBUG
        inline bool is_shared_buffer_frame_synced(const SharedBufferSyncState &state, u32 frame_id)
        {
            assert(state.buffer_versions);
            return state.buffer_versions[frame_id] == state.master_version;
        }

        inline bool is_hit_rects_frame_synced(u32 frame_id)
        {
            return is_shared_buffer_frame_synced(get_hit_rects_sync_state(), frame_id);
        }
#endif

        inline void mark_host_refresh_request()
        {
            auto &ctx = get_context();
            if (ctx.host_refresh_request) *ctx.host_refresh_request = true;
            ctx.dirty_flags |= DirtyFlagBits::host_update;
        }

        inline void mark_texture_bindings_mutation()
        {
            auto &ctx = get_context();
            ctx.dirty_flags |= DirtyFlagBits::textures | DirtyFlagBits::redraw;
        }

        WindowContext *create_window_context();

        inline WindowContext *get_window_context()
        {
            auto *ctx = get_context().window_ctx;
            assert(ctx && "auik window context is not initialized");
            return ctx;
        }

        inline IO &get_io() { return get_context().io; }

        inline const StyleSelectorTransition &get_style_selector_transition() { return get_context().style_selector; }

        inline ElementID get_prev_style_selector_id() { return get_style_selector_transition().prev_id; }

        inline ElementID get_style_selector_id() { return get_style_selector_transition().current_id; }

        inline StyleState get_prev_style_selector_state()
        {
            return static_cast<StyleState>(get_style_selector_transition().prev_state);
        }

        inline StyleState get_style_selector_state()
        {
            return static_cast<StyleState>(get_style_selector_transition().current_state);
        }

        inline bool set_style_selector(const ElementID &id, StyleState state)
        {
            auto &transition = get_context().style_selector;
            const u8 encoded_state = static_cast<u8>(state);
            if (transition.current_id.widget_id == id.widget_id && transition.current_id.tag_id == id.tag_id &&
                transition.current_id.element_id == id.element_id && transition.current_state == encoded_state)
                return false;
            transition.prev_id = transition.current_id;
            transition.prev_state = transition.current_state;
            transition.current_id = id;
            transition.current_state = encoded_state;
            return true;
        }

        inline void reset_style_selector(const ElementID &id = {}, StyleState state = static_cast<StyleState>(0))
        {
            auto &transition = get_context().style_selector;
            transition.prev_id = id;
            transition.current_id = id;
            transition.prev_state = static_cast<u8>(state);
            transition.current_state = static_cast<u8>(state);
        }

        struct WidgetStyleSelectorTransition
        {
            ElementID prev_id{};
            ElementID current_id{};
            StyleState prev_state = static_cast<StyleState>(0);
            StyleState current_state = static_cast<StyleState>(0);
        };

        inline WidgetStyleSelectorTransition get_widget_style_selector_transition(u32 widget_id)
        {
            const auto &transition = get_style_selector_transition();
            WidgetStyleSelectorTransition out{};
            if (transition.prev_id.widget_id == widget_id)
            {
                out.prev_id = transition.prev_id;
                out.prev_state = static_cast<StyleState>(transition.prev_state);
            }
            if (transition.current_id.widget_id == widget_id)
            {
                out.current_id = transition.current_id;
                out.current_state = static_cast<StyleState>(transition.current_state);
            }
            return out;
        }

        inline void clear_widget_pending_bits() {}
    } // namespace detail

    inline Theme *get_theme() { return detail::get_context().theme; }
    inline void set_theme(Theme *theme)
    {
        auto &ctx = detail::get_context();
        if (ctx.theme) ctx.dirty_flags |= DirtyFlagBits::layout;
        ctx.theme = theme;
    }

    inline DrawStream *get_primary_quads_stream()
    {
        auto *defaults = detail::get_context().streams.default_streams;
        assert(defaults && "Default streams are not initialized");
        return defaults[AUIK_PRIMARY_QUAD_STREAM];
    }
    inline void set_primary_quad_stream(DrawStream *stream)
    {
        auto *defaults = detail::get_context().streams.default_streams;
        assert(defaults && "Default streams are not initialized");
        defaults[AUIK_PRIMARY_QUAD_STREAM] = stream;
    }

    inline DrawStream *get_primary_textured_quads_stream()
    {
        auto *defaults = detail::get_context().streams.default_streams;
        assert(defaults && "Default streams are not initialized");
        return defaults[AUIK_PRIMARY_TEXTURED_QUADS_STREAM];
    }

    inline void set_primary_textured_quads_stream(DrawStream *stream)
    {
        auto *defaults = detail::get_context().streams.default_streams;
        assert(defaults && "Default streams are not initialized");
        defaults[AUIK_PRIMARY_TEXTURED_QUADS_STREAM] = stream;
    }

    inline DrawStream *get_overlay_quads_stream()
    {
        auto *defaults = detail::get_context().streams.default_streams;
        assert(defaults && "Default streams are not initialized");
        return defaults[AUIK_PRIMARY_OVERLAY_QUADS_STREAM];
    }

    inline void set_overlay_quads_stream(DrawStream *stream)
    {
        auto *defaults = detail::get_context().streams.default_streams;
        assert(defaults && "Default streams are not initialized");
        defaults[AUIK_PRIMARY_OVERLAY_QUADS_STREAM] = stream;
    }

    inline DrawStream *get_primary_vertex_stream()
    {
        auto *defaults = detail::get_context().streams.default_streams;
        assert(defaults && "Default streams are not initialized");
        return defaults[AUIK_PRIMARY_VERTEX_STREAM];
    }

    inline void set_primary_vertex_stream(DrawStream *stream)
    {
        auto *defaults = detail::get_context().streams.default_streams;
        assert(defaults && "Default streams are not initialized");
        defaults[AUIK_PRIMARY_VERTEX_STREAM] = stream;
    }

    inline DrawStream *get_primary_image_stream() { return get_primary_textured_quads_stream(); }
    inline void set_primary_image_stream(DrawStream *stream) { set_primary_textured_quads_stream(stream); }

    inline DrawStream *get_primary_textured_vertex_stream()
    {
        auto *defaults = detail::get_context().streams.default_streams;
        assert(defaults && "Default streams are not initialized");
        return defaults[AUIK_PRIMARY_TEXTURED_VERTEX_STREAM];
    }

    inline void set_primary_textured_vertex_stream(DrawStream *stream)
    {
        auto *defaults = detail::get_context().streams.default_streams;
        assert(defaults && "Default streams are not initialized");
        defaults[AUIK_PRIMARY_TEXTURED_VERTEX_STREAM] = stream;
    }

    inline u16 push_clip_rect(const amal::vec4 &rect)
    {
        auto *gpu = detail::get_context().gpu_ctx;
        assert(gpu && gpu->push_clip_rect && "GPU clip rect dispatch is not initialized");
        const u16 id = gpu->push_clip_rect(gpu, detail::snap_rect_to_pixel_grid(rect));
        detail::mark_clip_rects_mutation();
        return id;
    }

    inline void update_clip_rect(u16 clip_id, const amal::vec4 &rect)
    {
        auto *gpu = detail::get_context().gpu_ctx;
        assert(gpu && gpu->update_clip_rect && "GPU clip rect dispatch is not initialized");
        gpu->update_clip_rect(gpu, clip_id, detail::snap_rect_to_pixel_grid(rect));
        detail::mark_clip_rects_mutation();
    }

    inline void reset_gpu_clip_rects()
    {
        auto *gpu = detail::get_context().gpu_ctx;
        assert(gpu && gpu->reset_clip_rects && "GPU clip rect dispatch is not initialized");
        gpu->reset_clip_rects(gpu);
        detail::mark_clip_rects_mutation();
    }

    inline void clear_hit_rects()
    {
        auto *gpu = detail::get_context().gpu_ctx;
        assert(gpu && gpu->clear_hit_rects && "GPU hover rect dispatch is not initialized");
        gpu->clear_hit_rects(gpu);
        detail::mark_hit_rects_mutation();
    }

    inline amal::vec4 &get_clip_rect(u16 clip_id)
    {
        auto *gpu = detail::get_context().gpu_ctx;
        assert(gpu && gpu->get_clip_rect && "GPU clip rect dispatch is not initialized");
        auto *rect = gpu->get_clip_rect(gpu, clip_id);
        assert(rect && "Invalid clip rect id");
        return *rect;
    }

    inline const amal::vec2 &get_mouse_pos() { return detail::get_io().mouse_pos; }

    inline const amal::vec2 &get_display_size() { return detail::get_io().display_size; }

    inline u32 get_texture_bind_slot(u64 handle)
    {
        auto &ctx = detail::get_context();
        auto it = ctx.texture_bind_slots.find(handle);
        return (it != ctx.texture_bind_slots.end()) ? it->second : AUIK_INVALID_DRAW_DATA_ID;
    }
} // namespace auik::v2
