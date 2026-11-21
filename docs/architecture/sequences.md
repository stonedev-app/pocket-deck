# 処理の流れ（シーケンス図）

主な処理の流れを、5つの図で示す。用語は、[tasks.md](tasks.md)と[overview.md](overview.md)を参照。

## 1. 起動

```mermaid
sequenceDiagram
    participant S as 各部初期化
    participant SP as 音声の出力
    participant SD as SDカード
    participant K as キーボード
    participant E as イコライザーの計算
    participant PL as 曲一覧
    participant UI as 画面

    S->>S: 本体の初期化
    S->>SP: 起動し、音量を復元
    S->>SD: マウント
    S->>K: タスクと割り込みを作成
    S->>E: タスクを作成
    S->>PL: SDカードから曲一覧を作成
    S->>UI: 画面を組み立て、画面のタスクを作成
```

### 補足

| 処理 | 行うタイミング | 理由 |
|------|----------------|------|
| 曲一覧の作成 | SDカードのマウントの後 | 曲一覧はSDカードから読み込む |
| 画面の組み立て | 最後 | 画面はキー入力と曲一覧を使う |

## 2. 一覧から曲を選んで再生

```mermaid
sequenceDiagram
    participant K as キーボード
    participant UI as 画面
    participant PB as 再生制御
    participant P as 音声ファイルの読み込み

    Note over K: キーが押されると、割り込みで動き出す
    K->>UI: 押されたキーを送る
    UI->>UI: キーを操作に変える（Enter → 決定）
    UI->>PB: 選んだ曲の再生を指示
    PB->>PB: 音声ファイルの形式を確認
    alt 形式が対応
        PB->>P: タスクを作成し、再生を開始
        PB-->>UI: 開始の結果（成功/失敗）
    else 形式が非対応
        PB-->>UI: 開始の結果（失敗）
    end
    alt 成功
        UI->>UI: 再生中の画面を表示
    else 失敗
        Note over UI: 一覧画面のまま
    end
```

## 3. 再生中の音声とイコライザー

```mermaid
sequenceDiagram
    participant P as 音声ファイルの読み込み
    participant SD as SDカード
    participant SP as 音声の出力
    participant E as イコライザーの計算
    participant UI as 画面

    par 読み込みと計算
        loop ファイルの終わりまで
            P->>SD: 少しずつ読む
            P->>SP: 音声データを渡す（受け取れるまで待つ）
            P->>E: 同じ音声データを渡す（計算中なら破棄）
            E->>E: 12帯域のレベルを計算
        end
    and 描画
        loop 一定間隔
            UI->>E: レベルを読む
            UI->>UI: 棒を描き直す
        end
    end
```

## 4. 曲が終わったとき

```mermaid
sequenceDiagram
    participant P as 音声ファイルの読み込み
    participant PB as 再生制御
    participant UI as 画面

    P->>P: 最後まで読み、鳴り終わるのを待つ
    P->>PB: 再生の終了（完了）をキューに送る
    P->>P: タスクを破棄
    loop 一定間隔
        UI->>PB: 再生の状態を確認
        opt 再生の終了が届いている
            PB->>PB: 再生の終了を受け取る
            alt リピートがオン
                PB->>P: タスクを作成し、同じ曲を最初から再生
            else リピートがオフ
                Note over PB: 停止のまま。次の曲には進まない
            end
        end
        UI->>UI: 表示を更新
    end
```

## 5. 音量の変更

```mermaid
sequenceDiagram
    participant UI as 画面
    participant V as 音量
    participant SP as 音声の出力
    participant T as 保存のタイマー
    participant N as 不揮発メモリ（NVS）

    UI->>V: 音量のキーで、段階を上げ下げ
    V->>SP: 段階に応じた音量を設定
    V->>T: 1秒後に保存を予約
    loop 一定間隔
        UI->>V: 音量を確認
        UI->>UI: 変わっていたら、音量の表示を描き直す
    end
    Note over T: 最後の変更から1秒後。連打しても保存は1回だけ
    T->>V: 保存を指示
    V->>N: 段階を保存
```
