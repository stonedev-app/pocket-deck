#pragma once

#include <stddef.h>
#include <stdint.h>

// WAV ファイルのヘッダ解析。表示には依存しない。
// 対応は 16bit PCM（フォーマットタグ 1）のモノラル / ステレオのみ。

namespace wav {

/** @brief parse() が取り出した、再生に必要な WAV の情報 */
struct Info {
    uint32_t sampleRate;     // サンプルレート [Hz]
    uint16_t channels;       // チャンネル数（1 か 2）
    uint16_t bitsPerSample;  // ビット幅（16）
    uint32_t dataOffset;     // data チャンクの中身の開始位置（ファイル先頭からのバイト数）
    uint32_t dataBytes;      // data チャンクの中身のバイト数
};

/**
 * @brief ファイルの先頭から RIFF/WAVE のチャンクをたどり、形式とデータの位置を取り出す
 *
 * fmt チャンクより後に data チャンクがあるファイルだけを扱う。LIST などの未知のチャンクは読み飛ばす。
 *
 * @param[in]  path    WAV ファイルのパス
 * @param[out] out     解析結果。戻り値が true のときだけ有効
 * @param[out] err     失敗の理由（短い英語の文字列）を入れるバッファ
 * @param[in]  errSize err のバイト数
 * @return 成功で true。非対応の形式やヘッダの破損、読み出しの失敗は false
 */
bool parse(const char *path, Info &out, char *err, size_t errSize);

}  // namespace wav
