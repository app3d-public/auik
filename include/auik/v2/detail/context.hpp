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


struct FT_LibraryRec_;

#define AUIK_SYNC_CLIP_RECT       0
#define AUIK_SYNC_HIT_RECT        1
#define AUIK_PRIMARY_QUAD_STREAM  0
#define AUIK_PRIMARY_IMAGE_STREAM 1

namespace auik::v2
{
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
            textures = 0x200
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
            amal::vec2 screen_cursor{0.0f, 0.0f};
            DirtyFlags dirty_flags = DirtyFlagBits::none;
            Theme *theme = nullptr;
            bool *host_refresh_request = nullptr;
            PendingFilter *pending_filter = nullptr;
            FrameCache frame_cache;
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

        inline void clear_widget_pending_bits() {}
    } // namespace detail

    inline Theme *get_theme() { return detail::get_context().theme; }
    inline void set_theme(Theme *theme)
    {
        auto &ctx = detail::get_context();
        if (ctx.theme) ctx.dirty_flags |= DirtyFlagBits::layout;
        ctx.theme = theme;
    }

    inline DrawStream *get_primary_quad_stream()
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

    inline DrawStream *get_primary_image_stream()
    {
        auto *defaults = detail::get_context().streams.default_streams;
        assert(defaults && "Default streams are not initialized");
        return defaults[AUIK_PRIMARY_IMAGE_STREAM];
    }

    inline void set_primary_image_stream(DrawStream *stream)
    {
        auto *defaults = detail::get_context().streams.default_streams;
        assert(defaults && "Default streams are not initialized");
        defaults[AUIK_PRIMARY_IMAGE_STREAM] = stream;
    }

    inline u16 push_clip_rect(const amal::vec4 &rect)
    {
        auto *gpu = detail::get_context().gpu_ctx;
        assert(gpu && gpu->push_clip_rect && "GPU clip rect dispatch is not initialized");
        const u16 id = gpu->push_clip_rect(gpu, rect);
        detail::mark_clip_rects_mutation();
        return id;
    }

    inline void update_clip_rect(u16 clip_id, const amal::vec4 &rect)
    {
        auto *gpu = detail::get_context().gpu_ctx;
        assert(gpu && gpu->update_clip_rect && "GPU clip rect dispatch is not initialized");
        gpu->update_clip_rect(gpu, clip_id, rect);
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
