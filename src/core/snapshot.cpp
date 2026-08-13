#include <auik/auik.hpp>
#include <auik/detail/context.hpp>
#include <auik/snapshot.hpp>
#include <auik/widgets/checkbox.hpp>
#include <auik/widgets/color_picker.hpp>
#include <auik/widgets/column.hpp>
#include <auik/widgets/combobox.hpp>
#include <auik/widgets/containers.hpp>
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
#include <auik/widgets/slider.hpp>
#include <auik/widgets/switch_button.hpp>
#include <auik/widgets/tabbar.hpp>
#include <auik/widgets/table.hpp>
#include <auik/widgets/tree.hpp>
#include <auik/widgets/text.hpp>
#include <auik/widgets/text_button.hpp>
#include <auik/widgets/textbox.hpp>
#include <auik/widgets/titlebar.hpp>
#include <auik/widgets/widget.hpp>
#include <auik/widgets/window.hpp>

namespace auik
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

    namespace
    {
        SnapshotTree *snapshot_tree_from_file(const acul::shared_ptr<umbf::File> &snapshot)
        {
            if (!snapshot) return nullptr;
            for (auto &block : snapshot->blocks)
            {
                if (!block || block->signature() != AUIK_TAG_SNAPSHOT_TREE) continue;
                return static_cast<SnapshotTree *>(block.get());
            }
            return nullptr;
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

    acul::shared_ptr<umbf::File> make_snapshot()
    {
        auto file = acul::make_shared<umbf::File>();
        file->header.vendor_sign = AUIK_VENDOR_ID;
        file->header.vendor_version = 0u;
        file->header.spec_version = 0u;
        file->header.type_sign = AUIK_SIGN_TYPE_SNAPSHOT;
        file->header.flags = UMBF_COMPRESSION_PAYLOAD_BIT;

        auto tree = acul::make_shared<SnapshotTree>();
        tree->owns_roots = false;
        tree->roots = detail::get_context().widget_tree;
        file->blocks.push_back(tree);
        return file;
    }

    bool assign_snapshot(const acul::shared_ptr<umbf::File> &snapshot)
    {
        if (!snapshot) return false;
        if (snapshot->header.vendor_sign != AUIK_VENDOR_ID || snapshot->header.type_sign != AUIK_SIGN_TYPE_SNAPSHOT)
            return false;

        auto *tree = snapshot_tree_from_file(snapshot);
        if (!tree) return false;
        if (!tree->owns_roots) return false;

        auto &ctx = detail::get_context();
        acul::vector<Widget *> next_roots = tree->take_roots();
        detach_and_release_roots(ctx.widget_tree);
        ctx.widget_tree.swap(next_roots);
        detach_and_release_roots(next_roots);
        ctx.focus_id = 0u;
        ctx.active_id = 0u;
        ctx.hover_id = {};
        ctx.io.clicked_id = {};
        detail::cancel_unbounded_mouse_drag();
        ctx.io.drag_id = {};
        ctx.io.drag_key_flags = {};
        ctx.dirty_flags |=
            DirtyFlagBits::layout | DirtyFlagBits::locale | DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;

        for (auto *root : ctx.widget_tree) attach_snapshot_root(root);
        rebuild_root_widget_depths();
        if (ctx.main_viewport && ctx.main_viewport->rect.size.x > 0.0f && ctx.main_viewport->rect.size.y > 0.0f)
            update_root_widgets_layout(ctx.main_viewport);
        mark_host_refresh_request();
        return true;
    }

    acul::shared_ptr<umbf::File> load_snapshot(const acul::path &path)
    {
        acul::shared_ptr<umbf::File> file;
        if (!umbf::File::read_from_disk(path.str(), file).success()) return nullptr;
        if (!file || file->header.vendor_sign != AUIK_VENDOR_ID || file->header.type_sign != AUIK_SIGN_TYPE_SNAPSHOT)
            return nullptr;
        return file;
    }

    namespace
    {
        void write_snapshot_tree(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *tree = static_cast<SnapshotTree *>(block);
            acul::vector<umbf::Block *> blocks;
            blocks.reserve(tree->roots.size());
            for (auto *root : tree->roots)
                if (root && (root->widget_flags & WidgetFlagBits::configurable)) blocks.push_back(root);
            stream.write(blocks);
        }

        umbf::Block *read_snapshot_tree(acul::bin_stream &stream)
        {
            auto *tree = acul::alloc<SnapshotTree>();
            acul::vector<umbf::Block *> blocks;
            stream.read(blocks);

            tree->roots.reserve(blocks.size());
            for (auto *block : blocks) tree->roots.push_back(static_cast<Widget *>(block));
            return tree;
        }
    } // namespace

    namespace streams
    {
        AUIK_EXPORT const umbf::streams::Stream snapshot_tree{read_snapshot_tree, write_snapshot_tree};
    } // namespace streams

    void insert_umbf_streams(umbf::streams::HashResolver &resolver)
    {
        resolver.streams[AUIK_TAG_SNAPSHOT_TREE] = &streams::snapshot_tree;
        resolver.streams[AUIK_TAG_BLOCK] = &streams::block;
        resolver.streams[AUIK_TAG_DRAW_BLOCK] = &streams::draw_block;
        resolver.streams[AUIK_TAG_WIDGET_STACK] = &streams::widget_stack;
        resolver.streams[AUIK_TAG_WIDGET_REF] = &streams::widget_ref;
        resolver.streams[AUIK_TAG_COLLAPSE_HEADER] = &streams::collapse_header;
        resolver.streams[AUIK_TAG_DUMMY] = &streams::dummy;
        resolver.streams[AUIK_TAG_IMAGE] = &streams::image;
        resolver.streams[AUIK_TAG_CHECKER_IMAGE] = &streams::checker_image;
        resolver.streams[AUIK_TAG_IMAGE_BUTTON] = &streams::image_button;
        resolver.streams[AUIK_TAG_TEXT] = &streams::text;
        resolver.streams[AUIK_TAG_ETEXT] = &streams::etext;
        resolver.streams[AUIK_TAG_TEXT_BUTTON] = &streams::text_button;
        resolver.streams[AUIK_TAG_TEXTBOX] = &streams::textbox;
        resolver.streams[AUIK_TAG_MULTILINE_TEXTBOX] = &streams::multiline_textbox;
        resolver.streams[AUIK_TAG_CHECKBOX] = &streams::checkbox;
        resolver.streams[AUIK_TAG_RADIO_BUTTON] = &streams::radio_button;
        resolver.streams[AUIK_TAG_SWITCH_BUTTON] = &streams::switch_button;
        resolver.streams[AUIK_TAG_TABBAR] = &streams::tab_bar;
        resolver.streams[AUIK_TAG_TABLE] = &streams::table;
        resolver.streams[AUIK_TAG_TREE] = &streams::tree;
        resolver.streams[AUIK_TAG_TABLE_TREE] = &streams::table_tree;
        resolver.streams[AUIK_TAG_MENU_BAR] = &streams::menu_bar;
        resolver.streams[AUIK_TAG_POPUP_MENU] = &streams::popup_menu;
        resolver.streams[AUIK_TAG_COLUMN] = &streams::column;
        resolver.streams[AUIK_TAG_WINDOW] = &streams::window;
        resolver.streams[AUIK_TAG_DOCKSPACE] = &streams::dockspace;
        resolver.streams[AUIK_TAG_SLIDER] = &streams::slider;
        resolver.streams[AUIK_TAG_GRADIENT_SLIDER] = &streams::gradient_slider;
        resolver.streams[AUIK_TAG_TRANSPARENCY_SLIDER] = &streams::transparency_slider;
        resolver.streams[AUIK_TAG_RANGE_SLIDER] = &streams::range_slider;
        resolver.streams[AUIK_TAG_PROGRESS_BAR] = &streams::progress_bar;
        resolver.streams[AUIK_TAG_DRAG_INT] = &streams::drag_int;
        resolver.streams[AUIK_TAG_DRAG_FLOAT] = &streams::drag_float;
        resolver.streams[AUIK_TAG_DRAG_DOUBLE] = &streams::drag_double;
        resolver.streams[AUIK_TAG_RUBBER_BAND] = &streams::rubber_band;
        resolver.streams[AUIK_TAG_COMBO_BOX] = &streams::combobox;
        resolver.streams[AUIK_TAG_MULTIPLE_COMBO_BOX] = &streams::multiple_combobox;
        resolver.streams[AUIK_TAG_CIRCLE_COLOR_PICKER] = &streams::circle_color_picker;
        resolver.streams[AUIK_TAG_GRADIENT_COLOR_PICKER] = &streams::gradient_color_picker;
        resolver.streams[AUIK_TAG_SQUARE_COLOR_PICKER] = &streams::square_color_picker;
        resolver.streams[AUIK_TAG_MODAL_QUEUE] = &streams::modal_queue;
        resolver.streams[AUIK_TAG_WLINE] = &streams::w_line;
        resolver.streams[AUIK_TAG_WRECT] = &streams::w_rect;
        resolver.streams[AUIK_TAG_TITLEBAR] = &streams::titlebar;
    }
} // namespace auik
