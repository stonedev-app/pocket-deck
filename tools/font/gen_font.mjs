// 日本語フォント（main/ui/fonts/font_jp_16.c）を生成する。
//
// 元フォント（Noto Sans JP）を取得し、lv_font_conv で LVGL 用の C ファイルに変換する。
// 使い方は README.md を参照。

import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url));
const CACHE_DIR = join(HERE, '.cache');
const OUT_DIR = join(HERE, '..', '..', 'main', 'ui', 'fonts');

// 元フォント。タグで版を固定し、SHA-256 で中身を確かめる
const SOURCE_TAG = 'Sans2.004';
const SOURCE_BASE = `https://github.com/notofonts/noto-cjk/raw/${SOURCE_TAG}`;
const FONT = {
    url: `${SOURCE_BASE}/Sans/SubsetOTF/JP/NotoSansJP-Regular.otf`,
    file: 'NotoSansJP-Regular.otf',
    sha256: 'dff723ba59d57d136764a04b9b2d03205544f7cd785a711442d6d2d085ac5073',
};
const LICENSE = {
    url: `${SOURCE_BASE}/LICENSE`,
    file: 'LICENSE',
    sha256: '6a73f9541c2de74158c0e7cf6b0a58ef774f5a780bf191f2d7ec9cc53efe2bf2',
};

const FONT_NAME = 'font_jp_16';
const SIZE_PX = 16;
const BPP = 4;

// LVGL は、LV_FONT_FMT_TXT_LARGE が無効だと、ビットマップの位置を 20 ビットで持つ
const BITMAP_LIMIT = 1 << 20;

/** @brief ファイルを取得して .cache に置く（あれば再利用する）。SHA-256 が合わなければ止める */
async function fetchCached({ url, file, sha256 }) {
    const path = join(CACHE_DIR, file);
    if (!existsSync(path)) {
        console.log(`download: ${url}`);
        const res = await fetch(url);
        if (!res.ok) {
            throw new Error(`download failed: ${res.status} ${url}`);
        }
        mkdirSync(CACHE_DIR, { recursive: true });
        writeFileSync(path, Buffer.from(await res.arrayBuffer()));
    }
    const actual = createHash('sha256').update(readFileSync(path)).digest('hex');
    if (actual !== sha256) {
        throw new Error(`sha256 mismatch: ${file}\n  expected ${sha256}\n  actual   ${actual}`);
    }
    return path;
}

/**
 * @brief フォントの name テーブルから、著作権表示（nameID 0）を読む
 *
 * OFL は、著作権表示を付けて配ることを求める。取得した LICENSE には著作権表示がないので、
 * フォントが持っている表示をそのまま使う。
 */
function readCopyright(font) {
    const table = findTable(font, 'name');
    const count = font.readUInt16BE(table + 2);
    const strings = table + font.readUInt16BE(table + 4);
    for (let i = 0; i < count; i++) {
        const rec = table + 6 + i * 12;
        const platformId = font.readUInt16BE(rec);
        const languageId = font.readUInt16BE(rec + 4);
        const nameId = font.readUInt16BE(rec + 6);
        // Windows（UTF-16BE）の英語（米国）の著作権表示
        if (platformId !== 3 || languageId !== 0x409 || nameId !== 0) {
            continue;
        }
        const start = strings + font.readUInt16BE(rec + 10);
        const bytes = Buffer.from(font.subarray(start, start + font.readUInt16BE(rec + 8)));
        return bytes.swap16().toString('utf16le').trim();
    }
    throw new Error('copyright notice not found in the font');
}

/** @brief フォントファイルの中の、テーブル（'name'、'cmap' など）の位置を返す */
function findTable(font, tag) {
    const numTables = font.readUInt16BE(4);
    for (let i = 0; i < numTables; i++) {
        const entry = 12 + i * 16;
        if (font.toString('latin1', entry, entry + 4) === tag) {
            return font.readUInt32BE(entry + 8);
        }
    }
    throw new Error(`table not found in the font: ${tag}`);
}

/**
 * @brief フォントの cmap テーブルを読み、コードポイントがフォントにあるかを返す関数を作る
 *
 * lv_font_conv は、フォントにない文字だけの範囲を渡すとエラーで止まるので、先に除く。
 * Unicode 全体を持つ形式（format 12）だけを読む。
 */
function readCmap(font) {
    const table = findTable(font, 'cmap');
    const count = font.readUInt16BE(table + 2);
    for (let i = 0; i < count; i++) {
        const rec = table + 4 + i * 8;
        const subtable = table + font.readUInt32BE(rec + 4);
        if (font.readUInt16BE(subtable) !== 12) {
            continue;
        }
        const groups = [];
        const numGroups = font.readUInt32BE(subtable + 12);
        for (let j = 0; j < numGroups; j++) {
            const group = subtable + 16 + j * 12;
            groups.push([font.readUInt32BE(group), font.readUInt32BE(group + 4)]);
        }
        return (cp) => groups.some(([first, last]) => first <= cp && cp <= last);
    }
    throw new Error('cmap format 12 not found in the font');
}

/**
 * @brief 収録する文字（Unicode のコードポイント）を集める
 *
 * JIS X 0208 の文字は、EUC-JP の 2 バイトを区点の順に復号して得る。
 */
function collectCodepoints() {
    const set = new Set();
    const addRange = (first, last) => {
        for (let cp = first; cp <= last; cp++) {
            set.add(cp);
        }
    };

    // ASCII
    addRange(0x20, 0x7e);

    // JIS X 0208: 1〜8 区（記号・英数字・かな・ギリシャ・キリル・罫線）、
    // 16〜47 区（第1水準の漢字）、48〜84 区（第2水準の漢字）
    const decoder = new TextDecoder('euc-jp', { fatal: true });
    const rows = [];
    for (let row = 1; row <= 8; row++) rows.push(row);
    for (let row = 16; row <= 84; row++) rows.push(row);
    for (const row of rows) {
        for (let cell = 1; cell <= 94; cell++) {
            let text;
            try {
                text = decoder.decode(Uint8Array.of(0xa0 + row, 0xa0 + cell));
            } catch {
                continue;  // 文字が割り当てられていない区点
            }
            set.add(text.codePointAt(0));
        }
    }

    // 同じ JIS の文字でも、変換表によって Unicode のコードポイントが違うもの。
    // 上の復号は Windows 系の表（～ U+FF5E など）なので、JIS 系の表のほうを足す。
    // ファイル名を作った OS によって、どちらも現れる。
    //   〜 U+301C、‖ U+2016、− U+2212、¢ U+00A2、£ U+00A3、¬ U+00AC、— U+2014
    for (const cp of [0x301c, 0x2016, 0x2212, 0x00a2, 0x00a3, 0x00ac, 0x2014]) {
        set.add(cp);
    }

    // 半角カタカナ（JIS X 0201）
    addRange(0xff61, 0xff9f);

    return [...set].sort((a, b) => a - b);
}

/** @brief コードポイントの並びを、lv_font_conv の -r に渡す形（0x20-0x7e,0x3001,...）にする */
function toRangeArg(codepoints) {
    const parts = [];
    let first = codepoints[0];
    let last = first;
    const flush = () => {
        const hex = (cp) => `0x${cp.toString(16)}`;
        parts.push(first === last ? hex(first) : `${hex(first)}-${hex(last)}`);
    };
    for (const cp of codepoints.slice(1)) {
        if (cp === last + 1) {
            last = cp;
            continue;
        }
        flush();
        first = last = cp;
    }
    flush();
    return parts.join(',');
}

const fontPath = await fetchCached(FONT);
const licensePath = await fetchCached(LICENSE);

const font = readFileSync(fontPath);
const inFont = readCmap(font);
const wanted = collectCodepoints();
const codepoints = wanted.filter(inFont);
const missing = wanted.filter((cp) => !inFont(cp));
const outPath = join(OUT_DIR, `${FONT_NAME}.c`);
mkdirSync(OUT_DIR, { recursive: true });

try {
    execFileSync(
        join(HERE, 'node_modules', '.bin', 'lv_font_conv'),
        [
            '--font', fontPath,
            '-r', toRangeArg(codepoints),
            '--size', String(SIZE_PX),
            '--bpp', String(BPP),
            '--format', 'lvgl',
            '--no-compress',
            '--no-prefilter',
            '--no-kerning',
            '--lv-include', 'lvgl.h',
            '--lv-font-name', FONT_NAME,
            '-o', outPath,
        ],
        { stdio: 'inherit' },
    );
} catch (err) {
    // 理由は lv_font_conv が出している。ここで err を出すと、数万文字の引数まで出てしまう
    console.error(`lv_font_conv failed (exit ${err.status})`);
    process.exit(1);
}

// 生成されたファイルの先頭のコメント（数万文字の Opts の行を含む）を、出どころとライセンスの表示に置き換える
const copyright = readCopyright(font);
const header = `/*******************************************************************************
 * ${FONT_NAME}: Noto Sans JP Regular, ${SIZE_PX} px, ${BPP} bpp
 *
 * このファイルは tools/font/gen_font.mjs が生成する。手で編集しない。
 * 収録: ASCII、JIS X 0208 の非漢字と第1・第2水準の漢字、半角カタカナ（${codepoints.length} 文字）
 * 元フォント: ${FONT.url}
 *
 * ${copyright}
 * This Font Software is licensed under the SIL Open Font License, Version 1.1.
 * ライセンスの全文は、同じフォルダの OFL.txt を参照。
 ******************************************************************************/`;
const generated = readFileSync(outPath, 'utf8');
const commentEnd = generated.indexOf('*/');
if (!generated.startsWith('/*') || commentEnd < 0) {
    throw new Error('unexpected lv_font_conv output: no leading comment');
}
writeFileSync(outPath, header + generated.slice(commentEnd + 2));
writeFileSync(join(OUT_DIR, 'OFL.txt'), readFileSync(licensePath));

let bitmapSize = 0;
for (const m of generated.matchAll(/\.bitmap_index = (\d+)/g)) {
    bitmapSize = Math.max(bitmapSize, Number(m[1]));
}

console.log(`output: ${outPath}`);
console.log(`glyphs: ${codepoints.length} / ${wanted.length}`);
console.log(`bitmap: about ${Math.round(bitmapSize / 1024)} KB`);
if (missing.length > 0) {
    const list = missing.map((cp) => `U+${cp.toString(16).toUpperCase().padStart(4, '0')}`);
    console.warn(`warning: not in the source font: ${list.join(' ')}`);
}
if (bitmapSize >= BITMAP_LIMIT) {
    console.warn('warning: bitmap is 1 MB or larger. CONFIG_LV_FONT_FMT_TXT_LARGE=y is required.');
}
