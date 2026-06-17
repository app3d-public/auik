#pragma once

#include "selectable.hpp"

namespace auik::detail
{
    class MenuBase;
    class MenuGroupLayer;
    class MenuGroup;

    class MenuItem : public Selectable
    {
    public:
        AUIK_EXPORT MenuItem(MenuBase *owner, u32 element_id, StringView text, Widget *parent, u32 style_tag_id,
                             u32 selected_style_tag_id);
        AUIK_EXPORT ~MenuItem() override;

        AUIK_EXPORT MenuItem &set_shortcut(acul::string shortcut);
        const acul::string &shortcut() const { return _shortcut; }
        AUIK_EXPORT MenuGroupLayer *add_group_layer(u32 group_count = 1u);
        MenuGroupLayer *group_layer() const { return _group_layer; }
        bool has_group_layer() const;
        u32 element_id() const { return get_rect().id.element_id; }
        MenuBase *owner() const { return _owner; }

    private:
        MenuBase *_owner = nullptr;
        MenuGroupLayer *_group_layer = nullptr;
        acul::string _shortcut;
    };

    class MenuGroup
    {
        friend class MenuBase;

    public:
        MenuGroup() = default;
        explicit MenuGroup(MenuBase *owner) : _owner(owner) {}
        AUIK_EXPORT ~MenuGroup();

        AUIK_EXPORT MenuItem *add_item(StringView text = {});
        AUIK_EXPORT void add_items(u32 count);
        AUIK_EXPORT void add_items(const acul::vector<acul::string> &items);
        AUIK_EXPORT void add_items(const acul::vector<StringView> &items);
        AUIK_EXPORT void add_items(std::initializer_list<const char *> items);
        MenuItem *operator[](size_t index) const { return _items[index]; }
        size_t size() const { return _items.size(); }
        bool empty() const { return _items.empty(); }
        const acul::vector<MenuItem *> &items() const { return _items; }

    private:
        MenuBase *_owner = nullptr;
        acul::vector<MenuItem *> _items;
    };

    class MenuGroupLayer
    {
    public:
        MenuGroupLayer() = default;
        MenuGroupLayer(MenuBase *owner, MenuItem *parent_item = nullptr) : _owner(owner), _parent_item(parent_item) {}
        AUIK_EXPORT ~MenuGroupLayer();

        AUIK_EXPORT MenuGroup *add_group();
        AUIK_EXPORT void add_groups(u32 count);
        AUIK_EXPORT void clear();
        void set_owner(MenuBase *owner) { _owner = owner; }
        MenuGroup *operator[](size_t index) const { return _groups[index]; }
        size_t size() const { return _groups.size(); }
        bool empty() const { return _groups.empty(); }
        const acul::vector<MenuGroup *> &groups() const { return _groups; }
        MenuItem *parent_item() const { return _parent_item; }

    private:
        MenuBase *_owner = nullptr;
        MenuItem *_parent_item = nullptr;
        acul::vector<MenuGroup *> _groups;
    };

    class MenuBase
    {
    public:
        MenuBase() : _root_layer(this) {}
        MenuBase(Widget *owner, u32 item_style_tag, u32 selected_item_style_tag)
            : _owner(owner),
              _item_style_tag(item_style_tag),
              _selected_item_style_tag(selected_item_style_tag),
              _root_layer(this)
        {
        }
        AUIK_EXPORT ~MenuBase();

        MenuBase(const MenuBase &) = delete;
        MenuBase &operator=(const MenuBase &) = delete;

        AUIK_EXPORT void configure(Widget *owner, u32 item_style_tag, u32 selected_item_style_tag);
        AUIK_EXPORT void reset();
        AUIK_EXPORT MenuItem *create_item(StringView text, Widget *parent = nullptr);
        AUIK_EXPORT MenuItem *find_item(u32 element_id);
        AUIK_EXPORT const MenuItem *find_item(u32 element_id) const;
        AUIK_EXPORT void add_existing_item(MenuGroup *group, MenuItem *item);
        AUIK_EXPORT void release_group_items(MenuGroup *group);
        AUIK_EXPORT void set_item_style_tags(u32 item_style_tag, u32 selected_item_style_tag);
        AUIK_EXPORT void set_next_element_id(u32 value);
        AUIK_EXPORT void reserve_next_element_id(u32 used_id);

        MenuGroupLayer &root_layer() { return _root_layer; }
        const MenuGroupLayer &root_layer() const { return _root_layer; }
        const acul::vector<MenuItem *> &owned_items() const { return _owned_items; }
        u32 next_element_id() const { return _next_element_id; }
        Widget *owner_widget() const { return _owner; }
        u32 owner_widget_id() const { return _owner ? _owner->id() : 0u; }
        u32 item_style_tag() const { return _item_style_tag; }
        u32 selected_item_style_tag() const { return _selected_item_style_tag; }

    private:
        friend class MenuGroup;

        Widget *_owner = nullptr;
        u32 _item_style_tag = AUIK_STYLE_TAG_COMBO_BOX_ITEM;
        u32 _selected_item_style_tag = AUIK_STYLE_TAG_COMBO_BOX_ITEM_SELECTED;
        u32 _next_element_id = 1u;
        MenuGroupLayer _root_layer;
        acul::vector<MenuItem *> _owned_items;
    };
} // namespace auik::detail
