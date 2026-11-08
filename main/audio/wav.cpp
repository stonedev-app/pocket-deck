#include "audio/wav.hpp"

#include <stdio.h>
#include <string.h>

namespace wav {

// fmt チャンクの基本部分のバイト数。PCM（タグ 1）ならこれで全部で、
// 拡張部分（WAVE_FORMAT_EXTENSIBLE など）は使わないので読み飛ばす
static constexpr uint32_t FMT_MIN_SIZE = 16;

// WAV はリトルエンディアン固定。ESP32-S3 もリトルエンディアンだが、アラインされていない
// バッファ上の値を安全に読むため、バイト単位で組み立てる
static uint16_t le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// fmt には %u を 1 つだけ含められる（なくてもよい）
static void setError(char *err, size_t errSize, const char *fmt, unsigned a = 0)
{
    if (err && errSize > 0) snprintf(err, errSize, fmt, a);
}

bool parse(const char *path, Info &out, char *err, size_t errSize)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        setError(err, errSize, "cannot open file");
        return false;
    }

    bool ok = false;
    bool haveFmt = false;
    Info info = {};

    // ファイルサイズは、壊れた（途中で切れた）ファイルのチャンクサイズを検出するために使う。
    // サイズを信じて読み進めると、player が存在しないデータを再生しようとするため
    fseek(f, 0, SEEK_END);
    uint64_t fileSize = (uint64_t)ftell(f);
    fseek(f, 0, SEEK_SET);

    // RIFF ヘッダ（12 バイト）: "RIFF" + 全体のサイズ + "WAVE"
    uint8_t head[12];
    if (fread(head, 1, sizeof(head), f) != sizeof(head)) {
        setError(err, errSize, "file too short");
    } else if (memcmp(head, "RIFF", 4) != 0 || memcmp(head + 8, "WAVE", 4) != 0) {
        setError(err, errSize, "not a RIFF/WAVE file");
    } else {
        // チャンク（8 バイトのヘッダ: ID + サイズ）を順にたどる。pos は次に読む位置
        uint64_t pos = sizeof(head);
        while (true) {
            uint8_t ch[8];
            if (fread(ch, 1, sizeof(ch), f) != sizeof(ch)) {
                setError(err, errSize, haveFmt ? "data chunk not found" : "fmt chunk not found");
                break;
            }
            uint32_t size = le32(ch + 4);
            pos += sizeof(ch);

            if (memcmp(ch, "fmt ", 4) == 0) {
                uint8_t fmt[FMT_MIN_SIZE];
                if (size < FMT_MIN_SIZE || fread(fmt, 1, sizeof(fmt), f) != sizeof(fmt)) {
                    setError(err, errSize, "fmt chunk broken");
                    break;
                }
                uint16_t tag       = le16(fmt);
                info.channels      = le16(fmt + 2);
                info.sampleRate    = le32(fmt + 4);
                info.bitsPerSample = le16(fmt + 14);
                if (tag != 1) {
                    setError(err, errSize, "not PCM (tag %u)", tag);
                    break;
                }
                if (info.channels != 1 && info.channels != 2) {
                    setError(err, errSize, "unsupported channels: %u", info.channels);
                    break;
                }
                if (info.bitsPerSample != 16) {
                    setError(err, errSize, "unsupported bits: %u", info.bitsPerSample);
                    break;
                }
                haveFmt = true;
            } else if (memcmp(ch, "data", 4) == 0) {
                if (!haveFmt) {
                    setError(err, errSize, "data before fmt chunk");
                    break;
                }
                if (pos + size > fileSize) {
                    setError(err, errSize, "data chunk truncated");
                    break;
                }
                info.dataOffset = (uint32_t)pos;
                info.dataBytes  = size;
                ok = true;
                break;
            }

            // 次のチャンクの先頭へ進む。サイズが奇数のときは 1 バイトのパディングがある
            uint64_t next = pos + size + (size & 1);
            if (next > fileSize) {
                setError(err, errSize, "chunk exceeds file size");
                break;
            }
            if (fseek(f, (long)next, SEEK_SET) != 0) {
                setError(err, errSize, "seek failed");
                break;
            }
            pos = next;
        }
    }

    fclose(f);
    if (ok) out = info;
    return ok;
}

}  // namespace wav
