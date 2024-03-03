#pragma once

#include "stdio.h"
#include "string.h"
#include "stdarg.h"
#include "fatfs_sd.h"
#include "../../src/FATFS/App/fatfs.h"


int sd_buffer_size(char *buffer);
void sd_buffer_clear(void);
uint8_t sd_card_initialize();
uint8_t sd_open_file(const char *file_name, uint8_t instruction);
uint8_t sd_write_data_to_file(const char *data);
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