#pragma once

#include <acul/hash/hashmap.hpp>
#include <acul/io/path.hpp>
#include <auik/symbol_export.h>
#include <umbf/umbf.hpp>

#define AUIK_VENDOR_ID           0xA01C3Du
#define AUIK_SIGN_TYPE_CACHE     0xA11Cu
#define AUIK_TAG_SNAPSHOT_TREE   0x616451F3u
#define AUIK_TAG_GLOBAL_UI_CACHE 0x6BF4CE12u

namespace auik
{
    class Widget;

    AUIK_EXPORT void insert_umbf_streams(umbf::registry::HashResolver &resolver);
    AUIK_EXPORT acul::shared_ptr<umbf::WriteDescriptor> make_cache();
    AUIK_EXPORT bool assign_snapshot(const acul::shared_ptr<umbf::ReadDescriptor> &cache);
    AUIK_EXPORT bool assign_global_cache(const acul::shared_ptr<umbf::ReadDescriptor> &cache);
    AUIK_EXPORT acul::shared_ptr<umbf::ReadDescriptor> load_cache(const acul::path &path);

    namespace detail
    {
        struct SnapshotTree final : umbf::Block
        {
            acul::vector<Widget *> roots;
            bool owns_roots = true;

            AUIK_EXPORT ~SnapshotTree();
            AUIK_EXPORT void clear();
            AUIK_EXPORT acul::vector<Widget *> take_roots();

            u32 signature() const noexcept override { return AUIK_TAG_SNAPSHOT_TREE; }
        };

        struct WidgetStateData : umbf::Block
        {
            u32 widget_id = 0u;
            virtual ~WidgetStateData() = default;
            virtual u32 signature() const noexcept = 0;
        };

        struct GlobalCache final : umbf::Block
        {
            acul::vector<acul::shared_ptr<WidgetStateData>> entries;

            u32 signature() const noexcept override { return AUIK_TAG_GLOBAL_UI_CACHE; }
        };
    } // namespace detail

    namespace streams
    {
        extern AUIK_EXPORT const umbf::registry::BlockStream snapshot_tree;
        extern AUIK_EXPORT const umbf::registry::BlockStream global_cache;
    } // namespace streams
} // namespace auik
