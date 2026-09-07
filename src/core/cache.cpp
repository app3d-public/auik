#include <auik/auik.hpp>
#include <auik/cache.hpp>
#include <auik/detail/context.hpp>
#include <auik/widgets/checkbox.hpp>
#include <auik/widgets/color_picker.hpp>
#include <auik/widgets/column.hpp>
#include <auik/widgets/combobox.hpp>
#include <auik/widgets/containers.hpp>
#include <auik/widgets/dock_layout.hpp>
#include <auik/widgets/dockspace.hpp>
#include <auik/widgets/drag.hpp>
#include <auik/widgets/image.hpp>
#include <auik/widgets/image_button.hpp>
#include <auik/widgets/menu.hpp>
#include <auik/widgets/modal_window.hpp>
#include <auik/widgets/primitives.hpp>
#include <auik/widgets/progress_bar.hpp>
#include <auik/widgets/radio_button.hpp>
#include <auik/widgets/rubber_band.hpp>
#include <auik/widgets/shortcut.hpp>
#include <auik/widgets/slider.hpp>
#include <auik/widgets/switch_button.hpp>
#include <auik/widgets/tabbar.hpp>
#include <auik/widgets/table.hpp>
#include <auik/widgets/text.hpp>
#include <auik/widgets/text_button.hpp>
#include <auik/widgets/textbox.hpp>
#include <auik/widgets/titlebar.hpp>
#include <auik/widgets/tree.hpp>
#include <auik/widgets/widget.hpp>
#include <auik/widgets/window.hpp>
#include "acul/memory/smart_ptr.hpp"
#include "umbf/umbf.hpp"

namespace auik
{
    namespace detail
    {
        SnapshotTree::~SnapshotTree() { clear(); }

        void SnapshotTree::clear()
        {
            if (owns_roots)
                for (auto *root : roots) acul::release(root);
            roots.clear();
        }

        acul::vector<Widget *> SnapshotTree::take_roots()
        {
            acul::vector<Widget *> out;
            out.swap(roots);
            owns_roots = false;
            return out;
        }
    } // namespace detail

    namespace
    {
        template <class T>
        acul::unique_ptr<T> cache_block_from_file(const acul::shared_ptr<umbf::ReadDescriptor> &cache, u32 signature)
        {
            if (!cache || !cache->file) return nullptr;
            for (auto block = cache->begin(); block != cache->end(); ++block)
            {
                if (block->signature != signature) continue;
                auto value = umbf::get_block(block);
                if (!value) return nullptr;
                auto *typed = static_cast<T *>(value.release());
                return acul::unique_ptr<T>(typed);
            }
            return nullptr;
        }

        bool is_cache_file(const acul::shared_ptr<umbf::ReadDescriptor> &cache)
        {
            return cache && cache->file && cache->file->vendor_sign == AUIK_VENDOR_ID &&
                   cache->file->type_sign == AUIK_SIGN_TYPE_CACHE;
        }

        void detach_and_release_roots(acul::vector<Widget *> &roots)
        {
            for (auto *root : roots)
            {
                if (!root) continue;
                if (root->widget_flags & WidgetFlagBits::attachable) root->on_detach();
                acul::release(root);
            }
            roots.clear();
        }

        void attach_snapshot_root(Widget *root)
        {
            if (!root) return;
            if (!root->viewport()) root->attach_to_viewport(get_main_viewport());
            if (root->widget_flags & WidgetFlagBits::attachable) root->on_attach();
            root->update_style_invalidated();
        }

    } // namespace

    acul::shared_ptr<umbf::WriteDescriptor> make_cache()
    {
        umbf::Header header;
        header.vendor_sign = AUIK_VENDOR_ID;
        header.vendor_version = 0u;
        header.spec_version = 0u;
        header.type_sign = AUIK_SIGN_TYPE_CACHE;
        auto file = acul::make_shared<umbf::WriteDescriptor>();
        if (!umbf::create_write_descriptor(header, *file).success()) return nullptr;
        file->default_segment_signature = UMBF_SEGMENT_COMPRESSED;

        auto tree = acul::make_shared<detail::SnapshotTree>();
        tree->owns_roots = false;
        tree->roots = detail::get_context().widget_tree;
        umbf::add_block(*file, tree.get());

        auto global = acul::make_shared<detail::GlobalCache>();
        const auto &global_cache = detail::get_context().global_cache;
        global->entries.reserve(global_cache.size());
        for (const auto &[widget_id, state] : global_cache)
        {
            (void)widget_id;
            if (state) global->entries.push_back(state);
        }
        umbf::add_block(*file, global.get());
        return file;
    }

    bool assign_global_cache(const acul::shared_ptr<umbf::ReadDescriptor> &cache)
    {
        if (!is_cache_file(cache)) return false;
        auto global = cache_block_from_file<detail::GlobalCache>(cache, AUIK_TAG_GLOBAL_UI_CACHE);
        if (!global) return false;
        acul::hashmap<u32, acul::shared_ptr<detail::WidgetStateData>> entries;
        entries.reserve(global->entries.size());
        for (const auto &entry : global->entries)
        {
            if (!entry) continue;
            auto state = acul::dynamic_pointer_cast<detail::WidgetStateData>(entry);
            if (!state) continue;
            entries[state->widget_id] = state;
        }
        swap(detail::get_context().global_cache, entries);
        return true;
    }

    bool assign_snapshot(const acul::shared_ptr<umbf::ReadDescriptor> &cache)
    {
        if (!is_cache_file(cache)) return false;
        auto tree = cache_block_from_file<detail::SnapshotTree>(cache, AUIK_TAG_SNAPSHOT_TREE);
        if (!tree || !tree->owns_roots) return false;

        auto &ctx = detail::get_context();
        acul::vector<Widget *> next_roots = tree->take_roots();
        detach_and_release_roots(ctx.widget_tree);
        ctx.widget_tree.swap(next_roots);
        detach_and_release_roots(next_roots);
        ctx.focus_id = 0u;
        ctx.active_id = 0u;
        ctx.hover_id = {};
        ctx.last_hover_id = {};
        ctx.io.clicked_id = {};
        detail::cancel_unbounded_mouse_drag();
        ctx.io.drag_id = {};
        ctx.io.drag_key_flags = {};
        ctx.dirty_flags |= DirtyFlagBits::layout | DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;

        for (auto *root : ctx.widget_tree) attach_snapshot_root(root);
        mark_locale_changed();
        rebuild_root_widget_depths();
        if (ctx.main_viewport && ctx.main_viewport->rect.size.x > 0.0f && ctx.main_viewport->rect.size.y > 0.0f)
            update_root_widgets_layout(ctx.main_viewport);
        mark_host_refresh_request();
        return true;
    }

    acul::shared_ptr<umbf::ReadDescriptor> load_cache(const acul::path &path)
    {
        auto file = acul::make_shared<umbf::ReadDescriptor>();
        if (!umbf::create_read_descriptor(path, *file).success()) return nullptr;
        if (!is_cache_file(file)) return nullptr;
        return file;
    }

    namespace
    {
        void write_snapshot_tree(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *tree = static_cast<detail::SnapshotTree *>(block);
            acul::vector<Widget *> blocks;
            blocks.reserve(tree->roots.size());
            for (auto *root : tree->roots)
                if (root && (root->widget_flags & WidgetFlagBits::cache_snapshot)) blocks.push_back(root);
            stream.write(blocks);
        }

        umbf::Block *read_snapshot_tree(acul::bin_stream &stream)
        {
            auto *tree = acul::alloc<detail::SnapshotTree>();
            acul::vector<Widget *> blocks;
            stream.read(blocks);

            for (auto *block : blocks)
            {
                if (block) continue;
                for (auto *value : blocks)
                    if (value) acul::release(value);
                acul::release(tree);
                throw acul::runtime_error("Failed to read AUIK snapshot widget");
            }

            tree->roots.swap(blocks);
            return tree;
        }

        void write_global_cache(acul::bin_stream &stream, umbf::Block *block)
        {
            const auto *cache = static_cast<detail::GlobalCache *>(block);
            stream.write(static_cast<u64>(cache->entries.size()));
            for (const auto &entry : cache->entries)
            {
                if (entry) umbf::write_block_to_stream(stream, *entry);
                else stream.write(0ULL);
            }
        }

        umbf::Block *read_global_cache(acul::bin_stream &stream)
        {
            auto *cache = acul::alloc<detail::GlobalCache>();
            u64 count = 0u;
            stream.read(count);
            cache->entries.reserve(static_cast<size_t>(count));
            for (u64 i = 0u; i < count; ++i)
            {
                acul::unique_ptr<umbf::Block> block;
                if (!umbf::read_block_from_stream(stream, block) || !block)
                    throw acul::runtime_error("Failed to read AUIK widget state");
                cache->entries.emplace_back(static_cast<detail::WidgetStateData *>(block.release()));
            }
            return cache;
        }
    } // namespace

    namespace streams
    {
        AUIK_EXPORT const umbf::registry::BlockStream snapshot_tree{read_snapshot_tree, write_snapshot_tree};
        AUIK_EXPORT const umbf::registry::BlockStream global_cache{read_global_cache, write_global_cache};
    } // namespace streams

    void insert_umbf_streams(umbf::registry::HashResolver &resolver)
    {
        resolver.block_streams[AUIK_TAG_SNAPSHOT_TREE] = &streams::snapshot_tree;
        resolver.block_streams[AUIK_TAG_GLOBAL_UI_CACHE] = &streams::global_cache;
        resolver.block_streams[AUIK_TAG_COLLAPSE_HEADER_STATE] = &streams::collapse_header_state;
        resolver.block_streams[AUIK_TAG_WINDOW_STATE] = &streams::window_state;
        resolver.block_streams[AUIK_TAG_BLOCK] = &streams::block;
        resolver.block_streams[AUIK_TAG_DRAW_BLOCK] = &streams::draw_block;
        resolver.block_streams[AUIK_TAG_WIDGET_STACK] = &streams::widget_stack;
        resolver.block_streams[AUIK_TAG_WIDGET_REF] = &streams::widget_ref;
        resolver.block_streams[AUIK_TAG_COLLAPSE_HEADER] = &streams::collapse_header;
        resolver.block_streams[AUIK_TAG_DUMMY] = &streams::dummy;
        resolver.block_streams[AUIK_TAG_IMAGE] = &streams::image;
        resolver.block_streams[AUIK_TAG_CHECKER_IMAGE] = &streams::checker_image;
        resolver.block_streams[AUIK_TAG_IMAGE_BUTTON] = &streams::image_button;
        resolver.block_streams[AUIK_TAG_TEXT] = &streams::text;
        resolver.block_streams[AUIK_TAG_ETEXT] = &streams::etext;
        resolver.block_streams[AUIK_TAG_TEXT_BUTTON] = &streams::text_button;
        resolver.block_streams[AUIK_TAG_TEXTBOX] = &streams::textbox;
        resolver.block_streams[AUIK_TAG_MULTILINE_TEXTBOX] = &streams::multiline_textbox;
        resolver.block_streams[AUIK_TAG_CHECKBOX] = &streams::checkbox;
        resolver.block_streams[AUIK_TAG_RADIO_BUTTON] = &streams::radio_button;
        resolver.block_streams[AUIK_TAG_SWITCH_BUTTON] = &streams::switch_button;
        resolver.block_streams[AUIK_TAG_TABBAR] = &streams::tabbar;
        resolver.block_streams[AUIK_TAG_TABLE] = &streams::table;
        resolver.block_streams[AUIK_TAG_TREE] = &streams::tree;
        resolver.block_streams[AUIK_TAG_TABLE_TREE] = &streams::table_tree;
        resolver.block_streams[AUIK_TAG_MENU_BAR] = &streams::menu_bar;
        resolver.block_streams[AUIK_TAG_POPUP_MENU] = &streams::popup_menu;
        resolver.block_streams[AUIK_TAG_COLUMN] = &streams::column;
        resolver.block_streams[AUIK_TAG_WINDOW] = &streams::window;
        resolver.block_streams[AUIK_TAG_DOCKSPACE] = &streams::dockspace;
        resolver.block_streams[AUIK_TAG_DOCK_LAYOUT] = &streams::dock_layout;
        resolver.block_streams[AUIK_TAG_SLIDER] = &streams::slider;
        resolver.block_streams[AUIK_TAG_GRADIENT_SLIDER] = &streams::gradient_slider;
        resolver.block_streams[AUIK_TAG_TRANSPARENCY_SLIDER] = &streams::transparency_slider;
        resolver.block_streams[AUIK_TAG_RANGE_SLIDER] = &streams::range_slider;
        resolver.block_streams[AUIK_TAG_PROGRESS_BAR] = &streams::progress_bar;
        resolver.block_streams[AUIK_TAG_DRAG_INT] = &streams::drag_int;
        resolver.block_streams[AUIK_TAG_DRAG_FLOAT] = &streams::drag_float;
        resolver.block_streams[AUIK_TAG_DRAG_DOUBLE] = &streams::drag_double;
        resolver.block_streams[AUIK_TAG_RUBBER_BAND] = &streams::rubber_band;
        resolver.block_streams[AUIK_TAG_COMBO_BOX] = &streams::combobox;
        resolver.block_streams[AUIK_TAG_MULTIPLE_COMBO_BOX] = &streams::multiple_combobox;
        resolver.block_streams[AUIK_TAG_SHORTCUT] = &streams::shortcut_label;
        resolver.block_streams[AUIK_TAG_CIRCLE_COLOR_PICKER] = &streams::circle_color_picker;
        resolver.block_streams[AUIK_TAG_GRADIENT_COLOR_PICKER] = &streams::gradient_color_picker;
        resolver.block_streams[AUIK_TAG_SQUARE_COLOR_PICKER] = &streams::square_color_picker;
        resolver.block_streams[AUIK_TAG_MODAL_QUEUE] = &streams::modal_queue;
        resolver.block_streams[AUIK_TAG_WLINE] = &streams::w_line;
        resolver.block_streams[AUIK_TAG_WRECT] = &streams::w_rect;
        resolver.block_streams[AUIK_TAG_TITLEBAR] = &streams::titlebar;
    }
} // namespace auik
