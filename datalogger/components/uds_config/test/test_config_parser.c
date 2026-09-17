// tests
#include "config_parser.h"

// system includes
#include <stdlib.h>
#include "unity.h"

// project includes
#include "test_helper.h"
#include "storage.h"
#include "uds_config.h"

#define TAG "[config_parser]"

static config_column_t columns[] = {
    {"strLab", -1 },
    {"numIdTstr", -1 },
    {"numSid", -1 },
    {"numIdGtwy", -1 },
    {"numBytes", -1},
    {"strDbc", -1 },
    {"numOfs", -1 },
    {"numFac", -1 },
    {"numDid", -1 },
    {"strType", -1 },
    {"stIntp", -1 },
};
#define COLUMN_COUNT 11

TEST_CASE("clean token removes whitespaces", TAG)
{
    char str[] = " \"  Hallo ";
    char *result = clean_token(str);
    TEST_ASSERT_EQUAL_STRING("Hallo", result);
}

TEST_CASE("clean token removes double quotes", TAG)
{
    char str[] = "\"Hallo\"\"";
    char *result = clean_token(str);
    TEST_ASSERT_EQUAL_STRING("Hallo", result);
}

TEST_CASE("clean token removes mixed", TAG)
{
    char str[] = "   \"        Hallo   \"     \"";
    char *result = clean_token(str);
    TEST_ASSERT_EQUAL_STRING("Hallo", result);
}

TEST_CASE("find indexes where all columns exist", TAG)
{
    char line[] = "strEcu,strLabLong,strLab,numIdTstr,numIdGtwy,numSid,numDid,numDwnSamp,numBytes,strtByte,lenByte,strDbc,strType,numFac,numOfs,stIntp,stUse1,stUse2,stUse3,stUse4,stRvt,numMtyLvl,strCmnt,strName";
    TEST_ASSERT_ESP_ERR(ESP_OK, find_columns_indices(line, columns, COLUMN_COUNT));
    TEST_ASSERT_EQUAL(2, columns[STR_LAB].index);
    TEST_ASSERT_EQUAL(3, columns[NUM_ID_STR].index);
    TEST_ASSERT_EQUAL(5, columns[NUM_SID].index);
    TEST_ASSERT_EQUAL(4, columns[NUM_ID_GTWY].index);
    TEST_ASSERT_EQUAL(8, columns[NUM_BYTES].index);
    TEST_ASSERT_EQUAL(11, columns[STR_DBC].index);
    TEST_ASSERT_EQUAL(14, columns[NUM_OFS].index);
    TEST_ASSERT_EQUAL(13, columns[NUM_FAC].index);
    TEST_ASSERT_EQUAL(6, columns[NUM_DID].index);
    TEST_ASSERT_EQUAL(12, columns[STR_TYPE].index);
    TEST_ASSERT_EQUAL(15, columns[ST_INTP].index);
}

TEST_CASE("find indexes where all columns exist with spaces", TAG)
{
    char line[] = "strEcu,strLabLong,strLab              ,numIdTstr,     numIdGtwy    ,  numSid,  numDid,numDwnSamp,numBytes,strtByte,lenByte,strDbc               ,strType,numFac,numOfs,stIntp,stUse1,stUse2,stUse3,stUse4,stRvt,numMtyLvl,strCmnt,strName";
    TEST_ASSERT_ESP_ERR(ESP_OK, find_columns_indices(line, columns, COLUMN_COUNT));
    TEST_ASSERT_EQUAL(2, columns[STR_LAB].index);
    TEST_ASSERT_EQUAL(3, columns[NUM_ID_STR].index);
    TEST_ASSERT_EQUAL(5, columns[NUM_SID].index);
    TEST_ASSERT_EQUAL(4, columns[NUM_ID_GTWY].index);
    TEST_ASSERT_EQUAL(8, columns[NUM_BYTES].index);
    TEST_ASSERT_EQUAL(11, columns[STR_DBC].index);
    TEST_ASSERT_EQUAL(14, columns[NUM_OFS].index);
    TEST_ASSERT_EQUAL(13, columns[NUM_FAC].index);
    TEST_ASSERT_EQUAL(6, columns[NUM_DID].index);
    TEST_ASSERT_EQUAL(12, columns[STR_TYPE].index);
    TEST_ASSERT_EQUAL(15, columns[ST_INTP].index);
}

TEST_CASE("find indexes whith empty columns", TAG)
{
    char line[] = "strEcu,,strLab,numIdTstr,numIdGtwy,numSid,numDid,numDwnSamp,numBytes,strtByte,lenByte,strDbc,strType,numFac,numOfs,stIntp,stUse1,stUse2,stUse3,stUse4,stRvt,numMtyLvl,strCmnt,strName";
    TEST_ASSERT_ESP_ERR(ESP_OK, find_columns_indices(line, columns, COLUMN_COUNT));
    TEST_ASSERT_EQUAL(2, columns[STR_LAB].index);
    TEST_ASSERT_EQUAL(3, columns[NUM_ID_STR].index);
    TEST_ASSERT_EQUAL(5, columns[NUM_SID].index);
    TEST_ASSERT_EQUAL(4, columns[NUM_ID_GTWY].index);
    TEST_ASSERT_EQUAL(8, columns[NUM_BYTES].index);
    TEST_ASSERT_EQUAL(11, columns[STR_DBC].index);
    TEST_ASSERT_EQUAL(14, columns[NUM_OFS].index);
    TEST_ASSERT_EQUAL(13, columns[NUM_FAC].index);
    TEST_ASSERT_EQUAL(6, columns[NUM_DID].index);
    TEST_ASSERT_EQUAL(12, columns[STR_TYPE].index);
    TEST_ASSERT_EQUAL(15, columns[ST_INTP].index);
}

TEST_CASE("find indexes with two different lines", TAG)
{
    char line[] = "strEcu,strLabLong,strLab,numIdTstr,numIdGtwy,numSid,numDid,numDwnSamp,numBytes,strtByte,lenByte,strDbc,strType,numFac,numOfs,stIntp,stUse1,stUse2,stUse3,stUse4,stRvt,numMtyLvl,strCmnt,strName";
    TEST_ASSERT_ESP_ERR(ESP_OK, find_columns_indices(line, columns, COLUMN_COUNT));
    TEST_ASSERT_EQUAL(2, columns[STR_LAB].index);
    TEST_ASSERT_EQUAL(3, columns[NUM_ID_STR].index);
    TEST_ASSERT_EQUAL(5, columns[NUM_SID].index);
    TEST_ASSERT_EQUAL(4, columns[NUM_ID_GTWY].index);
    TEST_ASSERT_EQUAL(8, columns[NUM_BYTES].index);
    TEST_ASSERT_EQUAL(11, columns[STR_DBC].index);
    TEST_ASSERT_EQUAL(14, columns[NUM_OFS].index);
    TEST_ASSERT_EQUAL(13, columns[NUM_FAC].index);
    TEST_ASSERT_EQUAL(6, columns[NUM_DID].index);
    TEST_ASSERT_EQUAL(12, columns[STR_TYPE].index);
    TEST_ASSERT_EQUAL(15, columns[ST_INTP].index);

    char line2[] = "strName,strEcu,strLabLong,strLab,numIdTstr,numIdGtwy,numSid,numDid,numDwnSamp,numBytes,numStrtBit,numLenBit,strDbc,strType,numFac,numOfs,stIntp,stUse1,stUse2,stUse3,stUse4,stRvt,numMtyLvl,strCmnt,strtByte,lenByte";
    TEST_ASSERT_ESP_ERR(ESP_OK, find_columns_indices(line2, columns, COLUMN_COUNT));
    TEST_ASSERT_EQUAL(3, columns[STR_LAB].index);
    TEST_ASSERT_EQUAL(4, columns[NUM_ID_STR].index);
    TEST_ASSERT_EQUAL(6, columns[NUM_SID].index);
    TEST_ASSERT_EQUAL(5, columns[NUM_ID_GTWY].index);
    TEST_ASSERT_EQUAL(9, columns[NUM_BYTES].index);
    TEST_ASSERT_EQUAL(12, columns[STR_DBC].index);
    TEST_ASSERT_EQUAL(15, columns[NUM_OFS].index);
    TEST_ASSERT_EQUAL(14, columns[NUM_FAC].index);
    TEST_ASSERT_EQUAL(7, columns[NUM_DID].index);
    TEST_ASSERT_EQUAL(13, columns[STR_TYPE].index);
    TEST_ASSERT_EQUAL(16, columns[ST_INTP].index);
}

TEST_CASE("find indexes first ok then fail", TAG)
{
    char line[] = "strEcu,strLabLong,strLab,numIdTstr,numIdGtwy,numSid,numDid,numDwnSamp,numBytes,strtByte,lenByte,strDbc,strType,numFac,numOfs,stIntp,stUse1,stUse2,stUse3,stUse4,stRvt,numMtyLvl,strCmnt,strName";
    TEST_ASSERT_ESP_ERR(ESP_OK, find_columns_indices(line, columns, COLUMN_COUNT));
    TEST_ASSERT_EQUAL(2, columns[STR_LAB].index);
    TEST_ASSERT_EQUAL(3, columns[NUM_ID_STR].index);
    TEST_ASSERT_EQUAL(5, columns[NUM_SID].index);
    TEST_ASSERT_EQUAL(4, columns[NUM_ID_GTWY].index);
    TEST_ASSERT_EQUAL(8, columns[NUM_BYTES].index);
    TEST_ASSERT_EQUAL(11, columns[STR_DBC].index);
    TEST_ASSERT_EQUAL(14, columns[NUM_OFS].index);
    TEST_ASSERT_EQUAL(13, columns[NUM_FAC].index);
    TEST_ASSERT_EQUAL(6, columns[NUM_DID].index);
    TEST_ASSERT_EQUAL(12, columns[STR_TYPE].index);
    TEST_ASSERT_EQUAL(15, columns[ST_INTP].index);

    char line2[] = "strName,strEcu,strLabLong,strLab,numIdTstr,numIdGtwy,numSid,      ,numDwnSamp,numBytes,numStrtBit,numLenBit,strDbc,strType,numFac,numOfs,stIntp,stUse1,stUse2,stUse3,stUse4,stRvt,numMtyLvl,strCmnt,strtByte,lenByte";
    TEST_ASSERT_ESP_ERR(ESP_FAIL, find_columns_indices(line2, columns, COLUMN_COUNT));
    TEST_ASSERT_EQUAL(3, columns[STR_LAB].index);
    TEST_ASSERT_EQUAL(4, columns[NUM_ID_STR].index);
    TEST_ASSERT_EQUAL(6, columns[NUM_SID].index);
    TEST_ASSERT_EQUAL(5, columns[NUM_ID_GTWY].index);
    TEST_ASSERT_EQUAL(9, columns[NUM_BYTES].index);
    TEST_ASSERT_EQUAL(12, columns[STR_DBC].index);
    TEST_ASSERT_EQUAL(15, columns[NUM_OFS].index);
    TEST_ASSERT_EQUAL(14, columns[NUM_FAC].index);
    TEST_ASSERT_EQUAL(-1, columns[NUM_DID].index);
    TEST_ASSERT_EQUAL(13, columns[STR_TYPE].index);
    TEST_ASSERT_EQUAL(16, columns[ST_INTP].index);
}

TEST_CASE("find indexes with empty line", TAG)
{
    char line[] = "\0";
    TEST_ASSERT_ESP_ERR(ESP_FAIL, find_columns_indices(line, columns, COLUMN_COUNT));
}

TEST_CASE("find indexes with NULL columns or line", TAG)
{
    char line[] = "djöakegb";
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_ARG, find_columns_indices(NULL, columns, COLUMN_COUNT));
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_ARG, find_columns_indices(line, NULL, COLUMN_COUNT));
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_ARG, find_columns_indices(line, columns, 0));
}

static void init_columns() {
    char line[] = "strEcu,strLabLong,strLab,numIdTstr,numIdGtwy,numSid,numDid,numDwnSamp,numBytes,strtByte,lenByte,strDbc,strType,numFac,numOfs,stIntp,stUse1,stUse2,stUse3,stUse4,stRvt,numMtyLvl,strCmnt,strName";
    TEST_ASSERT_ESP_ERR(ESP_OK, find_columns_indices(line, columns, COLUMN_COUNT));
}

TEST_CASE("passing NULL to parse_row", TAG)
{
    char line[] = "";
    ConfigRow row;
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_ARG, parse_row(NULL, &row, columns, COLUMN_COUNT));
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_ARG, parse_row(line, NULL, columns, COLUMN_COUNT));
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_ARG, parse_row(line, &row, NULL, COLUMN_COUNT));
}

TEST_CASE("parse_row with all columns", TAG)
{
    init_columns();
    char line[] = "1,Fahrzeuggeschwindigkeit,vVeh1,402391158,402522230,34,62477,1,1,0,8,7|8@0,uint,1,0,1,0,0,1,0,0,,,ID3";
    ConfigRow row;
    TEST_ASSERT_ESP_ERR(ESP_OK, parse_row(line, &row, columns, COLUMN_COUNT));

    uint8_t bytes[2];
    did_to_bytes(62477, bytes);

    TEST_ASSERT_EQUAL_STRING("vVeh1", row.name);
    TEST_ASSERT_EQUAL(402391158, row.canId);
    TEST_ASSERT_EQUAL(402522230, row.responseCanId);
    TEST_ASSERT_EQUAL(34, row.sid);
    TEST_ASSERT_EQUAL(bytes[0], row.requestParameters[0]);
    TEST_ASSERT_EQUAL(bytes[1], row.requestParameters[1]);
    TEST_ASSERT_EQUAL(7, row.startBit);
    TEST_ASSERT_EQUAL(8, row.length);
    TEST_ASSERT_EQUAL(0, row.byte_order);
    TEST_ASSERT_EQUAL(1, row.scale);
    TEST_ASSERT_EQUAL(0, row.offset);
    TEST_ASSERT_EQUAL(UINT, row.strType);
    TEST_ASSERT_EQUAL(true, row.should_interpolate);
}

TEST_CASE("parse_row with empty column", TAG)
{
    init_columns();
    char line[] = "1,Fahrzeuggeschwindigkeit,,402391158,402522230,34,62477,1,1,0,8,7|8@0,uint,1,0,1,0,0,1,0,0,,,ID3";
    ConfigRow row;
    TEST_ASSERT_ESP_ERR(ESP_FAIL, parse_row(line, &row, columns, COLUMN_COUNT));
}

TEST_CASE("parse_row with wrong data type column", TAG)
{
    init_columns();
    char line[] = "1,Fahrzeuggeschwindigkeit,vVeh1,CANID,402522230,34,62477,1,1,0,8,7|8@0,uint,1,0,1,0,0,1,0,0,,,ID3";
    ConfigRow row;
    TEST_ASSERT_ESP_ERR(ESP_FAIL, parse_row(line, &row, columns, COLUMN_COUNT));
}

TEST_CASE("parse_row with float32 strType and disabled interpolation", TAG)
{
    init_columns();
    char line[] = "51,Inverter_current_in_d-direction_actual_value,curInvD,402391164,402522236,34,4099,1,4,0,32,7|32@0,float32,1,0,0,1,0,1,1,1,,,ID3";
    ConfigRow row;
    TEST_ASSERT_ESP_ERR(ESP_OK, parse_row(line, &row, columns, COLUMN_COUNT));

    TEST_ASSERT_EQUAL(FLOAT32, row.strType);
    TEST_ASSERT_EQUAL(false, row.should_interpolate);
}

TEST_CASE("parse_row with unknown strType", TAG)
{
    init_columns();
    char line[] = "1,Fahrzeuggeschwindigkeit,vVeh1,402391158,402522230,34,62477,1,1,0,8,7|8@0,unknownType,1,0,1,0,0,1,0,0,,,ID3";
    ConfigRow row;
    TEST_ASSERT_ESP_ERR(ESP_FAIL, parse_row(line, &row, columns, COLUMN_COUNT));
}

TEST_CASE("parse_row with empty stIntp column", TAG)
{
    init_columns();
    char line[] = "1,Fahrzeuggeschwindigkeit,vVeh1,402391158,402522230,34,62477,1,1,0,8,7|8@0,uint,1,0,,0,0,1,0,0,,,ID3";
    ConfigRow row;
    TEST_ASSERT_ESP_ERR(ESP_FAIL, parse_row(line, &row, columns, COLUMN_COUNT));
}

#define TEST_CONFIG_PATH "/sdcard/test/test_config.csv"

static esp_err_t create_test_config() {
    const char config[] = 
        "strEcu,strLabLong,strLab,numIdTstr,numIdGtwy,numSid,numDid,numDwnSamp,numBytes,strtByte,lenByte,strDbc,strType,numFac,numOfs,stIntp,stUse1,stUse2,stUse3,stUse4,stRvt,numMtyLvl,strCmnt,strName\n"
        "198,Powerline Kommunikation: Ladeablauf Zustand,stPwrLine,1860,1966,34,18785,1,1,0,8,7|8@0,uint,1,0,0,0,1,1,0,1,,,ID3\n"
        "1,Fahrzeuggeschwindigkeit,vVeh1,402391158,402522230,34,62477,1,1,0,8,7|8@0,uint,1,0,1,0,0,1,0,0,,,ID3\n"
        "140,Status Klemme 15 über CAN,stTerm15,402391163,402522235,34,690,1,1,0,1,7|1@0,uint,1,0,0,1,1,1,1,1,,,ID3\n"
        "51,Inverter_current_in_d-direction_actual_value,curInvD,402391164,402522236,34,4099,1,4,0,32,7|32@0,float32,1,0,1,1,0,1,1,1,,,ID3\n";

    return storage_write_file(TEST_CONFIG_PATH, config);
}

TEST_CASE("parse full config normal", TAG)
{
    if (create_test_config() != ESP_OK) {
        TEST_IGNORE_MESSAGE("file for testing could not be created");
    }
    UdsConfig config;
    int16_t index_term_15_row;
    TEST_ASSERT_ESP_ERR(ESP_OK, parse_config_file(TEST_CONFIG_PATH, &config, &index_term_15_row));

    uint8_t bytes[2];
    did_to_bytes(18785, bytes);

    TEST_ASSERT_EQUAL_STRING("stPwrLine", config.rows[0].name);
    TEST_ASSERT_EQUAL(1860, config.rows[0].canId);
    TEST_ASSERT_EQUAL(1966, config.rows[0].responseCanId);
    TEST_ASSERT_EQUAL(34, config.rows[0].sid);
    TEST_ASSERT_EQUAL(bytes[0], config.rows[0].requestParameters[0]);
    TEST_ASSERT_EQUAL(bytes[1], config.rows[0].requestParameters[1]);
    TEST_ASSERT_EQUAL(7, config.rows[0].startBit);
    TEST_ASSERT_EQUAL(8, config.rows[0].length);
    TEST_ASSERT_EQUAL(0, config.rows[0].byte_order);
    TEST_ASSERT_EQUAL(1, config.rows[0].scale);
    TEST_ASSERT_EQUAL(0, config.rows[0].offset);
    TEST_ASSERT_EQUAL(UINT, config.rows[0].strType);
    TEST_ASSERT_EQUAL(false, config.rows[0].should_interpolate);

    TEST_ASSERT_EQUAL(FLOAT32, config.rows[3].strType);
    TEST_ASSERT_EQUAL(true, config.rows[3].should_interpolate);

    TEST_ASSERT_EQUAL(2, index_term_15_row);

    TEST_ASSERT_EQUAL(4, config.num_rows);
    TEST_ASSERT_EQUAL_STRING("ID3", config.car_name);
    free((void *)config.car_name);
}

TEST_CASE("parse config without term15 line", TAG)
{
    const char config_data[] = 
        "strEcu,strLabLong,strLab,numIdTstr,numIdGtwy,numSid,numDid,numDwnSamp,numBytes,strtByte,lenByte,strDbc,strType,numFac,numOfs,stIntp,stUse1,stUse2,stUse3,stUse4,stRvt,numMtyLvl,strCmnt,strName\n"
        "198,Powerline Kommunikation: Ladeablauf Zustand,stPwrLine,1860,1966,34,18785,1,1,0,8,7|8@0,uint,1,0,0,0,1,1,0,1,,,ID3\n"
        "51,Inverter_current_in_d-direction_actual_value,curInvD,402391164,402522236,34,4099,1,4,0,32,7|32@0,float32,1,0,1,1,0,1,1,1,,,ID3\n";

    esp_err_t error = storage_write_file(TEST_CONFIG_PATH, config_data);
    if (error != ESP_OK) {
        TEST_IGNORE_MESSAGE("file for testing could not be created");
    }
    UdsConfig config;
    int16_t index_term_15_row;
    TEST_ASSERT_ESP_ERR(ESP_OK, parse_config_file(TEST_CONFIG_PATH, &config, &index_term_15_row));

    uint8_t bytes[2];
    did_to_bytes(18785, bytes);

    TEST_ASSERT_EQUAL_STRING("stPwrLine", config.rows[0].name);
    TEST_ASSERT_EQUAL(1860, config.rows[0].canId);
    TEST_ASSERT_EQUAL(1966, config.rows[0].responseCanId);
    TEST_ASSERT_EQUAL(34, config.rows[0].sid);
    TEST_ASSERT_EQUAL(bytes[0], config.rows[0].requestParameters[0]);
    TEST_ASSERT_EQUAL(bytes[1], config.rows[0].requestParameters[1]);
    TEST_ASSERT_EQUAL(7, config.rows[0].startBit);
    TEST_ASSERT_EQUAL(8, config.rows[0].length);
    TEST_ASSERT_EQUAL(0, config.rows[0].byte_order);
    TEST_ASSERT_EQUAL(1, config.rows[0].scale);
    TEST_ASSERT_EQUAL(0, config.rows[0].offset);
    TEST_ASSERT_EQUAL(UINT, config.rows[0].strType);
    TEST_ASSERT_EQUAL(false, config.rows[0].should_interpolate);

    TEST_ASSERT_EQUAL(FLOAT32, config.rows[1].strType);
    TEST_ASSERT_EQUAL(true, config.rows[1].should_interpolate);

    TEST_ASSERT_EQUAL(-1, index_term_15_row);
}

TEST_CASE("parse config with empty column in header", TAG)
{
    const char config_data[] = 
        "strEcu,,strLab,numIdTstr,numIdGtwy,numSid,numDid,numDwnSamp,numBytes,strtByte,lenByte,strDbc,strType,numFac,numOfs,stIntp,stUse1,stUse2,stUse3,stUse4,stRvt,numMtyLvl,strCmnt,strName\n"
        "198,Powerline Kommunikation: Ladeablauf Zustand,stPwrLine,1860,1966,34,18785,1,1,0,8,7|8@0,uint,1,0,0,0,1,1,0,1,,,ID3\n"
        "1,Fahrzeuggeschwindigkeit,vVeh1,402391158,402522230,34,62477,1,1,0,8,7|8@0,uint,1,0,1,0,0,1,0,0,,,ID3\n"
        "140,Status Klemme 15 über CAN,stTerm15,402391163,402522235,34,690,1,1,0,1,7|1@0,uint,1,0,0,1,1,1,1,1,,,ID3\n"
        "51,Inverter_current_in_d-direction_actual_value,curInvD,402391164,402522236,34,4099,1,4,0,32,7|32@0,float32,1,0,1,1,0,1,1,1,,,ID3\n";

    esp_err_t error = storage_write_file(TEST_CONFIG_PATH, config_data);
    if (error != ESP_OK) {
        TEST_IGNORE_MESSAGE("file for testing could not be created");
    }
    
    UdsConfig config;
    int16_t index_term_15_row;
    TEST_ASSERT_ESP_ERR(ESP_OK, parse_config_file(TEST_CONFIG_PATH, &config, &index_term_15_row));

    uint8_t bytes[2];
    did_to_bytes(18785, bytes);

    TEST_ASSERT_EQUAL_STRING("stPwrLine", config.rows[0].name);
    TEST_ASSERT_EQUAL(1860, config.rows[0].canId);
    TEST_ASSERT_EQUAL(1966, config.rows[0].responseCanId);
    TEST_ASSERT_EQUAL(34, config.rows[0].sid);
    TEST_ASSERT_EQUAL(bytes[0], config.rows[0].requestParameters[0]);
    TEST_ASSERT_EQUAL(bytes[1], config.rows[0].requestParameters[1]);
    TEST_ASSERT_EQUAL(7, config.rows[0].startBit);
    TEST_ASSERT_EQUAL(8, config.rows[0].length);
    TEST_ASSERT_EQUAL(0, config.rows[0].byte_order);
    TEST_ASSERT_EQUAL(1, config.rows[0].scale);
    TEST_ASSERT_EQUAL(0, config.rows[0].offset);
    TEST_ASSERT_EQUAL(UINT, config.rows[0].strType);
    TEST_ASSERT_EQUAL(false, config.rows[0].should_interpolate);

    TEST_ASSERT_EQUAL(FLOAT32, config.rows[3].strType);
    TEST_ASSERT_EQUAL(true, config.rows[3].should_interpolate);

    TEST_ASSERT_EQUAL(2, index_term_15_row);
}

TEST_CASE("parse config with wrong data", TAG)
{
    const char config_data[] = 
        "strEcu,strLabLong,strLab,numIdTstr,numIdGtwy,numSid,numDid,numDwnSamp,numBytes,strtByte,lenByte,strDbc,strType,numFac,numOfs,stIntp,stUse1,stUse2,stUse3,stUse4,stRvt,numMtyLvl,strCmnt,strName\n"
        "198,Powerline Kommunikation: Ladeablauf Zustand,stPwrLine,1860,1966,34,18785,1,1,0,8,7|8@0,uint,1,0,0,0,1,1,0,1,,,ID3\n"
        "1,Fahrzeuggeschwindigkeit,vVeh1,402391158,response can id,34,62477,1,1,0,8,7|8@0,uint,1,0,1,0,0,1,0,0,,,ID3\n"
        "140,Status Klemme 15 über CAN,stTerm15,402391163,402522235,34,690,1,1,0,1,7|1@0,uint,1,0,0,1,1,1,1,1,,,ID3\n"
        "51,Inverter_current_in_d-direction_actual_value,curInvD,402391164,402522236,34,4099,1,4,0,32,7|32@0,float32,1,0,1,1,0,1,1,1,,,ID3\n";

    esp_err_t error = storage_write_file(TEST_CONFIG_PATH, config_data);
    if (error != ESP_OK) {
        TEST_IGNORE_MESSAGE("file for testing could not be created");
    }
    
    UdsConfig config;
    int16_t index_term_15_row;
    TEST_ASSERT_ESP_ERR(ESP_FAIL, parse_config_file(TEST_CONFIG_PATH, &config, &index_term_15_row));
}

TEST_CASE("parse config with missing column", TAG)
{
    const char config_data[] = 
        "strEcu,strLabLong,strLab,numIdTstr,numIdGtwy,numSid,missing_column,numDwnSamp,numBytes,strtByte,lenByte,strDbc,strType,numFac,numOfs,stIntp,stUse1,stUse2,stUse3,stUse4,stRvt,numMtyLvl,strCmnt,strName\n"
        "198,Powerline Kommunikation: Ladeablauf Zustand,stPwrLine,1860,1966,34,18785,1,1,0,8,7|8@0,uint,1,0,0,0,1,1,0,1,,,ID3\n"
        "1,Fahrzeuggeschwindigkeit,vVeh1,402391158,402522230,34,62477,1,1,0,8,7|8@0,uint,1,0,1,0,0,1,0,0,,,ID3\n"
        "140,Status Klemme 15 über CAN,stTerm15,402391163,402522235,34,690,1,1,0,1,7|1@0,uint,1,0,0,1,1,1,1,1,,,ID3\n"
        "51,Inverter_current_in_d-direction_actual_value,curInvD,402391164,402522236,34,4099,1,4,0,32,7|32@0,float32,1,0,1,1,0,1,1,1,,,ID3\n";

    esp_err_t error = storage_write_file(TEST_CONFIG_PATH, config_data);
    if (error != ESP_OK) {
        TEST_IGNORE_MESSAGE("file for testing could not be created");
    }

    UdsConfig config;
    int16_t index_term_15_row;
    TEST_ASSERT_ESP_ERR(ESP_FAIL, parse_config_file(TEST_CONFIG_PATH, &config, &index_term_15_row));
}

TEST_CASE("parse metadata for existing config", TAG)
{
    if (create_test_config() != ESP_OK) {
        TEST_IGNORE_MESSAGE("file for testing could not be created");
    }

    size_t num_rows = 0;
    char *car_name = NULL;
    TEST_ASSERT_ESP_ERR(ESP_OK, parse_metadata_for_config(TEST_CONFIG_PATH, &num_rows, &car_name));

    TEST_ASSERT_EQUAL(4, num_rows);
    TEST_ASSERT_EQUAL_STRING("ID3", car_name);
    free(car_name);
}

TEST_CASE("parse metadata for missing config", TAG)
{
    size_t num_rows = 0;
    char *car_name = NULL;
    TEST_ASSERT_ESP_ERR(ESP_ERR_NOT_FOUND, parse_metadata_for_config("/sdcard/test/does_not_exist.csv", &num_rows, &car_name));
}

TEST_CASE("parse metadata with NULL arguments", TAG)
{
    size_t num_rows = 0;
    char *car_name = NULL;
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_ARG, parse_metadata_for_config(NULL, &num_rows, &car_name));
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_ARG, parse_metadata_for_config(TEST_CONFIG_PATH, NULL, &car_name));
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_ARG, parse_metadata_for_config(TEST_CONFIG_PATH, &num_rows, NULL));
}

TEST_CASE("parse metadata for config with invalid header", TAG)
{
    const char config_data[] =
        "strEcu,strLabLong,strLab,numIdTstr,numIdGtwy,numSid,numDid,numDwnSamp,numBytes,strtByte,lenByte,strDbc,strType,numFac,numOfs,stIntp,stUse1,stUse2,stUse3,stUse4,stRvt,numMtyLvl,strCmnt\n"
        "198,Powerline Kommunikation: Ladeablauf Zustand,stPwrLine,1860,1966,34,18785,1,1,0,8,7|8@0,uint,1,0,0,0,1,1,0,1,\n";

    esp_err_t error = storage_write_file(TEST_CONFIG_PATH, config_data);
    if (error != ESP_OK) {
        TEST_IGNORE_MESSAGE("file for testing could not be created");
    }

    size_t num_rows = 0;
    char *car_name = NULL;
    TEST_ASSERT_ESP_ERR(ESP_FAIL, parse_metadata_for_config(TEST_CONFIG_PATH, &num_rows, &car_name));
}
