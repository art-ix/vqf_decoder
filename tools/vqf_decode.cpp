#include "twinvq/vqf_file.hpp"
#include "twinvq/twinvq_decoder.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

void write_wav16(const std::string& path, int rate, int channels, const std::vector<float>& interleaved) {
    const int frames = static_cast<int>(interleaved.size() / channels);
    const uint32_t data_bytes = static_cast<uint32_t>(frames * channels * 2);
    std::ofstream out(path, std::ios::binary);
    auto wr32 = [&](uint32_t v) {
        char b[4] = {char(v), char(v >> 8), char(v >> 16), char(v >> 24)};
        out.write(b, 4);
    };
    auto wr16 = [&](uint16_t v) {
        char b[2] = {char(v), char(v >> 8)};
        out.write(b, 2);
    };
    out.write("RIFF", 4);
    wr32(36 + data_bytes);
    out.write("WAVEfmt ", 8);
    wr32(16);
    wr16(1);
    wr16(static_cast<uint16_t>(channels));
    wr32(static_cast<uint32_t>(rate));
    wr32(static_cast<uint32_t>(rate * channels * 2));
    wr16(static_cast<uint16_t>(channels * 2));
    wr16(16);
    out.write("data", 4);
    wr32(data_bytes);
    for (float s : interleaved) {
        float x = s;
        if (x > 1.0f)
            x = 1.0f;
        if (x < -1.0f)
            x = -1.0f;
        const int16_t pcm = static_cast<int16_t>(std::lrint(x * 32767.0f));
        wr16(static_cast<uint16_t>(pcm));
    }
}

} // namespace

static int test_tags(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "cannot open " << path << "\n";
        return 1;
    }
    std::vector<uint8_t> file((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    twinvq::VqfInfo info;
    std::string err;
    if (!twinvq::parse_vqf_header_mem(file.data(), file.size(), info, err)) {
        std::cerr << "parse: " << err << "\n";
        return 1;
    }
    const auto orig = twinvq::serialize_vqf_header(info);
    if (orig.size() != info.data_offset) {
        std::cerr << "serialize size " << orig.size() << " != data_offset " << info.data_offset << "\n";
        return 1;
    }
    twinvq::VqfInfo again;
    if (!twinvq::parse_vqf_header_mem(orig.data(), orig.size(), again, err)) {
        std::cerr << "reparse: " << err << "\n";
        return 1;
    }
    if (again.title != info.title || again.artist != info.artist || again.channels != info.channels) {
        std::cerr << "roundtrip metadata mismatch\n";
        return 1;
    }

    twinvq::VqfTags tags;
    tags.title = "Tag Write Test — La Grange";
    tags.artist = info.artist;
    tags.comment = info.comment;
    tags.copyright = info.copyright;
    tags.album = "Tres Hombres";
    tags.genre = "Rock";
    tags.track = "3";
    tags.year = "1973";
    twinvq::apply_tags(info, tags, false);
    const auto hdr = twinvq::serialize_vqf_header(info);
    twinvq::VqfInfo tagged;
    if (!twinvq::parse_vqf_header_mem(hdr.data(), hdr.size(), tagged, err)) {
        std::cerr << "tagged parse: " << err << "\n";
        return 1;
    }
    if (tagged.title != tags.title || tagged.album != tags.album || tagged.year != tags.year ||
        tagged.track != tags.track || tagged.artist != tags.artist) {
        std::cerr << "apply_tags mismatch: title=[" << tagged.title << "] artist=[" << tagged.artist
                  << "] album=[" << tagged.album << "] year=[" << tagged.year << "] track=[" << tagged.track
                  << "]\n";
        return 1;
    }
    std::cout << "tag roundtrip ok, header " << orig.size() << " -> " << hdr.size() << " bytes\n";
    return 0;
}

int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "--test-imdct") {
        float err = 0;
        if (!twinvq::imdct_self_test(&err)) {
            std::cerr << "imdct self-test failed, max abs err=" << err << "\n";
            return 1;
        }
        std::cout << "imdct self-test ok, max abs err=" << err << "\n";
        return 0;
    }
    if (argc >= 2 && std::string(argv[1]) == "--test-tags") {
        if (argc < 3) {
            std::cerr << "usage: vqf_decode --test-tags <input.vqf>\n";
            return 1;
        }
        return test_tags(argv[2]);
    }
    if (argc < 2) {
        std::cerr << "usage: vqf_decode [--test-imdct] [--test-tags] <input.vqf> [output.wav]\n";
        return 1;
    }
    const std::string in_path = argv[1];
    const std::string out_path = (argc >= 3) ? argv[2] : (in_path + ".wav");

    std::ifstream in(in_path, std::ios::binary);
    if (!in) {
        std::cerr << "cannot open " << in_path << "\n";
        return 1;
    }

    twinvq::VqfInfo info;
    std::string err;
    auto read = [&](void* dst, size_t n) -> size_t {
        in.read(static_cast<char*>(dst), static_cast<std::streamsize>(n));
        return static_cast<size_t>(in.gcount());
    };
    if (!twinvq::parse_vqf_header(read, info, err)) {
        std::cerr << "parse error: " << err << "\n";
        return 1;
    }

    std::cout << "version     " << info.version << "\n";
    std::cout << "title       " << info.title << "\n";
    std::cout << "artist      " << info.artist << "\n";
    std::cout << "channels    " << info.channels << "\n";
    std::cout << "sample_rate " << info.sample_rate << "\n";
    std::cout << "bitrate     " << info.bitrate_kbps << " kbps\n";
    std::cout << "frame       " << info.frame_samples << " samples, " << info.frame_bits << " bits\n";
    std::cout << "data        offset=" << info.data_offset << " size=" << info.data_size << "\n";
    std::cout << "duration    " << twinvq::duration_seconds(info) << " s\n";

    twinvq::Decoder dec(info);
    twinvq::Packetizer pkt;
    pkt.frame_bits = info.frame_bits;

    std::vector<float> pcm;
    if (info.sample_rate > 0)
        pcm.reserve(static_cast<size_t>(twinvq::duration_seconds(info) * info.sample_rate + 8192) *
                    static_cast<size_t>(info.channels));
    std::vector<float> frame(static_cast<size_t>(info.channels) * info.frame_samples);
    std::vector<uint8_t> file_bytes(static_cast<size_t>(pkt.bytes_to_read()) + 16);
    std::vector<uint8_t> packet(file_bytes.size() + 2);

    double peak = 0;
    int frames_out = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (;;) {
        const int nread = pkt.bytes_to_read();
        file_bytes.resize(static_cast<size_t>(nread));
        in.read(reinterpret_cast<char*>(file_bytes.data()), nread);
        if (in.gcount() < nread)
            break;
        packet.resize(static_cast<size_t>(nread) + 2);
        const int psz = pkt.build(file_bytes.data(), packet.data());
        const int got = dec.decode_packet(packet.data(), psz, frame.data());
        if (got > 0) {
            pcm.insert(pcm.end(), frame.begin(), frame.begin() + got * info.channels);
            frames_out += got;
            for (int i = 0; i < got * info.channels; i++)
                peak = std::max(peak, std::fabs(static_cast<double>(frame[i])));
        }
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double decode_s = std::chrono::duration<double>(t1 - t0).count();
    const double audio_s = twinvq::duration_seconds(info);

    std::cout << "decoded     " << frames_out << " samples, peak=" << peak << "\n";
    std::cout << "decode_time " << decode_s << " s";
    if (decode_s > 0)
        std::cout << " (" << (audio_s / decode_s) << "x realtime)";
    std::cout << "\n";
    write_wav16(out_path, info.sample_rate, info.channels, pcm);
    std::cout << "wrote       " << out_path << "\n";
    return 0;
}
