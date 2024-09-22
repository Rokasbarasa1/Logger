#include "./sd_card.h"

FATFS sd_file_system; // File system of the mounted sd card
FIL sd_file; // Handle of the file that is opened 
FILINFO sd_file_info; // info about the file
FRESULT sd_result; // to store result

char sd_buffer[SD_BUFFER_SIZE];
uint16_t sd_buffer_index = 0;

UINT sd_bytes_read, sd_bytes_written; // FIle read/write count

// Capacity of sd card 
FATFS *sd_pointer_fatfs_structure;
DWORD sd_free_clusters; // Amount of smallest logical disk space units.
uint32_t sd_total_space, sd_free_space;


#define SD_CARD_DEBUG 0

uint16_t sd_buffer_size(void){
    return strlen(sd_buffer);
}

void sd_buffer_clear(void){
    for(int i=0; i< SD_BUFFER_SIZE; i++){
        sd_buffer[i] = 0;
    }
}

uint8_t sd_card_initialize(){
    sd_result = f_mount(&sd_file_system, "", 1);

    if(sd_result != FR_OK){
#if(SD_CARD_DEBUG)
        printf("SD card not mounted\n");
#endif
        return sd_result;
    }

#if(SD_CARD_DEBUG)
    printf("SD card mounted\n");
#endif

    sd_result = f_getfree("", &sd_free_clusters, &sd_pointer_fatfs_structure);
    sd_total_space = (uint32_t)((sd_pointer_fatfs_structure->n_fatent - 2) * sd_pointer_fatfs_structure->csize * 0.5);
    sd_free_space = (uint32_t)(sd_free_clusters * sd_pointer_fatfs_structure->csize * 0.5);


    if(sd_result != FR_OK) {
#if(SD_CARD_DEBUG)
        printf("Sd card: flailed to get size\n");
#endif
        return sd_result;
    }
#if(SD_CARD_DEBUG)
    printf("Total space: %lu bytes.  Free space: %lu bytes\n", sd_total_space, sd_free_space);
#endif
    return 0;
}

uint8_t sd_open_file(const char *file_name, uint8_t instruction){

    sd_result = f_open(
        &sd_file, 
        file_name,
        instruction
    );

    if(sd_result != FR_OK){
#if(SD_CARD_DEBUG)
        printf("SD card: failed to opened file\n");
#endif
        return 0;
    } 
#if(SD_CARD_DEBUG)
    printf("SD card: opened file\n");
#endif
    sd_result = f_stat(file_name, &sd_file_info);
    
    if(sd_result != FR_OK) return 0;
#if(SD_CARD_DEBUG)
    printf("SD card: got file info\n");
#endif
    return 1;
}

uint8_t sd_write_string_data_to_file(const char *data){
    // Why does this give random value??? I wrote 156 bytes and got a result form this of 160, what does that mean???
    sd_result = f_puts(data, &sd_file);
    
    if(sd_result != FR_OK) {
#if(SD_CARD_DEBUG)
        printf("SD card: failed to write data\n");
#endif
        return 0;
    }
#if(SD_CARD_DEBUG)
    printf("SD card: wrote data\n");
#endif
    return 1;
};

uint8_t sd_write_byte_data_to_file(const char *data, uint16_t length){

    // f_write doesn't like when more than 1000 bytes are given to it at once. Divide into multiple writes
    uint8_t* current_data_position = (uint8_t *)data;
    uint16_t remaining_length = length;
    uint16_t bytes_written = 0;
    uint16_t bytes_to_write = 0;


    while(remaining_length > 0) {

        bytes_to_write = (remaining_length > 1000) ? 1000: remaining_length;
        UINT bytes_written_temp = 0;
        sd_result = f_write(&sd_file, current_data_position, bytes_to_write, &bytes_written_temp);

        if(sd_result != FR_OK) {
#if(SD_CARD_DEBUG)
            printf("SD card: failed to write data %d, %d / %d\n", sd_result, bytes_written, length);
#endif
            return 0;
        }

        if (bytes_written_temp != bytes_to_write) {
#if(SD_CARD_DEBUG)
            printf("SD card: not all data was written %d / %d\n", bytes_written_temp, bytes_to_write);
#endif
            return 0;
        }

        bytes_written += bytes_written_temp;
        current_data_position += bytes_written_temp; // Move  the pointer forward by how much was written
        remaining_length -= bytes_written_temp;
    }

#if(SD_CARD_DEBUG)
    printf("SD card: wrote data\n");
#endif
    return 1;
};

uint8_t sd_read_data_from_file(){
    f_gets(sd_buffer, sizeof(sd_buffer), &sd_file);
#if(SD_CARD_DEBUG)
    printf("SD card: read data\n");
#endif
    return 1;
}

uint8_t sd_set_file_cursor_offset(uint32_t cursor){
    sd_result = f_lseek(&sd_file, cursor);
    
    if(sd_result != FR_OK) {
#if(SD_CARD_DEBUG)
        printf("SD card: failed to set cursor\n");
#endif
        return 0;
    }
#if(SD_CARD_DEBUG)
    printf("SD card: set cursor\n");
#endif
    return 1;
}

uint8_t sd_close_file(){
    sd_result = f_close(&sd_file);
    
    if(sd_result != FR_OK) {
#if(SD_CARD_DEBUG)
        printf("SD card: failed to close file\n");
#endif
        return 0;
    }
#if(SD_CARD_DEBUG)
    printf("SD card: closed file\n");
#endif
    return 1;
}

uint8_t sd_card_deinitialize(){
    sd_result = f_mount(NULL, "", 0);
    
    if(sd_result != FR_OK) {
#if(SD_CARD_DEBUG)
        printf("SD card: failed to deinitialize\n");
#endif
        return 0;
    }
#if(SD_CARD_DEBUG)
    printf("SD card: deinitialize\n");
#endif


    return 1;
}

void sd_card_append_to_buffer(const char *string_format, ...){
    va_list args;
    va_start(args, string_format);
    // Ensure we don't write beyond the buffer
    int written = vsnprintf(&sd_buffer[sd_buffer_index], SD_BUFFER_SIZE - sd_buffer_index, string_format, args);
    if (written > 0) {
        sd_buffer_index += written < (SD_BUFFER_SIZE - sd_buffer_index) ? written : (SD_BUFFER_SIZE - sd_buffer_index - 1);
    }
    va_end(args);
}

char* sd_card_get_buffer_pointer(){
    return sd_buffer;
}

uint32_t sd_card_get_selected_file_size(){
    return sd_file_info.fsize;
}

uint8_t sd_write_buffer_to_file(){
    if(sd_buffer_index == 0) {
        // If the buffer index is 0, there's nothing to write.
#if(SD_CARD_DEBUG)
        printf("SD card: Buffer is empty, nothing to write\n");
#endif

        return 0; // You might want to return 0 or a specific error code depending on your design
    }

    // Write only the portion of the buffer that has been used.
    UINT bytesToWrite = sd_buffer_index;
    UINT bytesWritten;
    sd_result = f_write(&sd_file, sd_buffer, bytesToWrite, &bytesWritten);

    if(sd_result != FR_OK || bytesWritten != bytesToWrite) {
#if(SD_CARD_DEBUG)
        printf("SD card: failed to write buffer data\n");
#endif

        return 0;
    }

    // Optionally, you can clear the buffer and reset the index after writing, if that's your intended behavior.
    sd_buffer_clear();
    sd_buffer_index = 0;
#if(SD_CARD_DEBUG)
    printf("SD card: wrote buffer data\n");
#endif

    return 1;
}

uint8_t sd_file_exists(const char *file_name){
    FRESULT sd_result = f_stat(file_name, &sd_file_info);
    
    if(sd_result != FR_OK){
#if(SD_CARD_DEBUG)
        printf("SD card: File does not exist\n");
#endif

        return 0; // File does not exist or an error occurred
    }
#if(SD_CARD_DEBUG)
    printf("SD card: File exists\n");
#endif

    return 1; // File exists
}

uint8_t sd_save_file(){
    sd_result = f_sync(&sd_file);

    if(sd_result != FR_OK){
#if(SD_CARD_DEBUG)
        printf("SD card: File was not saved\n");
#endif
        return 0; // File does not exist or an error occurred
    }

    return 1;
}

FRESULT sd_get_result(){
    return sd_result;
}

// Example code ************************************************
    // sd_card_initialize();
    // sd_open_file("Quadcopter.txt", FA_WRITE | FA_READ | FA_OPEN_ALWAYS);
    // sd_set_file_cursor_offset(sd_card_get_selected_file_size());
    // sd_write_data_to_file("Quadcopter restarted\n");
    // sd_write_data_to_file("Quadcopter data2\n");
    // sd_write_data_to_file("Quadcopter data3\n");
    // sd_write_data_to_file("Quadcopter data4\n");
    // sd_close_file();
    // sd_open_file("Quadcopter.txt", FA_READ);
    // sd_read_data_from_file();
    // printf("Read Data : %s\n", sd_card_get_buffer_pointer());
    // sd_buffer_clear();
    // sd_close_file();
    // sd_open_file("Quadcopter.txt", FA_WRITE);
    // sd_set_file_cursor_offset(sd_card_get_selected_file_size()); // set cursor to end of file to so data is not overwritten
    // sd_write_data_to_file("Quadcopter restarted\n");
    // sd_write_data_to_file("Quadcopter data2\n");
    // sd_write_data_to_file("Quadcopter data3\n");
    // sd_write_data_to_file("Quadcopter data4\n");
    // sd_close_file();
    // sd_open_file("Quadcopter.txt", FA_READ);
    // sd_read_data_from_file();
    // printf("Read Data : %s\n", sd_card_get_buffer_pointer());
    // sd_buffer_clear();
    // sd_close_file();
    // sd_card_deinitialize();