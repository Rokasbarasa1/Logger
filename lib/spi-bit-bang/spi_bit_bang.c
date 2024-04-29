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

#define SPI_BIT_BANG_RECEIVE_BUFFER_SIZE 20000
volatile uint8_t receive_buffer_selected = 0; // 0- 1 buffer, 1- 2 buffer
uint8_t receive_buffer0[SPI_BIT_BANG_RECEIVE_BUFFER_SIZE];
uint8_t receive_buffer1[SPI_BIT_BANG_RECEIVE_BUFFER_SIZE];

volatile uint16_t receive_buffer0_index = 0;
volatile uint16_t receive_buffer1_index = 0;
volatile uint16_t skipped_bytes_buffer0 = 0;
volatile uint16_t skipped_bytes_buffer1 = 0;
volatile uint8_t receive_bit_index_counter = 0;
volatile uint16_t receive_bit_skip = 0;

// SHared between both buffers 
volatile uint16_t receive_bytes_queue = 0;

#define SPI_BIT_BANG_TRANSMIT_BUFFER_SIZE 1000
uint8_t transmit_buffer[SPI_BIT_BANG_TRANSMIT_BUFFER_SIZE];
volatile uint16_t transmit_buffer_index = 0;
volatile uint8_t transmit_bit_index_counter = 0;
volatile uint16_t transmit_bit_skip = 0;
volatile uint16_t transmit_bytes_queue = 0;

volatile uint8_t slave_selected = 0;

void spi_bit_bang_ss_interrupt(){
    if(!driver_initialized) return;
    
    // Direct control with minimal branching
    uint8_t pin_state = HAL_GPIO_ReadPin(m_SS_port, m_SS_pin);
    slave_selected = pin_state^1;
    HAL_GPIO_WritePin(m_MISO_port, m_MISO_pin, 0);
}

void spi_bit_bang_clk_interrupt(){
    if(!driver_initialized || !slave_selected) return;

    uint8_t clock_state = HAL_GPIO_ReadPin(m_CLK_port, m_CLK_pin);

    if (clock_state) { // 1 Rising edge READ
        if (receive_bytes_queue == 0 || receive_bit_skip > 0) { // Check if any bytes need to be read
            if (receive_bit_skip > 0) --receive_bit_skip; // Reduce the amount of skipped bytes as one was just skipped
            return;
        }
        uint8_t MOSI_state = (uint8_t)((m_MOSI_port->IDR >> m_MOSI_pin) & 0x1); // Read the pin but faster
        // uint8_t MOSI_state = HAL_GPIO_ReadPin(m_MOSI_port, m_MOSI_pin);

        // Get the correct selected buffer and its index
        uint8_t *current_buffer = (receive_buffer_selected == 0) ? receive_buffer0 : receive_buffer1;
        uint16_t *current_index = (receive_buffer_selected == 0) ? &receive_buffer0_index : &receive_buffer1_index;

        if (receive_bit_index_counter == 0) current_buffer[*current_index] = 0; // IMPORTANT, reset the byte that will be written to. It makes sense not to but in testing this was essential for it to work.

        current_buffer[*current_index] |= MOSI_state << (7 - receive_bit_index_counter); // Place the mosi bit into the buffer at index
        if (++receive_bit_index_counter == 8) { // If last bit reached move the index and reset bit index
            if (SPI_BIT_BANG_RECEIVE_BUFFER_SIZE-1 > *current_index+1) { // Prevent from the last byte being overwritten
                // Do not ever overwrite the last byte of the buffer and dont move in the buffer if end is reached
                (*current_index)++;
            }
            receive_bytes_queue--;
            receive_bit_index_counter = 0;
        }

        // Maybe placebo but it helps the read read data somehow
        HAL_GPIO_WritePin(m_MISO_port, m_MISO_pin, 1);
        HAL_GPIO_WritePin(m_MISO_port, m_MISO_pin, 0);
        // Good for debugging reading also. Doesn't affect communication

    } else { // 0 Falling edge WRITE
        if (transmit_bytes_queue == 0) { // Check if something needs to be transmitted
            HAL_GPIO_WritePin(m_MISO_port, m_MISO_pin, GPIO_PIN_RESET); // Always default to low signal on MISO for cleanness on logic analyzer
            return;
        }

        // Get the bit that needs to be written to the MISO pin next
        uint8_t MISO_state = (transmit_buffer[transmit_buffer_index] >> (7 - transmit_bit_index_counter)) & 0x01;
        HAL_GPIO_WritePin(m_MISO_port, m_MISO_pin, MISO_state);
        if (++transmit_bit_index_counter == 8) { // If last bit reached move the index and reset bit index
            if(SPI_BIT_BANG_TRANSMIT_BUFFER_SIZE-1 > transmit_buffer_index+1){
                // Do not go more than the last byte in the transmit buffer
                transmit_buffer_index++;
            }
            transmit_bytes_queue--;
            transmit_bit_index_counter = 0;
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
    if(receive_buffer_selected == 0){
        receive_buffer0_index = 0;
    }else{
        receive_buffer1_index = 0;
    }
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
        if(receive_buffer_selected == 0){
            receive_data[i] = receive_buffer0[i];
        }else{
            receive_data[i] = receive_buffer1[i];
        }
    }

    if(receive_buffer_selected == 0){
        receive_buffer0_index -= receive_data_size;
    }else{
        receive_buffer1_index -= receive_data_size;
    }
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
    // Not relevant anymore as with more speed this is not problem? Or maybe because of mode 3.
    receive_bit_skip = 0; 
    if(!wait_for_skips(timeout_ms)) return 0; // Skips are important for timing
    transmit_buffer_index -= transmit_data_size;

    return 1;
}

uint8_t spi_bit_bang_receive_async(){
    // You better be in control already cause this is not waiting.
    receive_bytes_queue = MAX_UINT16;
    return 1;
}


uint8_t spi_bit_bang_read_receive_async_response_form_non_active_buffer(uint8_t * receive_data){
    while (slave_selected); // Wait for slave not to be selected

    // SKIP BYTES THAT ARE 0 BEFORE THE END THAT WAY YOU DONT HAVE TO LOOK FOR THEM AFTERWARDS
    // AND DONT HAVE TO MODIFY THE ARRAY

    if(receive_buffer_selected == 0){
        for(uint16_t i = 0; i < receive_buffer1_index; i++){
            // if(receive_buffer1[i] == '\0' && i != receive_buffer1_index - 1 ){
            //     receive_data[i] = 'X';
            // }else{
            //     receive_data[i] = receive_buffer1[i];
            // }

            if(receive_buffer1[i] == '\0' && i != receive_buffer1_index - 1 ){
                skipped_bytes_buffer1++;
            }else{
                receive_data[i-skipped_bytes_buffer1] = receive_buffer1[i];
            }
        }
    }else{
        for(uint16_t i = 0; i < receive_buffer0_index; i++){
            if(receive_buffer0[i] == '\0' && i != receive_buffer0_index - 1 ){
                skipped_bytes_buffer0++;
            }else{
                receive_data[i - skipped_bytes_buffer0] = receive_buffer0[i];
            }
        }
    }

    return 1;
}

uint8_t spi_bit_bang_soft_cancel_receive_async(){
    while (slave_selected); // Wait for slave not to be selected
    receive_bit_index_counter = 0;
    receive_bit_skip = 0;
    receive_bytes_queue = 0;
    return 1;
}

uint8_t spi_bit_bang_hard_cancel_receive_async(){
    // Stop ongoing operation immediately
    slave_selected = 0;
    // Reset operation parameter
    receive_bytes_queue = 0;
    receive_bit_index_counter = 0;
    receive_bit_skip = 0;

    // Reset buffers
    receive_buffer0_index = 0;
    receive_buffer1_index = 0;
    receive_buffer_selected = 0;

    return 1;
}

uint8_t spi_bit_bang_swap_receive_async_buffer(){
    while (slave_selected); // Wait for slave not to be selected
    if(receive_buffer_selected == 0){
        receive_buffer_selected = 1;
    }else if(receive_buffer_selected == 1){
        receive_buffer_selected = 0;
    }
    return 1;
}

uint8_t spi_bit_bang_reset_non_active_receive_buffer(){
    if(receive_buffer_selected == 1){ // If 0 is selected then reset the 1
        receive_buffer0_index = 0;
        receive_buffer0[0] = 0;
        skipped_bytes_buffer0 = 0;
    }else{
        receive_buffer1_index = 0;
        receive_buffer1[0] = 0;
        skipped_bytes_buffer1 = 0;
    }

    return 1;
}

uint8_t spi_bit_bang_wipe_non_active_receive_buffer(){
    if(receive_buffer_selected == 1){ // If 0 is selected then reset the 1
        for (uint16_t i = 0; i < SPI_BIT_BANG_RECEIVE_BUFFER_SIZE; i++){
            receive_buffer0[i] = 0;
        }
    }else{
        for (uint16_t i = 0; i < SPI_BIT_BANG_RECEIVE_BUFFER_SIZE; i++){
            receive_buffer1[i] = 0;
        }
    }

    return 1;
}

uint16_t spi_bit_bang_get_non_active_buffer_size(){
    if(receive_buffer_selected == 1){
        return receive_buffer0_index - skipped_bytes_buffer0;
        // return receive_buffer0_index;
    }else{
        return receive_buffer1_index - skipped_bytes_buffer1;
        // return receive_buffer1_index;
    }
}