#pragma once

#include <auik/symbol_export.h>
#include <acul/io/path.hpp>
#include <umbf/umbf.hpp>

#define AUIK_VENDOR_ID          0xA01C3Du
#define AUIK_SIGN_TYPE_SNAPSHOT 0xA11Cu
#define AUIK_TAG_SNAPSHOT_TREE  0x616451F3u

namespace auik
{
    class Widget;

    AUIK_EXPORT void insert_umbf_streams(umbf::streams::HashResolver &resolver);
    AUIK_EXPORT acul::shared_ptr<umbf::File> make_snapshot();
    AUIK_EXPORT bool assign_snapshot(const acul::shared_ptr<umbf::File> &snapshot);
    AUIK_EXPORT acul::shared_ptr<umbf::File> load_snapshot(const acul::path &path);

    struct SnapshotTree final : public umbf::Block
    {
        acul::vector<Widget *> roots;
        bool owns_roots = true;

        AUIK_EXPORT ~SnapshotTree() override;
        AUIK_EXPORT void clear();
        AUIK_EXPORT acul::vector<Widget *> take_roots();

        u32 signature() const override { return AUIK_TAG_SNAPSHOT_TREE; }
    };

    namespace streams
    {
        extern AUIK_EXPORT const umbf::streams::Stream snapshot_tree;
    }
} // namespace auik
