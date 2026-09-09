#pragma once

#include "twinvq_types.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace twinvq {

using ReadFn = std::function<size_t(void* dst, size_t n)>;

// Parse a VQF/TwinVQ header from a sequential reader (starts at file offset 0).
// On success, the reader is positioned at the start of the DATA payload.
bool parse_vqf_header(const ReadFn& read, VqfInfo& info, std::string& error);

// In-memory helper.
bool parse_vqf_header_mem(const uint8_t* data, size_t size, VqfInfo& info, std::string& error);

const ModeTab* select_mode(int sample_rate, int bitrate_kbps, int channels);

// Rebuild TWIN header + DATA marker (not the audio payload).
std::vector<uint8_t> serialize_vqf_header(const VqfInfo& info);

struct VqfTags {
    std::string title;
    std::string artist;
    std::string comment;
    std::string copyright;
    std::string album;
    std::string genre;
    std::string track;
    std::string year;
    std::string composer;
    std::string publisher;
};

// Replace NAME/AUTH/… chunks. COMM, DSIZ and unknown chunks are kept.
// If strip_all is true, all mapped tag chunks are removed.
void apply_tags(VqfInfo& info, const VqfTags& tags, bool strip_all = false);

inline double duration_seconds(const VqfInfo& info) {
    if (info.bitrate_kbps <= 0)
        return 0;
    return (static_cast<double>(info.data_size) * 8.0) / (static_cast<double>(info.bitrate_kbps) * 1000.0);
}

} // namespace twinvq
