#include "app/playlist.hpp"

#include <dirent.h>
#include <string.h>
#include <strings.h>

#include <algorithm>
#include <vector>

#include "device/sdcard.hpp"
#include "esp_log.h"

namespace playlist {

static const char *TAG = "playlist";

static std::vector<std::string> s_files;

/**
 * @brief ファイルの拡張子が .wav（大文字小文字を区別しない）か
 *
 * macOS がコピー時に作る隠しファイル（._ で始まる名前）は音声ではないので除く。
 */
static bool isWav(const struct dirent *ent)
{
    if (ent->d_type == DT_DIR) return false;
    if (ent->d_name[0] == '.') return false;
    const char *dot = strrchr(ent->d_name, '.');
    return dot && strcasecmp(dot, ".wav") == 0;
}

void load()
{
    s_files.clear();
    DIR *dir = opendir(sdcard::MOUNT_POINT);
    if (!dir) {
        ESP_LOGE(TAG, "opendir failed: %s", sdcard::MOUNT_POINT);
        return;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (!isWav(ent)) continue;
        ESP_LOGI(TAG, "wav: %s", ent->d_name);
        s_files.push_back(ent->d_name);
    }
    closedir(dir);

    // 大文字小文字を区別せず昇順。同じならバイト順で決める（順序を毎回同じにするため）
    std::sort(s_files.begin(), s_files.end(), [](const std::string &a, const std::string &b) {
        int c = strcasecmp(a.c_str(), b.c_str());
        if (c != 0) return c < 0;
        return strcmp(a.c_str(), b.c_str()) < 0;
    });

    ESP_LOGI(TAG, "wav files: %d", (int)s_files.size());
}

int count()
{
    return (int)s_files.size();
}

const char *nameAt(int index)
{
    if (index < 0 || index >= (int)s_files.size()) return nullptr;
    return s_files[index].c_str();
}

std::string pathAt(int index)
{
    const char *name = nameAt(index);
    if (!name) return std::string();
    return std::string(sdcard::MOUNT_POINT) + "/" + name;
}

int indexOf(const char *name)
{
    for (int i = 0; i < (int)s_files.size(); i++) {
        if (s_files[i] == name) return i;
    }
    return -1;
}

}  // namespace playlist
