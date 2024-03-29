#include "./spi_bit_bang.h"

// SPI1 pins
// PA4  SPI1 SS
// PA5  SPI1 SCK
// PA6  SPI1 MISO
// PA7  SPI1 MOSI

uint8_t driver_initialized = 0;

// INTERRUPT falling and rising
GPIO_TypeDef* m_SS_port;
uint16_t m_SS_pin;

// INTERRUPT falling and rising
GPIO_TypeDef* m_CLK_port;
uint16_t m_CLK_pin;

// INPUT 
GPIO_TypeDef* m_MOSI_port;
uint16_t m_MOSI_pin;

// OUTPUT
GPIO_TypeDef* m_MISO_port;
uint16_t m_MISO_pin;

#define SPI_BIT_BANG_RECEIVE_BUFFER_SIZE 10000
uint8_t receive_buffer[SPI_BIT_BANG_RECEIVE_BUFFER_SIZE];
volatile uint16_t receive_buffer_index = 0;
volatile uint8_t receive_bit_index_counter = 0;
volatile uint16_t receive_bit_skip = 0;
volatile uint16_t receive_bytes_queue = 0;

#define SPI_BIT_BANG_TRANSMIT_BUFFER_SIZE 1000
uint8_t transmit_buffer[SPI_BIT_BANG_TRANSMIT_BUFFER_SIZE];
volatile uint16_t transmit_buffer_index = 0;
volatile uint8_t transmit_bit_index_counter = 0;
volatile uint16_t transmit_bit_skip = 0;
volatile uint16_t transmit_bytes_queue = 0;

volatile uint8_t slave_selected = 0;

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if(!driver_initialized){
        return;
    }

    if(GPIO_Pin == m_SS_pin)
    {
        if(HAL_GPIO_ReadPin(m_SS_port, m_SS_pin)){
            slave_selected = 0;
            HAL_GPIO_WritePin(m_MISO_port, m_MISO_pin, GPIO_PIN_RESET);
        }else{
            slave_selected = 1;
        }
    }else if (GPIO_Pin == m_CLK_pin){
        if(slave_selected){
            uint8_t clock_state =  HAL_GPIO_ReadPin(m_CLK_port, m_CLK_pin);

            // clock high - rising
            // clock low  - falling

            // MODE 3

            if(clock_state == 1 && receive_bytes_queue != 0 && receive_bit_skip == 0){
                // Read on rising edge
                uint8_t MOSI_state = HAL_GPIO_ReadPin(m_MOSI_port, m_MOSI_pin);

                // Reset when reading the 0 bit as the first action.
                if(!receive_bit_index_counter){
                    receive_buffer[receive_buffer_index] = 0;
                }
                receive_buffer[receive_buffer_index] = receive_buffer[receive_buffer_index] | (MOSI_state << (7 - receive_bit_index_counter));
                
                receive_bit_index_counter++;
                if(receive_bit_index_counter == 8){
                    // Do not ever overwrite the last byte of the buffer and dont move in the buffer if end is reached
                    if(SPI_BIT_BANG_RECEIVE_BUFFER_SIZE-1 > receive_buffer_index+1){
                        receive_buffer_index++;
                    }
                    // receive_buffer_index++;
                    receive_bytes_queue--;
                    receive_bit_index_counter = 0;
                }
                
            }else if(clock_state == 0 && transmit_bytes_queue != 0){
                // Write on falling edge
                uint8_t MISO_state = (transmit_buffer[transmit_buffer_index] >> (7 - transmit_bit_index_counter)) & 0x01;
                HAL_GPIO_WritePin(m_MISO_port, m_MISO_pin, MISO_state);

                transmit_bit_index_counter++;
                if(transmit_bit_index_counter == 8){
                    if(SPI_BIT_BANG_TRANSMIT_BUFFER_SIZE-1 > transmit_buffer_index+1){
                        transmit_buffer_index++;
                    }
                    // transmit_buffer_index++;
                    transmit_bytes_queue--;
                    transmit_bit_index_counter = 0;
                }
            }else if(clock_state == 0 && transmit_bytes_queue == 0){
                HAL_GPIO_WritePin(m_MISO_port, m_MISO_pin, GPIO_PIN_RESET);
            }else if(clock_state == 1 && receive_bit_skip != 0){
                receive_bit_skip--;
            }
        }
    }
}


// interrupt to sense when it is selected

// Separate interrupt to sense when clock is doing things


/**
 * @brief 
 * 
 * SS   - interrupt with RISE and FALL
 * CLK  - with RISE or FALL
 * MOSI - INPUT
 * MISO - OUTPUT
 * 
 * @param SS_port
 * @param SS_pin 
 * @param CLK_port 
 * @param CLK_pin 
 * @param MOSI_port 
 * @param MOSI_pin 
 * @param MISO_port 
 * @param MISO_pin 
 * @return uint8_t 
 */
uint8_t spi_bit_bang_initialize(GPIO_TypeDef* SS_port, uint16_t SS_pin, GPIO_TypeDef* CLK_port, uint16_t CLK_pin, GPIO_TypeDef* MOSI_port, uint16_t MOSI_pin, GPIO_TypeDef* MISO_port, uint16_t MISO_pin){
    m_SS_port = SS_port;
    m_SS_pin = SS_pin;
    m_CLK_port = CLK_port;
    m_CLK_pin = CLK_pin;
    m_MOSI_port = MOSI_port;
    m_MOSI_pin = MOSI_pin;
    m_MISO_port = MISO_port;
    m_MISO_pin = MISO_pin;

    
    driver_initialized = 1;
    return 1;
}

void clear_spi_queues(){
    receive_buffer_index = 0;
    receive_bit_index_counter = 0;
    receive_bit_skip = 0;
    receive_bytes_queue = 0;

    transmit_buffer_index = 0;
    transmit_bit_index_counter = 0;
    transmit_bit_skip = 0;
    transmit_bytes_queue = 0;
}

uint8_t wait_for_transmit_queue_empty(uint16_t timeout_ms){
    uint32_t start_time = HAL_GetTick();
    uint32_t delta_time = 0;


    // consider checking if the slave select goes to disabled
    while(transmit_bytes_queue != 0 && (delta_time = (HAL_GetTick() - start_time)) < timeout_ms );

    if(delta_time >= timeout_ms){
        clear_spi_queues();
        return 0;
    }

    return 1;
}

uint8_t wait_for_receive_queue_empty(uint16_t timeout_ms){
    uint32_t start_time = HAL_GetTick();
    uint32_t delta_time = 0;

    while(receive_bytes_queue != 0 && (delta_time = (HAL_GetTick() - start_time)) < timeout_ms);

    if(delta_time >= timeout_ms){
        clear_spi_queues();
        return 0;
    }

    return 1;
}

uint8_t wait_for_skips(uint16_t timeout_ms){
    uint32_t start_time = HAL_GetTick();
    uint32_t delta_time = 0;

    while((receive_bit_skip != 0 || transmit_bit_skip != 0) && (delta_time = (HAL_GetTick() - start_time)) < timeout_ms );

    if(delta_time >= timeout_ms){
        clear_spi_queues();
        return 0;
    }

    return 1;
}

uint8_t spi_bit_bang_receive(uint8_t * receive_data, uint16_t receive_data_size, uint16_t timeout_ms){
    if(receive_data_size >= SPI_BIT_BANG_RECEIVE_BUFFER_SIZE){
        return 0;
    }

    if(!wait_for_receive_queue_empty(timeout_ms)) return 0;
    if(!wait_for_transmit_queue_empty(timeout_ms)) return 0;
    receive_bytes_queue += receive_data_size;
    if(!wait_for_receive_queue_empty(timeout_ms)) return 0;


    // Could cause problems with interrupt happening durring this proccess
    for(uint16_t i = 0; i < receive_data_size; i++){
        receive_data[i] = receive_buffer[i];
    }

    receive_buffer_index -= receive_data_size;

    return 1;
}

uint8_t spi_bit_bang_transmit(uint8_t * transmit_data, uint16_t transmit_data_size, uint16_t timeout_ms){
    if(transmit_data_size >= SPI_BIT_BANG_TRANSMIT_BUFFER_SIZE){
        return 0;
    }

    if(!wait_for_receive_queue_empty(timeout_ms)) return 0;
    if(!wait_for_transmit_queue_empty(timeout_ms)) return 0;
    for(uint16_t i = 0; i < transmit_data_size; i++){
        transmit_buffer[i] = transmit_data[i];
    }

    transmit_bytes_queue += transmit_data_size;
    if(!wait_for_transmit_queue_empty(timeout_ms)) return 0;
    // add a skip for receive as it is too quick
    receive_bit_skip = 1;
    if(!wait_for_skips(timeout_ms)) return 0; // Skips are important for timing
    transmit_buffer_index -= transmit_data_size;

    return 1;
}
