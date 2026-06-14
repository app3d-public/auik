#pragma once
#include <auik/pipelines.hpp>

namespace auik::detail
{
    using StreamSyncState = SharedBufferSyncState;

    inline void mark_stream_frame_synced(DrawStream *stream, u32 frame_id)
    {
        if (!stream || !stream->runtime_data) return;
        auto *state = static_cast<StreamSyncState *>(stream->runtime_data);
        if (!state->buffer_versions) return;
        state->buffer_versions[frame_id] = state->master_version;
    }
} // namespace auik::detail
