#pragma once

#include <acul/comparator.hpp>
#include <acul/disposal_queue.hpp>
#include <acul/functional/unique_function.hpp>
#include "detail/context.hpp"
#include "detail/events.hpp"
#include "draw.hpp"
#include "pending_filter.hpp"
#include "widgets/widget.hpp"

struct FT_LibraryRec_;
struct FT_FaceRec_;

#define AUIK_ICON_CHEVRON_RIGHT 0x87DCB881u
#define AUIK_ICON_CHEVRON_DOWN  0xE5275EADu
#define AUIK_ICON_CHEVRON_UP    0x9DE2813Bu
#define AUIK_ICON_CHECKMARK     0x0E3F8C25u
#define AUIK_ICON_CLOSE         0xBF822112u
#define AUIK_ICON_MENU          0xDDD65C07u
#define AUIK_HITBOX_PAD         4.0f

namespace auik
{
    namespace detail
    {
        struct TextFontAccess;
        AUIK_EXPORT u64 schedule_delayed_task_fn(u64 owner_id, f64 due_time, acul::unique_function<void()> fn);
    } // namespace detail

    using FT_Library = ::FT_LibraryRec_ *;
    using FT_Face = ::FT_FaceRec_ *;

    struct SyncOptions
    {
        PendingFilter *pending_filter = nullptr;
        bool *host_refresh_request = nullptr;

        SyncOptions &set_pending_filter(PendingFilter *value)
        {
            pending_filter = value;
            return *this;
        }

        SyncOptions &set_host_refresh_request(bool *value)
        {
            host_refresh_request = value;
            return *this;
        }
    };

    struct CreateInfo
    {
        DrawStream *streams = nullptr;
        u32 streams_count = 0;
        detail::GPUContext *gpu_ctx = nullptr;
        detail::WindowContext *window_ctx = nullptr;
        SoundContext *sound_ctx = nullptr;
        u32 frames_in_flight = 0;
        u32 max_textures_size = 32;
        SyncOptions sync_options{};
        WidgetCreateOptions widget_create_options{};

        CreateInfo &set_gpu_backend(detail::GPUContext *gpu_backend)
        {
            this->gpu_ctx = gpu_backend;
            return *this;
        }

        CreateInfo &set_draw_streams(DrawStream *streams, u32 streams_count)
        {
            this->streams = streams;
            this->streams_count = streams_count;
            return *this;
        }

        CreateInfo &set_window_backend(detail::WindowContext *window_backend)
        {
            this->window_ctx = window_backend;
            return *this;
        }

        CreateInfo &set_sound_backend(SoundContext *sound_backend)
        {
            this->sound_ctx = sound_backend;
            return *this;
        }

        CreateInfo &set_frames_in_flight(u32 frames_in_flight)
        {
            this->frames_in_flight = frames_in_flight;
            return *this;
        }

        CreateInfo &set_max_textures_size(u32 max_textures_size)
        {
            this->max_textures_size = max_textures_size;
            return *this;
        }

        CreateInfo &set_sync_options(const SyncOptions &value)
        {
            sync_options = value;
            return *this;
        }

        CreateInfo &set_widget_create_options(const WidgetCreateOptions &value)
        {
            widget_create_options = value;
            return *this;
        }
    };

    AUIK_EXPORT bool init_library(const CreateInfo &create_info);
    AUIK_EXPORT void destroy_library();
    AUIK_EXPORT void record_layout_commands();
    AUIK_EXPORT void redraw_all_commands();
    AUIK_EXPORT void rebuild_root_widget_depths();
    AUIK_EXPORT void add_widget_to_root(Widget *widget, DepthZone zone = DepthZone::work);
    AUIK_EXPORT void mark_locale_changed();
    AUIK_EXPORT bool remove_widget_from_root_unsync(Widget *widget);
    AUIK_EXPORT bool remove_widget_from_root_unsync(u32 id);
    AUIK_EXPORT bool remove_widget(Widget *widget);
    AUIK_EXPORT bool remove_widget(u32 id);
    AUIK_EXPORT bool hide_widget(u32 id);
    AUIK_EXPORT bool show_widget(u32 id);
    AUIK_EXPORT void focus_widget(Widget *widget);
    AUIK_EXPORT void push_widget_to_transient_cache(Widget *widget);
    AUIK_EXPORT bool erase_widget_from_transient_cache(Widget *widget);
    AUIK_EXPORT void cancel_all_delayed_tasks();
    AUIK_EXPORT void cancel_delayed_tasks(u64 owner_id);
    AUIK_EXPORT void pause_delayed_tasks(f64 now);
    AUIK_EXPORT void resume_delayed_tasks(f64 now);
    AUIK_EXPORT f64 next_delayed_task_in(f64 now);
    AUIK_EXPORT bool dispatch_delayed_tasks(f64 now);
    AUIK_EXPORT void show_tooltip(f32 x, const acul::string *text_source);
    AUIK_EXPORT void hide_tooltip();
    AUIK_EXPORT void clear_tooltip_if_source(const acul::string *text_source);
    AUIK_EXPORT void sync_draw_streams();
    AUIK_EXPORT void sync_clip_rect_cache();
    AUIK_EXPORT void sync_hit_rect_cache();
    AUIK_EXPORT Viewport *make_viewport();
    AUIK_EXPORT ViewportGroup *make_viewport_group();
    AUIK_EXPORT void add_viewport(Viewport *viewport);
    AUIK_EXPORT bool destroy_viewport(Viewport *viewport);
    AUIK_EXPORT bool destroy_viewport_group(ViewportGroup *viewport);
    AUIK_EXPORT void set_main_viewport(Viewport *viewport);
    AUIK_EXPORT void sync_viewport(Viewport *viewport);
    AUIK_EXPORT void update_root_widgets_layout(Viewport *viewport);
    inline void sync_gpu_cache();
    template <class Traits, class F>
    inline bool add_render_command(Widget *widget, F &&fn);
    template <class F>
    inline bool add_render_command(F &&fn);
    template <class F>
    inline u64 schedule_delayed_host_task(u64 owner_id, f64 due_time, F &&fn);

    inline void set_window_size(const amal::vec2 &size) { detail::get_io().display_size = size; }

    inline Widget *get_widget_by_id(u32 id)
    {
        auto &ctx = detail::get_context();
        auto it = ctx.id_map.find(id);
        return it != ctx.id_map.end() ? it->second : nullptr;
    }

    inline void set_main_viewport(const amal::vec4 &viewport)
    {
        auto *main_viewport = detail::get_context().main_viewport;
        assert(main_viewport && "main viewport is not set");
        main_viewport->rect = {amal::vec2{viewport.x, viewport.y}, amal::vec2{viewport.z, viewport.w}};
        sync_viewport(main_viewport);
    }

    inline void sync_pending_events()
    {
        auto &ctx = detail::get_context();
        auto *pf = ctx.pending_filter;
        if (!pf || !pf->allow()) return;
        if (pf->has(PendingMaskBits::resize)) detail::mark_layout_dirty();
        if (pf->has(PendingMaskBits::mouse_move)) detail::on_mouse_move({0, 0});
    }

    inline void sync_frame()
    {
        auto &ctx = detail::get_context();
        if (ctx.pending_filter && ctx.pending_filter->mask != PendingMaskBits::none) sync_pending_events();
        if (ctx.dirty_flags & DirtyFlagBits::hit_rect_sync) auik::sync_hit_rect_cache();
        detail::flush_frame_changes();
        if (!ctx.disposal_queue.is_main_queue_empty()) ctx.disposal_queue.flush_main_queue();
        if (!ctx.transient_cache.empty() && !(ctx.dirty_flags & detail::layout_update_dirty_mask))
        {
            ctx.dirty_flags |= DirtyFlagBits::redraw;
            for (Widget *widget : ctx.transient_cache)
            {
                if (!widget) continue;
                widget->update_draw_commands(DrawReasonBits::transient);
            }
        }
    }

    inline void next_frame(void *sync_ctx)
    {
        auto &ctx = detail::get_context();
        detail::update_hover_id(ctx.gpu_ctx, sync_ctx);
        if (ctx.gpu_ctx && ctx.gpu_ctx->clear_clip_rects_reallocated)
            ctx.gpu_ctx->clear_clip_rects_reallocated(ctx.gpu_ctx, ctx.frame_id);
        detail::new_window_frame(ctx.window_ctx);
        ctx.frame_id = (ctx.frame_id + 1) % ctx.frames_in_flight;
        detail::clear_dirty_flags(detail::one_frame_dirty_mask);
    }

    inline void sync_gpu_cache()
    {
        auto &ctx = detail::get_context();
        if (ctx.dirty_flags & detail::layout_dirty_mask) record_layout_commands();
        else if (ctx.dirty_flags & DirtyFlagBits::clip_rect) sync_clip_rect_cache();
        if (ctx.dirty_flags & DirtyFlagBits::streams) sync_draw_streams();
    }

    inline bool is_dirty_render()
    {
        return detail::get_context().dirty_flags &
               (DirtyFlagBits::redraw | DirtyFlagBits::layout | DirtyFlagBits::fast_update);
    }

    inline bool is_dirty_layout() { return detail::get_context().dirty_flags & detail::layout_update_dirty_mask; }

    inline bool is_dirty_stream() { return detail::get_context().dirty_flags & DirtyFlagBits::streams; }

    inline bool is_dirty_hit_rect() { return detail::get_context().dirty_flags & DirtyFlagBits::hit_rect_sync; }

    inline bool is_dirty_clip_rect() { return detail::get_context().dirty_flags & DirtyFlagBits::clip_rect; }

    inline bool has_delayed_tasks() { return detail::get_context().dirty_flags & DirtyFlagBits::delayed_tasks; }

    inline bool is_host_update_pending() { return detail::get_context().dirty_flags & DirtyFlagBits::host_update; }

    inline void set_raw_mouse_mode(bool value) { detail::get_context().raw_mouse_mode = value; }

    inline bool is_raw_mouse_mode() { return detail::get_context().raw_mouse_mode; }

    inline HostWindowState get_host_window_state() { return detail::get_window_context()->host_state; }

    inline f64 get_max_animation_delay()
    {
        auto *pf = detail::get_context().pending_filter;
        return pf ? pf->get_frame_rate() : 1.0 / 60.0;
    }

    template <class F>
    inline void register_shortcut(const Shortcut &shortcut, F &&fn)
    {
        auto &ctx = detail::get_context();
        const u64 shortcut_hash = detail::make_shortcut_hash(shortcut.keys, shortcut.mouse, shortcut.mods, 0);
        ctx.io.shortcuts[shortcut_hash] = acul::unique_function<void()>(std::forward<F>(fn));
    }

    template <class F>
    inline void register_shortcut(u32 widget_id, const Shortcut &shortcut, F &&fn)
    {
        auto &ctx = detail::get_context();
        const u64 shortcut_hash = detail::make_shortcut_hash(shortcut.keys, shortcut.mouse, shortcut.mods, widget_id);
        ctx.io.shortcuts[shortcut_hash] = acul::unique_function<void()>(std::forward<F>(fn));
        auto &hashes = ctx.io.widget_shortcuts[widget_id];
        bool exists = false;
        for (u64 hash : hashes)
        {
            if (hash != shortcut_hash) continue;
            exists = true;
            break;
        }
        if (!exists) hashes.push_back(shortcut_hash);
        auto it = ctx.id_map.find(widget_id);
        if (it != ctx.id_map.end() && it->second) it->second->add_event_flags(EventFlagBits::shortcut);
    }

    inline void deregister_shortcut(const Shortcut &shortcut)
    {
        auto &ctx = detail::get_context();
        const u64 shortcut_hash = detail::make_shortcut_hash(shortcut.keys, shortcut.mouse, shortcut.mods, 0);
        ctx.io.shortcuts.erase(shortcut_hash);
    }

    inline void deregister_shortcut(u32 widget_id, const Shortcut &shortcut)
    {
        auto &ctx = detail::get_context();
        const u64 shortcut_hash = detail::make_shortcut_hash(shortcut.keys, shortcut.mouse, shortcut.mods, widget_id);
        ctx.io.shortcuts.erase(shortcut_hash);

        auto hashes_it = ctx.io.widget_shortcuts.find(widget_id);
        if (hashes_it != ctx.io.widget_shortcuts.end())
        {
            auto &hashes = hashes_it->second;
            for (size_t i = 0; i < hashes.size(); ++i)
            {
                if (hashes[i] != shortcut_hash) continue;
                hashes.erase(hashes.begin() + i);
                break;
            }
            if (hashes.empty()) ctx.io.widget_shortcuts.erase(hashes_it);
        }

        auto it = ctx.id_map.find(widget_id);
        if (it != ctx.id_map.end() && it->second &&
            ctx.io.widget_shortcuts.find(widget_id) == ctx.io.widget_shortcuts.end())
            it->second->remove_event_flags(EventFlagBits::shortcut);
    }

    inline void deregister_shortcuts(u32 widget_id) { detail::deregister_widget_shortcuts(widget_id); }

    template <class Traits, class F>
    inline bool add_render_command(Widget *widget, F &&fn)
    {
        if constexpr (std::is_same_v<typename Traits::category, detail::immediate_event_traits_tag>)
        {
            fn();
            return true;
        }

        assert(widget && "widget is null");
        auto &ctx = detail::get_context();
        if (ctx.dirty_flags & DirtyFlagBits::destroying) return false;
        ctx.disposal_queue.emplace(std::forward<F>(fn));
        detail::mark_host_refresh_request();
        return true;
    }

    template <class F>
    inline bool add_render_command(F &&fn)
    {
        auto &ctx = detail::get_context();
        if (ctx.dirty_flags & DirtyFlagBits::destroying) return false;
        ctx.disposal_queue.emplace(std::forward<F>(fn));
        detail::mark_host_refresh_request();
        return true;
    }

    template <class F>
    inline u64 schedule_delayed_host_task(u64 owner_id, f64 due_time, F &&fn)
    {
        return detail::schedule_delayed_task_fn(owner_id, due_time, acul::unique_function<void()>(std::forward<F>(fn)));
    }

    AUIK_EXPORT u32 get_service_pipelines_count();
    AUIK_EXPORT u32 get_default_streams_pipelines_count();
    AUIK_EXPORT u32 get_default_streams_count();

    struct FontLoadFlagBits
    {
        enum enum_type : u32
        {
            none = 0x0,
            no_scale = (1u << 0),
            no_hinting = (1u << 1),
            render = (1u << 2),
            no_bitmap = (1u << 3),
            vertical_layout = (1u << 4),
            force_autohint = (1u << 5),
            crop_bitmap = (1u << 6),
            pedantic = (1u << 7),
            advance_only = (1u << 8),
            ignore_global_advance_width = (1u << 9),
            no_recurse = (1u << 10),
            ignore_transform = (1u << 11),
            monochrome = (1u << 12),
            linear_design = (1u << 13),
            sbits_only = (1u << 14),
            no_autohint = (1u << 15),
            target_normal = (0u << 16),
            target_light = (1u << 16),
            target_mono = (2u << 16),
            target_lcd = (3u << 16),
            target_lcd_v = (4u << 16),
            color = (1u << 20),
            compute_metrics = (1u << 21),
            bitmap_metrics_only = (1u << 22),
            svg_only = (1u << 23),
            no_svg = (1u << 24)
        };
        using flag_bitmask = std::true_type;
    };

    using FontLoadFlags = acul::flags<FontLoadFlagBits>;

    enum class FontRenderMode : u32
    {
        normal = 0,
        light = 1,
        mono = 2,
        lcd = 3,
        lcd_v = 4,
        sdf = 5
    };

    struct FontInfo
    {
        acul::string path = "";
        acul::string fullname = "";
        int weight = 0;
        int slant = 0;
    };

    struct Glyph
    {
        u32 atlas_id = AUIK_INVALID_DRAW_DATA_ID;
        TextureID texture_id = AUIK_INVALID_TEXTURE_ID;
        amal::ivec2 size{0, 0};
        amal::ivec2 offset{0, 0};
        amal::irect pixel_rect{};
        amal::rect uv_rect{};
        f32 advance_x = 0.0f;
        u32 colored : 1;
        u32 empty : 1;
        u32 codepoint : 30;

        Glyph() : colored(0), empty(0), codepoint(0) {}

        bool visible() const { return !empty && !amal::is_rect_empty(pixel_rect) && texture_id.handle != 0; }
    };

    using GlyphCache = acul::hashmap<u32, Glyph>;

    using FontRegistry = acul::case_insensitive_map<acul::string, FontInfo>;

    inline FontInfo *get_font_info_by_family(const FontRegistry &fonts, const acul::string &family,
                                             const acul::string &fullname)
    {
        auto it = fonts[family];
        if (it != fonts.cend())
        {
            auto &font_list = it->second;
            auto found = std::find_if(font_list.begin(), font_list.end(),
                                      [&fullname](const FontInfo &font) { return font.fullname == fullname; });
            if (found != font_list.end()) return const_cast<FontInfo *>(&(*found));
            return const_cast<FontInfo *>(&(*font_list.begin()));
        }
        return nullptr;
    }

    inline FontInfo *get_font_info_by_family(const FontRegistry &fonts, const acul::string &family)
    {
        return get_font_info_by_family(fonts, family, family);
    }

    class Font
    {
    public:
        Font() = default;
        AUIK_EXPORT explicit Font(const FontInfo &info, int face_index = 0);
        AUIK_EXPORT explicit Font(const acul::string &path, int face_index = 0);
        AUIK_EXPORT ~Font();

        Font(const Font &) = delete;
        Font &operator=(const Font &) = delete;

        Font(Font &&other) noexcept;
        Font &operator=(Font &&other) noexcept;

        AUIK_EXPORT bool load(const FontInfo &info, int face_index = 0);
        AUIK_EXPORT bool load(const acul::string &path, int face_index = 0);
        AUIK_EXPORT void clear();

        AUIK_EXPORT void set_load_flags(FontLoadFlags flags);
        AUIK_EXPORT void add_load_flags(FontLoadFlags flags);
        AUIK_EXPORT void remove_load_flags(FontLoadFlags flags);
        FontLoadFlags load_flags() const { return _load_flags; }
        AUIK_EXPORT void set_render_mode(FontRenderMode mode);
        FontRenderMode render_mode() const { return _render_mode; }
        AUIK_EXPORT bool load_glyph(u32 size_px, u32 codepoint);
        AUIK_EXPORT bool load_glyphs(u32 size_px, const acul::vector<u32> &codepoints);
        AUIK_EXPORT bool load_glyphs(u32 size_px, const acul::string &utf8_text);

        bool is_loaded() const { return _face != nullptr; }
        int face_index() const { return _face_index; }
        size_t glyph_count() const;
        const FontInfo &info() const { return _info; }
        const acul::string &path() const { return _info.path; }
        const acul::string &fullname() const { return _info.fullname; }
        int weight() const { return _info.weight; }
        int slant() const { return _info.slant; }
        FT_Face native_handle() const { return _face; }
        Glyph *find_glyph(u32 size_px, u32 codepoint);
        const Glyph *find_glyph(u32 size_px, u32 codepoint) const;

    private:
        friend struct detail::TextFontAccess;

        bool ensure_size_px(u32 size_px);
        GlyphCache *find_cache(u32 size_px);
        const GlyphCache *find_cache(u32 size_px) const;
        GlyphCache &ensure_cache(u32 size_px);

        FontInfo _info;
        FT_Face _face = nullptr;
        int _face_index = 0;
        FontLoadFlags _load_flags = FontLoadFlagBits::none;
        FontRenderMode _render_mode = FontRenderMode::normal;
        u32 _active_size_px = 0;
        acul::hashmap<u32, GlyphCache> _glyphs;
    };

    AUIK_EXPORT bool load_fonts(FontRegistry &fonts, const acul::vector<acul::string> &search_dirs = {});

    inline bool load_font(const FontRegistry &fonts, Font &dst, const acul::string &family,
                          const acul::string &fullname)
    {
        FontInfo *font_info = get_font_info_by_family(fonts, family, fullname);
        if (!font_info) return false;
        return dst.load(*font_info);
    }

    inline bool load_font(const FontRegistry &fonts, Font &dst, const acul::string &family)
    {
        return load_font(fonts, dst, family, family);
    }

    inline f32 pt_to_px(f32 pt, f32 dpi)
    {
        constexpr f32 pixels_per_pt = 96.0f / 72.0f;
        return pt * dpi * pixels_per_pt;
    }

    inline u32 round_font_px(f32 size) { return static_cast<u32>(size + 0.5f); }

    struct FontIconGlyphLoader
    {
        u32 size = 0;
        acul::vector<u32> codepoints;
        acul::vector<u32> ids;
        FontLoadFlags load_flags = FontLoadFlagBits::none;
        FontRenderMode render_mode = FontRenderMode::normal;
        const FontIconGlyphLoader *next = nullptr;
    };

    AUIK_EXPORT bool load_material_icons(const FontRegistry &fonts, f32 dpi = 1.0f,
                                         const FontIconGlyphLoader *next = nullptr);

#ifdef _WIN32
    AUIK_EXPORT bool load_win32_icons(const FontRegistry &fonts, f32 dpi = 1.0f,
                                      const FontIconGlyphLoader *next = nullptr);
#endif
} // namespace auik
