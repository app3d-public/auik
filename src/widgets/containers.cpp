#include <auik/auik.hpp>
#include <auik/detail/depth.hpp>
#include <auik/detail/rect.hpp>
#include <auik/pipelines.hpp>
#include <auik/widgets/containers.hpp>
#include <auik/widgets/detail/draw_cull.hpp>
#include "../core/session_stream_utils.hpp"

namespace auik
{
    static amal::vec4 zero_vec4() { return {0.0f, 0.0f, 0.0f, 0.0f}; }

    static void activate_scrollbar_thumb_style(Widget *owner, u32 thumb_tag_id)
    {
        if (!owner) return;
        const auto thumb_id = make_element_id(owner->id(), thumb_tag_id);
        if (!detail::set_style_selector(thumb_id, StyleState::active)) return;
        const auto style_flags = owner->update_style_invalidated();
        if (style_flags & StyleUpdateFlagBits::redraw)
            owner->update_draw_commands(get_draw_reason_from_style_update(style_flags));
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
    }

    static inline void release_scrollbar(detail::Scrollbar *&scrollbar)
    {
        if (!scrollbar) return;
        acul::release(scrollbar);
        scrollbar = nullptr;
    }

    static void ensure_styled_scrollbar(detail::Scrollbar *&scrollbar, Widget *parent, amal::axis axis,
                                        u32 track_style_tag, u32 thumb_style_tag)
    {
        if (scrollbar) return;
        const u32 id = axis == amal::axis::x ? AUIK_ID_SCROLLBAR_X : AUIK_ID_SCROLLBAR_Y;
        const u32 track_tag = axis == amal::axis::x ? AUIK_TAG_SCROLLBAR_TRACK_X : AUIK_TAG_SCROLLBAR_TRACK_Y;
        const u32 thumb_tag = axis == amal::axis::x ? AUIK_TAG_SCROLLBAR_THUMB_X : AUIK_TAG_SCROLLBAR_THUMB_Y;
        scrollbar = acul::alloc<detail::Scrollbar>(id, track_tag, thumb_tag, parent, axis);
        scrollbar->set_track_style_tag(track_style_tag);
        scrollbar->set_thumb_style_tag(thumb_style_tag);
        scrollbar->unset_visible();
    }

    static inline u32 scrollbar_thumb_style_for_track(u32 track_style_tag)
    {
        if (track_style_tag == AUIK_STYLE_TAG_SCROLLBAR_TRACK_INTERNAL) return AUIK_STYLE_TAG_SCROLLBAR_THUMB_INTERNAL;
        return AUIK_STYLE_TAG_SCROLLBAR_THUMB;
    }

    static inline u32 depth_requirement_of(Widget *widget)
    {
        return widget ? amal::max(widget->get_depth_requirement(), 1u) : 0u;
    }

    namespace detail
    {
        static inline bool is_inline_layout(ChildLayoutFlags flags) { return flags & ChildLayoutFlagBits::linline; }
        static inline bool is_right_layout(ChildLayoutFlags flags) { return flags & ChildLayoutFlagBits::aright; }
        static inline bool is_hcenter_layout(ChildLayoutFlags flags) { return flags & ChildLayoutFlagBits::hcenter; }
        static inline bool is_top_layout(ChildLayoutFlags flags) { return flags & ChildLayoutFlagBits::top; }
        static inline bool is_center_layout(ChildLayoutFlags flags) { return flags & ChildLayoutFlagBits::vcenter; }
        static inline bool is_bottom_layout(ChildLayoutFlags flags) { return flags & ChildLayoutFlagBits::bottom; }
        struct ResolvedChildLayoutSize
        {
            amal::vec2 layout_size{0.0f, 0.0f};
            amal::vec2 occupied_size{0.0f, 0.0f};
        };

        static inline ResolvedChildLayoutSize resolve_child_layout_size(Widget *child, const amal::vec2 &fallback_size,
                                                                        f32 available_width, f32 available_height,
                                                                        f32 fill_available_width)
        {
            if (!child) return {};
            const f32 safe_available = amal::max(available_width, 0.0f);
            const f32 safe_available_height = amal::max(available_height, 0.0f);
            const f32 safe_fill_available = amal::max(fill_available_width, 0.0f);
            const f32 requested_width = amal::max(fallback_size.x, 0.0f);
            const f32 occupied_width = child->fill_width()       ? amal::min(safe_fill_available, safe_available)
                                       : child->is_width_fixed() ? requested_width
                                                                 : amal::min(requested_width, safe_available);
            const f32 occupied_height = child->fill_height() ? safe_available_height : amal::max(fallback_size.y, 0.0f);
            return {{occupied_width, occupied_height}, {occupied_width, occupied_height}};
        }

        amal::vec2 compute_children_layout_required_size(const acul::vector<Widget *> &children,
                                                         const acul::vector<ChildLayoutFlags> &layouts,
                                                         f32 inline_spacing_x, f32 wrap_width, bool refresh_min_size)
        {
            f32 max_width = 0.0f;
            f32 total_height = 0.0f;
            f32 row_width = 0.0f;
            f32 row_height = 0.0f;
            const bool wrap_enabled = wrap_width > 0.0f;
            for (size_t i = 0; i < children.size(); ++i)
            {
                auto *child = children[i];
                if (!child) continue;
                if (!child->is_visible()) continue;
                if (refresh_min_size) child->update_layout_min_size();
                const ChildLayoutFlags layout = i < layouts.size() ? layouts[i] : ChildLayoutFlagBits::none;
                const amal::vec2 req = child->required_size();
                if (is_inline_layout(layout))
                {
                    const f32 gap_before = row_width > 0.0f ? inline_spacing_x : 0.0f;
                    if (wrap_enabled && row_height > 0.0f && row_width + gap_before + req.x > wrap_width)
                    {
                        max_width = amal::max(max_width, row_width);
                        total_height += row_height;
                        row_width = 0.0f;
                        row_height = 0.0f;
                    }
                    if (wrap_enabled && req.x > wrap_width)
                    {
                        if (row_height > 0.0f)
                        {
                            max_width = amal::max(max_width, row_width);
                            total_height += row_height;
                            row_width = 0.0f;
                            row_height = 0.0f;
                        }
                        max_width = amal::max(max_width, req.x);
                        total_height += req.y;
                        continue;
                    }
                    row_width += gap_before + req.x;
                    row_height = amal::max(row_height, req.y);
                    continue;
                }

                if (row_height > 0.0f)
                {
                    const f32 gap_before = inline_spacing_x;
                    if (!wrap_enabled || row_width + gap_before + req.x <= wrap_width)
                    {
                        row_width += gap_before + req.x;
                        row_height = amal::max(row_height, req.y);
                        max_width = amal::max(max_width, row_width);
                        total_height += row_height;
                        row_width = 0.0f;
                        row_height = 0.0f;
                        continue;
                    }

                    max_width = amal::max(max_width, row_width);
                    total_height += row_height;
                    row_width = 0.0f;
                    row_height = 0.0f;
                }
                const f32 child_width = wrap_enabled && child->fill_width() ? amal::max(wrap_width, req.x) : req.x;
                max_width = amal::max(max_width, child_width);
                total_height += req.y;
            }
            if (row_height > 0.0f)
            {
                max_width = amal::max(max_width, row_width);
                total_height += row_height;
            }
            return {max_width, total_height};
        }

        static void align_inline_row_vertical(const acul::vector<Widget *> &children,
                                              const acul::vector<ChildLayoutFlags> &layouts, size_t row_start,
                                              size_t row_end, f32 row_height)
        {
            for (size_t row_i = row_start; row_i < row_end; ++row_i)
            {
                auto *row_child = children[row_i];
                if (!row_child) continue;
                if (!row_child->is_visible()) continue;
                const ChildLayoutFlags layout = row_i < layouts.size() ? layouts[row_i] : ChildLayoutFlagBits::none;
                if (!is_top_layout(layout) && !is_center_layout(layout) && !is_bottom_layout(layout)) continue;
                const f32 child_h = amal::max(amal::max(row_child->required_size().y, row_child->size().y), 0.0f);
                f32 delta_y = 0.0f;
                if (is_center_layout(layout)) delta_y = amal::floor((row_height - child_h) * 0.5f);
                else if (is_bottom_layout(layout)) delta_y = amal::max(row_height - child_h, 0.0f);
                if (delta_y <= 0.0f) continue;
                row_child->translate({0.0f, delta_y});
            }
        }

        static f32 inline_row_overflow(const acul::vector<Widget *> &children,
                                       const acul::vector<ChildLayoutFlags> &layouts, size_t row_start, size_t row_end,
                                       f32 available_width, f32 inline_spacing_x)
        {
            f32 required_width = 0.0f;
            size_t child_count = 0u;
            for (size_t i = row_start; i < row_end; ++i)
            {
                auto *child = children[i];
                const ChildLayoutFlags layout = i < layouts.size() ? layouts[i] : ChildLayoutFlagBits::none;
                if (!child || !child->is_visible()) continue;
                const bool terminal_child = i + 1u == row_end && !is_inline_layout(layout);
                if (!is_inline_layout(layout) && !terminal_child) continue;
                if (!child->fill_width()) required_width += amal::max(child->required_size().x, 0.0f);
                ++child_count;
            }
            if (child_count > 1u) required_width += inline_spacing_x * static_cast<f32>(child_count - 1u);
            return amal::max(required_width - available_width, 0.0f);
        }

        static void layout_inline_row(Widget *layout_owner, const acul::vector<Widget *> &children,
                                      const acul::vector<ChildLayoutFlags> &layouts, size_t row_start, size_t row_end,
                                      const amal::rect &content_rect, const amal::vec2 &row_pos, f32 inline_spacing_x,
                                      f32 fill_available_width, f32 &row_height)
        {
            const f32 available_width = content_rect.size.x;
            const f32 available_height = content_rect.offset.y + content_rect.size.y - row_pos.y;
            f32 left_x = row_pos.x;
            f32 right_x = row_pos.x + available_width +
                          inline_row_overflow(children, layouts, row_start, row_end, available_width, inline_spacing_x);
            row_height = 0.0f;

            for (size_t i = row_end; i > row_start; --i)
            {
                const size_t index = i - 1u;
                auto *child = children[index];
                if (!child || !child->is_visible()) continue;
                const ChildLayoutFlags layout = index < layouts.size() ? layouts[index] : ChildLayoutFlagBits::none;
                const bool terminal_child = index + 1u == row_end && !is_inline_layout(layout);
                if ((!is_inline_layout(layout) && !terminal_child) || !is_right_layout(layout)) continue;
                const amal::vec2 req = child->required_size();
                const auto resolved =
                    resolve_child_layout_size(child, req, right_x - left_x, available_height, fill_available_width);
                right_x -= resolved.occupied_size.x;
                child->set_position({right_x, row_pos.y});
                child->set_layout_size(resolved.layout_size);
                child->update_layout(true);
                row_height = amal::max(row_height, child->required_size().y);
                right_x -= inline_spacing_x;
            }

            for (size_t i = row_start; i < row_end; ++i)
            {
                auto *child = children[i];
                if (!child || !child->is_visible()) continue;
                const ChildLayoutFlags layout = i < layouts.size() ? layouts[i] : ChildLayoutFlagBits::none;
                const bool terminal_child = i + 1u == row_end && !is_inline_layout(layout);
                if ((!is_inline_layout(layout) && !terminal_child) || is_right_layout(layout)) continue;
                const amal::vec2 req = child->required_size();
                const auto resolved =
                    resolve_child_layout_size(child, req, right_x - left_x, available_height, fill_available_width);
                child->set_position({left_x, row_pos.y});
                child->set_layout_size(resolved.layout_size);
                child->update_layout(true);
                row_height = amal::max(row_height, child->required_size().y);
                left_x += resolved.occupied_size.x + inline_spacing_x;
            }

            align_inline_row_vertical(children, layouts, row_start, row_end, row_height);
        }

        void layout_child_widgets(Widget *layout_owner, const acul::vector<Widget *> &children,
                                  const acul::vector<ChildLayoutFlags> &layouts, const amal::rect &content_rect,
                                  f32 inline_spacing_x, f32 fill_available_width)
        {
            assert(layout_owner && "layout owner is null");
            const amal::vec2 content_pos = content_rect.offset;
            const f32 available_width = content_rect.size.x;
            amal::vec2 cursor = content_rect.offset;
            size_t inline_row_start = 0;
            bool inline_row_active = false;

            const auto flush_inline_row = [&](size_t row_end, amal::vec2 &cursor_ref, bool &active, size_t &start) {
                if (!active) return;
                f32 row_height = 0.0f;
                layout_inline_row(layout_owner, children, layouts, start, row_end, content_rect, cursor_ref,
                                  inline_spacing_x, fill_available_width, row_height);
                cursor_ref = {content_pos.x, cursor_ref.y + row_height};
                active = false;
                start = row_end;
            };

            for (size_t i = 0; i < children.size(); ++i)
            {
                auto *child = children[i];
                if (!child) continue;
                if (!child->is_visible()) continue;
                const ChildLayoutFlags layout = i < layouts.size() ? layouts[i] : ChildLayoutFlagBits::none;
                if (is_inline_layout(layout))
                {
                    if (!inline_row_active)
                    {
                        inline_row_start = i;
                        inline_row_active = true;
                    }
                    continue;
                }

                if (inline_row_active)
                {
                    flush_inline_row(i + 1u, cursor, inline_row_active, inline_row_start);
                    continue;
                }
                const amal::vec2 req = child->required_size();
                const f32 available_height = content_pos.y + content_rect.size.y - cursor.y;
                const auto resolved =
                    resolve_child_layout_size(child, req, available_width, available_height, fill_available_width);
                f32 child_x = cursor.x;
                f32 child_y = cursor.y;
                if (is_right_layout(layout))
                    child_x = content_pos.x + amal::max(available_width - resolved.occupied_size.x, 0.0f);
                else if (is_hcenter_layout(layout))
                    child_x =
                        content_pos.x + amal::floor(amal::max(available_width - resolved.occupied_size.x, 0.0f) * 0.5f);
                if (is_center_layout(layout))
                    child_y =
                        cursor.y + amal::floor(amal::max(available_height - resolved.occupied_size.y, 0.0f) * 0.5f);
                else if (is_bottom_layout(layout))
                    child_y = cursor.y + amal::max(available_height - resolved.occupied_size.y, 0.0f);
                child->set_position({child_x, child_y});
                child->set_layout_size(resolved.layout_size);
                child->update_layout(true);
                cursor = {content_pos.x, child_y + child->required_size().y};
            }

            flush_inline_row(children.size(), cursor, inline_row_active, inline_row_start);
        }

        void layout_child_widgets(Widget *layout_owner, const acul::vector<Widget *> &children,
                                  const acul::vector<ChildLayoutFlags> &layouts, const amal::rect &content_rect,
                                  f32 inline_spacing_x)
        {
            layout_child_widgets(layout_owner, children, layouts, content_rect, inline_spacing_x, content_rect.size.x);
        }

        static void update_child_layout_bounds(Widget *child, const amal::rect &bounds)
        {
            if (!child) return;
            child->set_position(bounds.offset);
            child->set_layout_size(bounds.size);
            child->update_layout(true);
        }

        static void fast_update_inline_row(const acul::vector<Widget *> &children,
                                           const acul::vector<ChildLayoutFlags> &layouts, size_t row_start,
                                           size_t row_end, const amal::rect &content_rect, const amal::vec2 &row_pos,
                                           f32 inline_spacing_x, f32 fill_available_width, f32 &row_height)
        {
            const f32 available_width = content_rect.size.x;
            const f32 available_height = content_rect.offset.y + content_rect.size.y - row_pos.y;
            f32 left_x = row_pos.x;
            f32 right_x = row_pos.x + available_width +
                          inline_row_overflow(children, layouts, row_start, row_end, available_width, inline_spacing_x);
            row_height = 0.0f;

            for (size_t i = row_end; i > row_start; --i)
            {
                const size_t index = i - 1u;
                auto *child = children[index];
                if (!child || !child->is_visible()) continue;
                const ChildLayoutFlags layout = index < layouts.size() ? layouts[index] : ChildLayoutFlagBits::none;
                const bool terminal_child = index + 1u == row_end && !is_inline_layout(layout);
                if ((!is_inline_layout(layout) && !terminal_child) || !is_right_layout(layout)) continue;
                const amal::vec2 req = child->required_size();
                const auto resolved =
                    resolve_child_layout_size(child, req, right_x - left_x, available_height, fill_available_width);
                right_x -= resolved.occupied_size.x;
                update_child_layout_bounds(child, {{right_x, row_pos.y}, resolved.layout_size});
                row_height = amal::max(row_height, req.y);
                right_x -= inline_spacing_x;
            }

            for (size_t i = row_start; i < row_end; ++i)
            {
                auto *child = children[i];
                if (!child || !child->is_visible()) continue;
                const ChildLayoutFlags layout = i < layouts.size() ? layouts[i] : ChildLayoutFlagBits::none;
                const bool terminal_child = i + 1u == row_end && !is_inline_layout(layout);
                if ((!is_inline_layout(layout) && !terminal_child) || is_right_layout(layout)) continue;
                const amal::vec2 req = child->required_size();
                const auto resolved =
                    resolve_child_layout_size(child, req, right_x - left_x, available_height, fill_available_width);
                update_child_layout_bounds(child, {{left_x, row_pos.y}, resolved.layout_size});
                row_height = amal::max(row_height, req.y);
                left_x += resolved.occupied_size.x + inline_spacing_x;
            }

            align_inline_row_vertical(children, layouts, row_start, row_end, row_height);
        }

        static void layout_child_widgets_fast_update(const acul::vector<Widget *> &children,
                                                     const acul::vector<ChildLayoutFlags> &layouts,
                                                     const amal::rect &content_rect, f32 inline_spacing_x,
                                                     f32 fill_available_width)
        {
            const amal::vec2 content_pos = content_rect.offset;
            const f32 available_width = content_rect.size.x;
            amal::vec2 cursor = content_rect.offset;
            size_t inline_row_start = 0;
            bool inline_row_active = false;

            const auto flush_inline_row = [&](size_t row_end, amal::vec2 &cursor_ref, bool &active, size_t &start) {
                if (!active) return;
                f32 row_height = 0.0f;
                fast_update_inline_row(children, layouts, start, row_end, content_rect, cursor_ref, inline_spacing_x,
                                       fill_available_width, row_height);
                cursor_ref = {content_pos.x, cursor_ref.y + row_height};
                active = false;
                start = row_end;
            };

            for (size_t i = 0; i < children.size(); ++i)
            {
                auto *child = children[i];
                if (!child) continue;
                if (!child->is_visible()) continue;
                const ChildLayoutFlags layout = i < layouts.size() ? layouts[i] : ChildLayoutFlagBits::none;
                if (is_inline_layout(layout))
                {
                    if (!inline_row_active)
                    {
                        inline_row_start = i;
                        inline_row_active = true;
                    }
                    continue;
                }

                if (inline_row_active)
                {
                    flush_inline_row(i + 1u, cursor, inline_row_active, inline_row_start);
                    continue;
                }
                const amal::vec2 req = child->required_size();
                const f32 available_height = content_pos.y + content_rect.size.y - cursor.y;
                const auto resolved =
                    resolve_child_layout_size(child, req, available_width, available_height, fill_available_width);
                f32 child_x = cursor.x;
                f32 child_y = cursor.y;
                if (is_right_layout(layout))
                    child_x = content_pos.x + amal::max(available_width - resolved.occupied_size.x, 0.0f);
                else if (is_hcenter_layout(layout))
                    child_x =
                        content_pos.x + amal::floor(amal::max(available_width - resolved.occupied_size.x, 0.0f) * 0.5f);
                if (is_center_layout(layout))
                    child_y =
                        cursor.y + amal::floor(amal::max(available_height - resolved.occupied_size.y, 0.0f) * 0.5f);
                else if (is_bottom_layout(layout))
                    child_y = cursor.y + amal::max(available_height - resolved.occupied_size.y, 0.0f);
                update_child_layout_bounds(child, {{child_x, child_y}, resolved.layout_size});
                cursor = {content_pos.x, child_y + req.y};
            }

            flush_inline_row(children.size(), cursor, inline_row_active, inline_row_start);
        }

        static void collect_layer_children(const acul::vector<Widget *> &children,
                                           const acul::vector<ChildLayoutFlags> &layouts,
                                           const acul::vector<BlockChildLayer> &layers, BlockChildLayer layer,
                                           acul::vector<Widget *> &out_children,
                                           acul::vector<ChildLayoutFlags> &out_layouts)
        {
            out_children.clear();
            out_layouts.clear();
            for (size_t i = 0u; i < children.size(); ++i)
            {
                const BlockChildLayer child_layer = i < layers.size() ? layers[i] : BlockChildLayer::work;
                if (child_layer != layer) continue;
                out_children.push_back(children[i]);
                out_layouts.push_back(i < layouts.size() ? layouts[i] : default_child_layout_flags());
            }
        }

        static amal::vec2 compute_layer_children_required_size(const acul::vector<Widget *> &children,
                                                               const acul::vector<ChildLayoutFlags> &layouts,
                                                               const acul::vector<BlockChildLayer> &layers,
                                                               BlockChildLayer layer, f32 inline_spacing_x,
                                                               f32 wrap_width, bool refresh_min_size)
        {
            acul::vector<Widget *> layer_children;
            acul::vector<ChildLayoutFlags> layer_layouts;
            collect_layer_children(children, layouts, layers, layer, layer_children, layer_layouts);
            return compute_children_layout_required_size(layer_children, layer_layouts, inline_spacing_x, wrap_width,
                                                         refresh_min_size);
        }

        static void layout_layer_children(Widget *layout_owner, const acul::vector<Widget *> &children,
                                          const acul::vector<ChildLayoutFlags> &layouts,
                                          const acul::vector<BlockChildLayer> &layers, BlockChildLayer layer,
                                          const amal::rect &content_rect, f32 inline_spacing_x,
                                          f32 fill_available_width)
        {
            acul::vector<Widget *> layer_children;
            acul::vector<ChildLayoutFlags> layer_layouts;
            collect_layer_children(children, layouts, layers, layer, layer_children, layer_layouts);
            // The owning Block measured this subtree before arrange. Keep propagating the
            // min_size_known contract instead of starting a second recursive measure pass.
            layout_child_widgets(layout_owner, layer_children, layer_layouts, content_rect, inline_spacing_x,
                                 fill_available_width);
        }

        static void layout_layer_children_fast_update(const acul::vector<Widget *> &children,
                                                      const acul::vector<ChildLayoutFlags> &layouts,
                                                      const acul::vector<BlockChildLayer> &layers,
                                                      BlockChildLayer layer, const amal::rect &content_rect,
                                                      f32 inline_spacing_x, f32 fill_available_width)
        {
            acul::vector<Widget *> layer_children;
            acul::vector<ChildLayoutFlags> layer_layouts;
            collect_layer_children(children, layouts, layers, layer, layer_children, layer_layouts);
            layout_child_widgets_fast_update(layer_children, layer_layouts, content_rect, inline_spacing_x,
                                             fill_available_width);
        }
    } // namespace detail

    static inline amal::vec4 sum_padding(const amal::vec4 &left, const amal::vec4 &right)
    {
        return {left.x + right.x, left.y + right.y, left.z + right.z, left.w + right.w};
    }

    f32 Block::resolved_inline_spacing() const { return amal::max(_inline_spacing, 0.0f); }

    void Block::erase_children(size_t first, size_t count)
    {
        if (first >= children.size() || count == 0u) return;
        const size_t last = first + amal::min(count, children.size() - first);
        const bool destroying = detail::g_context && (detail::get_context().dirty_flags & DirtyFlagBits::destroying);
        invalidate_layout_measure();
        for (size_t i = first; i < last; ++i)
        {
            auto *child = children[i];
            if (!child) continue;
            if (!destroying && child->is_attached())
            {
                child->on_detach();
                child->invalidate_draw_commands(DrawReasonBits::external);
            }
            child->set_parent(nullptr);
            child->set_focus_parent(nullptr);
            if (detail::g_context && !destroying)
            {
                // Structural callbacks already queued for this child must observe
                // the detached state before its storage is released. A forced
                // disposal runs after the current queue wave and therefore after
                // every callback that could have been queued while it was attached.
                detail::get_context().disposal_queue.emplace([child]() { acul::release(child); }, true);
            }
            else acul::release(child);
        }
        children.erase(children.begin() + first, children.begin() + last);
        _explicit_child_layouts.erase(_explicit_child_layouts.begin() + first, _explicit_child_layouts.begin() + last);
        _child_layouts.erase(_child_layouts.begin() + first, _child_layouts.begin() + last);
        _child_layers.erase(_child_layers.begin() + first, _child_layers.begin() + last);
        if (!destroying) dispatch_change();
    }

    void Block::add_child(Widget *child, ChildLayoutFlags layout)
    {
        add_child_to_layer(child, layout, BlockChildLayer::work);
    }

    void Block::add_child_to_background(Widget *child, ChildLayoutFlags layout)
    {
        add_child_to_layer(child, layout, BlockChildLayer::background);
    }

    void Block::add_child_to_foreground(Widget *child, ChildLayoutFlags layout)
    {
        add_child_to_layer(child, layout, BlockChildLayer::foreground);
    }

    void Block::add_child_to_layer(Widget *child, ChildLayoutFlags layout, BlockChildLayer layer)
    {
        assert(child && "child is null");
        child->set_parent(this);
        child->set_focus_parent(parent() && id() == parent()->id() ? parent() : this);
        child->update_style_invalidated();
        children.push_back(child);
        _explicit_child_layouts.push_back(layout);
        _child_layouts.push_back(layout);
        _child_layers.push_back(layer);
        invalidate_layout_measure();
        dispatch_change();
    }

    void Block::set_child_layout(size_t index, ChildLayoutFlags layout)
    {
        assert(index < _child_layouts.size() && "child index out of range");
        assert(index < _explicit_child_layouts.size() && "child explicit layout index out of range");
        _explicit_child_layouts[index] = layout;
        refresh_child_layout(index);
    }

    bool Block::has_explicit_width() const { return !is_size_fit(_explicit_size.x); }

    bool Block::has_explicit_height() const { return !is_size_fit(_explicit_size.y); }

    f32 Block::resolved_explicit_width() const { return _explicit_size.x; }

    f32 Block::resolved_explicit_height() const { return _explicit_size.y; }

    void Block::refresh_child_layout(size_t index)
    {
        if (index >= children.size() || index >= _explicit_child_layouts.size() || index >= _child_layouts.size())
            return;
        _child_layouts[index] = _explicit_child_layouts[index];
    }

    void Block::refresh_child_layouts()
    {
        for (size_t child_i = 0u; child_i < children.size(); ++child_i) refresh_child_layout(child_i);
    }

    void Block::set_width(f32 value)
    {
        _explicit_size.x = (is_size_fit(value) || is_size_fill(value)) ? value : amal::max(value, 0.0f);
        Widget::set_size({_explicit_size.x, style_size().y});
    }

    void Block::set_height(f32 value)
    {
        _explicit_size.y = (is_size_fit(value) || is_size_fill(value)) ? value : amal::max(value, 0.0f);
        Widget::set_size({style_size().x, _explicit_size.y});
    }

    void Block::set_size(const amal::vec2 &value)
    {
        _explicit_size.x = (is_size_fit(value.x) || is_size_fill(value.x)) ? value.x : amal::max(value.x, 0.0f);
        _explicit_size.y = (is_size_fit(value.y) || is_size_fill(value.y)) ? value.y : amal::max(value.y, 0.0f);
        Widget::set_size(_explicit_size);
    }

    StyleUpdateFlags Block::update_style()
    {
        StyleUpdateFlags out = StyleUpdateFlagBits::none;
        for (size_t child_i = 0u; child_i < children.size(); ++child_i)
        {
            auto *child = children[child_i];
            if (!child || !child->is_visible()) continue;
            out |= child->update_style_invalidated();
            refresh_child_layout(child_i);
        }
        return out;
    }

    amal::vec2 Block::compute_content_min_size()
    {
        return detail::compute_layer_children_required_size(
            children, _child_layouts, _child_layers, BlockChildLayer::work, resolved_inline_spacing(), 0.0f, true);
    }

    void Block::layout_children(const amal::rect &content_rect)
    {
        detail::layout_layer_children(this, children, _child_layouts, _child_layers, BlockChildLayer::background,
                                      content_rect, resolved_inline_spacing(), content_rect.size.x);
        detail::layout_layer_children(this, children, _child_layouts, _child_layers, BlockChildLayer::work,
                                      content_rect, resolved_inline_spacing(), content_rect.size.x);
        detail::layout_layer_children(this, children, _child_layouts, _child_layers, BlockChildLayer::foreground,
                                      content_rect, resolved_inline_spacing(), content_rect.size.x);
    }

    void Block::update_layout_min_size_force() { update_layout_min_size_with(zero_vec4(), zero_vec4()); }

    void Block::update_layout_min_size_with(const amal::vec4 &margin, const amal::vec4 &padding)
    {
        const amal::vec2 content_required = compute_content_min_size();
        amal::vec2 required{margin.x + margin.z + padding.x + padding.z + content_required.x,
                            margin.y + margin.w + padding.y + padding.w + content_required.y};
        const f32 fixed_width = is_size_concrete(resolved_explicit_width())
                                    ? resolved_explicit_width()
                                    : (is_width_fixed() ? style_size().x : AUIK_SIZE_X_FIT);
        const f32 fixed_height = is_size_concrete(resolved_explicit_height())
                                     ? resolved_explicit_height()
                                     : (is_height_fixed() ? style_size().y : AUIK_SIZE_Y_FIT);
        if (is_size_concrete(fixed_width)) required.x = amal::max(fixed_width, 0.0f) + margin.x + margin.z;
        if (is_size_concrete(fixed_height)) required.y = amal::max(fixed_height, 0.0f) + margin.y + margin.w;
        set_required_size(required);
    }

    void Block::update_layout(bool min_size_known) { update_layout_with(min_size_known, zero_vec4(), zero_vec4()); }

    void Block::update_layout_with(bool min_size_known, const amal::vec4 &margin, const amal::vec4 &padding)
    {
        if (layout_measure_required(min_size_known)) update_layout_min_size_with(margin, padding);

        if (detail::is_fast_layout_update() && !is_fixed())
        {
            Widget::update_layout(true);
            set_clip_id(content_clip_id());
            const amal::rect content_rect{position(), size()};
            detail::layout_layer_children_fast_update(children, _child_layouts, _child_layers,
                                                      BlockChildLayer::background, content_rect,
                                                      resolved_inline_spacing(), size().x);
            detail::layout_layer_children_fast_update(children, _child_layouts, _child_layers, BlockChildLayer::work,
                                                      content_rect, resolved_inline_spacing(), size().x);
            detail::layout_layer_children_fast_update(children, _child_layouts, _child_layers,
                                                      BlockChildLayer::foreground, content_rect,
                                                      resolved_inline_spacing(), size().x);
            return;
        }

        const amal::vec2 layout_origin = position();
        const amal::vec2 inner_required = {amal::max(required_size().x - margin.x - margin.z, 0.0f),
                                           amal::max(required_size().y - margin.y - margin.w, 0.0f)};
        amal::vec2 inner_size = {amal::max(size().x - margin.x - margin.z, 0.0f),
                                 amal::max(size().y - margin.y - margin.w, 0.0f)};
        if (is_size_concrete(resolved_explicit_width())) inner_size.x = amal::max(resolved_explicit_width(), 0.0f);
        else if (inner_size.x <= 0.0f) inner_size.x = inner_required.x;
        if (is_size_concrete(resolved_explicit_height())) inner_size.y = amal::max(resolved_explicit_height(), 0.0f);
        else if (inner_size.y <= 0.0f) inner_size.y = inner_required.y;

        set_position({layout_origin.x + margin.x, layout_origin.y + margin.y});
        Widget::set_layout_size(inner_size);
        Widget::update_layout(true);
        set_clip_id(content_clip_id());

        const amal::vec2 content_pos = position() + amal::vec2{padding.x, padding.y};
        const amal::vec2 content_size = {amal::max(size().x - padding.x - padding.z, 0.0f),
                                         amal::max(size().y - padding.y - padding.w, 0.0f)};
        layout_children({content_pos, content_size});
    }

    void Block::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        for (auto *child : children)
        {
            if (!child) continue;
            child->translate(delta);
        }
    }

    void Block::reset_clip_rect_records()
    {
        Widget::reset_clip_rect_records();
        for (auto *child : children)
        {
            if (!child) continue;
            child->reset_clip_rect_records();
        }
    }

    void Block::rebuild_clip_rects()
    {
        set_clip_id(content_clip_id());
        for (auto *child : children)
        {
            if (!child || !child->is_visible()) continue;
            child->rebuild_clip_rects();
        }
    }

    void Block::reset_draw_records()
    {
        Widget::reset_draw_records();
        for (auto *child : children)
        {
            if (!child) continue;
            child->reset_external_draw_cull_state();
            child->reset_draw_records();
        }
    }

    u32 Block::get_depth_requirement() const
    {
        u32 requirement = 1u;
        auto add_layer = [&](BlockChildLayer layer) {
            for (size_t i = 0u; i < children.size(); ++i)
            {
                const BlockChildLayer child_layer = i < _child_layers.size() ? _child_layers[i] : BlockChildLayer::work;
                if (child_layer != layer) continue;
                auto *child = children[i];
                if (!child || !child->is_visible()) continue;
                requirement += amal::max(child->get_depth_requirement(), 1u);
            }
        };
        add_layer(BlockChildLayer::background);
        add_layer(BlockChildLayer::work);
        add_layer(BlockChildLayer::foreground);
        return requirement;
    }

    void Block::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        DepthCursor cursor(this->depth_range(), get_depth_requirement());
        cursor.next(1u);
        auto update_layer = [&](BlockChildLayer layer) {
            for (size_t i = 0u; i < children.size(); ++i)
            {
                auto *child = children[i];
                if (!child || !child->is_visible()) continue;
                const BlockChildLayer child_layer = i < _child_layers.size() ? _child_layers[i] : BlockChildLayer::work;
                if (child_layer != layer) continue;
                child->update_depth(cursor.next(depth_requirement_of(child)));
            }
        };
        update_layer(BlockChildLayer::background);
        update_layer(BlockChildLayer::work);
        update_layer(BlockChildLayer::foreground);
    }

    void Block::back_hit_depth()
    {
        Widget::back_hit_depth();
        for (auto *child : children)
            if (child) child->back_hit_depth();
    }

    void Block::restore_hit_depth()
    {
        Widget::restore_hit_depth();
        for (auto *child : children)
            if (child) child->restore_hit_depth();
    }

    void Block::draw(DrawCtx &ctx)
    {
        if (!is_visible() && !(ctx.reason & DrawReasonBits::invalidate)) return;
        const amal::vec4 content_clip = get_content_clip_rect();
        auto draw_layer = [&](BlockChildLayer layer) {
            for (size_t i = 0u; i < children.size(); ++i)
            {
                auto *child = children[i];
                if (!child || (!child->is_visible() && !(ctx.reason & DrawReasonBits::invalidate))) continue;
                const BlockChildLayer child_layer = i < _child_layers.size() ? _child_layers[i] : BlockChildLayer::work;
                if (child_layer != layer) continue;
                detail::draw_child_in_clip(child, ctx, content_clip);
            }
        };
        draw_layer(BlockChildLayer::background);
        draw_layer(BlockChildLayer::work);
        draw_layer(BlockChildLayer::foreground);
    }

    void Block::on_attach()
    {
        Widget::on_attach();
        for (auto *child : children)
            if (child && !child->is_attached() && (child->widget_flags & WidgetFlagBits::attachable))
                child->on_attach();
    }

    void Block::on_detach()
    {
        for (auto *child : children)
            if (child && child->is_attached()) child->on_detach();
        Widget::on_detach();
    }

    void Block::on_change(ChangeEvent &event)
    {
        if (event.target != id() || !is_attached()) return;
        if (!add_render_command(
                [this]() {
                    if (!is_attached()) return;
                    for (auto *child : children)
                        if (child && !child->is_attached() && (child->widget_flags & WidgetFlagBits::attachable))
                            child->on_attach();

                    Widget *layout_target = resolve_parent_layout_update_target(this);
                    if (!layout_target) layout_target = this;
                    layout_target->update_layout(false);
                    rebuild_root_widget_depths();
                    layout_target->update_draw_commands(DrawReasonBits::layout);
                    detail::get_context().dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
                },
                true))
            return;
        event.prevent_default();
    }

    DrawBlock::DrawBlock(u32 id, WidgetFlags widget_flags, u32 tag_id)
        : Block(id, widget_flags, tag_id), _bg_rect(detail::make_rect_data(id, tag_id))
    {
    }

    DrawBlock::~DrawBlock()
    {
        release_scrollbar(_scrollbar_x);
        release_scrollbar(_scrollbar_y);
    }

    void DrawBlock::ensure_scrollbars()
    {
        if (_draw_flags & DrawBlockFlagBits::scrollbar_x)
            ensure_styled_scrollbar(_scrollbar_x, this, amal::axis::x, _scrollbar_track_style_tag,
                                    _scrollbar_thumb_style_tag);
        if (_draw_flags & DrawBlockFlagBits::scrollbar_y)
            ensure_styled_scrollbar(_scrollbar_y, this, amal::axis::y, _scrollbar_track_style_tag,
                                    _scrollbar_thumb_style_tag);
        if (_scrollbar_x)
        {
            _scrollbar_x->set_clip_id(clip_id());
            _scrollbar_x->update_depth(detail::depth_foreground_range(this->depth_range()));
            _scrollbar_x->update_style_invalidated();
        }
        if (_scrollbar_y)
        {
            _scrollbar_y->set_clip_id(clip_id());
            _scrollbar_y->update_depth(detail::depth_foreground_range(this->depth_range()));
            _scrollbar_y->update_style_invalidated();
        }
    }

    void DrawBlock::set_scrollbars_enabled(bool x, bool y)
    {
        if (x) _draw_flags |= DrawBlockFlagBits::scrollbar_x;
        else _draw_flags &= ~DrawBlockFlagBits::scrollbar_x;
        if (y) _draw_flags |= DrawBlockFlagBits::scrollbar_y;
        else _draw_flags &= ~DrawBlockFlagBits::scrollbar_y;
        if (!x && _scrollbar_x)
        {
            _scrollbar_x->unset_visible();
            _scrollbar_x->sync_widget_flags();
        }
        if (!y && _scrollbar_y)
        {
            _scrollbar_y->unset_visible();
            _scrollbar_y->sync_widget_flags();
        }
    }

    void DrawBlock::set_style_tag(u32 tag_id)
    {
        if (_style_tag_id == tag_id) return;
        _style_tag_id = tag_id;
        _style = {Theme::STYLE_ID_INVALID, tag_id};
    }

    const Style *DrawBlock::draw_style() const
    {
        if (_style_tag_id == 0u) return nullptr;
        auto *theme = get_theme();
        const u32 parent_id = parent() ? parent()->id() : 0u;
        const StyleID style_id = _style.id != Theme::STYLE_ID_INVALID && _style.tag_id == _style_tag_id
                                     ? _style.id
                                     : theme->get_resolved_style(_style_tag_id, id(), parent_id, style_state());
        if (style_id == Theme::STYLE_ID_INVALID) return nullptr;
        return &theme->get_style(style_id);
    }

    amal::vec4 DrawBlock::draw_margin() const
    {
        const auto *style = draw_style();
        return style ? style->margin() : zero_vec4();
    }

    amal::vec4 DrawBlock::draw_padding() const
    {
        const auto *style = draw_style();
        return style ? style->padding() : zero_vec4();
    }

    f32 DrawBlock::resolved_inline_spacing() const
    {
        const auto *style = draw_style();
        return style ? amal::max(style->inline_spacing(), 0.0f) : 0.0f;
    }

    void DrawBlock::sync_draw_bounds()
    {
        _bg_rect.id = get_rect().id;
        _bg_rect.bounds = bounds();
        _bg_rect.clip_id = clip_id();
    }

    void DrawBlock::set_scrollbar_style_tag(u32 track_tag_id)
    {
        set_scrollbar_style_tags(track_tag_id, scrollbar_thumb_style_for_track(track_tag_id));
    }

    void DrawBlock::set_scrollbar_style_tags(u32 track_tag_id, u32 thumb_tag_id)
    {
        if (_scrollbar_track_style_tag == track_tag_id && _scrollbar_thumb_style_tag == thumb_tag_id) return;
        _scrollbar_track_style_tag = track_tag_id;
        _scrollbar_thumb_style_tag = thumb_tag_id;
        release_scrollbar(_scrollbar_x);
        release_scrollbar(_scrollbar_y);
    }

    void DrawBlock::update_layout_min_size_force()
    {
        const auto *style = draw_style();
        amal::vec4 padding = sum_padding(draw_padding(), _content_padding);
        ensure_scrollbars();
        if ((_draw_flags & DrawBlockFlagBits::scrollbar_y) && _scrollbar_y && _scrollbar_y->is_visible())
            padding.z += _scrollbar_y->get_min_track_thickness();
        if ((_draw_flags & DrawBlockFlagBits::scrollbar_x) && _scrollbar_x && _scrollbar_x->is_visible())
            padding.w += _scrollbar_x->get_min_track_thickness();
        const amal::vec4 margin = draw_margin();
        update_layout_min_size_with(margin, padding);
        if (style)
        {
            auto required = required_size();
            required.x = amal::max(required.x, amal::max(style->min_width(), 0.0f) + margin.x + margin.z);
            required.y = amal::max(required.y, amal::max(style->min_height(), 0.0f) + margin.y + margin.w);
            set_required_size(required);
        }
    }

    void DrawBlock::update_scroll_clip(const amal::vec2 &content_pos, const amal::vec2 &view_size)
    {
        const amal::vec4 own_clip = clip_id() != 0xFFFFu
                                        ? get_clip_rect(clip_id())
                                        : (parent() ? parent()->get_content_clip_rect() : get_main_viewport_rect());
        const amal::vec4 clip =
            _content_clip_rect_overridden
                ? _content_clip_rect_override
                : detail::intersect_rects(own_clip, {content_pos.x, content_pos.y, view_size.x, view_size.y});
        if (_content_clip_id == 0xFFFFu) _content_clip_id = push_clip_rect(clip);
        else update_clip_rect(_content_clip_id, clip);
        for (auto *child : children)
            if (child) child->set_clip_id(_content_clip_id);
    }

    void DrawBlock::override_content_clip_rect(const amal::vec4 &rect)
    {
        _content_clip_rect_overridden = true;
        _content_clip_rect_override = rect;
        if (_content_clip_id == 0xFFFFu) _content_clip_id = push_clip_rect(rect);
        else update_clip_rect(_content_clip_id, rect);
        for (auto *child : children)
            if (child) child->set_clip_id(_content_clip_id);
    }

    void DrawBlock::rebuild_scroll_clip_rect()
    {
        if (clip_id() == 0xFFFFu) return;
        const amal::vec4 padding = sum_padding(draw_padding(), _content_padding);
        const amal::vec2 content_pos = position() + amal::vec2{padding.x, padding.y};
        const bool clip_ignores_padding_x = _draw_flags & DrawBlockFlagBits::clip_ignores_padding_x;
        const bool clip_ignores_padding_y = _draw_flags & DrawBlockFlagBits::clip_ignores_padding_y;
        const bool scroll_x_enabled = _draw_flags & DrawBlockFlagBits::scrollbar_x;
        const bool scroll_y_enabled = _draw_flags & DrawBlockFlagBits::scrollbar_y;
        const f32 bar_w = has_visible_scrollbar_y() && _scrollbar_y ? _scrollbar_y->get_min_track_thickness() : 0.0f;
        const f32 bar_h = has_visible_scrollbar_x() && _scrollbar_x ? _scrollbar_x->get_min_track_thickness() : 0.0f;
        const amal::vec2 clip_pos{clip_ignores_padding_x ? position().x : content_pos.x,
                                  clip_ignores_padding_y ? position().y : content_pos.y};
        const amal::vec2 clip_view_size{
            clip_ignores_padding_x
                ? amal::max(size().x - bar_w, 0.0f)
                : (scroll_x_enabled ? _scroll_view_size.x : amal::max(size().x - padding.x - padding.z, 0.0f)),
            clip_ignores_padding_y
                ? amal::max(size().y - bar_h, 0.0f)
                : (scroll_y_enabled ? _scroll_view_size.y : amal::max(size().y - padding.y - padding.w, 0.0f))};
        update_scroll_clip(clip_pos, clip_view_size);
    }

    void DrawBlock::request_scroll_layout_update(DrawReasonFlags reason)
    {
        sync_clip_rect_cache();
        update_layout(true);
        update_draw_commands(reason);
        auto &ctx = detail::get_context();
        ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
        mark_host_refresh_request();
    }

    StyleUpdateFlags DrawBlock::update_style()
    {
        StyleUpdateFlags out = Block::update_style();
        if (_style_tag_id != 0u)
        {
            out |= resolve_style_selector(_style, id(), parent() ? parent()->id() : 0u, style_state());
            const auto &style = get_theme()->get_style(_style.id);
            const auto style_mask = style.mask();
            amal::vec2 next_size = requested_size();

            // Block owns an explicit size separately from Widget's inline size. Its default
            // fit-content value must not mask a width/height declared by the DrawBlock style.
            // An explicit Block::set_width()/set_height() remains the highest-priority value.
            next_size.x = has_explicit_width()
                              ? resolved_explicit_width()
                              : ((style_mask & detail::StylePropertiesBits::width) ? style.width() : AUIK_SIZE_X_FIT);
            next_size.y = has_explicit_height()
                              ? resolved_explicit_height()
                              : ((style_mask & detail::StylePropertiesBits::height) ? style.height() : AUIK_SIZE_Y_FIT);
            set_requested_size(next_size);
        }
        if (_scrollbar_x) out |= _scrollbar_x->update_style_invalidated();
        if (_scrollbar_y) out |= _scrollbar_y->update_style_invalidated();
        return out;
    }

    void DrawBlock::update_layout(bool min_size_known)
    {
        if (layout_measure_required(min_size_known)) update_layout_min_size_force();

        if (_content_clip_rect_overridden) ensure_own_clip_rect(_content_clip_rect_override);
        else if (parent()) set_clip_id(parent()->content_clip_id());

        const amal::vec4 margin = draw_margin();
        const amal::vec4 padding = sum_padding(draw_padding(), _content_padding);
        const amal::vec2 layout_origin = position();
        if (detail::is_fast_layout_update() && !is_fixed())
        {
            const amal::vec2 inner_size = {
                amal::max(size().x - margin.x - margin.z, 0.0f),
                amal::max(size().y - margin.y - margin.w, 0.0f),
            };

            set_position({layout_origin.x + margin.x, layout_origin.y + margin.y});
            Widget::set_layout_size(inner_size);
            Widget::update_layout(true);
            sync_draw_bounds();

            const amal::vec2 content_pos = position() + amal::vec2{padding.x, padding.y};
            const amal::vec2 raw_available_size = {amal::max(size().x - padding.x - padding.z, 0.0f),
                                                   amal::max(size().y - padding.y - padding.w, 0.0f)};
            const amal::vec4 frame_clip = get_clip_rect(clip_id());
            const amal::vec4 visible_scroll_rect =
                detail::intersect_rects(frame_clip, {position().x, position().y, size().x, size().y});
            const bool scroll_x_enabled = _draw_flags & DrawBlockFlagBits::scrollbar_x;
            const bool scroll_y_enabled = _draw_flags & DrawBlockFlagBits::scrollbar_y;
            const bool need_scroll_y = scroll_y_enabled && _scrollbar_y && _scrollbar_y->is_visible();
            const bool need_scroll_x = scroll_x_enabled && _scrollbar_x && _scrollbar_x->is_visible();
            const f32 bar_w = need_scroll_y && _scrollbar_y ? _scrollbar_y->get_min_track_thickness() : 0.0f;
            const f32 bar_h = need_scroll_x && _scrollbar_x ? _scrollbar_x->get_min_track_thickness() : 0.0f;

            _scroll_view_size = {amal::max(raw_available_size.x - bar_w, 0.0f),
                                 amal::max(raw_available_size.y - bar_h, 0.0f)};
            const bool clip_ignores_padding_x = _draw_flags & DrawBlockFlagBits::clip_ignores_padding_x;
            const bool clip_ignores_padding_y = _draw_flags & DrawBlockFlagBits::clip_ignores_padding_y;
            const amal::vec2 clip_pos{clip_ignores_padding_x ? position().x : content_pos.x,
                                      clip_ignores_padding_y ? position().y : content_pos.y};
            const amal::vec2 clip_view_size{
                clip_ignores_padding_x ? amal::max(size().x - bar_w, 0.0f) : _scroll_view_size.x,
                clip_ignores_padding_y ? amal::max(size().y - bar_h, 0.0f) : _scroll_view_size.y};
            update_scroll_clip(clip_pos, clip_view_size);
            if (_content_clip_rect_overridden) ensure_own_clip_rect(_content_clip_rect_override);
            else set_clip_id(parent() ? parent()->content_clip_id() : clip_id());

            if (_scrollbar_y && _scrollbar_y->is_visible())
            {
                const amal::vec4 track_margin = _scrollbar_y->get_track_margin();
                const f32 track_w = _scrollbar_y->get_min_track_thickness();
                const f32 track_x =
                    visible_scroll_rect.x + amal::max(visible_scroll_rect.z - track_margin.z - track_w, 0.0f);
                const f32 track_y = visible_scroll_rect.y + track_margin.y;
                const f32 track_h = amal::max(visible_scroll_rect.w - track_margin.y - track_margin.w, 0.0f);
                _scrollbar_y->configure({track_x, track_y}, {track_w, track_h}, _scroll_content_size.y,
                                        _scroll_view_size.y);
            }
            if (_scrollbar_x && _scrollbar_x->is_visible())
            {
                const amal::vec4 track_margin = _scrollbar_x->get_track_margin();
                const f32 track_h = _scrollbar_x->get_min_track_thickness();
                const f32 track_x = visible_scroll_rect.x + track_margin.x;
                const f32 track_y =
                    visible_scroll_rect.y + amal::max(visible_scroll_rect.w - track_margin.w - track_h, 0.0f);
                const f32 track_w = amal::max(visible_scroll_rect.z - bar_w - track_margin.x - track_margin.z, 0.0f);
                _scrollbar_x->configure({track_x, track_y}, {track_w, track_h}, _scroll_content_size.x,
                                        _scroll_view_size.x);
            }

            const amal::vec2 layout_view_size{need_scroll_x ? amal::max(_scroll_content_size.x, _scroll_view_size.x)
                                                            : _scroll_view_size.x,
                                              _scroll_view_size.y};
            detail::layout_layer_children_fast_update(children, _child_layouts, _child_layers,
                                                      BlockChildLayer::background, {content_pos, _scroll_view_size},
                                                      resolved_inline_spacing(), _scroll_view_size.x);
            detail::layout_layer_children_fast_update(children, _child_layouts, _child_layers, BlockChildLayer::work,
                                                      {content_pos - _content_offset, layout_view_size},
                                                      resolved_inline_spacing(), _scroll_view_size.x);
            detail::layout_layer_children_fast_update(children, _child_layouts, _child_layers,
                                                      BlockChildLayer::foreground, {content_pos, _scroll_view_size},
                                                      resolved_inline_spacing(), _scroll_view_size.x);
            return;
        }

        const amal::vec2 inner_required = {amal::max(required_size().x - margin.x - margin.z, 0.0f),
                                           amal::max(required_size().y - margin.y - margin.w, 0.0f)};
        amal::vec2 inner_size = {amal::max(size().x - margin.x - margin.z, 0.0f),
                                 amal::max(size().y - margin.y - margin.w, 0.0f)};
        if (is_size_concrete(resolved_explicit_width())) inner_size.x = amal::max(resolved_explicit_width(), 0.0f);
        else if (inner_size.x <= 0.0f) inner_size.x = inner_required.x;
        if (is_size_concrete(resolved_explicit_height())) inner_size.y = amal::max(resolved_explicit_height(), 0.0f);
        else if (inner_size.y <= 0.0f) inner_size.y = inner_required.y;

        set_position({layout_origin.x + margin.x, layout_origin.y + margin.y});
        Widget::set_layout_size(inner_size);
        Widget::update_layout(true);
        sync_draw_bounds();

        const amal::vec2 content_pos = position() + amal::vec2{padding.x, padding.y};
        const amal::vec2 raw_available_size = {amal::max(size().x - padding.x - padding.z, 0.0f),
                                               amal::max(size().y - padding.y - padding.w, 0.0f)};
        const amal::vec4 frame_clip = get_clip_rect(clip_id());
        const amal::vec4 visible_scroll_rect =
            detail::intersect_rects(frame_clip, {position().x, position().y, size().x, size().y});
        const amal::vec2 available_size = raw_available_size;
        ensure_scrollbars();
        const bool scroll_x_enabled = _draw_flags & DrawBlockFlagBits::scrollbar_x;
        const bool scroll_y_enabled = _draw_flags & DrawBlockFlagBits::scrollbar_y;
        const f32 bar_w = scroll_y_enabled && _scrollbar_y ? _scrollbar_y->get_min_track_thickness() : 0.0f;
        const f32 bar_h = scroll_x_enabled && _scrollbar_x ? _scrollbar_x->get_min_track_thickness() : 0.0f;
        auto with_scroll_trailing_padding = [padding](const amal::vec2 &content_size, bool scroll_x, bool scroll_y) {
            return content_size + amal::vec2{scroll_x ? padding.z : 0.0f, scroll_y ? padding.w : 0.0f};
        };

        // Child minimum sizes are part of the min_size_known contract. Re-evaluate only the
        // container's wrapping with the cached child sizes; measuring every child again here
        // duplicated the full subtree traversal performed before arrange.
        amal::vec2 children_layout_size = detail::compute_layer_children_required_size(
            children, _child_layouts, _child_layers, BlockChildLayer::work, resolved_inline_spacing(), 0.0f, false);
        bool need_scroll_y = scroll_y_enabled && children_layout_size.y > available_size.y;
        bool need_scroll_x = scroll_x_enabled && children_layout_size.x > available_size.x;
        for (int i = 0; i < 2; ++i)
        {
            const f32 viewport_w = amal::max(available_size.x - (need_scroll_y ? bar_w : 0.0f), 0.0f);
            const f32 viewport_h = amal::max(available_size.y - (need_scroll_x ? bar_h : 0.0f), 0.0f);
            children_layout_size = detail::compute_layer_children_required_size(
                children, _child_layouts, _child_layers, BlockChildLayer::work, resolved_inline_spacing(), viewport_w,
                false);
            const bool next_y = scroll_y_enabled && children_layout_size.y > viewport_h;
            const bool next_x = scroll_x_enabled && children_layout_size.x > viewport_w;
            if (next_y == need_scroll_y && next_x == need_scroll_x) break;
            need_scroll_y = next_y;
            need_scroll_x = next_x;
        }

        _scroll_view_size = {amal::max(available_size.x - (need_scroll_y ? bar_w : 0.0f), 0.0f),
                             amal::max(available_size.y - (need_scroll_x ? bar_h : 0.0f), 0.0f)};
        _scroll_content_size = with_scroll_trailing_padding(children_layout_size, need_scroll_x, need_scroll_y);
        const bool clip_ignores_padding_x = _draw_flags & DrawBlockFlagBits::clip_ignores_padding_x;
        const bool clip_ignores_padding_y = _draw_flags & DrawBlockFlagBits::clip_ignores_padding_y;
        const amal::vec2 clip_pos{clip_ignores_padding_x ? position().x : content_pos.x,
                                  clip_ignores_padding_y ? position().y : content_pos.y};
        const amal::vec2 clip_view_size{
            clip_ignores_padding_x ? amal::max(size().x - (need_scroll_y ? bar_w : 0.0f), 0.0f) : _scroll_view_size.x,
            clip_ignores_padding_y ? amal::max(size().y - (need_scroll_x ? bar_h : 0.0f), 0.0f) : _scroll_view_size.y};
        update_scroll_clip(clip_pos, clip_view_size);
        if (_content_clip_rect_overridden) ensure_own_clip_rect(_content_clip_rect_override);
        else set_clip_id(parent() ? parent()->content_clip_id() : clip_id());

        if (_scrollbar_x) _scrollbar_x->set_metrics(_scroll_content_size.x, _scroll_view_size.x);
        if (_scrollbar_y) _scrollbar_y->set_metrics(_scroll_content_size.y, _scroll_view_size.y);
        const amal::vec2 max_scroll{_scrollbar_x ? _scrollbar_x->max_scroll() : 0.0f,
                                    _scrollbar_y ? _scrollbar_y->max_scroll() : 0.0f};
        _content_offset = amal::clamp(_content_offset, amal::vec2{0.0f}, max_scroll);
        if (_scrollbar_x) _scrollbar_x->set_scroll_offset(_content_offset.x);
        if (_scrollbar_y) _scrollbar_y->set_scroll_offset(_content_offset.y);

        const amal::vec2 pre_layout_children_size = _scroll_content_size;
        amal::vec2 layout_view_size{need_scroll_x ? amal::max(_scroll_content_size.x, _scroll_view_size.x)
                                                  : _scroll_view_size.x,
                                    _scroll_view_size.y};
        detail::layout_layer_children(this, children, _child_layouts, _child_layers, BlockChildLayer::background,
                                      {content_pos, _scroll_view_size}, resolved_inline_spacing(), _scroll_view_size.x);
        detail::layout_layer_children(this, children, _child_layouts, _child_layers, BlockChildLayer::work,
                                      {content_pos - _content_offset, layout_view_size}, resolved_inline_spacing(),
                                      _scroll_view_size.x);
        detail::layout_layer_children(this, children, _child_layouts, _child_layers, BlockChildLayer::foreground,
                                      {content_pos, _scroll_view_size}, resolved_inline_spacing(), _scroll_view_size.x);
        const amal::vec2 laid_out_children_size =
            detail::compute_layer_children_required_size(children, _child_layouts, _child_layers, BlockChildLayer::work,
                                                         resolved_inline_spacing(), layout_view_size.x, false);
        const amal::vec2 laid_out_scroll_content_size =
            with_scroll_trailing_padding(laid_out_children_size, need_scroll_x, need_scroll_y);
        if (laid_out_scroll_content_size != pre_layout_children_size)
        {
            children_layout_size = laid_out_children_size;
            bool refined_need_y = scroll_y_enabled && children_layout_size.y > available_size.y;
            bool refined_need_x = scroll_x_enabled && children_layout_size.x > available_size.x;
            for (int i = 0; i < 2; ++i)
            {
                const f32 viewport_w = amal::max(available_size.x - (refined_need_y ? bar_w : 0.0f), 0.0f);
                const f32 viewport_h = amal::max(available_size.y - (refined_need_x ? bar_h : 0.0f), 0.0f);
                const f32 layout_w = refined_need_x ? amal::max(children_layout_size.x, viewport_w) : viewport_w;
                detail::layout_layer_children(this, children, _child_layouts, _child_layers,
                                              BlockChildLayer::background, {content_pos, _scroll_view_size},
                                              resolved_inline_spacing(), viewport_w);
                detail::layout_layer_children(this, children, _child_layouts, _child_layers, BlockChildLayer::work,
                                              {content_pos - _content_offset, {layout_w, viewport_h}},
                                              resolved_inline_spacing(), viewport_w);
                detail::layout_layer_children(this, children, _child_layouts, _child_layers,
                                              BlockChildLayer::foreground, {content_pos, _scroll_view_size},
                                              resolved_inline_spacing(), viewport_w);
                children_layout_size = detail::compute_layer_children_required_size(
                    children, _child_layouts, _child_layers, BlockChildLayer::work, resolved_inline_spacing(), layout_w,
                    false);
                const bool next_y = scroll_y_enabled && children_layout_size.y > viewport_h;
                const bool next_x = scroll_x_enabled && children_layout_size.x > viewport_w;
                if (next_y == refined_need_y && next_x == refined_need_x) break;
                refined_need_y = next_y;
                refined_need_x = next_x;
            }
            need_scroll_y = refined_need_y;
            need_scroll_x = refined_need_x;
            _scroll_content_size = with_scroll_trailing_padding(children_layout_size, need_scroll_x, need_scroll_y);
            _scroll_view_size = {amal::max(available_size.x - (need_scroll_y ? bar_w : 0.0f), 0.0f),
                                 amal::max(available_size.y - (need_scroll_x ? bar_h : 0.0f), 0.0f)};
            const amal::vec2 refined_clip_view_size{
                clip_ignores_padding_x ? amal::max(size().x - (need_scroll_y ? bar_w : 0.0f), 0.0f)
                                       : _scroll_view_size.x,
                clip_ignores_padding_y ? amal::max(size().y - (need_scroll_x ? bar_h : 0.0f), 0.0f)
                                       : _scroll_view_size.y};
            update_scroll_clip(clip_pos, refined_clip_view_size);
            if (_scrollbar_x) _scrollbar_x->set_metrics(_scroll_content_size.x, _scroll_view_size.x);
            if (_scrollbar_y) _scrollbar_y->set_metrics(_scroll_content_size.y, _scroll_view_size.y);
            const amal::vec2 refined_max_scroll{_scrollbar_x ? _scrollbar_x->max_scroll() : 0.0f,
                                                _scrollbar_y ? _scrollbar_y->max_scroll() : 0.0f};
            _content_offset = amal::clamp(_content_offset, amal::vec2{0.0f}, refined_max_scroll);
            layout_view_size = {need_scroll_x ? amal::max(_scroll_content_size.x, _scroll_view_size.x)
                                              : _scroll_view_size.x,
                                _scroll_view_size.y};
            detail::layout_layer_children(this, children, _child_layouts, _child_layers, BlockChildLayer::background,
                                          {content_pos, _scroll_view_size}, resolved_inline_spacing(),
                                          _scroll_view_size.x);
            detail::layout_layer_children(this, children, _child_layouts, _child_layers, BlockChildLayer::work,
                                          {content_pos - _content_offset, layout_view_size}, resolved_inline_spacing(),
                                          _scroll_view_size.x);
            detail::layout_layer_children(this, children, _child_layouts, _child_layers, BlockChildLayer::foreground,
                                          {content_pos, _scroll_view_size}, resolved_inline_spacing(),
                                          _scroll_view_size.x);
        }

        const bool was_scrollbar_y_visible = _scrollbar_y && _scrollbar_y->is_visible();
        const bool was_scrollbar_x_visible = _scrollbar_x && _scrollbar_x->is_visible();
        if (need_scroll_y && _scrollbar_y)
        {
            const amal::vec4 track_margin = _scrollbar_y->get_track_margin();
            const f32 track_w = _scrollbar_y->get_min_track_thickness();
            const f32 track_x =
                visible_scroll_rect.x + amal::max(visible_scroll_rect.z - track_margin.z - track_w, 0.0f);
            const f32 track_y = visible_scroll_rect.y + track_margin.y;
            const f32 track_h = amal::max(visible_scroll_rect.w - track_margin.y - track_margin.w, 0.0f);
            const amal::vec2 track_pos = {track_x, track_y};
            const amal::vec2 track_size = {track_w, track_h};
            _scrollbar_y->set_visible();
            _scrollbar_y->sync_widget_flags();
            _scrollbar_y->set_clip_id(clip_id());
            _scrollbar_y->set_scroll_offset(_content_offset.y);
            _scrollbar_y->configure(track_pos, track_size, _scroll_content_size.y, _scroll_view_size.y);
            _content_offset.y = _scrollbar_y->scroll_offset();
        }
        else if (_scrollbar_y)
        {
            if (_scrollbar_y->is_visible()) _scrollbar_y->invalidate_draw_commands(DrawReasonBits::layout);
            _scrollbar_y->unset_visible();
            _scrollbar_y->sync_widget_flags();
        }

        if (need_scroll_x && _scrollbar_x)
        {
            const amal::vec4 track_margin = _scrollbar_x->get_track_margin();
            const f32 track_h = _scrollbar_x->get_min_track_thickness();
            const f32 track_x = visible_scroll_rect.x + track_margin.x;
            const f32 track_y =
                visible_scroll_rect.y + amal::max(visible_scroll_rect.w - track_margin.w - track_h, 0.0f);
            const f32 track_w = amal::max(
                visible_scroll_rect.z - (need_scroll_y ? bar_w : 0.0f) - track_margin.x - track_margin.z, 0.0f);
            const amal::vec2 track_pos = {track_x, track_y};
            const amal::vec2 track_size = {track_w, track_h};
            _scrollbar_x->set_visible();
            _scrollbar_x->sync_widget_flags();
            _scrollbar_x->set_clip_id(clip_id());
            _scrollbar_x->set_scroll_offset(_content_offset.x);
            _scrollbar_x->configure(track_pos, track_size, _scroll_content_size.x, _scroll_view_size.x);
            _content_offset.x = _scrollbar_x->scroll_offset();
        }
        else if (_scrollbar_x)
        {
            if (_scrollbar_x->is_visible()) _scrollbar_x->invalidate_draw_commands(DrawReasonBits::layout);
            _scrollbar_x->unset_visible();
            _scrollbar_x->sync_widget_flags();
        }

        const bool is_scrollbar_y_visible = _scrollbar_y && _scrollbar_y->is_visible();
        const bool is_scrollbar_x_visible = _scrollbar_x && _scrollbar_x->is_visible();
        const bool needs_scroll_events = is_scrollbar_y_visible || is_scrollbar_x_visible;
        const bool user_click = _user_bind && _user_bind->on_click_fn;
        const bool user_drag = _user_bind && _user_bind->on_drag_fn;
        const bool user_scroll = _user_bind && _user_bind->on_scroll_fn;
        if (focus_parent() || needs_scroll_events || user_click) add_event_flags(EventFlagBits::click);
        else remove_event_flags(EventFlagBits::click);
        if (needs_scroll_events || user_scroll) add_event_flags(EventFlagBits::scroll);
        else remove_event_flags(EventFlagBits::scroll);
        if (needs_scroll_events || user_drag) add_event_flags(EventFlagBits::drag);
        else remove_event_flags(EventFlagBits::drag);
        if (was_scrollbar_y_visible != is_scrollbar_y_visible || was_scrollbar_x_visible != is_scrollbar_x_visible)
        {
            auto &ctx = detail::get_context();
            ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
            mark_host_refresh_request();
        }
    }

    void DrawBlock::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        _bg_rect.bounds.offset += delta;
        if (_scrollbar_x) _scrollbar_x->translate(delta);
        if (_scrollbar_y) _scrollbar_y->translate(delta);
        if (_content_clip_rect_overridden)
        {
            _content_clip_rect_override.x += delta.x;
            _content_clip_rect_override.y += delta.y;
        }
        rebuild_scroll_clip_rect();
        for (auto *child : children)
        {
            if (!child) continue;
            child->translate(delta);
        }
    }

    void DrawBlock::reset_clip_rect_records()
    {
        Block::reset_clip_rect_records();
        _bg_rect.clip_id = 0xFFFFu;
        _content_clip_id = 0xFFFFu;
        if (_scrollbar_x) _scrollbar_x->reset_clip_rect_records();
        if (_scrollbar_y) _scrollbar_y->reset_clip_rect_records();
    }

    void DrawBlock::rebuild_clip_rects()
    {
        if (_content_clip_rect_overridden) ensure_own_clip_rect(_content_clip_rect_override);
        else if (parent()) set_clip_id(parent()->content_clip_id());
        _bg_rect.clip_id = clip_id();
        rebuild_scroll_clip_rect();
        if (_scrollbar_x) _scrollbar_x->rebuild_clip_rects();
        if (_scrollbar_y) _scrollbar_y->rebuild_clip_rects();
        for (auto *child : children)
        {
            if (!child || !child->is_visible()) continue;
            child->rebuild_clip_rects();
        }
    }

    void DrawBlock::reset_draw_records()
    {
        Block::reset_draw_records();
        _bg_draw_id = {};
        if (_scrollbar_x) _scrollbar_x->reset_draw_records();
        if (_scrollbar_y) _scrollbar_y->reset_draw_records();
    }

    u32 DrawBlock::get_depth_requirement() const
    {
        u32 requirement = Block::get_depth_requirement();
        if (_scrollbar_x && _scrollbar_x->is_visible()) requirement += _scrollbar_x->get_depth_requirement();
        if (_scrollbar_y && _scrollbar_y->is_visible()) requirement += _scrollbar_y->get_depth_requirement();
        return requirement;
    }

    void DrawBlock::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        DepthCursor cursor(this->depth_range(), get_depth_requirement());
        const amal::vec2 bg_range = cursor.next(1u);
        _bg_rect.depth = next_depth(bg_range);
        _bg_rect.hit_depth = _bg_rect.depth;
        auto update_layer = [&](BlockChildLayer layer) {
            for (size_t i = 0u; i < children.size(); ++i)
            {
                auto *child = children[i];
                if (!child || !child->is_visible()) continue;
                const BlockChildLayer child_layer = i < _child_layers.size() ? _child_layers[i] : BlockChildLayer::work;
                if (child_layer != layer) continue;
                child->update_depth(cursor.next(depth_requirement_of(child)));
            }
        };
        update_layer(BlockChildLayer::background);
        update_layer(BlockChildLayer::work);
        update_layer(BlockChildLayer::foreground);
        if (_scrollbar_x && _scrollbar_x->is_visible())
            _scrollbar_x->update_depth(cursor.next(_scrollbar_x->get_depth_requirement()));
        if (_scrollbar_y && _scrollbar_y->is_visible())
            _scrollbar_y->update_depth(cursor.next(_scrollbar_y->get_depth_requirement()));
    }

    void DrawBlock::back_hit_depth()
    {
        Block::back_hit_depth();
        if (_scrollbar_x) _scrollbar_x->back_hit_depth();
        if (_scrollbar_y) _scrollbar_y->back_hit_depth();
    }

    void DrawBlock::restore_hit_depth()
    {
        Block::restore_hit_depth();
        if (_scrollbar_x) _scrollbar_x->restore_hit_depth();
        if (_scrollbar_y) _scrollbar_y->restore_hit_depth();
    }

    void DrawBlock::draw(DrawCtx &ctx)
    {
        if (!is_visible() && !(ctx.reason & DrawReasonBits::invalidate)) return;
        if (draw_style())
        {
            auto *quads_stream = get_primary_quads_stream();
            QuadsInstanceData bg{};
            bg.rect = _bg_rect.bounds;
            bg.z_order = _bg_rect.depth;
            const bool visible = fill_quads_instance_by_style(*draw_style(), clip_id(), bg);
            emit_quads_instance(ctx, quads_stream, _bg_draw_id, bg, _bg_rect, visible, can_emit_hit(ctx));
        }
        Block::draw(ctx);
        if (_scrollbar_y && _scrollbar_y->is_visible())
        {
            DrawCtx scrollbar_ctx = ctx;
            _scrollbar_y->draw_local(scrollbar_ctx);
        }
        if (_scrollbar_x && _scrollbar_x->is_visible())
        {
            DrawCtx scrollbar_ctx = ctx;
            _scrollbar_x->draw_local(scrollbar_ctx);
        }
    }

    WidgetRef::WidgetRef(Widget *target, WidgetFlags widget_flags)
        : Widget(AUIK_TAG_WIDGET_REF, widget_flags, EventFlagBits::none, {{0.0f, 0.0f}, AUIK_SIZE_INHERIT},
                 AUIK_TAG_WIDGET_REF),
          _target(target)
    {
    }

    WidgetRef::~WidgetRef() { restore_target_layout(); }

    void WidgetRef::set_target(Widget *target)
    {
        if (_target == target) return;
        const bool was_active = _ref_active;
        if (was_active) _ref_active = false;
        restore_target_layout();
        _target = target;
        if (was_active)
        {
            _ref_active = true;
            save_target_layout();
        }
        detail::mark_layout_dirty();
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
    }

    void WidgetRef::set_ref_active(bool active)
    {
        if (_ref_active == active) return;
        _ref_active = active;
        if (!_ref_active) restore_target_layout();
        detail::mark_layout_dirty();
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
    }

    void WidgetRef::save_target_layout()
    {
        if (!_target || _saved_layout_valid) return;
        _saved_parent = _target->parent();
        _saved_position = _target->position();
        _saved_size = _target->size();
        _target->set_parent(this);
        _saved_layout_valid = true;
    }

    void WidgetRef::restore_target_layout()
    {
        if (!_target) return;
        if (!_saved_layout_valid) return;
        _target->set_parent(_saved_parent);
        _target->set_position(_saved_position);
        _target->set_layout_size(_saved_size);
        _saved_parent = nullptr;
        _saved_layout_valid = false;
    }

    void WidgetRef::apply_target_layout()
    {
        if (!_target || !_ref_active) return;
        save_target_layout();
        ensure_own_clip_rect({position().x, position().y, size().x, size().y});
        _target->set_position(position());
        _target->set_layout_size(size());
        _target->update_layout(true);
        _target->set_clip_id(content_clip_id());
    }

    StyleUpdateFlags WidgetRef::update_style() { return StyleUpdateFlagBits::none; }

    void WidgetRef::update_layout_min_size_force()
    {
        if (_target) _target->update_layout_min_size();
        set_required_size(_target ? _target->required_size() : amal::vec2{0.0f, 0.0f});
    }

    void WidgetRef::update_layout(bool min_size_known)
    {
        if (layout_measure_required(min_size_known)) update_layout_min_size_force();
        set_layout_size(resolve_layout_size_from_required());
        Widget::update_layout(true);
        ensure_own_clip_rect({position().x, position().y, size().x, size().y});
        apply_target_layout();
    }

    void WidgetRef::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        ensure_own_clip_rect({position().x, position().y, size().x, size().y});
        if (_target && _ref_active) _target->translate(delta);
    }

    void WidgetRef::reset_clip_rect_records()
    {
        Widget::reset_clip_rect_records();
        if (_target && _ref_active) _target->reset_clip_rect_records();
    }

    void WidgetRef::rebuild_clip_rects()
    {
        ensure_own_clip_rect({position().x, position().y, size().x, size().y});
        if (_target && _ref_active)
        {
            _target->rebuild_clip_rects();
            _target->set_clip_id(content_clip_id());
        }
    }

    void WidgetRef::reset_draw_records()
    {
        Widget::reset_draw_records();
        if (_target && _ref_active) _target->reset_draw_records();
    }

    u32 WidgetRef::get_depth_requirement() const
    {
        return 1u + (_target && _ref_active ? amal::max(_target->get_depth_requirement(), 1u) : 0u);
    }

    void WidgetRef::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        if (!_target || !_ref_active) return;
        DepthCursor cursor(this->depth_range(), get_depth_requirement());
        cursor.next(1u);
        _target->update_depth(cursor.next(amal::max(_target->get_depth_requirement(), 1u)));
    }

    void WidgetRef::back_hit_depth()
    {
        Widget::back_hit_depth();
        if (_target && _ref_active) _target->back_hit_depth();
    }

    void WidgetRef::restore_hit_depth()
    {
        Widget::restore_hit_depth();
        if (_target && _ref_active) _target->restore_hit_depth();
    }

    void WidgetRef::draw(DrawCtx &ctx)
    {
        if (!is_visible() && !(ctx.reason & DrawReasonBits::invalidate)) return;
        if (!_target || !_ref_active) return;
        detail::draw_child_in_clip(_target, ctx, {position().x, position().y, size().x, size().y});
    }

    amal::vec2 WidgetRef::requested_size() const
    {
        return _target ? _target->requested_size() : Widget::requested_size();
    }

    WidgetStack::WidgetStack(WidgetFlags widget_flags)
        : Widget(AUIK_TAG_WIDGET_STACK, widget_flags, EventFlagBits::none, {{0.0f, 0.0f}, AUIK_SIZE_INHERIT},
                 AUIK_TAG_WIDGET_STACK)
    {
    }

    WidgetStack::~WidgetStack() { clear_children(); }

    void WidgetStack::clear_children()
    {
        if (!_children.empty()) invalidate_layout_measure();
        for (size_t i = _children.size(); i > 0u; --i)
        {
            auto *child = _children[i - 1u];
            if (!child) continue;
            set_child_ref_active(child, false);
            if (child->widget_flags & WidgetFlagBits::attachable) child->on_detach();
            acul::release(child);
        }
        _children.clear();
        _child_layers.clear();
        _active_index = 0u;
    }

    void WidgetStack::set_child_ref_active(Widget *child, bool active)
    {
        if (!child || child->signature() != AUIK_TAG_WIDGET_REF) return;
        static_cast<WidgetRef *>(child)->set_ref_active(active);
    }

    void WidgetStack::add_layer_child(Widget *child, BlockChildLayer layer)
    {
        assert(child && "child is null");
        child->set_parent(this);
        child->set_focus_parent(parent() && id() == parent()->id() ? parent() : this);
        child->update_style_invalidated();
        _children.push_back(child);
        _child_layers.push_back(layer);
        if (layer == BlockChildLayer::work && active_child() == child) set_child_ref_active(child, true);
        invalidate_layout_measure();
    }

    void WidgetStack::add_child(Widget *child) { add_layer_child(child, BlockChildLayer::work); }

    void WidgetStack::add_child_to_background(Widget *child) { add_layer_child(child, BlockChildLayer::background); }

    void WidgetStack::add_child_to_foreground(Widget *child) { add_layer_child(child, BlockChildLayer::foreground); }

    Widget *WidgetStack::active_child() const
    {
        size_t work_index = 0u;
        for (size_t i = 0u; i < _children.size(); ++i)
        {
            const auto layer = i < _child_layers.size() ? _child_layers[i] : BlockChildLayer::work;
            if (layer != BlockChildLayer::work) continue;
            if (work_index == _active_index) return _children[i];
            ++work_index;
        }
        return nullptr;
    }

    bool WidgetStack::is_active_child(const Widget *child) const { return child && active_child() == child; }

    bool WidgetStack::accepts_child_style_update(const Widget *child) const
    {
        for (size_t i = 0u; i < _children.size(); ++i)
        {
            if (_children[i] != child) continue;
            const auto layer = i < _child_layers.size() ? _child_layers[i] : BlockChildLayer::work;
            return layer != BlockChildLayer::work || is_active_child(child);
        }
        return true;
    }

    void WidgetStack::set_active_index(size_t index)
    {
        size_t work_count = 0u;
        for (size_t i = 0u; i < _children.size(); ++i)
        {
            const auto layer = i < _child_layers.size() ? _child_layers[i] : BlockChildLayer::work;
            if (layer == BlockChildLayer::work) ++work_count;
        }
        if (index >= work_count || index == _active_index) return;
        set_child_ref_active(active_child(), false);
        _active_index = index;
        set_child_ref_active(active_child(), true);
        reset_clip_rect_records();
        detail::mark_layout_dirty();
        rebuild_root_widget_depths();
        redraw_all_commands();
    }

    StyleUpdateFlags WidgetStack::update_style()
    {
        StyleUpdateFlags out = StyleUpdateFlagBits::none;
        for (auto *child : _children)
            if (child && child->is_visible()) out |= child->update_style_invalidated();
        return out;
    }

    void WidgetStack::update_layout_min_size_force()
    {
        if (auto *child = active_child())
        {
            child->update_layout_min_size();
            set_required_size(child->required_size());
        }
        else set_required_size({0.0f, 0.0f});
        for (size_t i = 0u; i < _children.size(); ++i)
        {
            auto *child = _children[i];
            if (!child || !child->is_visible()) continue;
            const auto layer = i < _child_layers.size() ? _child_layers[i] : BlockChildLayer::work;
            if (layer == BlockChildLayer::work) continue;
            child->update_layout_min_size();
        }
    }

    void WidgetStack::update_layer_layout(BlockChildLayer layer)
    {
        for (size_t i = 0u; i < _children.size(); ++i)
        {
            auto *child = _children[i];
            if (!child || !child->is_visible()) continue;
            const auto child_layer = i < _child_layers.size() ? _child_layers[i] : BlockChildLayer::work;
            if (child_layer != layer) continue;
            child->set_position(position());
            child->set_layout_size(size());
            child->update_layout(true);
        }
    }

    void WidgetStack::update_layout(bool min_size_known)
    {
        if (layout_measure_required(min_size_known)) update_layout_min_size_force();
        set_layout_size(resolve_layout_size_from_required());
        Widget::update_layout(true);
        set_clip_id(content_clip_id());
        update_layer_layout(BlockChildLayer::background);
        if (auto *child = active_child())
        {
            child->set_position(position());
            child->set_layout_size(size());
            child->update_layout(true);
        }
        update_layer_layout(BlockChildLayer::foreground);
    }

    void WidgetStack::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        if (auto *child = active_child()) child->translate(delta);
        for (size_t i = 0u; i < _children.size(); ++i)
        {
            auto *child = _children[i];
            if (!child || child == active_child()) continue;
            const auto layer = i < _child_layers.size() ? _child_layers[i] : BlockChildLayer::work;
            if (layer != BlockChildLayer::background && layer != BlockChildLayer::foreground) continue;
            child->translate(delta);
        }
    }

    void WidgetStack::reset_clip_rect_records()
    {
        Widget::reset_clip_rect_records();
        if (auto *child = active_child()) child->reset_clip_rect_records();
        for (size_t i = 0u; i < _children.size(); ++i)
        {
            auto *child = _children[i];
            if (!child || child == active_child()) continue;
            const auto layer = i < _child_layers.size() ? _child_layers[i] : BlockChildLayer::work;
            if (layer != BlockChildLayer::background && layer != BlockChildLayer::foreground) continue;
            child->reset_clip_rect_records();
        }
    }

    void WidgetStack::rebuild_clip_rects()
    {
        set_clip_id(content_clip_id());
        for (size_t i = 0u; i < _children.size(); ++i)
        {
            auto *child = _children[i];
            if (!child || !child->is_visible()) continue;
            const auto layer = i < _child_layers.size() ? _child_layers[i] : BlockChildLayer::work;
            if (layer == BlockChildLayer::background) child->rebuild_clip_rects();
        }
        if (auto *child = active_child()) child->rebuild_clip_rects();
        for (size_t i = 0u; i < _children.size(); ++i)
        {
            auto *child = _children[i];
            if (!child || !child->is_visible()) continue;
            const auto layer = i < _child_layers.size() ? _child_layers[i] : BlockChildLayer::work;
            if (layer == BlockChildLayer::foreground) child->rebuild_clip_rects();
        }
    }

    void WidgetStack::reset_draw_records()
    {
        Widget::reset_draw_records();
        if (auto *child = active_child()) child->reset_draw_records();
        for (size_t i = 0u; i < _children.size(); ++i)
        {
            auto *child = _children[i];
            if (!child || child == active_child()) continue;
            const auto layer = i < _child_layers.size() ? _child_layers[i] : BlockChildLayer::work;
            if (layer != BlockChildLayer::background && layer != BlockChildLayer::foreground) continue;
            child->reset_draw_records();
        }
    }

    u32 WidgetStack::get_depth_requirement() const
    {
        u32 requirement = 1u;
        auto add_layer = [&](BlockChildLayer layer) {
            for (size_t i = 0u; i < _children.size(); ++i)
            {
                const auto child_layer = i < _child_layers.size() ? _child_layers[i] : BlockChildLayer::work;
                if (child_layer != layer) continue;
                auto *child = _children[i];
                if (!child || !child->is_visible()) continue;
                if (layer == BlockChildLayer::work && child != active_child()) continue;
                requirement += amal::max(child->get_depth_requirement(), 1u);
            }
        };
        add_layer(BlockChildLayer::background);
        add_layer(BlockChildLayer::work);
        add_layer(BlockChildLayer::foreground);
        return requirement;
    }

    void WidgetStack::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        DepthCursor cursor(this->depth_range(), get_depth_requirement());
        cursor.next(1u);
        auto update_layer = [&](BlockChildLayer layer) {
            for (size_t i = 0u; i < _children.size(); ++i)
            {
                auto *child = _children[i];
                if (!child || !child->is_visible()) continue;
                const auto child_layer = i < _child_layers.size() ? _child_layers[i] : BlockChildLayer::work;
                if (child_layer != layer) continue;
                if (layer == BlockChildLayer::work && child != active_child()) continue;
                child->update_depth(cursor.next(amal::max(child->get_depth_requirement(), 1u)));
            }
        };
        update_layer(BlockChildLayer::background);
        update_layer(BlockChildLayer::work);
        update_layer(BlockChildLayer::foreground);
    }

    void WidgetStack::back_hit_depth()
    {
        Widget::back_hit_depth();
        if (auto *child = active_child()) child->back_hit_depth();
        for (size_t i = 0u; i < _children.size(); ++i)
        {
            auto *child = _children[i];
            if (!child || child == active_child()) continue;
            const auto layer = i < _child_layers.size() ? _child_layers[i] : BlockChildLayer::work;
            if (layer != BlockChildLayer::background && layer != BlockChildLayer::foreground) continue;
            child->back_hit_depth();
        }
    }

    void WidgetStack::restore_hit_depth()
    {
        Widget::restore_hit_depth();
        if (auto *child = active_child()) child->restore_hit_depth();
        for (size_t i = 0u; i < _children.size(); ++i)
        {
            auto *child = _children[i];
            if (!child || child == active_child()) continue;
            const auto layer = i < _child_layers.size() ? _child_layers[i] : BlockChildLayer::work;
            if (layer != BlockChildLayer::background && layer != BlockChildLayer::foreground) continue;
            child->restore_hit_depth();
        }
    }

    void WidgetStack::draw(DrawCtx &ctx)
    {
        if (!is_visible() && !(ctx.reason & DrawReasonBits::invalidate)) return;
        auto draw_layer = [&](BlockChildLayer layer) {
            for (size_t i = 0u; i < _children.size(); ++i)
            {
                auto *child = _children[i];
                if (!child || (!child->is_visible() && !(ctx.reason & DrawReasonBits::invalidate))) continue;
                const auto child_layer = i < _child_layers.size() ? _child_layers[i] : BlockChildLayer::work;
                if (child_layer != layer) continue;
                if (layer == BlockChildLayer::work && child != active_child()) continue;
                detail::draw_child_in_clip(child, ctx, get_content_clip_rect());
            }
        };
        draw_layer(BlockChildLayer::background);
        if (auto *child = active_child()) detail::draw_child_in_clip(child, ctx, get_content_clip_rect());
        draw_layer(BlockChildLayer::foreground);
    }

    void WidgetStack::on_attach()
    {
        Widget::on_attach();
        for (auto *child : _children)
            if (child && (child->widget_flags & WidgetFlagBits::attachable)) child->on_attach();
    }

    void WidgetStack::on_detach()
    {
        for (auto *child : _children)
            if (child && (child->widget_flags & WidgetFlagBits::attachable)) child->on_detach();
        Widget::on_detach();
    }

    amal::vec2 WidgetStack::requested_size() const
    {
        if (auto *child = active_child()) return child->requested_size();
        return Widget::requested_size();
    }

    void DrawBlock::on_scroll(const amal::vec2 &delta)
    {
        const amal::vec2 step = -delta * f32(AUIK_SCROLL_STEP);
        const amal::vec2 old_offset = _content_offset;
        if (_scrollbar_y && _scrollbar_y->is_visible())
        {
            _scrollbar_y->set_scroll_offset(_content_offset.y);
            _scrollbar_y->scroll_by_pixels(step.y);
            _content_offset.y = _scrollbar_y->scroll_offset();
        }
        if (_scrollbar_x && _scrollbar_x->is_visible())
        {
            _scrollbar_x->set_scroll_offset(_content_offset.x);
            _scrollbar_x->scroll_by_pixels(step.x);
            _content_offset.x = _scrollbar_x->scroll_offset();
        }
        if (_content_offset != old_offset) request_scroll_layout_update(DrawReasonBits::layout);
    }

    void DrawBlock::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        (void)click_count;
        if (key != MouseKey::left || state != KeyPressState::press) return;

        auto &ctx = detail::get_context();
        if (!detail::is_scrollbar_tag(ctx.hover_id.tag_id))
        {
            if (focus_parent()) focus_widget(focus_parent());
            return;
        }

        bool is_offset_changed = false;
        if (_scrollbar_y && _scrollbar_y->is_visible() &&
            (ctx.hover_id.tag_id == AUIK_TAG_SCROLLBAR_TRACK_Y || ctx.hover_id.tag_id == AUIK_TAG_SCROLLBAR_THUMB_Y))
        {
            _scrollbar_y->set_scroll_offset(_content_offset.y);
            if (ctx.hover_id.tag_id == AUIK_TAG_SCROLLBAR_THUMB_Y) _scrollbar_y->begin_thumb_drag(ctx.io.mouse_pos);
            else
            {
                is_offset_changed = _scrollbar_y->scroll_to_track_click(ctx.io.mouse_pos) || is_offset_changed;
                activate_scrollbar_thumb_style(this, AUIK_TAG_SCROLLBAR_THUMB_Y);
            }
            _content_offset.y = _scrollbar_y->scroll_offset();
        }

        if (_scrollbar_x && _scrollbar_x->is_visible() &&
            (ctx.hover_id.tag_id == AUIK_TAG_SCROLLBAR_TRACK_X || ctx.hover_id.tag_id == AUIK_TAG_SCROLLBAR_THUMB_X))
        {
            _scrollbar_x->set_scroll_offset(_content_offset.x);
            if (ctx.hover_id.tag_id == AUIK_TAG_SCROLLBAR_THUMB_X) _scrollbar_x->begin_thumb_drag(ctx.io.mouse_pos);
            else
            {
                is_offset_changed = _scrollbar_x->scroll_to_track_click(ctx.io.mouse_pos) || is_offset_changed;
                activate_scrollbar_thumb_style(this, AUIK_TAG_SCROLLBAR_THUMB_X);
            }
            _content_offset.x = _scrollbar_x->scroll_offset();
        }

        if (is_offset_changed) request_scroll_layout_update(DrawReasonBits::layout);
    }

    void DrawBlock::on_drag(const amal::vec2 &delta, KeyPressState state)
    {
        auto &ctx = detail::get_context();
        const auto drag_id = ctx.io.drag_id;
        const bool drag_scrollbar_y =
            _scrollbar_y && _scrollbar_y->is_visible() && detail::is_scrollbar_y_drag(drag_id, id());
        const bool drag_scrollbar_x =
            _scrollbar_x && _scrollbar_x->is_visible() && detail::is_scrollbar_x_drag(drag_id, id());
        if (!drag_scrollbar_y && !drag_scrollbar_x) return;

        const bool thumb_drag = detail::is_scrollbar_thumb_drag(drag_id);
        if (state == KeyPressState::press)
        {
            if (thumb_drag && drag_scrollbar_y) _scrollbar_y->begin_thumb_drag(ctx.io.mouse_pos);
            if (thumb_drag && drag_scrollbar_x) _scrollbar_x->begin_thumb_drag(ctx.io.mouse_pos);
            return;
        }
        if (state == KeyPressState::release) return;

        bool is_offset_changed = false;
        if (drag_scrollbar_y)
        {
            _scrollbar_y->set_scroll_offset(_content_offset.y);
            is_offset_changed = thumb_drag ? _scrollbar_y->scroll_thumb_to_mouse_pos(ctx.io.mouse_pos)
                                           : _scrollbar_y->scroll_thumb_by_drag_delta(delta);
            _content_offset.y = _scrollbar_y->scroll_offset();
        }
        if (drag_scrollbar_x)
        {
            _scrollbar_x->set_scroll_offset(_content_offset.x);
            is_offset_changed = thumb_drag ? _scrollbar_x->scroll_thumb_to_mouse_pos(ctx.io.mouse_pos)
                                           : _scrollbar_x->scroll_thumb_by_drag_delta(delta);
            _content_offset.x = _scrollbar_x->scroll_offset();
        }
        if (is_offset_changed) request_scroll_layout_update(DrawReasonBits::layout);
    }

    CollapseHeader::CollapseHeader(u32 id, StringView label, bool expanded, WidgetFlags widget_flags, u32 style_tag_id)
        : Block(id, widget_flags, AUIK_TAG_COLLAPSE_HEADER),
          _style({Theme::STYLE_ID_INVALID, style_tag_id}),
          _expanded(expanded)
    {
        set_size({AUIK_SIZE_X_FILL, AUIK_SIZE_Y_FIT});
        add_event_flags(EventFlagBits::click);
        set_rect_tag_id(current_header_style_tag());
        _label = acul::alloc<Text>(AUIK_TAG_TEXT, label, amal::vec2{0.0f, 0.0f}, WidgetFlagBits::visible,
                                   make_text_layout_flags(TextOverflowMode::ellipsis));
        _label->set_style_tag(AUIK_STYLE_TAG_NO_PAD);
        _label->set_parent(this);
        _trigger = acul::alloc<detail::PopupTrigger>(_trigger_style_tag, AUIK_TAG_COLLAPSE_HEADER_TRIGGER,
                                                     AUIK_ICON_CHEVRON_RIGHT, AUIK_ICON_CHEVRON_DOWN, true,
                                                     amal::half_pi<f32>());
        _trigger->set_update_target(this);
        _trigger->set_hit_id(make_element_id(id, AUIK_TAG_COLLAPSE_HEADER_TRIGGER, 0u));
        _trigger->set_open(_expanded);
        _header_rect = detail::make_rect_data(id, current_header_style_tag());
        _content_rect = detail::make_rect_data(id, _content_style.tag_id);
    }

    CollapseHeader::~CollapseHeader()
    {
        if (_label)
        {
            acul::release(_label);
            _label = nullptr;
        }
        if (_trigger)
        {
            acul::release(_trigger);
            _trigger = nullptr;
        }
    }

    void CollapseHeader::set_label(StringView value)
    {
        if (!_label) return;
        const acul::string next = value.str ? value.str : "";
        if (_label->text() == next && (!value.is_translated || _label->is_translated_text())) return;
        _label->set_text(value);
        if (!mark_changed()) invalidate_layout();
    }

    const acul::string &CollapseHeader::label() const
    {
        static const acul::string empty;
        return _label ? _label->text() : empty;
    }

    void CollapseHeader::set_expanded(bool value)
    {
        if (_expanded == value) return;
        _expanded = value;
        _style.id = Theme::STYLE_ID_INVALID;
        if (_expanded) _content_style.id = Theme::STYLE_ID_INVALID;
        set_required_size({0.0f, 0.0f});
        set_rect_tag_id(current_header_style_tag());
        _header_rect.id.tag_id = current_header_style_tag();
        if (!_expanded)
        {
            for (auto *child : children)
                if (child) child->invalidate_draw_commands(DrawReasonBits::layout);
            if (_content_bg.render_id != AUIK_INVALID_DRAW_DATA_ID)
            {
                if (auto *stream = get_primary_quads_stream(); stream && stream->invalidate_data_in_stream)
                    stream->invalidate_data_in_stream(stream, _content_bg);
                _content_bg = {};
            }
        }
        if (_trigger)
        {
            _trigger->set_open(_expanded);
            _trigger->start_icon_animation(_expanded);
        }
        invalidate_layout();
    }

    void CollapseHeader::set_style_tag(u32 tag_id)
    {
        if (_style.tag_id == tag_id) return;
        _style = {Theme::STYLE_ID_INVALID, tag_id};
        set_rect_tag_id(current_header_style_tag());
        _header_rect.id.tag_id = current_header_style_tag();
        invalidate_layout();
    }

    void CollapseHeader::set_closed_style_tag(u32 tag_id)
    {
        if (_closed_style_tag == tag_id) return;
        _closed_style_tag = tag_id;
        if (!_expanded)
        {
            set_rect_tag_id(current_header_style_tag());
            _header_rect.id.tag_id = current_header_style_tag();
        }
        invalidate_layout();
    }

    void CollapseHeader::set_content_style_tag(u32 tag_id)
    {
        if (_content_style.tag_id == tag_id) return;
        _content_style = {Theme::STYLE_ID_INVALID, tag_id};
        _content_rect.id.tag_id = tag_id;
        invalidate_layout();
    }

    void CollapseHeader::set_trigger_style_tag(u32 tag_id)
    {
        if (_trigger_style_tag == tag_id) return;
        _trigger_style_tag = tag_id;
        if (_trigger)
        {
            acul::release(_trigger);
            _trigger = acul::alloc<detail::PopupTrigger>(_trigger_style_tag, AUIK_TAG_COLLAPSE_HEADER_TRIGGER,
                                                         AUIK_ICON_CHEVRON_RIGHT, AUIK_ICON_CHEVRON_DOWN, true,
                                                         amal::half_pi<f32>());
            _trigger->set_update_target(this);
            _trigger->set_hit_id(make_element_id(id(), AUIK_TAG_COLLAPSE_HEADER_TRIGGER, 0u));
            _trigger->set_open(_expanded);
        }
        invalidate_layout();
    }

    StyleUpdateFlags CollapseHeader::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        const u32 header_style_tag = current_header_style_tag();
        if (get_rect().id.tag_id != header_style_tag) set_rect_tag_id(header_style_tag);
        _header_rect.id.tag_id = header_style_tag;
        StyleSelector header_style{_style.id, header_style_tag};
        StyleUpdateFlags out = resolve_style_selector(header_style, id(), parent_id, style_state());
        _style.id = header_style.id;
        if (_label) out |= _label->update_style_invalidated();
        if (_trigger) out |= _trigger->update_style(id(), parent_id, style_state());
        if (_expanded) out |= resolve_style_selector(_content_style, _content_style.tag_id, id(), StyleState::normal);
        if (_expanded)
        {
            for (auto *child : children)
                if (child && child->is_visible()) out |= child->update_style_invalidated();
        }
        return out;
    }

    amal::vec2 CollapseHeader::compute_content_min_size()
    {
        amal::vec2 required{0.0f, 0.0f};
        if (!_expanded) return required;
        for (auto *child : children)
        {
            if (!child || !child->is_visible()) continue;
            child->update_layout_min_size();
            const auto child_required = child->required_size();
            required.x = amal::max(required.x, child_required.x);
            required.y += child_required.y;
        }
        return required;
    }

    void CollapseHeader::update_layout_min_size_force()
    {
        if (_style.id == Theme::STYLE_ID_INVALID || (_expanded && _content_style.id == Theme::STYLE_ID_INVALID))
            update_style();
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();
        amal::vec4 content_margin{0.0f};
        amal::vec4 content_padding{0.0f};
        if (_expanded)
        {
            const auto &content_style = get_theme()->get_style(_content_style.id);
            content_margin = content_style.margin();
            content_padding = content_style.padding();
        }

        if (_trigger) _trigger->update_layout_min_size_force({0.0f, 0.0f}, true);
        if (_label) _label->update_layout_min_size();
        const amal::vec2 trigger_required = _trigger ? _trigger->required_size() : amal::vec2{0.0f, 0.0f};
        const amal::vec2 label_required = _label ? _label->required_size() : amal::vec2{0.0f, 0.0f};
        const f32 spacing = amal::max(style.inline_spacing(), 0.0f);
        const f32 header_w = padding.x + padding.z + trigger_required.x + spacing + label_required.x;
        const f32 header_h = padding.y + padding.w + amal::max(trigger_required.y, label_required.y);

        const amal::vec2 content_required = compute_content_min_size();
        const amal::vec2 content_outer = _expanded
                                             ? amal::vec2{content_margin.x + content_margin.z + content_padding.x +
                                                              content_padding.z + content_required.x,
                                                          content_margin.y + content_margin.w + content_padding.y +
                                                              content_padding.w + content_required.y}
                                             : amal::vec2{0.0f, 0.0f};
        const f32 resolved_w = fill_width() ? 0.0f : amal::max(header_w, content_outer.x);
        set_required_size({margin.x + margin.z + resolved_w, margin.y + margin.w + header_h + content_outer.y});
    }

    void CollapseHeader::update_layout(bool min_size_known)
    {
        if (layout_measure_required(min_size_known)) update_layout_min_size_force();
        if (_style.id == Theme::STYLE_ID_INVALID || (_expanded && _content_style.id == Theme::STYLE_ID_INVALID))
            update_style();

        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();
        amal::vec4 content_margin{0.0f};
        amal::vec4 content_padding{0.0f};
        if (_expanded)
        {
            const auto &content_style = get_theme()->get_style(_content_style.id);
            content_margin = content_style.margin();
            content_padding = content_style.padding();
        }
        const f32 spacing = amal::max(style.inline_spacing(), 0.0f);
        const amal::vec2 layout_origin = position();
        const amal::vec2 inner_required = {amal::max(required_size().x - margin.x - margin.z, 0.0f),
                                           amal::max(required_size().y - margin.y - margin.w, 0.0f)};
        amal::vec2 inner_size = {amal::max(size().x - margin.x - margin.z, 0.0f),
                                 amal::max(size().y - margin.y - margin.w, 0.0f)};
        if (is_width_fixed() || inner_size.x <= 0.0f) inner_size.x = amal::max(inner_size.x, inner_required.x);
        inner_size.y = amal::max(inner_size.y, inner_required.y);

        set_position({layout_origin.x + margin.x, layout_origin.y + margin.y});
        Widget::set_layout_size(inner_size);
        Widget::update_layout(true);
        set_clip_id(content_clip_id());

        const amal::vec2 trigger_required = _trigger ? _trigger->required_size() : amal::vec2{0.0f, 0.0f};
        const amal::vec2 label_required = _label ? _label->required_size() : amal::vec2{0.0f, 0.0f};
        const f32 header_h = padding.y + padding.w + amal::max(trigger_required.y, label_required.y);
        _header_rect.id = make_element_id(id(), current_header_style_tag());
        _header_rect.bounds = {position(), {size().x, header_h}};
        _header_rect.clip_id = clip_id();

        const f32 content_y = position().y + padding.y;
        f32 cursor_x = position().x + padding.x;
        if (_trigger)
        {
            const f32 trigger_y =
                content_y + amal::max((header_h - padding.y - padding.w - trigger_required.y) * 0.5f, 0.0f);
            _trigger->update_layout({{cursor_x, trigger_y}, trigger_required}, clip_id());
            cursor_x += trigger_required.x + spacing;
        }
        if (_label)
        {
            const f32 label_y =
                content_y + amal::max((header_h - padding.y - padding.w - label_required.y) * 0.5f, 0.0f);
            const f32 label_w = amal::max(size().x - padding.z - (cursor_x - position().x), 0.0f);
            _label->set_position({cursor_x, label_y});
            _label->set_layout_size({label_w, label_required.y});
            _label->update_layout(true);
        }

        if (_expanded)
        {
            const amal::vec2 content_pos{position().x + content_margin.x, position().y + header_h + content_margin.y};
            const amal::vec2 content_size{amal::max(size().x - content_margin.x - content_margin.z, 0.0f),
                                          amal::max(size().y - header_h - content_margin.y - content_margin.w, 0.0f)};
            _content_rect.id = make_element_id(id(), _content_style.tag_id);
            _content_rect.bounds = {content_pos, content_size};
            _content_rect.clip_id = clip_id();
            const amal::vec2 child_pos{content_pos.x + content_padding.x, content_pos.y + content_padding.y};
            const amal::vec2 child_size{amal::max(content_size.x - content_padding.x - content_padding.z, 0.0f),
                                        amal::max(content_size.y - content_padding.y - content_padding.w, 0.0f)};
            layout_children({child_pos, child_size});
        }
    }

    void CollapseHeader::layout_children(const amal::rect &content_rect)
    {
        detail::layout_child_widgets(this, children, _child_layouts, content_rect, resolved_inline_spacing());
    }

    void CollapseHeader::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        _header_rect.bounds.offset += delta;
        _content_rect.bounds.offset += delta;
        if (_trigger) _trigger->translate(delta);
        if (_label) _label->translate(delta);
        if (_expanded)
        {
            for (auto *child : children)
                if (child && child->is_visible()) child->translate(delta);
        }
    }

    void CollapseHeader::rebuild_clip_rects()
    {
        set_clip_id(content_clip_id());
        _header_rect.clip_id = clip_id();
        _content_rect.clip_id = clip_id();
        if (_trigger) _trigger->rebuild_clip_rects(clip_id());
        if (_label)
        {
            _label->set_clip_id(content_clip_id());
            _label->rebuild_clip_rects();
        }
        if (_expanded)
        {
            for (auto *child : children)
                if (child && child->is_visible()) child->rebuild_clip_rects();
        }
    }

    void CollapseHeader::reset_draw_records()
    {
        _header_bg = {};
        _content_bg = {};
        if (_trigger) _trigger->reset_draw_records();
        if (_label) _label->reset_draw_records();
        for (auto *child : children)
            if (child) child->reset_draw_records();
    }

    void CollapseHeader::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        DepthCursor cursor(this->depth_range(), 3u);
        const amal::vec2 content_range = cursor.next(1u);
        const amal::vec2 header_bg_range = cursor.next(1u);
        const amal::vec2 header_content_range = cursor.next(1u);
        if (_trigger)
        {
            _trigger->update_depth(header_content_range);
        }
        if (_label)
        {
            _label->update_depth(header_content_range);
        }
        for (auto *child : children)
        {
            if (!child || !child->is_visible()) continue;
            child->update_depth(content_range);
        }
        _header_rect.depth = next_depth(detail::depth_background_range(header_bg_range));
        _header_rect.hit_depth = _header_rect.depth;
        _content_rect.depth = next_depth(detail::depth_background_range(content_range));
        _content_rect.hit_depth = _content_rect.depth;
    }

    void CollapseHeader::back_hit_depth()
    {
        Block::back_hit_depth();
        if (_trigger) _trigger->back_hit_depth();
        if (_label) _label->back_hit_depth();
        _header_rect.hit_depth = get_rect().hit_depth;
        _content_rect.hit_depth = _header_rect.hit_depth;
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
    }

    void CollapseHeader::restore_hit_depth()
    {
        Block::restore_hit_depth();
        if (_trigger) _trigger->restore_hit_depth();
        if (_label) _label->restore_hit_depth();
        _header_rect.hit_depth = _header_rect.depth;
        _content_rect.hit_depth = _content_rect.depth;
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
    }

    void CollapseHeader::draw(DrawCtx &ctx)
    {
        if (!is_visible() && !(ctx.reason & DrawReasonBits::invalidate)) return;
        auto *quads_stream = get_primary_quads_stream();
        QuadsInstanceData bg{};
        bg.rect = _header_rect.bounds;
        bg.z_order = _header_rect.depth;
        const bool bg_visible = fill_quads_instance_by_style(get_theme()->get_style(_style.id), clip_id(), bg);
        emit_quads_instance(ctx, quads_stream, _header_bg, bg, _header_rect, bg_visible, can_emit_hit(ctx));
        if (_trigger) _trigger->draw(ctx, false);
        if (_label)
        {
            DrawCtx label_ctx = ctx;
            label_ctx.is_hit_allowed = false;
            _label->draw_local(label_ctx);
        }
        if (_expanded)
        {
            QuadsInstanceData content_bg{};
            content_bg.rect = _content_rect.bounds;
            content_bg.z_order = _content_rect.depth;
            const bool content_bg_visible =
                fill_quads_instance_by_style(get_theme()->get_style(_content_style.id), clip_id(), content_bg);
            emit_quads_instance(ctx, quads_stream, _content_bg, content_bg, _content_rect, content_bg_visible, false);
            const amal::vec4 content_clip = get_content_clip_rect();
            for (auto *child : children)
            {
                if (!child || (!child->is_visible() && !(ctx.reason & DrawReasonBits::invalidate))) continue;
                detail::draw_child_in_clip(child, ctx, content_clip);
            }
        }
    }

    void CollapseHeader::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        (void)click_count;
        if (key != MouseKey::left || state != KeyPressState::press) return;
        const auto hover = detail::get_context().hover_id;
        if (hover.widget_id != id() || hover.tag_id != current_header_style_tag()) return;
        add_render_command<detail::ClickEventTraits>(this, [this]() { toggle(); });
        mark_host_refresh_request();
    }

    u32 CollapseHeader::current_header_style_tag() const { return _expanded ? _style.tag_id : _closed_style_tag; }

    void CollapseHeader::on_attach() { Block::on_attach(); }

    void CollapseHeader::on_detach() { Block::on_detach(); }

    void CollapseHeader::invalidate_layout()
    {
        invalidate_layout_measure();
        auto *layout_parent = parent();
        if (!layout_parent || clip_id() == 0xFFFFu || layout_parent->clip_id() == 0xFFFFu) return;

        auto &ctx = detail::get_context();
        ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
        layout_parent->update_layout(false);
        layout_parent->update_draw_commands(DrawReasonBits::layout);
        mark_host_refresh_request();
    }

    StyleUpdateFlags Dummy::update_style()
    {
        if (_style_tag_id == 0u) return StyleUpdateFlagBits::none;
        const auto flags = resolve_style_selector(_style, id(), parent() ? parent()->id() : 0u, style_state());
        apply_style_layout(get_theme()->get_style(_style.id));
        return flags;
    }

    void Dummy::update_layout_min_size_force()
    {
        amal::vec4 margin{0.0f};
        amal::vec4 padding{0.0f};
        if (_style_tag_id != 0u && _style.id != Theme::STYLE_ID_INVALID)
        {
            const auto &style = get_theme()->get_style(_style.id);
            margin = style.margin();
            padding = style.padding();
        }
        set_required_size({margin.x + margin.z + padding.x + padding.z + size().x,
                           margin.y + margin.w + padding.y + padding.w + size().y});
    }

    void Dummy::update_layout(bool min_size_known)
    {
        if (layout_measure_required(min_size_known)) update_layout_min_size_force();
        amal::vec4 margin{0.0f};
        if (_style_tag_id != 0u)
        {
            if (_style.id == Theme::STYLE_ID_INVALID) update_style();
            margin = get_theme()->get_style(_style.id).margin();
        }
        const amal::vec2 min_size = {amal::max(required_size().x - margin.x - margin.z, 0.0f),
                                     amal::max(required_size().y - margin.y - margin.w, 0.0f)};
        amal::vec2 layout_size = {amal::max(size().x - margin.x - margin.z, 0.0f),
                                  amal::max(size().y - margin.y - margin.w, 0.0f)};
        if (fill_width() || is_width_fixed()) layout_size.x = amal::max(layout_size.x, min_size.x);
        else layout_size.x = min_size.x;
        if (fill_height() || is_height_fixed()) layout_size.y = amal::max(layout_size.y, min_size.y);
        else layout_size.y = min_size.y;
        if (parent()) set_position(position() + amal::vec2{margin.x, margin.y});
        set_layout_size(layout_size);
        Widget::update_layout(true);
    }

    namespace
    {
        struct BlockSizeData
        {
            amal::vec2 explicit_size = AUIK_SIZE_FIT;
        };

        struct BlockChildData
        {
            umbf::Block *block = nullptr;
            ChildLayoutFlags layout = default_child_layout_flags();
        };

        struct BlockHeaderData
        {
            detail::WidgetCommonData common{};
            BlockSizeData size{};
        };

        acul::vector<BlockChildData> collect_block_children(const Block &block)
        {
            acul::vector<BlockChildData> out;
            const auto &layouts = block.child_layouts();
            for (size_t child_i = 0u; child_i < block.children.size(); ++child_i)
            {
                auto *child = block.children[child_i];
                if (!(child->widget_flags & WidgetFlagBits::configurable)) continue;
                const ChildLayoutFlags layout =
                    child_i < layouts.size() ? layouts[child_i] : default_child_layout_flags();
                out.push_back({child, layout});
            }
            return out;
        }

        void write_block_size_data(acul::bin_stream &stream, const Block &block)
        {
            stream.write(block.explicit_size());
        }

        BlockSizeData read_block_size_data(acul::bin_stream &stream)
        {
            BlockSizeData out{};
            stream.read(out.explicit_size);
            return out;
        }

        void apply_block_size_data(Block *block, const BlockSizeData &size) { block->set_size(size.explicit_size); }

        void write_block_children(acul::bin_stream &stream, const Block &block)
        {
            auto children = collect_block_children(block);
            stream.write(static_cast<u32>(children.size()));

            acul::vector<umbf::Block *> blocks;
            blocks.reserve(children.size());
            for (auto &child : children)
            {
                stream.write(static_cast<u32>(child.layout));
                blocks.push_back(child.block);
            }
            stream.write(blocks);
        }

        void read_block_children(acul::bin_stream &stream, Block *block)
        {
            u32 child_count = 0u;
            stream.read(child_count);

            acul::vector<ChildLayoutFlags> layouts;
            layouts.reserve(child_count);
            for (u32 child_i = 0u; child_i < child_count; ++child_i)
            {
                u32 layout = 0u;
                stream.read(layout);
                layouts.push_back(ChildLayoutFlags(layout));
            }

            acul::vector<umbf::Block *> children;
            stream.read(children);
            for (u32 child_i = 0u; child_i < child_count; ++child_i)
                block->add_child(static_cast<Widget *>(children[child_i]), layouts[child_i]);
        }

        void write_block_payload(acul::bin_stream &stream, const Block &block)
        {
            detail::write_widget_common_data(stream, block);
            write_block_size_data(stream, block);
            write_block_children(stream, block);
        }

        BlockHeaderData read_block_header_data(acul::bin_stream &stream)
        {
            BlockHeaderData out{};
            out.common = detail::read_widget_common_data(stream);
            out.size = read_block_size_data(stream);
            return out;
        }

        void apply_block_header_data(Block *block, const BlockHeaderData &header)
        {
            detail::apply_widget_common_data(block, header.common);
            apply_block_size_data(block, header.size);
        }

        static u32 serializable_widget_ref_flags(WidgetFlags flags)
        {
            return static_cast<u32>(flags) & static_cast<u32>(WidgetFlagBits::visible | WidgetFlagBits::configurable);
        }

        void write_widget_ref_common_data(acul::bin_stream &stream, const WidgetRef &widget)
        {
            stream.write(widget.id())
                .write(serializable_widget_ref_flags(widget.widget_flags))
                .write(widget.bounds().offset)
                .write(widget.bounds().size)
                .write(widget.inline_size());
        }

        void sanitize_widget_ref_common_data(detail::WidgetCommonData &common)
        {
            common.widget_flags = serializable_widget_ref_flags(WidgetFlags(common.widget_flags));
        }

        void write_block(acul::bin_stream &stream, umbf::Block *block)
        {
            write_block_payload(stream, *static_cast<Block *>(block));
        }

        umbf::Block *read_block(acul::bin_stream &stream)
        {
            const auto header = read_block_header_data(stream);
            auto *block = acul::alloc<Block>(header.common.id, WidgetFlags(header.common.widget_flags), AUIK_TAG_BLOCK);
            apply_block_header_data(block, header);
            read_block_children(stream, block);
            return block;
        }

        void write_draw_block(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *widget = static_cast<DrawBlock *>(block);
            write_block_payload(stream, *widget);
            stream.write(widget->style_tag())
                .write(static_cast<u32>(widget->draw_block_flags()))
                .write(widget->content_padding())
                .write(widget->scrollbar_track_style_tag())
                .write(widget->scrollbar_thumb_style_tag());
        }

        umbf::Block *read_draw_block(acul::bin_stream &stream)
        {
            const auto header = read_block_header_data(stream);
            auto *widget =
                acul::alloc<DrawBlock>(header.common.id, WidgetFlags(header.common.widget_flags), AUIK_TAG_DRAW_BLOCK);
            apply_block_header_data(widget, header);
            read_block_children(stream, widget);

            u32 style_tag = 0u;
            u32 draw_flags = 0u;
            amal::vec4 content_padding{0.0f};
            u32 scrollbar_track_style_tag = AUIK_STYLE_TAG_SCROLLBAR_TRACK_INTERNAL;
            u32 scrollbar_thumb_style_tag = AUIK_STYLE_TAG_SCROLLBAR_THUMB_INTERNAL;
            stream.read(style_tag)
                .read(draw_flags)
                .read(content_padding)
                .read(scrollbar_track_style_tag)
                .read(scrollbar_thumb_style_tag);

            widget->set_style_tag(style_tag);
            widget->set_draw_block_flags(DrawBlockFlags(draw_flags));
            widget->set_content_padding(content_padding);
            widget->set_scrollbar_style_tags(scrollbar_track_style_tag, scrollbar_thumb_style_tag);
            return widget;
        }

        void write_widget_ref(acul::bin_stream &stream, umbf::Block *block)
        {
            write_widget_ref_common_data(stream, *static_cast<WidgetRef *>(block));
        }

        umbf::Block *read_widget_ref(acul::bin_stream &stream)
        {
            auto common = detail::read_widget_common_data(stream);
            sanitize_widget_ref_common_data(common);
            auto *widget = acul::alloc<WidgetRef>(nullptr, WidgetFlags(common.widget_flags));
            detail::apply_widget_common_data(widget, common);
            return widget;
        }

        struct StackChildData
        {
            umbf::Block *block = nullptr;
            BlockChildLayer layer = BlockChildLayer::work;
        };

        acul::vector<StackChildData> collect_stack_children(const WidgetStack &stack)
        {
            acul::vector<StackChildData> out;
            const auto &children = stack.children();
            const auto &layers = stack.child_layers();
            for (size_t child_i = 0u; child_i < children.size(); ++child_i)
            {
                auto *child = children[child_i];
                if (!child || !(child->widget_flags & WidgetFlagBits::configurable)) continue;
                const auto layer = child_i < layers.size() ? layers[child_i] : BlockChildLayer::work;
                out.push_back({child, layer});
            }
            return out;
        }

        void write_widget_stack(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *widget = static_cast<WidgetStack *>(block);
            detail::write_widget_common_data(stream, *widget);
            stream.write(static_cast<u32>(widget->active_index()));

            auto children = collect_stack_children(*widget);
            stream.write(static_cast<u32>(children.size()));

            acul::vector<umbf::Block *> blocks;
            blocks.reserve(children.size());
            for (const auto &child : children)
            {
                stream.write(static_cast<u32>(child.layer));
                blocks.push_back(child.block);
            }
            stream.write(blocks);
        }

        umbf::Block *read_widget_stack(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            u32 active_index = 0u;
            u32 child_count = 0u;
            stream.read(active_index).read(child_count);

            acul::vector<BlockChildLayer> layers;
            layers.reserve(child_count);
            for (u32 child_i = 0u; child_i < child_count; ++child_i)
            {
                u32 layer = static_cast<u32>(BlockChildLayer::work);
                stream.read(layer);
                layers.push_back(static_cast<BlockChildLayer>(layer));
            }

            acul::vector<umbf::Block *> children;
            stream.read(children);

            auto *widget = acul::alloc<WidgetStack>(WidgetFlags(common.widget_flags));
            detail::apply_widget_common_data(widget, common);
            for (u32 child_i = 0u; child_i < child_count; ++child_i)
            {
                auto *child = static_cast<Widget *>(children[child_i]);
                switch (child_i < layers.size() ? layers[child_i] : BlockChildLayer::work)
                {
                    case BlockChildLayer::background:
                        widget->add_child_to_background(child);
                        break;
                    case BlockChildLayer::foreground:
                        widget->add_child_to_foreground(child);
                        break;
                    case BlockChildLayer::work:
                    default:
                        widget->add_child(child);
                        break;
                }
            }
            widget->set_active_index(static_cast<size_t>(active_index));
            return widget;
        }

        void write_collapse_header(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *widget = static_cast<CollapseHeader *>(block);
            write_block_payload(stream, *widget);
            const bool translated = widget->is_translated_label();
            const char *literal = translated ? widget->label_literal() : nullptr;
            detail::write_localized_string(stream, translated ? acul::string(literal ? literal : "") : widget->label(),
                                           translated);
            stream.write(widget->expanded())
                .write(widget->style_tag())
                .write(widget->closed_style_tag())
                .write(widget->content_style_tag())
                .write(widget->trigger_style_tag());
        }

        umbf::Block *read_collapse_header(acul::bin_stream &stream)
        {
            const auto header = read_block_header_data(stream);
            auto *widget =
                acul::alloc<CollapseHeader>(header.common.id, acul::string{}, true,
                                            WidgetFlags(header.common.widget_flags), AUIK_STYLE_TAG_COLLAPSE_HEADER);
            apply_block_header_data(widget, header);
            read_block_children(stream, widget);

            const auto label = detail::read_localized_string(stream);
            bool expanded = true;
            u32 style_tag = AUIK_STYLE_TAG_COLLAPSE_HEADER;
            u32 closed_style_tag = AUIK_STYLE_TAG_COLLAPSE_HEADER_CLOSED;
            u32 content_style_tag = AUIK_STYLE_TAG_COLLAPSE_HEADER_CONTENT;
            u32 trigger_style_tag = AUIK_STYLE_TAG_COLLAPSE_HEADER_TRIGGER;
            stream.read(expanded)
                .read(style_tag)
                .read(closed_style_tag)
                .read(content_style_tag)
                .read(trigger_style_tag);

            widget->set_label(StringView{label.text.c_str(), label.translated});
            widget->set_expanded(expanded);
            widget->set_style_tag(style_tag);
            widget->set_closed_style_tag(closed_style_tag);
            widget->set_content_style_tag(content_style_tag);
            widget->set_trigger_style_tag(trigger_style_tag);
            return widget;
        }

        void write_dummy(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *widget = static_cast<Dummy *>(block);
            detail::write_widget_common_data(stream, *widget);
            stream.write(widget->style_tag());
        }

        umbf::Block *read_dummy(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            u32 style_tag = 0u;
            stream.read(style_tag);

            auto *widget = acul::alloc<Dummy>(common.id, common.inline_size, WidgetFlags(common.widget_flags));
            widget->set_style_tag(style_tag);
            detail::apply_widget_common_data(widget, common);
            return widget;
        }
    } // namespace

    namespace streams
    {
        AUIK_EXPORT const umbf::streams::Stream block{read_block, write_block};
        AUIK_EXPORT const umbf::streams::Stream draw_block{read_draw_block, write_draw_block};
        AUIK_EXPORT const umbf::streams::Stream widget_stack{read_widget_stack, write_widget_stack};
        AUIK_EXPORT const umbf::streams::Stream widget_ref{read_widget_ref, write_widget_ref};
        AUIK_EXPORT const umbf::streams::Stream collapse_header{read_collapse_header, write_collapse_header};
        AUIK_EXPORT const umbf::streams::Stream dummy{read_dummy, write_dummy};
    } // namespace streams
} // namespace auik
