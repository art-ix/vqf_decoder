#include "stdafx.h"

#include "twinvq/vqf_file.hpp"
#include "twinvq/twinvq_decoder.hpp"

#include <memory>
#include <string>
#include <vector>

namespace {

bool parse_from_file(service_ptr_t<file>& f, twinvq::VqfInfo& info, abort_callback& abort, std::string& err) {
    auto read = [&](void* dst, size_t n) -> size_t {
        return f->read(dst, n, abort);
    };
    return twinvq::parse_vqf_header(read, info, err);
}

} // namespace

class input_vqf : public input_stubs {
public:
    void open(service_ptr_t<file> p_filehint, const char* p_path, t_input_open_reason p_reason, abort_callback& p_abort) {
        m_file = p_filehint;
        input_open_file_helper(m_file, p_path, p_reason, p_abort);
        std::string err;
        if (!parse_from_file(m_file, m_info, p_abort, err))
            throw exception_io_unsupported_format();
        m_decoder = std::make_unique<twinvq::Decoder>(m_info);
        m_pkt.frame_bits = m_info.frame_bits;
    }

    void get_info(file_info& p_info, abort_callback&) {
        p_info.set_length(twinvq::duration_seconds(m_info));
        p_info.info_set_int("samplerate", m_info.sample_rate);
        p_info.info_set_int("channels", m_info.channels);
        p_info.info_set_int("bitspersample", 32);
        p_info.info_set("encoding", "lossy");
        p_info.info_set("codec", "TwinVQ");
        p_info.info_set_bitrate(m_info.bitrate_kbps);
        if (!m_info.title.empty())
            p_info.meta_set("title", m_info.title.c_str());
        if (!m_info.artist.empty())
            p_info.meta_set("artist", m_info.artist.c_str());
        if (!m_info.comment.empty())
            p_info.meta_set("comment", m_info.comment.c_str());
        if (!m_info.copyright.empty())
            p_info.meta_set("copyright", m_info.copyright.c_str());
        if (!m_info.album.empty())
            p_info.meta_set("album", m_info.album.c_str());
        if (!m_info.genre.empty())
            p_info.meta_set("genre", m_info.genre.c_str());
        if (!m_info.track.empty())
            p_info.meta_set("tracknumber", m_info.track.c_str());
        if (!m_info.year.empty())
            p_info.meta_set("date", m_info.year.c_str());
        if (!m_info.composer.empty())
            p_info.meta_set("composer", m_info.composer.c_str());
        if (!m_info.publisher.empty())
            p_info.meta_set("publisher", m_info.publisher.c_str());
    }

    t_filestats2 get_stats2(unsigned f, abort_callback& a) { return m_file->get_stats2_(f, a); }
    t_filestats get_file_stats(abort_callback& p_abort) { return m_file->get_stats(p_abort); }

    void decode_initialize(unsigned, abort_callback& p_abort) {
        m_file->seek(m_info.data_offset, p_abort);
        m_decoder->reset();
        m_pkt = twinvq::Packetizer{};
        m_pkt.frame_bits = m_info.frame_bits;
        m_eof = false;
        m_priming = true;
        const size_t frame = static_cast<size_t>(m_info.channels) * m_info.frame_samples;
        m_pcm.resize(frame);
        m_conv.resize(frame);
    }

    bool decode_run(audio_chunk& p_chunk, abort_callback& p_abort) {
        if (m_eof)
            return false;
        for (;;) {
            const int nread = m_pkt.bytes_to_read();
            m_filebuf.resize(static_cast<size_t>(nread));
            const size_t got = m_file->read(m_filebuf.data(), nread, p_abort);
            if (got < static_cast<size_t>(nread)) {
                m_eof = true;
                return false;
            }
            m_packet.resize(static_cast<size_t>(nread) + 2);
            const int psz = m_pkt.build(m_filebuf.data(), m_packet.data());
            const int samples = m_decoder->decode_packet(m_packet.data(), psz, m_pcm.data());
            if (samples > 0) {
                const t_size n = static_cast<t_size>(samples) * m_info.channels;
                for (t_size i = 0; i < n; i++)
                    m_conv[i] = static_cast<audio_sample>(m_pcm[i]);
                p_chunk.set_data(m_conv.data(), samples, m_info.channels, m_info.sample_rate);
                return true;
            }
        }
    }

    void decode_seek(double p_seconds, abort_callback& p_abort) {
        m_file->ensure_seekable();
        const t_uint64 target_sample = audio_math::time_to_samples(p_seconds, m_info.sample_rate);
        t_uint64 frame = target_sample / m_info.frame_samples;
        const t_uint64 total_bits = m_info.data_size * 8;
        const t_uint64 max_frame = m_info.frame_bits ? (total_bits / m_info.frame_bits) : 0;
        if (frame > max_frame)
            frame = max_frame;
        const int64_t bit_pos = static_cast<int64_t>(frame * static_cast<t_uint64>(m_info.frame_bits));
        const int64_t off = twinvq::Packetizer::file_offset_for_bit(bit_pos, m_info.data_offset);
        m_file->seek(off < static_cast<int64_t>(m_info.data_offset) ? m_info.data_offset : off, p_abort);
        m_decoder->reset();
        m_pkt.seek_prep(bit_pos);
        m_eof = false;
    }

    bool decode_can_seek() { return m_file->can_seek(); }

    void retag(const file_info& p_info, abort_callback& p_abort) {
        twinvq::VqfTags tags;
        tags.title = meta0(p_info, "title");
        tags.artist = meta0(p_info, "artist");
        tags.comment = meta0(p_info, "comment");
        tags.copyright = meta0(p_info, "copyright");
        tags.album = meta0(p_info, "album");
        tags.genre = meta0(p_info, "genre");
        tags.track = meta0(p_info, "tracknumber");
        tags.year = meta0(p_info, "date");
        if (tags.year.empty())
            tags.year = meta0(p_info, "year");
        tags.composer = meta0(p_info, "composer");
        tags.publisher = meta0(p_info, "publisher");
        rewrite_tags(tags, false, p_abort);
    }

    void remove_tags(abort_callback& p_abort) {
        rewrite_tags({}, true, p_abort);
    }

    static bool g_is_our_content_type(const char*) { return false; }
    static bool g_is_our_path(const char*, const char* ext) {
        return stricmp_utf8(ext, "vqf") == 0 || stricmp_utf8(ext, "vql") == 0 || stricmp_utf8(ext, "vqe") == 0;
    }
    static const char* g_get_name() { return "TwinVQ decoder"; }
    static GUID g_get_guid() {
        static const GUID guid = {0x7c3a9e21, 0x4b8f, 0x4d12, {0x9e, 0x55, 0xa1, 0x2c, 0x8d, 0x3f, 0x70, 0x1b}};
        return guid;
    }

private:
    static std::string meta0(const file_info& info, const char* name) {
        const char* v = info.meta_get(name, 0);
        return v ? std::string(v) : std::string();
    }

    void rewrite_tags(const twinvq::VqfTags& tags, bool strip_all, abort_callback& p_abort) {
        if (!m_file->can_seek())
            throw exception_io_denied();
        const uint64_t old_off = m_info.data_offset;
        uint64_t data_size = m_info.data_size;
        const t_filesize file_size = m_file->get_size(p_abort);
        if (data_size == 0 && file_size != filesize_invalid && file_size > old_off)
            data_size = file_size - old_off;
        m_info.data_size = data_size;

        twinvq::apply_tags(m_info, tags, strip_all);
        const std::vector<uint8_t> header = twinvq::serialize_vqf_header(m_info);

        if (header.size() == old_off) {
            m_file->seek(0, p_abort);
            m_file->write(header.data(), header.size(), p_abort);
            return;
        }

        service_ptr_t<file> temp;
        filesystem::g_open_temp(temp, p_abort);
        temp->write(header.data(), header.size(), p_abort);
        m_file->seek(old_off, p_abort);
        file::g_transfer(m_file, temp, data_size, p_abort);

        const t_filesize new_size = temp->get_size(p_abort);
        temp->seek(0, p_abort);
        m_file->seek(0, p_abort);
        file::g_transfer(temp, m_file, new_size, p_abort);
        m_file->resize(new_size, p_abort);
    }

    service_ptr_t<file> m_file;
    twinvq::VqfInfo m_info;
    std::unique_ptr<twinvq::Decoder> m_decoder;
    twinvq::Packetizer m_pkt;
    std::vector<uint8_t> m_filebuf, m_packet;
    std::vector<float> m_pcm;
    std::vector<audio_sample> m_conv;
    bool m_eof = false;
    bool m_priming = false;
};

static input_singletrack_factory_t<input_vqf> g_input_vqf_factory;

DECLARE_FILE_TYPE_EX("vqf;vql;vqe", "TwinVQ file", "TwinVQ files");
