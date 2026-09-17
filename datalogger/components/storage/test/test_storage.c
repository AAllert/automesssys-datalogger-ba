// tests
#include "storage.h" 

// system includes
#include <dirent.h>
#include "unity.h"
#include "esp_err.h"
#include "esp_log.h"

// project includes
#include "test_helper.h"

#define TAG "[storage]"
#define TEST_FOLDER "/sdcard/test/"

TEST_CASE("append creates file", TAG)
{
    esp_err_t result = storage_delete_file(TEST_FOLDER "dragon.txt");
    if (!(result == ESP_OK || result == ESP_ERR_NOT_FOUND)) {
        TEST_IGNORE_MESSAGE("Path was not empty and could not be deleted.");
    }
    TEST_ASSERT_ESP_ERR(ESP_OK, storage_append_file(TEST_FOLDER "dragon.txt", "In the land of the dragon"));
    TEST_ASSERT_TRUE(file_exists(TEST_FOLDER "dragon.txt"));
}

TEST_CASE("write file creates path", TAG)
{
    storage_delete_directory(TEST_FOLDER "subfolder", true);
    if (storage_directory_exists(TEST_FOLDER "subfolder")) {
        TEST_IGNORE_MESSAGE("folder for testing could not be deleted");
    }
    TEST_ASSERT_ESP_ERR(ESP_OK, storage_write_file(TEST_FOLDER "subfolder/path.txt", "Toller Text"));
    TEST_ASSERT_TRUE(storage_directory_exists(TEST_FOLDER "subfolder"));
}

TEST_CASE("append creates path", TAG)
{
    storage_delete_directory(TEST_FOLDER "subfolder", true);
    if (storage_directory_exists(TEST_FOLDER "subfolder")) {
        TEST_IGNORE_MESSAGE("folder for testing could not be deleted");
    }
    TEST_ASSERT_ESP_ERR(ESP_OK, storage_append_file(TEST_FOLDER "subfolder/pathappend.txt", "Toller Text"));
    TEST_ASSERT_TRUE(storage_directory_exists(TEST_FOLDER "subfolder"));
}

TEST_CASE("on append previous and new content are written", TAG)
{
    esp_err_t result = storage_delete_file(TEST_FOLDER "journey.txt");
    if (!(result == ESP_OK || result == ESP_ERR_NOT_FOUND)) {
        TEST_IGNORE_MESSAGE("Path was not empty and could not be deleted.");
    }
    TEST_ASSERT_ESP_ERR(ESP_OK, storage_append_file(TEST_FOLDER "journey.txt", "On my way to another world\n"));
    TEST_ASSERT_ESP_ERR(ESP_OK, storage_append_file(TEST_FOLDER "journey.txt", "A desert world where an outrage occurred"));
    char *data = NULL;
    size_t size = 0;
    result = storage_read_file(TEST_FOLDER "journey.txt", &data, &size);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("On my way to another world\nA desert world where an outrage occurred", data, "read text is not the expected text.");
    free(data);
}

TEST_CASE("delete deletes file successfully", TAG)
{
    storage_write_file(TEST_FOLDER "thoughts.txt", "These thoughts of emptiness");
    if (!file_exists(TEST_FOLDER "thoughts.txt")) {
        TEST_IGNORE_MESSAGE("file to delete could not be created");
    }
    TEST_ASSERT_ESP_ERR(ESP_OK, storage_delete_file(TEST_FOLDER "thoughts.txt"));
    TEST_ASSERT_FALSE(file_exists(TEST_FOLDER "thoughts.txt"));
}

TEST_CASE("delete on empty path", TAG)
{
    storage_delete_file(TEST_FOLDER "mockingbird.txt");
    if (file_exists(TEST_FOLDER "mockingbird.txt")) {
        TEST_IGNORE_MESSAGE("Path was not empty and could not be deleted.");
    }
    TEST_ASSERT_ESP_ERR(ESP_OK, storage_delete_file(TEST_FOLDER "mockingbird.txt"));
}

TEST_CASE("delete directory recursive normal", TAG)
{
    storage_write_file(TEST_FOLDER "got/house_stark.txt", "Winter is Coming");
    if (!storage_directory_exists(TEST_FOLDER "got")){
        TEST_IGNORE_MESSAGE("directory to delete could not be created");
    }
    TEST_ASSERT_ESP_ERR(ESP_OK, storage_delete_directory(TEST_FOLDER "got", true));
    TEST_ASSERT_FALSE(storage_directory_exists(TEST_FOLDER "got"));
}

TEST_CASE("delete directory not recursive fails with existing file", TAG)
{
    storage_write_file(TEST_FOLDER "got/house_piper.txt", "Brave and Beautiful");
    if (!storage_directory_exists(TEST_FOLDER "got")){
        TEST_IGNORE_MESSAGE("directory to delete could not be created");
    }
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_STATE, storage_delete_directory(TEST_FOLDER "got", false));
    TEST_ASSERT_TRUE(storage_directory_exists(TEST_FOLDER "got"));
}

TEST_CASE("delete directory not recursive empty succeeds", TAG)
{
    storage_write_file(TEST_FOLDER "lotr/ring_verse.txt", "Ash nazg durbatulûk, ash nazg gimbatul, ash nazg thrakatulûk, agh burzum-ishi krimpatul");
    storage_delete_file(TEST_FOLDER "lotr/ring_verse.txt");
    if (!storage_directory_exists(TEST_FOLDER "lotr") || file_exists(TEST_FOLDER "lotr/ring_verse.txt")){
        TEST_IGNORE_MESSAGE("empty directory to delete could not be created");
    }
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_STATE, storage_delete_directory(TEST_FOLDER "got", false));
    TEST_ASSERT_TRUE(storage_directory_exists(TEST_FOLDER "got"));
}

TEST_CASE("file exists where file exists", TAG)
{
    esp_err_t result = storage_write_file(TEST_FOLDER "scratchstone.txt", "Claws as hard as steel");
    ESP_LOGI(TAG, "Fehlermeldung write file: %s", esp_err_to_name(result));
    if (result != ESP_OK) {
        TEST_IGNORE_MESSAGE("File for testing could not be created");
    }
    TEST_ASSERT_TRUE(file_exists(TEST_FOLDER "scratchstone.txt"));
}

TEST_CASE("file exists on empty path", TAG)
{
    esp_err_t result = storage_delete_file(TEST_FOLDER "wizards_call.txt");
    if (!(result == ESP_OK || result == ESP_ERR_NOT_FOUND)) {
        TEST_IGNORE_MESSAGE("Path was not empty and could not be deleted.");
    }
    TEST_ASSERT_FALSE(file_exists("/sdcard/wizards_call.txt"));
}

TEST_CASE("read on existing file", TAG)
{
    storage_write_file(TEST_FOLDER "goddess.txt", "Harried, betrayed and tantalized");
    if (!file_exists(TEST_FOLDER "goddess.txt")) {
        TEST_IGNORE_MESSAGE("File for testing could not be created");
    }
    char *data = NULL;
    size_t size = 0;
    TEST_ASSERT_ESP_ERR(ESP_OK, storage_read_file(TEST_FOLDER "goddess.txt", &data, &size));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("Harried, betrayed and tantalized", data, "read text is not the expected text.");
    free(data);
}

TEST_CASE("read on empty path", TAG)
{
    esp_err_t result = storage_delete_file(TEST_FOLDER "doomed_to_die.txt");
    if (!(result == ESP_OK || result == ESP_ERR_NOT_FOUND)) {
        TEST_IGNORE_MESSAGE("Path was not empty and could not be deleted.");
    }
    char *data = NULL;
    size_t size = 0;
    TEST_ASSERT_ESP_ERR(ESP_ERR_NOT_FOUND, storage_read_file(TEST_FOLDER "doomed_to_die.txt", &data, &size));
    free(data);
}

TEST_CASE("write on existing path", TAG)
{
    storage_write_file(TEST_FOLDER "karawanen.txt", "Die Sonne strahlt, man spürt ihren Zorn");
    if (!file_exists(TEST_FOLDER "karawanen.txt")) {
        TEST_IGNORE_MESSAGE("File for testing could not be created");
    }
    TEST_ASSERT_ESP_ERR(ESP_OK, storage_write_file(TEST_FOLDER "karawanen.txt", "ständig fliegt ins Auge ein Korn"));
    char *data = NULL;
    size_t size = 0;
    esp_err_t result = storage_read_file(TEST_FOLDER "karawanen.txt", &data, &size);
    if (result != ESP_OK) {
        TEST_IGNORE_MESSAGE("created file could not be read");
    }
    TEST_ASSERT_EQUAL_STRING_MESSAGE("ständig fliegt ins Auge ein Korn", data, "read text is not the expected text.");
    free(data);
}

TEST_CASE("min filename length for date is accepted", TAG)
{
    // This is neccessary to check, because FatFS decides between SFN and LFN
    TEST_ASSERT_ESP_ERR(ESP_OK, storage_write_file(TEST_FOLDER "raw_2025-07-29_09-19-36.log", "Tolle Log Daten"));
}

TEST_CASE("LFN is enabled, max. filename length is set correctly", TAG)
{
    // Check sdkconfig value
    TEST_ASSERT_TRUE(MAX_FILENAME_LENGTH >= 100);
    // Check long filename really works
    TEST_ASSERT_ESP_ERR(ESP_OK, storage_write_file(TEST_FOLDER "123456789abcdefghijklmnopqrstuvwxyz.log", "Langer Dateiname"));
}
