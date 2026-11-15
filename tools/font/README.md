# 日本語フォントの生成

`main/ui/fonts/font_jp_16.c`（LVGL 用の日本語フォント）を生成するツールです。
生成したファイルはリポジトリにコミットしてあるので、ふだんのビルドでは実行する必要はありません。
収録する文字やサイズを変えるときだけ使います。

## 必要なもの

- Node.js（v24 で確認）
- pnpm

## 使い方

```sh
cd tools/font
pnpm install
pnpm gen
```

`pnpm gen` は次のことをします。

1. 元フォント（Noto Sans JP Regular）とライセンス文を、`.cache/` に取得する。取得済みなら再利用する。
2. 収録する文字の一覧を作る。
3. [lv_font_conv](https://github.com/lvgl/lv_font_conv) で `main/ui/fonts/font_jp_16.c` を生成する。
4. ライセンス文を `main/ui/fonts/OFL.txt` に置く。

最後に、収録した文字数とビットマップの大きさを表示します。

## 生成するフォント

| 項目 | 内容 |
|---|---|
| 元フォント | Noto Sans JP Regular（[notofonts/noto-cjk](https://github.com/notofonts/noto-cjk) のタグ `Sans2.004`） |
| 大きさ | 16 px、4 bpp |
| 収録する文字 | ASCII、JIS X 0208 の非漢字（1〜8 区）、第1水準の漢字（16〜47 区）、第2水準の漢字（48〜84 区）、半角カタカナ |

元フォントにない文字は、収録せずに警告を出します。現在は「≒」（U+2252）の 1 文字です。

## 設定を変えるとき

`gen_font.mjs` の先頭の定数と `collectCodepoints()` を変えます。

- 元フォントを変えるときは、`FONT` の `url` と `sha256` を変える。`sha256` が合わないと、実際の値を表示して止まるので、その値を書く。
- ビットマップが 1 MB 以上になると警告が出る。その場合は、`sdkconfig.defaults` に `CONFIG_LV_FONT_FMT_TXT_LARGE=y` を足すか、`BPP` を下げる。

## ライセンス

Noto Sans JP は SIL Open Font License 1.1 です。生成した `font_jp_16.c` も同じライセンスで、著作権表示を先頭のコメントに入れています。
このライセンスは、元フォントが予約名を宣言している場合、変更したフォントの名前にその予約名を使うことを認めていません。
`Sans2.004` の著作権表示とライセンス文には、予約名の宣言は見当たりません。ただし、以前の版は「Source」を予約名にしていて、「Noto」は Google の商標です。フォントの名前（`FONT_NAME`）を変えるときは、どちらも含めないでください。
