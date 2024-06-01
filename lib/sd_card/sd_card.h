#pragma once

#include "stdio.h"
#include "string.h"
#include "stdarg.h"
#include "fatfs_sd.h"
#include "../../src/FATFS/App/fatfs.h"

#define SD_BUFFER_SIZE 1024

enum t_logger_commands {
    LOGGER_SD_BUFFER_CLEAR = 1,
    LOGGER_SD_CARD_INITIALIZE = 2,
    LOGGER_SD_OPEN_FILE = 3,
    LOGGER_SD_WRITE_DATA_TO_FILE = 4,
    LOGGER_SD_READ_DATA_FORM_FILE = 5,
    LOGGER_SD_SET_FILE_CURSOR_OFFSET = 6,
    LOGGER_SD_CLOSE_FILE = 7,
    LOGGER_SD_CARD_DEINITIALIZE = 8,
    LOGGER_SD_CARD_APPEND_TO_BUFFER = 10,
    LOGGER_SD_GET_BUFFER_POINTER = 11,
    LOGGER_SD_GET_SELECTED_FILE_SIZE = 12,
    LOGGER_SD_WRITE_BUFFER_TO_FILE = 13,
    LOGGER_SD_FILE_EXISTS = 14,
    LOGGER_SD_SAVE_FILE = 15,
    LOGGER_SD_BUFFER_SIZE = 16,

    LOGGER_INITIALIZE = 17,
    LOGGER_RESET = 18,
    LOGGER_WRITE_CHUNK_OF_STRING_DATA = 20,
    LOGGER_WRITE_CHUNK_OF_BYTE_DATA = 21,
    LOGGER_TEST_INTERFACE = 19,
    LOGGER_TEST_INTERFACE_BROKEN = 9,

    LOGGER_WRITE_CHUNK_OF_STRING_DATA_ASYNC = 22,
    LOGGER_WRITE_CHUNK_OF_BYTE_DATA_ASYNC = 23,

    LOGGER_ENTER_ASYNC_STRING_MODE = 24,
    LOGGER_ENTER_ASYNC_BYTE_MODE = 25,
    LOGGER_LEAVE_ASYNC_MODE = 26
};

#define LOGGER_INTERFACE_TEST_VALUE 0b01101010

#define MIN_SD_COMMAND (0x02 + 16)
#define MAX_SD_COMMAND (0x16 + 16)

uint16_t sd_buffer_size(void);
void sd_buffer_clear(void);
uint8_t sd_card_initialize();
uint8_t sd_open_file(const char *file_name, uint8_t instruction);
uint8_t sd_write_string_data_to_file(const char *data);
uint8_t sd_write_byte_data_to_file(const char *data, uint16_t length);
uint8_t sd_read_data_from_file();
uint8_t sd_set_file_cursor_offset(uint32_t cursor);
uint8_t sd_close_file();
uint8_t sd_card_deinitialize();
void sd_card_append_to_buffer(const char *string_format, ...);
char* sd_card_get_buffer_pointer();
uint32_t sd_card_get_selected_file_size();
uint8_t sd_write_buffer_to_file();
uint8_t sd_file_exists(const char *file_name);
uint8_t sd_save_file();