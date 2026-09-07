#pragma once

#include "containers.hpp"
#include "text.hpp"

#define AUIK_TAG_SHORTCUT 0x33B5B50Eu
#define AUIK_TAG_SHORTCUT_INPUT 0x0C5D43AEu

namespace auik
{
    class ShortcutLabel final : public Widget, public umbf::Block
    {
    public:
        AUIK_EXPORT explicit ShortcutLabel(u32 id, const acul::vector<StringView> &parts = {});
        AUIK_EXPORT ~ShortcutLabel() override;

        AUIK_EXPORT void set_parts(const acul::vector<StringView> &parts);
        AUIK_EXPORT acul::vector<acul::string> parts() const;

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size_force() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT void invalidate_style() override;
        AUIK_EXPORT bool update_locale() override;
        AUIK_EXPORT void add_state_flags_inherit(WidgetStateFlags flags) override;
        AUIK_EXPORT void remove_state_flags_inherit(WidgetStateFlags flags) override;
        AUIK_EXPORT u32 get_depth_requirement() const override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        AUIK_EXPORT void on_attach() override;
        AUIK_EXPORT void on_detach() override;
        u32 signature() const noexcept override { return AUIK_TAG_SHORTCUT; }
        umbf::Block *as_snapshot_block() noexcept override { return this; }

    private:
        void clear_parts();

        acul::vector<Text *> _parts;
        acul::vector<detail::RectData> _key_rects;
        acul::vector<DrawDataID> _key_draw_ids;
        StyleSelector _style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_SHORTCUT};
        StyleSelector _key_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_SHORTCUT_KEY};
    };

    inline ShortcutLabel *make_shortcut_label(u32 id, const acul::vector<StringView> &parts = {})
    {
        return acul::alloc<ShortcutLabel>(id, parts);
    }

    class ShortcutInput final : public DrawBlock
    {
    public:
        AUIK_EXPORT explicit ShortcutInput(u32 id, const acul::vector<StringView> &parts = {});

        AUIK_EXPORT void set_parts(const acul::vector<StringView> &parts);
        AUIK_EXPORT void set_capturing(bool value);
        ShortcutLabel *label() const { return _label; }
        u32 signature() const noexcept override { return AUIK_TAG_SHORTCUT_INPUT; }
        umbf::Block *as_snapshot_block() noexcept override { return nullptr; }

    private:
        ShortcutLabel *_label = nullptr;
        Text *_caret = nullptr;
        bool _capturing = true;
        bool _empty = true;
    };

    inline ShortcutInput *make_shortcut_input(u32 id, const acul::vector<StringView> &parts = {})
    {
        return acul::alloc<ShortcutInput>(id, parts);
    }

    namespace streams
    {
        extern AUIK_EXPORT const umbf::registry::BlockStream shortcut_label;
    }
} // namespace auik
