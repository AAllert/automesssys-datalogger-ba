// implements
#include "storage.h"

// system includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "esp_log.h"

#define TAG "storage"
#define READLINE_CHUNK_SIZE 128

/**
 * @brief Analyses the passed path and creates all directories if needed. 
 */
static esp_err_t create_directories(const char *path)
{
    char tmp[MAX_STORAGE_PATH_LENGTH];

    if (strlen(path) >= sizeof(tmp)) return ESP_ERR_INVALID_SIZE;

    strcpy(tmp, path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';

            struct stat st;
            if (stat(tmp, &st) != 0) {
                if (mkdir(tmp, 0755) != 0) {
                    return ESP_FAIL;
                }
            }

            *p = '/';
        }
    }

    return ESP_OK;
}

esp_err_t storage_write_file(const char *path, const char *data)
{
    esp_err_t error = create_directories(path);
    if (error != ESP_OK) return error;

    FILE *f = fopen(path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", path);
        return ESP_FAIL;
    }

    fprintf(f, "%s", data);
    fclose(f);

    ESP_LOGI(TAG, "File written: %s", path);
    return ESP_OK;
}

esp_err_t storage_append_file(const char *path, const char *data)
{
    esp_err_t error = create_directories(path);
    if (error != ESP_OK) return error;
    
    FILE *f = fopen(path, "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for appending: %s", path);
        return ESP_FAIL;
    }

    fprintf(f, "%s", data);
    fclose(f);

    ESP_LOGI(TAG, "File appended: %s", path);
    return ESP_OK;
}

esp_err_t storage_read_file(const char *path, char **data, size_t *size)
{
    if (path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!file_exists(path)) {
        return ESP_ERR_NOT_FOUND;
    }
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", path);
        return ESP_FAIL;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    *data = malloc(file_size + 1);
    if (*data == NULL) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    size_t bytes_read = fread(*data, 1, file_size, f);
    (*data)[bytes_read] = '\0';
    *size = bytes_read;

    fclose(f);
    return ESP_OK;
}

bool storage_read_line(FILE *file, char **line) {
    if (file == NULL || line == NULL) return false;
    static size_t capacity = 0;

    // Buffer beim ersten Aufruf anlegen
    if (*line == NULL) {
        capacity = READLINE_CHUNK_SIZE;
        *line = malloc(capacity);

        if (*line == NULL) {
            return false;
        }
    }

    (*line)[0] = '\0';
    size_t length = 0;

    char chunk[READLINE_CHUNK_SIZE];

    while (fgets(chunk, sizeof(chunk), file) != NULL) {
        size_t chunkLen = strlen(chunk);

        // allocate memory
        while (length + chunkLen + 1 > capacity) {
            capacity *= 2;

            char *tmp = realloc(*line, capacity);

            if (tmp == NULL) {
                free(*line);
                *line = NULL;
                capacity = 0;
                return false;
            }

            *line = tmp;
        }

        memcpy(*line + length, chunk, chunkLen);
        length += chunkLen;
        (*line)[length] = '\0';

        if (chunkLen > 0 && chunk[chunkLen - 1] == '\n') {
            return true;
        }
    }

    if (length > 0) {
        return true;
    }
    return false;
}

esp_err_t storage_delete_file(const char *path){
    if (path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!file_exists(path)) {
        return ESP_OK;
    }
    if (remove(path) == 0) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t storage_delete_directory(const char *path, bool recursive)
{
    if (path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    DIR *dir = opendir(path);
    if (dir == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t error = ESP_OK;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        if (!recursive) {
            ESP_LOGE(TAG, "Directory not empty: %s", path);
            error = ESP_ERR_INVALID_STATE;
            break;
        }

        char child_path[MAX_STORAGE_PATH_LENGTH];
        int written = snprintf(child_path, sizeof(child_path), "%s/%s", path, entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(child_path)) {
            error = ESP_ERR_INVALID_SIZE;
            break;
        }

        struct stat st;
        if (stat(child_path, &st) != 0) {
            error = ESP_FAIL;
            break;
        }

        if (S_ISDIR(st.st_mode)) {
            error = storage_delete_directory(child_path, true);
        } else if (remove(child_path) != 0) {
            error = ESP_FAIL;
        }

        if (error != ESP_OK) break;
    }
    closedir(dir);

    if (error != ESP_OK) return error;

    if (rmdir(path) != 0) {
        ESP_LOGE(TAG, "Failed to delete directory: %s", path);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Directory deleted: %s", path);
    return ESP_OK;
}

bool file_exists(const char *path)
{
    if (path == NULL) return false;
    FILE *f = fopen(path, "r");
    if (f == NULL) return false;
    fclose(f);
    return true;
}

bool storage_directory_exists(const char *path)
{
    DIR *dir = opendir(path);
    if (dir != NULL) {
        closedir(dir);
        return true;
    }
    return false;
}

bool storage_resolve_data_path(const char *uri, char *path, size_t path_size)
{
    if (uri == NULL || path == NULL || path_size == 0) return false;

    static const struct {
        const char *uri_prefix;
        const char *fs_prefix;
    } data_dirs[] = {
        { "/configs/", "/sdcard/configs/" },
        { "/logs/",    "/sdcard/logs/" },
    };

    for (size_t i = 0; i < sizeof(data_dirs) / sizeof(data_dirs[0]); i++) {
        size_t prefix_len = strlen(data_dirs[i].uri_prefix);
        if (strncmp(uri, data_dirs[i].uri_prefix, prefix_len) != 0) continue;

        const char *filename = uri + prefix_len;
        if (filename[0] == '\0' || strstr(filename, "..") != NULL) return false;

        int written = snprintf(path, path_size, "%s%s", data_dirs[i].fs_prefix, filename);
        return written > 0 && (size_t)written < path_size;
    }

    return false;
}

bool storage_list_files(const char *directory, const char *extension, FileInfo *files, size_t offset, size_t max_files, size_t *file_count) {
    if (directory == NULL || files == NULL || file_count == NULL || max_files == 0) return false;

    *file_count = 0;
    DIR *dir = opendir(directory);
    if (dir == NULL) return false;

    struct dirent *entry;
    size_t index = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        const char *ext = strrchr(entry->d_name, '.');
        if (extension != NULL) {
            if (ext == NULL || strcmp(ext, extension) != 0) continue;
        }
        if (index++ < offset) continue;
        if (*file_count >= max_files) break;

        strlcpy(files[*file_count].name, entry->d_name, MAX_FILENAME_LENGTH);

        char path[MAX_STORAGE_PATH_LENGTH];
        snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
        struct stat st;
        if (stat(path, &st) != 0) continue;
        files[*file_count].size = (st.st_size + 1023) / 1024;

        (*file_count)++;
    }
    closedir(dir);
    return true;
}