#pragma once

// 起動順序。ui を呼び、ui は app を呼ぶので、依存が循環しないよう app/ ではなく main/ の直下に置く。
namespace startup {

/**
 * @brief 初期化とタスクの起動だけを行い、あとは画面のタスクに任せる
 *
 * 失敗したら、ログと画面に理由を出して return する（以降のタスクは起動しない）。
 */
void run();

}  // namespace startup
