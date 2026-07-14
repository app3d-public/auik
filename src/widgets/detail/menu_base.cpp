#include <auik/widgets/detail/menu_base.hpp>

namespace auik::detail
{
    static acul::vector<StringView> make_string_views(const acul::vector<acul::string> &items)
    {
        acul::vector<StringView> out;
        out.reserve(items.size());
        for (const auto &item : items) out.push_back(StringView{item});
        return out;
    }

    static acul::vector<StringView> make_string_views(std::initializer_list<const char *> items)
    {
        acul::vector<StringView> out;
        out.reserve(items.size());
        for (const char *item : items) out.push_back(StringView{item});
        return out;
    }

    MenuItem::MenuItem(MenuBase *owner, u32 element_id, StringView text, Widget *parent, u32 style_tag_id,
                       u32 selected_style_tag_id)
        : Widget(owner ? owner->owner_widget_id() : 0u, WidgetFlagBits::visible, EventFlagBits::none,
                 {{0.0f, 0.0f}, {0.0f, 0.0f}}, style_tag_id),
          _owner(owner)
    {
        (void)selected_style_tag_id;
        get_rect().id = make_element_id(owner ? owner->owner_widget_id() : 0u, style_tag_id, element_id);
        set_parent(parent);
        set_focus_parent(owner ? owner->owner_widget() : nullptr);
        set_source_text(text);
    }

    MenuItem::~MenuItem()
    {
        if (_group_layer) acul::release(_group_layer);
        _group_layer = nullptr;
    }

    MenuItem &MenuItem::set_shortcut(acul::string shortcut)
    {
        _shortcut = std::move(shortcut);
        _shortcut_translated = false;
        return *this;
    }

    MenuItem &MenuItem::set_shortcut(StringView shortcut)
    {
        _shortcut = shortcut.str ? shortcut.str : "";
        _shortcut_translated = shortcut.is_translated;
        return *this;
    }

    void MenuItem::set_source_text(StringView text)
    {
        _text = text.str ? text.str : "";
        _translated = text.is_translated;
    }

    MenuGroupLayer *MenuItem::add_group_layer(u32 group_count)
    {
        if (!_group_layer) _group_layer = acul::alloc<MenuGroupLayer>(_owner, this);
        for (u32 i = 0u; i < group_count; ++i) _group_layer->add_group();
        return _group_layer;
    }

    bool MenuItem::has_group_layer() const { return _group_layer && !_group_layer->empty(); }

    MenuGroup::~MenuGroup() { _items.clear(); }

    MenuItem *MenuGroup::add_item(StringView text)
    {
        auto *item = _owner ? _owner->create_item(text) : nullptr;
        _items.push_back(item);
        return item;
    }

    void MenuGroup::add_items(u32 count)
    {
        for (u32 i = 0u; i < count; ++i) add_item({});
    }

    void MenuGroup::add_items(const acul::vector<acul::string> &items) { add_items(make_string_views(items)); }

    void MenuGroup::add_items(const acul::vector<StringView> &items)
    {
        for (auto text : items) add_item(text);
    }

    void MenuGroup::add_items(std::initializer_list<const char *> items) { add_items(make_string_views(items)); }

    MenuGroupLayer::~MenuGroupLayer() { clear(); }

    MenuGroup *MenuGroupLayer::add_group()
    {
        auto *group = acul::alloc<MenuGroup>(_owner);
        _groups.push_back(group);
        return group;
    }

    void MenuGroupLayer::add_groups(u32 count)
    {
        while (_groups.size() < count) add_group();
    }

    void MenuGroupLayer::clear()
    {
        for (auto *group : _groups) acul::release(group);
        _groups.clear();
    }

    MenuBase::~MenuBase() { reset(); }

    void MenuBase::configure(Widget *owner, u32 item_style_tag, u32 selected_item_style_tag)
    {
        _owner = owner;
        _item_style_tag = item_style_tag;
        _selected_item_style_tag = selected_item_style_tag;
        _root_layer.set_owner(this);
    }

    void MenuBase::reset()
    {
        _root_layer.clear();
        for (auto *item : _owned_items) acul::release(item);
        _owned_items.clear();
        _next_element_id = 1u;
    }

    MenuItem *MenuBase::create_item(StringView text, Widget *parent)
    {
        const u32 element_id = _next_element_id++;
        auto *item = acul::alloc<MenuItem>(this, element_id, text, parent, _item_style_tag, _selected_item_style_tag);
        _owned_items.push_back(item);
        return item;
    }

    MenuItem *MenuBase::find_item(u32 element_id)
    {
        for (auto *item : _owned_items)
            if (item && item->element_id() == element_id) return item;
        return nullptr;
    }

    const MenuItem *MenuBase::find_item(u32 element_id) const
    {
        for (auto *item : _owned_items)
            if (item && item->element_id() == element_id) return item;
        return nullptr;
    }

    void MenuBase::add_existing_item(MenuGroup *group, MenuItem *item)
    {
        if (group) group->_items.push_back(item);
    }

    void MenuBase::release_group_items(MenuGroup *group)
    {
        if (!group) return;
        for (auto *item : group->_items)
        {
            if (!item) continue;
            for (auto it = _owned_items.begin(); it != _owned_items.end(); ++it)
            {
                if (*it != item) continue;
                _owned_items.erase(it);
                break;
            }
            acul::release(item);
        }
        group->_items.clear();
    }

    void MenuBase::set_item_style_tags(u32 item_style_tag, u32 selected_item_style_tag)
    {
        _item_style_tag = item_style_tag;
        _selected_item_style_tag = selected_item_style_tag;
        for (auto *item : _owned_items)
        {
            if (!item) continue;
            item->set_rect_tag_id(item_style_tag);
        }
    }

    void MenuBase::set_next_element_id(u32 value) { _next_element_id = value; }

    void MenuBase::reserve_next_element_id(u32 used_id)
    {
        _next_element_id = amal::max(_next_element_id, used_id + 1u);
    }
} // namespace auik::detail
