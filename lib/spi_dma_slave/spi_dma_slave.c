#include "./spi_dma_slave.h"

SPI_HandleTypeDef hspi;
DMA_HandleTypeDef hdma_spi_rx;
DMA_HandleTypeDef hdma_spi_tx;

#define SPI_BIT_BANG_RECEIVE_BUFFER_SIZE 30000
volatile uint8_t sds_receive_buffer_selected = 0; // 0- 1 buffer, 1- 2 buffer
uint8_t receive_buffer0[SPI_BIT_BANG_RECEIVE_BUFFER_SIZE];
uint8_t receive_buffer1[SPI_BIT_BANG_RECEIVE_BUFFER_SIZE];

volatile uint16_t receiving_amount_of_bytes = 0;

volatile uint8_t dma_rx_in_progress = 0;
volatile uint16_t sds_receive_buffer0_index = 0;
volatile uint16_t receive_buffer0_midway_bytes_added = 0;
volatile uint16_t sds_receive_buffer1_index = 0;
volatile uint16_t receive_buffer1_midway_bytes_added = 0;
volatile uint16_t sds_skipped_bytes_buffer0 = 0; // How many bytes of string terminators are skipped
volatile uint16_t sds_skipped_bytes_buffer1 = 0; // How many bytes of string terminators are skipped

// SHared between both buffers 
volatile uint16_t sds_receive_bytes_queue = 0;
uint16_t sds_receive_bytes_queue_midway = 0; // If received bytes are read while DMA RX is in progress then the value is logged here

#define SPI_BIT_BANG_TRANSMIT_BUFFER_SIZE 1000
uint8_t transmit_buffer[SPI_BIT_BANG_TRANSMIT_BUFFER_SIZE];
volatile uint16_t transmitting_amount_of_bytes = 0;

volatile uint8_t dma_tx_in_progress = 0;
volatile uint16_t sds_transmit_buffer_index = 0;
volatile uint16_t sds_transmit_bytes_queue = 0;

volatile uint8_t sds_slave_selected = 0;


// SLave select
GPIO_TypeDef* spi_dma_slave_SS_port;
uint32_t spi_dma_slave_SS_pin;
uint8_t spi_dma_slave_SS_pin_index;

// Clock pin
GPIO_TypeDef* spi_dma_slave_CLK_port;
uint32_t spi_dma_slave_CLK_pin;

// MOSI Input
GPIO_TypeDef* spi_dma_slave_MOSI_port;
uint32_t spi_dma_slave_MOSI_pin;

// MISO Output
GPIO_TypeDef* spi_dma_slave_MISO_port;
uint32_t spi_dma_slave_MISO_pin;


// Turn on the GPIO port clock based on the port
void sda_enable_gpio_port(GPIO_TypeDef* port){
    if(port == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
    else if(port == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
    else if(port == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();
    else if(port == GPIOD) __HAL_RCC_GPIOD_CLK_ENABLE();
}

// Initialize a gpio pin as a SPI functional pin
void sda_init_pin_as_spi(GPIO_TypeDef* port, uint16_t pin, SPI_TypeDef *spi_peripheral){
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    if (spi_peripheral == SPI1)
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    else if (spi_peripheral == SPI2)
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
    else if (spi_peripheral == SPI3)
        GPIO_InitStruct.Alternate = GPIO_AF6_SPI3;
    HAL_GPIO_Init(port, &GPIO_InitStruct);
}

// Needed to determine pin position for setting up interrupts without HAL
#define PIN_POSITION(pin) (__builtin_ctz(pin))

// Initialize the SPI, DMA RX AND TX, by passing SPI, DMA and gpio pins 
uint8_t spi_dma_slave_init(
    SPI_TypeDef *spi_peripheral, 
    DMA_Stream_TypeDef *rx_dma_instance_stream, 
    uint32_t rx_dma_channel, 
    DMA_Stream_TypeDef *tx_dma_instance_stream, 
    uint32_t tx_dma_channel,
    GPIO_TypeDef* SS_port, 
    uint16_t SS_pin,
    GPIO_TypeDef* CLK_port, 
    uint16_t CLK_pin, 
    GPIO_TypeDef* MOSI_port, 
    uint16_t MOSI_pin, 
    GPIO_TypeDef* MISO_port, 
    uint16_t MISO_pin
){

    spi_dma_slave_SS_port = SS_port;
    spi_dma_slave_SS_pin = SS_pin;
    spi_dma_slave_CLK_port = CLK_port;
    spi_dma_slave_CLK_pin = CLK_pin;
    spi_dma_slave_MOSI_port = MOSI_port;
    spi_dma_slave_MOSI_pin = MOSI_pin;
    spi_dma_slave_MISO_port = MISO_port;
    spi_dma_slave_MISO_pin = MISO_pin;

    sda_enable_gpio_port(SS_port);
    sda_enable_gpio_port(CLK_port);
    sda_enable_gpio_port(MOSI_port);
    sda_enable_gpio_port(MISO_port);
    
    sda_init_pin_as_spi(SS_port, SS_pin, spi_peripheral);
    sda_init_pin_as_spi(CLK_port, CLK_pin, spi_peripheral);
    sda_init_pin_as_spi(MOSI_port, MOSI_pin, spi_peripheral);
    sda_init_pin_as_spi(MISO_port, MISO_pin, spi_peripheral);

    spi_dma_slave_SS_pin_index = PIN_POSITION(SS_pin); // Convert bitmask to pin number
    // Configure the SS to have interrupts also
    SYSCFG->EXTICR[spi_dma_slave_SS_pin_index / 4] = (uint32_t)(SS_port == GPIOA ? 0 : SS_port == GPIOB ? 1 : 2) << (4 * (spi_dma_slave_SS_pin_index % 4));
    EXTI->IMR |= (1 << spi_dma_slave_SS_pin_index);      // Unmask interrupt
    EXTI->RTSR |= (1 << spi_dma_slave_SS_pin_index);     // Rising edge trigger
    EXTI->FTSR |= (1 << spi_dma_slave_SS_pin_index);     // Falling edge trigger

    HAL_NVIC_SetPriority(EXTI0_IRQn + spi_dma_slave_SS_pin_index, 1, 0); // Set priority (use the correct IRQn based on the pin)
    HAL_NVIC_EnableIRQ(EXTI0_IRQn + spi_dma_slave_SS_pin_index);         // Enable EXTI interrupt

    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();

    if (spi_peripheral == SPI1)
        __HAL_RCC_SPI1_CLK_ENABLE();
    else if (spi_peripheral == SPI2)
        __HAL_RCC_SPI2_CLK_ENABLE();
    else if (spi_peripheral == SPI3)
        __HAL_RCC_SPI3_CLK_ENABLE();

    // SPIIIIII
    hspi.Instance = spi_peripheral;
    hspi.Init.Mode = SPI_MODE_SLAVE;
    hspi.Init.Direction = SPI_DIRECTION_2LINES;
    hspi.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi.Init.CLKPolarity = SPI_POLARITY_HIGH;
    hspi.Init.CLKPhase = SPI_PHASE_2EDGE;
    hspi.Init.NSS = SPI_NSS_HARD_INPUT;
    hspi.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi.Init.CRCPolynomial = 10;
    if (HAL_SPI_Init(&hspi) != HAL_OK){
        printf("Failed to init SPI\n");
        return 0;
    }


    // RECEIVE
    hdma_spi_rx.Instance = rx_dma_instance_stream;
    hdma_spi_rx.Init.Channel = rx_dma_channel;
    hdma_spi_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_spi_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_spi_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_spi_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_spi_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_spi_rx.Init.Mode = DMA_NORMAL;
    hdma_spi_rx.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_spi_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_spi_rx) != HAL_OK){
        printf("Failed to init DMA RX\n");
        return 0;
    }
    __HAL_LINKDMA(&hspi, hdmarx, hdma_spi_rx);

    // TRANSMIT
    hdma_spi_tx.Instance = tx_dma_instance_stream;
    hdma_spi_tx.Init.Channel = tx_dma_channel;
    hdma_spi_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_spi_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_spi_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_spi_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_spi_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_spi_tx.Init.Mode = DMA_NORMAL;
    hdma_spi_tx.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_spi_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_spi_tx) != HAL_OK){
        printf("Failed to init DMA TX\n");
        return 0;
    }
    /* Configure DMA for SPI1 TX */
    __HAL_LINKDMA(&hspi, hdmatx, hdma_spi_tx);

    IRQn_Type irq_type_rx;
    if(rx_dma_instance_stream == DMA1_Stream0) irq_type_rx = DMA1_Stream0_IRQn;
    else if(rx_dma_instance_stream == DMA1_Stream1) irq_type_rx = DMA1_Stream1_IRQn;
    else if(rx_dma_instance_stream == DMA1_Stream2) irq_type_rx = DMA1_Stream2_IRQn;
    else if(rx_dma_instance_stream == DMA1_Stream3) irq_type_rx = DMA1_Stream3_IRQn;
    else if(rx_dma_instance_stream == DMA1_Stream4) irq_type_rx = DMA1_Stream4_IRQn;
    else if(rx_dma_instance_stream == DMA1_Stream5) irq_type_rx = DMA1_Stream5_IRQn;
    else if(rx_dma_instance_stream == DMA1_Stream6) irq_type_rx = DMA1_Stream6_IRQn;
    else if(rx_dma_instance_stream == DMA1_Stream7) irq_type_rx = DMA1_Stream7_IRQn;
    else if(rx_dma_instance_stream == DMA2_Stream0) irq_type_rx = DMA2_Stream0_IRQn;
    else if(rx_dma_instance_stream == DMA2_Stream1) irq_type_rx = DMA2_Stream1_IRQn;
    else if(rx_dma_instance_stream == DMA2_Stream2) irq_type_rx = DMA2_Stream2_IRQn;
    else if(rx_dma_instance_stream == DMA2_Stream3) irq_type_rx = DMA2_Stream3_IRQn;
    else if(rx_dma_instance_stream == DMA2_Stream4) irq_type_rx = DMA2_Stream4_IRQn;
    else if(rx_dma_instance_stream == DMA2_Stream5) irq_type_rx = DMA2_Stream5_IRQn;
    else if(rx_dma_instance_stream == DMA2_Stream6) irq_type_rx = DMA2_Stream6_IRQn;
    else if(rx_dma_instance_stream == DMA2_Stream7) irq_type_rx = DMA2_Stream7_IRQn;
    else return 0; // Should not get this far

    /* Configure NVIC */
    HAL_NVIC_SetPriority(irq_type_rx, 0, 0);
    HAL_NVIC_EnableIRQ(irq_type_rx);

    IRQn_Type irq_type_tx;
    if(tx_dma_instance_stream == DMA1_Stream0) irq_type_tx = DMA1_Stream0_IRQn;
    else if(tx_dma_instance_stream == DMA1_Stream1) irq_type_tx = DMA1_Stream1_IRQn;
    else if(tx_dma_instance_stream == DMA1_Stream2) irq_type_tx = DMA1_Stream2_IRQn;
    else if(tx_dma_instance_stream == DMA1_Stream3) irq_type_tx = DMA1_Stream3_IRQn;
    else if(tx_dma_instance_stream == DMA1_Stream4) irq_type_tx = DMA1_Stream4_IRQn;
    else if(tx_dma_instance_stream == DMA1_Stream5) irq_type_tx = DMA1_Stream5_IRQn;
    else if(tx_dma_instance_stream == DMA1_Stream6) irq_type_tx = DMA1_Stream6_IRQn;
    else if(tx_dma_instance_stream == DMA1_Stream7) irq_type_tx = DMA1_Stream7_IRQn;
    else if(tx_dma_instance_stream == DMA2_Stream0) irq_type_tx = DMA2_Stream0_IRQn;
    else if(tx_dma_instance_stream == DMA2_Stream1) irq_type_tx = DMA2_Stream1_IRQn;
    else if(tx_dma_instance_stream == DMA2_Stream2) irq_type_tx = DMA2_Stream2_IRQn;
    else if(tx_dma_instance_stream == DMA2_Stream3) irq_type_tx = DMA2_Stream3_IRQn;
    else if(tx_dma_instance_stream == DMA2_Stream4) irq_type_tx = DMA2_Stream4_IRQn;
    else if(tx_dma_instance_stream == DMA2_Stream5) irq_type_tx = DMA2_Stream5_IRQn;
    else if(tx_dma_instance_stream == DMA2_Stream6) irq_type_tx = DMA2_Stream6_IRQn;
    else if(tx_dma_instance_stream == DMA2_Stream7) irq_type_tx = DMA2_Stream7_IRQn;
    else return 0; // Should not get this far

    /* Configure NVIC */
    HAL_NVIC_SetPriority(irq_type_tx, 0, 0);
    HAL_NVIC_EnableIRQ(irq_type_tx);

    return 1;
}

// Check if the slave is selected
uint8_t spi_dma_slave_is_sds_slave_selected(){
    return sds_slave_selected;
}

// DMA RX handle for the DMA callback
DMA_HandleTypeDef* spi_dma_slave_get_dma_rx_handle(){
    return &hdma_spi_rx;
}

// DMA TX handle for the DMA callback
DMA_HandleTypeDef* spi_dma_slave_get_dma_tx_handle(){
    return &hdma_spi_tx;
}

// Start a DMA RX
void spi_dma_slave_rx(uint8_t * buffer, uint16_t amount_of_data_to_receive){
    volatile HAL_StatusTypeDef status;
    receiving_amount_of_bytes = amount_of_data_to_receive;
    if ((status = HAL_SPI_Receive_DMA(&hspi, buffer, amount_of_data_to_receive)) != HAL_OK){
        volatile uint32_t spi_error = hspi.ErrorCode;
        volatile uint32_t dma_error_rx = hspi.hdmarx->ErrorCode;
        volatile uint32_t dma_error_tx = hspi.hdmatx->ErrorCode;
        printf("test");
        __disable_irq();
        while (1)
        {
        }
    }
    dma_rx_in_progress = 1;
}

// Start a DMA TX
void spi_dma_slave_tx(uint8_t * buffer, uint16_t amount_of_data_to_transfer){
    volatile HAL_StatusTypeDef status;
    transmitting_amount_of_bytes = amount_of_data_to_transfer;
    if ((status = HAL_SPI_Transmit_DMA(&hspi, buffer, amount_of_data_to_transfer)) != HAL_OK){
        volatile uint32_t spi_error = hspi.ErrorCode;
        volatile uint32_t dma_error_rx = hspi.hdmarx->ErrorCode;
        volatile uint32_t dma_error_tx = hspi.hdmatx->ErrorCode;
        printf("test");
        __disable_irq();
        while (1)
        {
        }
    }
    dma_tx_in_progress = 1;
}

// DMA RX finished interrupt handler
void spi_dma_slave_receive_finished(){
    uint32_t count =  receiving_amount_of_bytes - __HAL_DMA_GET_COUNTER(hspi.hdmarx);;
    sds_receive_bytes_queue = sds_receive_bytes_queue - (count - sds_receive_bytes_queue_midway) ;

    sds_receive_bytes_queue_midway = 0;

    if(sds_receive_buffer_selected == 0){ 
        sds_receive_buffer0_index = sds_receive_buffer0_index + (count - receive_buffer0_midway_bytes_added);
        receive_buffer0_midway_bytes_added = 0;
    }else{ 
        sds_receive_buffer1_index = sds_receive_buffer1_index + (count - receive_buffer1_midway_bytes_added);
        receive_buffer1_midway_bytes_added = 0;
    }
    dma_rx_in_progress = 0;
    receiving_amount_of_bytes = 0;
}

// DMA TX finished interrupt handler
void spi_dma_slave_transmit_finished(){
    uint32_t count = transmitting_amount_of_bytes - __HAL_DMA_GET_COUNTER(hspi.hdmatx);
    sds_transmit_bytes_queue = sds_transmit_bytes_queue - count;
    sds_transmit_buffer_index = sds_transmit_buffer_index + count;

    dma_tx_in_progress = 0;
    transmitting_amount_of_bytes = 0;
}

// Code that aborts the DAM RX AND TX. Some code added to fix the broken HAL version of abort
void spi_dma_slave_abort_rx_and_tx_dma(){

    // Do no stop it if there is nothing to stop
    if(!dma_rx_in_progress && !dma_tx_in_progress){
        return;
    }

    volatile HAL_StatusTypeDef status = HAL_SPI_Abort(&hspi);
    if (status != HAL_OK){

        if(hspi.hdmarx->ErrorCode == 128){
            // If it is this error then dont really care and continue.
            dma_rx_in_progress = 0;
            return;
        }

        volatile uint32_t spi_error = hspi.ErrorCode;
        volatile uint32_t dma_error_rx = hspi.hdmarx->ErrorCode;
        volatile uint32_t dma_error_tx = hspi.hdmatx->ErrorCode;
        printf("test");
        __disable_irq();
        while (1)
        {
        }
    }

    uint32_t count;
    uint32_t resetcount;

    /* Initialized local variable  */
    resetcount = 100U * (SystemCoreClock / 24U / 1000U);
    count = resetcount;

    if (dma_rx_in_progress && hspi.hdmarx != NULL){
        /* Set the SPI DMA Abort callback :
        will lead to call HAL_SPI_AbortCpltCallback() at end of DMA abort procedure */
        hspi.hdmarx->XferAbortCallback = NULL;

        /* Abort DMA Rx Handle linked to SPI Peripheral */
        if (HAL_DMA_Abort(hspi.hdmarx) != HAL_OK){
            hspi.ErrorCode = HAL_SPI_ERROR_ABORT;
        }

        /* Disable peripheral */
        __HAL_SPI_DISABLE(&hspi);

        /* Disable Rx DMA Request */
        CLEAR_BIT(hspi.Instance->CR2, (SPI_CR2_RXDMAEN));
    }

    /* Abort the SPI DMA Tx Stream/Channel : use blocking DMA Abort API (no callback) */
    if (dma_tx_in_progress && hspi.hdmatx != NULL){
        /* Set the SPI DMA Abort callback :
        will lead to call HAL_SPI_AbortCpltCallback() at end of DMA abort procedure */
        hspi.hdmatx->XferAbortCallback = NULL;

        /* Abort DMA Tx Handle linked to SPI Peripheral */
        if (HAL_DMA_Abort(hspi.hdmatx) != HAL_OK){
            hspi.ErrorCode = HAL_SPI_ERROR_ABORT;
        }

        /* Disable Tx DMA Request */
        CLEAR_BIT(hspi.Instance->CR2, (SPI_CR2_TXDMAEN));

        /* Wait until TXE flag is set */
        do{
            if (count == 0U){
                SET_BIT(hspi.ErrorCode, HAL_SPI_ERROR_ABORT);
                break;
            }
            count--;
        } while ((hspi.Instance->SR & SPI_FLAG_TXE) == RESET);
    }
}

// Stop all spi dma activity and reset everything
void sds_stop_all_dma(){

    spi_dma_slave_abort_rx_and_tx_dma();

    dma_rx_in_progress = 0;
    dma_tx_in_progress = 0;

    if(sds_receive_buffer_selected == 0){
        sds_receive_buffer0_index = 0;
        receive_buffer0_midway_bytes_added = 0;
        sds_skipped_bytes_buffer0 = 0;
    }else{
        sds_receive_buffer1_index = 0;
        receive_buffer1_midway_bytes_added = 0;
        sds_skipped_bytes_buffer1 = 0;
    }

    transmitting_amount_of_bytes = 0;
    receiving_amount_of_bytes = 0;

    sds_receive_bytes_queue = 0;
    sds_receive_bytes_queue_midway = 0;

    sds_transmit_buffer_index = 0;
    sds_transmit_bytes_queue = 0;
}

// Block until the receive queue is empty. Timeout if it takes to long and reset the DMA RX TX
uint8_t sds_wait_for_receive_queue_empty(uint16_t timeout_ms){
    uint32_t start_time = HAL_GetTick();
    uint32_t delta_time = 0;

    while(sds_receive_bytes_queue != 0 && (delta_time = (HAL_GetTick() - start_time)) < timeout_ms);

    if(delta_time >= timeout_ms){
        sds_stop_all_dma();
        return 0;
    }

    return 1;
}

// Block until the transmit queue is empty. Timeout if it takes to long and reset the DMA RX TX
uint8_t sds_wait_for_transmit_queue_empty(uint16_t timeout_ms){
    uint32_t start_time = HAL_GetTick();
    uint32_t delta_time = 0;

    // consider checking if the slave select goes to disabled
    while(sds_transmit_bytes_queue != 0  && (delta_time = (HAL_GetTick() - start_time)) < timeout_ms );

    // Stupid compiler does not register the reset of dma_tx_in_progress
    if(sds_transmit_bytes_queue == 0 && dma_tx_in_progress != 0){
        dma_tx_in_progress = 0;
    }

    if(delta_time >= timeout_ms){
        sds_stop_all_dma();

        return 0;
    }

    return 1;
}

// Reset the buffer data and the index of the active buffer
void sds_clear_residual_data(){
    if (sds_receive_buffer_selected == 0) {
        receive_buffer0[0] = 0;
        // no need to wipe all data
        sds_receive_buffer0_index = 0;
    } else {
        receive_buffer1[0] = 0;
        sds_receive_buffer1_index = 0;
    }
}

// Clear the flags from the previous RX or TX
void sds_check_and_clear_overrun_error(){
    // Check for overrun error
    if (__HAL_SPI_GET_FLAG(&hspi, SPI_FLAG_OVR)) {
        // Clear the overrun flag by reading DR and SR
        __HAL_SPI_CLEAR_OVRFLAG(&hspi);
    }
}

// Try to flushe whatever is in the DR register of the SPI. This is needed, because sometimes the RX picks up data that was sent in the TX
void sds_flush_dr_register(){
    volatile uint8_t temp = hspi.Instance->DR;
    temp = hspi.Instance->SR;
}

// Calback for the salve select pin interrupt on rising and falling
void sds_handle_slave_select(){
    // Toggle the salve select
    if (sds_slave_selected == 0) {
        sds_slave_selected = 1;
    } else {
        sds_slave_selected = 0;
    }
}

// Initiate a blcoking DMA RX
uint8_t spi_dma_slave_receive(uint8_t * receive_data, uint16_t receive_data_size, uint16_t timeout_ms){
    if(receive_data_size >= SPI_BIT_BANG_RECEIVE_BUFFER_SIZE){
        return 0;
    }

    if(!sds_wait_for_receive_queue_empty(timeout_ms)) return 0;
    if(!sds_wait_for_transmit_queue_empty(timeout_ms)) return 0;
    sds_receive_bytes_queue = sds_receive_bytes_queue + receive_data_size;

    sds_check_and_clear_overrun_error();
    sds_flush_dr_register();

    if(sds_receive_buffer_selected == 0){
        spi_dma_slave_rx(receive_buffer0, sds_receive_bytes_queue);
    }else{
        spi_dma_slave_rx(receive_buffer1, sds_receive_bytes_queue);
    }

    if(!sds_wait_for_receive_queue_empty(timeout_ms)) return 0;

    // Could cause problems with interrupt happening durring this proccess
    for(uint16_t i = 0; i < receive_data_size; i++){
        if(sds_receive_buffer_selected == 0){
            receive_data[i] = receive_buffer0[i];
        }else{
            receive_data[i] = receive_buffer1[i];
        }
    }

    if(sds_receive_buffer_selected == 0){
        sds_receive_buffer0_index = sds_receive_buffer0_index - receive_data_size;
        receive_buffer0[0] = 0;
    }else{
        sds_receive_buffer1_index = sds_receive_buffer1_index - receive_data_size;
        receive_buffer1[0] = 0;
    }
    return 1;
}

// Initiate a blocking DMA TX
uint8_t spi_dma_slave_transmit(uint8_t * transmit_data, uint16_t transmit_data_size, uint16_t timeout_ms){
    if(transmit_data_size >= SPI_BIT_BANG_TRANSMIT_BUFFER_SIZE){
        return 0;
    }

    if(!sds_wait_for_receive_queue_empty(timeout_ms)) return 0;
    if(!sds_wait_for_transmit_queue_empty(timeout_ms)) return 0;

    for(uint16_t i = 0; i < transmit_data_size; i++){
        transmit_buffer[i] = transmit_data[i];
    }

    sds_transmit_bytes_queue = sds_transmit_bytes_queue + transmit_data_size;

    spi_dma_slave_tx(transmit_buffer, sds_transmit_bytes_queue);

    if(!sds_wait_for_transmit_queue_empty(timeout_ms)) return 0;

    sds_transmit_buffer_index = sds_transmit_buffer_index - transmit_data_size;

    sds_clear_residual_data();

    return 1;
}

// Reset everything about the non active buffer so another transfer can be made to it.
uint8_t spi_dma_slave_reset_non_active_receive_buffer(){
    if(sds_receive_buffer_selected == 1){ // If 0 is selected then reset the 1
        sds_receive_buffer0_index = 0;
        receive_buffer0[0] = 0;
        receive_buffer0_midway_bytes_added = 0;
        sds_skipped_bytes_buffer0 = 0;
    }else{
        sds_receive_buffer1_index = 0;
        receive_buffer1[0] = 0;
        receive_buffer1_midway_bytes_added = 0;
        sds_skipped_bytes_buffer1 = 0;
    }

    return 1;
}

// Completely wipe the non active buffer by setting all values in it to zero
uint8_t spi_dma_slave_wipe_non_active_receive_buffer(){
    if(sds_receive_buffer_selected == 1){ // If 1 is selected then reset the 0 buffer
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

// Toggle which buffer is the active one. It will block until slave is not selected
uint8_t spi_dma_slave_swap_receive_async_buffer(){
    while (sds_slave_selected); // Wait for slave not to be selected
    if(sds_receive_buffer_selected == 0){
        sds_receive_buffer_selected = 1;
    }else if(sds_receive_buffer_selected == 1){
        sds_receive_buffer_selected = 0;
    }
    return 1;
}

// Stop the async DMA RX
void sds_stop_async_dma_rx(){
    spi_dma_slave_abort_rx_and_tx_dma();

    transmitting_amount_of_bytes = 0;
    receiving_amount_of_bytes = 0;

    sds_receive_bytes_queue = 0;
    sds_receive_bytes_queue_midway = 0;
}

// Update the bytes received while being already in a async DMA RX
void spi_dma_slave_update_bytes_received(){
    // Only update if the receive is in progress
    // Might not be used at all but nice to  have just in case
    if(dma_rx_in_progress){
        volatile uint32_t counter = __HAL_DMA_GET_COUNTER(hspi.hdmarx);
        volatile uint16_t midway_received_bytes =  receiving_amount_of_bytes - counter;
        
        sds_receive_bytes_queue = sds_receive_bytes_queue + (midway_received_bytes - sds_receive_bytes_queue_midway);
        sds_receive_bytes_queue_midway = midway_received_bytes;

        if(sds_receive_buffer_selected == 0){
            
            // Update the index without adding too much of the midway values
            // 10 bytes received, no midway set
            // 0 += (10 - 0)
            // 5 bytes received, midway is 10 
            // 10 += (15 - 10)
            // 5 bytes received, midway is 15
            // 15 += (20 - 15)

            sds_receive_buffer0_index = sds_receive_buffer0_index + (midway_received_bytes - receive_buffer0_midway_bytes_added);
            receive_buffer0_midway_bytes_added = midway_received_bytes;
        }else{
            sds_receive_buffer1_index = sds_receive_buffer1_index + (midway_received_bytes - receive_buffer1_midway_bytes_added);
            receive_buffer1_midway_bytes_added = midway_received_bytes;
        }
    }
}

// Loads the data from the spi buffers to the provided buffer, optional feature of 
uint8_t spi_dma_slave_read_receive_async_response_form_non_active_buffer(uint8_t * receive_data, uint16_t receive_data_size, uint8_t skip_zeros){

    // Make sure the destination buffer has the same amount of storage. Otherwise stop everything
    if(receive_data_size != SPI_BIT_BANG_RECEIVE_BUFFER_SIZE){
        printf("receive_data size is too small to keep all the data\n");
        while(1);
    }

    // Skip bytes is the count of how many string terminator characters are in the buffer. There can only be one and it has to be at the end of the buffer index

    // If we just copy the buffer over to the new one. What is written to the sd card will be terminated with the first occuring string terminator.
    // If that happens only the first log from the buffer will be written to sd card an not the rest.

    if(sds_receive_buffer_selected == 0){
        if(skip_zeros){
            for(uint16_t i = 0; i < sds_receive_buffer1_index; i++){
                if(receive_buffer1[i] == '\0' && i != sds_receive_buffer1_index - 1 ){
                    sds_skipped_bytes_buffer1++;
                }else{
                    receive_data[i-sds_skipped_bytes_buffer1] = receive_buffer1[i];
                }
            }
        }else{
            memcpy(receive_data, receive_buffer1, sds_receive_buffer1_index);
        }
    }else{
        if(skip_zeros){
            for(uint16_t i = 0; i < sds_receive_buffer0_index; i++){
                if(receive_buffer0[i] == '\0' && i != sds_receive_buffer0_index - 1 ){
                    sds_skipped_bytes_buffer0++;
                }else{
                    receive_data[i - sds_skipped_bytes_buffer0] = receive_buffer0[i];
                }
            }
        }else{
            memcpy(receive_data, receive_buffer0, sds_receive_buffer0_index);
        }
    }

    return 1;
}


// Stop the async DMA RX
uint8_t spi_dma_slave_hard_cancel_receive_async(){
    sds_stop_all_dma();

    return 1;
}

// Start new async DMA RX. All other DMA RX have to be finished before starting
uint8_t spi_dma_slave_receive_async(){
    // Assuming that any other transfer is already finished

    sds_receive_bytes_queue = SPI_BIT_BANG_RECEIVE_BUFFER_SIZE;

    // Make sure the SPI RX peripheral does not have anything left in the buffer
    sds_check_and_clear_overrun_error();
    sds_flush_dr_register();

    // Use the active buffer only
    if(sds_receive_buffer_selected == 0){
        spi_dma_slave_rx(receive_buffer0, sds_receive_bytes_queue);
    }else{
        spi_dma_slave_rx(receive_buffer1, sds_receive_bytes_queue);
    }
    
    return 1;
}

// Get non active buffer size with or without skip bytes
uint16_t spi_dma_slave_get_non_active_buffer_size(uint8_t skip_bytes){
    if(sds_receive_buffer_selected == 1){
        if(skip_bytes){
            return sds_receive_buffer0_index - sds_skipped_bytes_buffer0;
        }else{
            return sds_receive_buffer0_index;
        }
    }else{
        if(skip_bytes){
            return sds_receive_buffer1_index - sds_skipped_bytes_buffer1;
        }else{
            return sds_receive_buffer1_index;
        }
    }
}

// Get active buffer size with or without skip bytes
uint16_t spi_dma_slave_get_active_buffer_size(uint8_t skip_bytes){
    if(sds_receive_buffer_selected == 1){
        if(skip_bytes){
            return sds_receive_buffer1_index - sds_skipped_bytes_buffer1;
        }else{
            return sds_receive_buffer1_index;
        }
    }else{
        if(skip_bytes){
            return sds_receive_buffer0_index - sds_skipped_bytes_buffer0;
        }else{
            return sds_receive_buffer0_index;
        }
    }
}


// Block until a specific slave selected state is reached
void sda_wait_for_slave_select_state(uint8_t state){
    if(state == 1){
        while(sds_slave_selected == 0);
        return;
    }else{ 
        while(sds_slave_selected == 1);
        return;
    }
}

void sda_update_how_many_bytes_received_async_dma(){
    spi_dma_slave_update_bytes_received();
}