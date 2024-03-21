/* Auto generated shit -----------------------------------------------*/
#include "main.h"
#include "./FATFS/App/fatfs.h"

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi3;

TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart1;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI3_Init(void);
/* Actual functional code -----------------------------------------------*/



#include "../lib/printf/retarget.h"
#include "stdio.h"
#include "string.h"
#include <stdint.h>
#include <stdlib.h>

#include "../lib/sd_card/sd_card.h"

void init_loop_timer();
void handle_loop_timing();
void init_STM32_peripherals();

// SPI1 pins
// PA4  SPI1 SS
// PA5  SPI1 SCK
// PA6  SPI1 MISO
// PA7  SPI1 MOSI

// SPI3 pins
// PB3  SPI3 SCK
// PB4  SPI3 MISO
// PB5  SPI3 MOSI
// 

#define REFRESH_RATE_HZ 1

// handling loop timing ###################################################################################
uint32_t loop_start_time = 0;
uint32_t loop_end_time = 0;
int16_t delta_loop_time = 0;

// SD card ################################################################################################
// For deciding which log file to log to
uint16_t log_file_index = 1;
char log_file_base_name[] = "Quadcopter.txt";
#define LOG_FILE_NAME_MAX 45
char log_file_name[LOG_FILE_NAME_MAX];
uint8_t log_file_location_found = 0;
uint8_t sd_card_initialized = 0;
uint8_t log_loop_count = 0;


// SPI slave stuff functionality

#define SLAVE_BUFFER_SIZE 100
uint8_t slave_buffer[SLAVE_BUFFER_SIZE];

uint8_t dummy_buffer[SLAVE_BUFFER_SIZE];



// void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
// {
//     if (hspi->Instance == SPI1)
//     {

//         // sd_card_append_to_buffer("Got stuff from master ");
//         // sd_card_append_to_buffer("\n");
//         // Handle the received data
//         uint8_t test = slave_buffer[0];
//         if(test == 1){
//             HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
//         }else{
//             HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
//         }
//         printf("Test, %d\n", test);
//         HAL_SPI_Receive_IT(&hspi1, slave_buffer, sizeof(slave_buffer));

//     }
//     printf("Test\n");
// }

// void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
// {
//     uint8_t spi_error_code = hspi->ErrorCode;

//     if (hspi->Instance == SPI1) // Check if the callback is for SPI1
//     {
//         uint8_t spi_data_register = hspi->Instance->DR;
//         uint8_t spi_status_register = hspi->Instance->SR;
//         (void)spi_data_register;
//         (void)spi_status_register;

//         // Code to handle SPI1 error
//         printf("Error test: %d\n", spi_error_code);
//         HAL_SPI_Receive_IT(&hspi1, slave_buffer, sizeof(slave_buffer));
//     }
//     printf("Test\n");
// }

// enum t_power_management {
//     LOGGER_SD_BUFFER_SIZE    = 0b10000000,
//     LOGGER_SD_BUFFER_CLEAR    = 0b10000000,
//     LOGGER_SD_CARD_INITIALIZE    = 0b10000000,
//     LOGGER_SD_OPEN_FILE    = 0b10000000,
//     LOGGER_SD_WRITE_DATA_TO_FILE    = 0b10000000,
//     LOGGER_SD_READ_DATA_FORM_FILE    = 0b10000000,
//     LOGGER_SD_SET_FILE_CURSOR_OFFSET    = 0b10000000,
//     LOGGER_SD_CLOSE_FILE    = 0b10000000,
//     LOGGER_SD_CARD_DEINITIALIZE    = 0b10000000,
//     LOGGER_SD_CARD_APPEND_TO_BUFFER    = 0b10000000,
//     LOGGER_SD_GET_BUFFER_POINTER    = 0b10000000,
//     LOGGER_SD_GET_SELECTED_FILE_SIZE    = 0b10000000,
//     LOGGER_SD_WRITE_BUFFER_TO_FILE    = 0b10000000,
//     LOGGER_SD_FILE_EXISTS    = 0b10000000,
//     LOGGER_SD_SAVE_FILE    = 0b10000000,

//     LOGGER_INITIALIZE    = 0b10000000,
//     LOGGER_RESET    = 0b10000000,
//     LOGGER_WRITE_CHUNK_OF_DATA = 0b10000000,
//     LOGGER_CHECK_READY = 0b10000000,
// };'


/**
 * Extracts and returns a dynamically allocated string from SPI received data starting from a specific index.
 * The function assumes the first byte of interest (at the specified index) is the start of the string.
 * 
 * @param data The received SPI data buffer.
 * @param size The size of the received SPI data buffer.
 * @param startIndex The index to start looking for the string.
 * @return A pointer to a dynamically allocated string. The caller is responsible
 *         for freeing this memory using free(). Returns NULL if the function fails.
 */
char* extract_string_from_spi_data_at_index(uint8_t* data, size_t size, size_t startIndex) {
    if (startIndex >= size || startIndex + 1 > size) { // Ensure startIndex is within bounds and leaves room for null terminator
        return NULL;
    }

    // Calculate the length of the string starting from startIndex
    size_t stringLength = strlen((char*)&data[startIndex]);
    if (stringLength + startIndex >= size) { // Ensure string is null-terminated within bounds
        return NULL;
    }

    // Allocate memory for the string
    char* string = (char*)malloc(stringLength + 1); // +1 for null terminator
    if (string == NULL) {
        // Memory allocation failed
        return NULL;
    }

    // Copy the string from the SPI data starting at startIndex
    strncpy(string, (char*)&data[startIndex], stringLength + 1);

    return string;
}



int main(void){
    init_STM32_peripherals();

    // Try to initialize the file and test some 


    // HAL_SPI_Receive_IT(&hspi1, slave_buffer, sizeof(slave_buffer));

    // sd_card_initialized = sd_card_initialize();
    // if(sd_card_initialized){
    //     printf("Looking for viable log file.\n");
    //     do{
    //         uint16_t log_file_name_length = sprintf(log_file_name, "%d%s", log_file_index, log_file_base_name);

    //         // Quit of the string is too big
    //         if(log_file_name_length > LOG_FILE_NAME_MAX){
    //             printf("Log file string too long\n");
    //             return 0;
    //         }
    //         printf("Looking for viable log file. Testing file name: %s\n", log_file_name);
    //         log_file_location_found = !sd_file_exists(log_file_name);
    //         log_file_index++;
    //     }
    //     while(!log_file_location_found);
    //     printf("Found viable log file.\n");
    //     sd_open_file(log_file_name, FA_WRITE | FA_READ | FA_OPEN_ALWAYS);
    //     sd_close_file();

    //     sd_card_initialized = sd_open_file(log_file_name, FA_WRITE);
    //     sd_set_file_cursor_offset(sd_card_get_selected_file_size());
    // }



    printf("Looping\n");
    init_loop_timer();
    while (1){

        HAL_StatusTypeDef spi_slave_result_receive = HAL_SPI_Receive(&hspi1, slave_buffer, 1, 1000);
        // HAL_StatusTypeDef spi_slave_result_receive = HAL_SPI_TransmitReceive(&hspi1, dummy_buffer, slave_buffer, 1, 1000);

        
        if(spi_slave_result_receive == HAL_OK){// || spi_slave_result_receive == HAL_TIMEOUT){
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
            switch (slave_buffer[0])
            {
                case LOGGER_SD_BUFFER_SIZE:
                {
                    uint16_t result = sd_buffer_size();
                    uint8_t result_array[] = {
                        (result >> 8) & 0xFF, 
                        result & 0xFF
                    };
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
                    HAL_SPI_Transmit(&hspi1, result_array, 2, 100);
                    break;
                }
                case LOGGER_SD_BUFFER_CLEAR:
                {
                    sd_buffer_clear();
                    uint8_t result = 1; // Just to confirm that it is completed
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
                    HAL_SPI_Transmit(&hspi1, &result, 1, 100);
                    break;
                }
                case LOGGER_SD_CARD_INITIALIZE:
                {
                    uint8_t result = sd_card_initialize();
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
                    HAL_SPI_Transmit(&hspi1, &result, 1, 100);
                    break;
                }
                case LOGGER_SD_OPEN_FILE:
                {
                    volatile HAL_StatusTypeDef status_receive;

                    // We ask how much data should we receive. 
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
                    if((status_receive = HAL_SPI_Receive(&hspi1, slave_buffer, 2, 1000)) != HAL_OK) break;
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);

                    volatile uint16_t amount_of_data_to_receive = (slave_buffer[0] << 8) | slave_buffer[1];

                    // Receive the instruction and file name
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
                    if( (status_receive = HAL_SPI_Receive(&hspi1, slave_buffer, amount_of_data_to_receive, 1000)) != HAL_OK) break;
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);


                    // instruction is byte 0
                    char* extracted_string = extract_string_from_spi_data_at_index(slave_buffer, SLAVE_BUFFER_SIZE, 1);

                    volatile uint8_t result = sd_open_file(extracted_string, slave_buffer[0]);
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
                    HAL_SPI_Transmit(&hspi1, &result, 1, 100);
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
                    HAL_SPI_Transmit(&hspi1, slave_buffer, amount_of_data_to_receive, 100);
                    free(extracted_string);
                    break;
                }
                case LOGGER_SD_WRITE_DATA_TO_FILE:
                {   
                    // We ask how much data should we receive. 
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
                    if(HAL_SPI_Receive(&hspi1, slave_buffer, 2, 1000) != HAL_OK) break;
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);

                    uint16_t amount_of_data_to_receive = (slave_buffer[0] << 8) | slave_buffer[1];

                    // Receive the data
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
                    if(HAL_SPI_Receive(&hspi1, slave_buffer, amount_of_data_to_receive, 1000) != HAL_OK) break;
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);

                    char* extracted_string = extract_string_from_spi_data_at_index(slave_buffer, SLAVE_BUFFER_SIZE, 0);

                    uint8_t result = sd_write_data_to_file(extracted_string);
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
                    HAL_SPI_Transmit(&hspi1, &result, 1, 100);
                    free(extracted_string);
                    break;
                }
                case LOGGER_SD_READ_DATA_FORM_FILE:
                {
                    uint8_t result = sd_read_data_from_file();
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
                    HAL_SPI_Transmit(&hspi1, &result, 1, 100);
                    break;
                }
                case LOGGER_SD_SET_FILE_CURSOR_OFFSET:
                {
                    // Receive 4 bytes because need 32 bit number
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
                    if(HAL_SPI_Receive(&hspi1, slave_buffer, 4, 1000) != HAL_OK) break;
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);

                    uint32_t number = (slave_buffer[0] << 24) | 
                        (slave_buffer[1] << 16) | 
                        (slave_buffer[2] << 8) | 
                        slave_buffer[3];

                    uint8_t result = sd_set_file_cursor_offset(number);

                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
                    HAL_SPI_Transmit(&hspi1, &result, 1, 100);
                    break;
                }
                case LOGGER_SD_CLOSE_FILE:
                {
                    uint8_t result = sd_close_file();
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
                    HAL_SPI_Transmit(&hspi1, &result, 1, 100);
                    break;
                }
                case LOGGER_SD_CARD_DEINITIALIZE:
                {
                    uint8_t result = sd_card_deinitialize();
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
                    HAL_SPI_Transmit(&hspi1, &result, 1, 100);
                    break;
                }
                case LOGGER_SD_CARD_APPEND_TO_BUFFER:
                {
                    // We ask how much data should we receive. 
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
                    if(HAL_SPI_Receive(&hspi1, slave_buffer, 2, 1000) != HAL_OK) break;
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
                    uint16_t amount_of_data_to_receive = (slave_buffer[0] << 8) | slave_buffer[1];

                    // Receive the data
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
                    if(HAL_SPI_Receive(&hspi1, slave_buffer, amount_of_data_to_receive, 1000) != HAL_OK) break;
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
                    char* extracted_string = extract_string_from_spi_data_at_index(slave_buffer, SLAVE_BUFFER_SIZE, 0);

                    sd_card_append_to_buffer(extracted_string);
                    free(extracted_string);
                    uint8_t result = 1; // Just to confirm that it is completed
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
                    HAL_SPI_Transmit(&hspi1, &result, 1, 100);
                    break;
                }
                case LOGGER_SD_GET_BUFFER_POINTER:
                {   
                    // Get the data
                    char* sd_buffer = sd_card_get_buffer_pointer();
                    char* extracted_string = extract_string_from_spi_data_at_index((uint8_t*)sd_buffer, SD_BUFFER_SIZE, 0);

                    
                    uint16_t amount_of_data_to_transmit = sd_buffer_size()+1;
                    uint8_t amount_of_data_to_transmit_bytes[] = {(amount_of_data_to_transmit >> 8) & 0xFF, amount_of_data_to_transmit & 0xFF}; 

                    // Tell how much data will be sent
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
                    if(HAL_SPI_Transmit(&hspi1, amount_of_data_to_transmit_bytes, 2, 100) != HAL_OK) {
                        free(extracted_string);
                        break;
                    }
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);

                    // Send the data
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
                    HAL_SPI_Transmit(&hspi1, (uint8_t*)extracted_string, amount_of_data_to_transmit, 100);
                    free(extracted_string);
                    break;
                }
                case LOGGER_SD_GET_SELECTED_FILE_SIZE:
                {
                    uint32_t result = sd_card_get_selected_file_size();
                    uint8_t result_array[] = {
                        (result >> 24) & 0xFF, 
                        (result >> 16) & 0xFF, 
                        (result >> 8) & 0xFF, 
                        result & 0xFF
                    };

                    // It knows to receive 4 bytes
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
                    HAL_SPI_Transmit(&hspi1, result_array, 4, 100);
                    break;
                }
                case LOGGER_SD_WRITE_BUFFER_TO_FILE:
                {
                    uint8_t result = sd_write_buffer_to_file();
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
                    HAL_SPI_Transmit(&hspi1, &result, 1, 100);
                    break;
                }
                case LOGGER_SD_FILE_EXISTS:
                {
                    // We ask how much data should we receive. 
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
                    if(HAL_SPI_Receive(&hspi1, slave_buffer, 2, 1000) != HAL_OK) break;
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
                    uint16_t amount_of_data_to_receive = (slave_buffer[0] << 8) | slave_buffer[1];

                    // Receive the file name
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
                    if(HAL_SPI_Receive(&hspi1, slave_buffer, amount_of_data_to_receive, 1000) != HAL_OK) break;
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
                    char* extracted_string = extract_string_from_spi_data_at_index(slave_buffer, SLAVE_BUFFER_SIZE, 0);

                    uint8_t result = sd_file_exists(extracted_string);
                    free(extracted_string);
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
                    HAL_SPI_Transmit(&hspi1, &result, 1, 100);
                    break;
                }
                case LOGGER_SD_SAVE_FILE:
                {
                    uint8_t result = sd_save_file();
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
                    HAL_SPI_Transmit(&hspi1, &result, 1, 100);
                    break;
                }



                case LOGGER_INITIALIZE:
                {
                    break;
                }
                case LOGGER_RESET:
                {
                    break;
                }
                case LOGGER_WRITE_CHUNK_OF_DATA:
                {
                    break;
                }
                case LOGGER_CHECK_READY:
                {
                    break;
                }
                default:
                    // if(slave_buffer[0] == 1){
                    //     uint8_t result = 1;
                    //     HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
                    //     HAL_SPI_Transmit(&hspi1, &result, 1, 100);
                    // }else if(slave_buffer[0] == 0){
                    //     uint8_t result = 1;
                    //     HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
                    //     HAL_SPI_Transmit(&hspi1, &result, 1, 100);
                    // }
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
                    break;
            }
        }else{
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
        }
        
        
        // if(sd_card_initialized){
        //     sd_card_append_to_buffer("ACCELdasda asdasdas, ");
        //     sd_card_append_to_buffer("ACCELdasda asdasdas, ");
        //     sd_card_append_to_buffer("ACCELdasda asdasdas, ");
        //     sd_card_append_to_buffer("ACCELdasda asdasdas, ");
        //     sd_card_append_to_buffer("ACCELdasda asdasdas, ");
        //     sd_card_append_to_buffer("\n");
            
        //     // printf(" Logged data to SD buffer. ");
        //     log_loop_count++;
        // }


        // if(sd_card_initialized && log_loop_count > 0){
        //     // sd_card_initialize();

        //     sd_close_file();
        //     sd_card_initialized = sd_open_file(log_file_name, FA_WRITE);
        //     sd_set_file_cursor_offset(sd_card_get_selected_file_size());
        //     sd_card_initialized = sd_write_buffer_to_file();
        //     sd_save_file();
        //     // sd_close_file();
        //     sd_buffer_clear();
        //     log_loop_count = 0;

        //     // sd_card_deinitialize();
            
        //     // printf(" Logged data from buffer to SD. ");
        // }

        // handle_loop_timing();
        // HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
    }
}

void init_STM32_peripherals(){
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    HAL_Delay(1);
    MX_SPI1_Init();
    MX_SPI3_Init();
    HAL_Delay(1);
    MX_USART1_UART_Init();
    RetargetInit(&huart1);
    MX_FATFS_Init();

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);
    
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
}

void init_loop_timer(){
    loop_start_time = HAL_GetTick();
}

void handle_loop_timing(){
    loop_end_time = HAL_GetTick();
    delta_loop_time = loop_end_time - loop_start_time;

    printf("Tb: %dms ", delta_loop_time);
    
    int time_to_wait = (1000 / REFRESH_RATE_HZ) - delta_loop_time;
    if (time_to_wait > 0)
    {
        HAL_Delay(time_to_wait);
    }

    loop_end_time = HAL_GetTick();
    delta_loop_time = loop_end_time - loop_start_time;
    printf("Ta: %dms\n", delta_loop_time);

    loop_start_time = HAL_GetTick();
}

// Print out how much time has passed since the start of the loop. To debug issues with performance
void track_time(){
    uint32_t delta_loop_time_temp = loop_end_time - loop_start_time;

    printf("%5ld ms ", delta_loop_time_temp);
}

/* Auto generated shit again-----------------------------------------------*/


/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 13;
  RCC_OscInitStruct.PLL.PLLN = 75;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/

// High - 1 edge  Not good, it gets the correct command but not the ones afterwards
// high - 2 edge  Not good at all.
// low  - 1 edge current Not very good
// low  - 2 edge Worked very good, but some data was missing. Literally bytes missing

// High - 1 edge 
// high - 2 edge 
// low  - 1 edge current
// low  - 2 edge Same as normal one

  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_SLAVE;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_HARD_INPUT;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief SPI3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI3_Init(void)
{

  /* USER CODE BEGIN SPI3_Init 0 */

  /* USER CODE END SPI3_Init 0 */

  /* USER CODE BEGIN SPI3_Init 1 */

  /* USER CODE END SPI3_Init 1 */
  /* SPI3 parameter configuration*/
  hspi3.Instance = SPI3;
  hspi3.Init.Mode = SPI_MODE_MASTER;
  hspi3.Init.Direction = SPI_DIRECTION_2LINES;
  hspi3.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi3.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi3.Init.NSS = SPI_NSS_SOFT;
  hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi3.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI3_Init 2 */

  /* USER CODE END SPI3_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
  /* DMA2_Stream2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream2_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA3 */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB1 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PA8 */
  GPIO_InitStruct.Pin = GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  GPIO_InitStruct.Alternate = GPIO_AF1_TIM1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PA15 */
  GPIO_InitStruct.Pin = GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB6 PB7 */
  GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
