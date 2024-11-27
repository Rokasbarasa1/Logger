#pragma once

#include "stdio.h"
#include "stm32f4xx_hal.h"
#include "string.h"

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
);

DMA_HandleTypeDef* spi_dma_slave_get_dma_rx_handle();
DMA_HandleTypeDef* spi_dma_slave_get_dma_tx_handle();
void spi_dma_slave_receive_finished();
void spi_dma_slave_transmit_finished();

uint8_t spi_dma_slave_receive(uint8_t * receive_data, uint16_t receive_data_size, uint16_t timeout_ms);
uint8_t spi_dma_slave_transmit(uint8_t * transmit_data, uint16_t transmit_data_size, uint16_t timeout_ms);

uint8_t spi_dma_slave_reset_non_active_receive_buffer();
uint8_t spi_dma_slave_wipe_non_active_receive_buffer();
uint8_t spi_dma_slave_swap_receive_async_buffer();

void spi_dma_slave_update_bytes_received();
uint8_t spi_dma_slave_read_receive_async_response_form_non_active_buffer(uint8_t * receive_data, uint16_t receive_data_size, uint8_t skip_zeros);
uint8_t spi_dma_slave_hard_cancel_receive_async();
uint8_t spi_dma_slave_receive_async();
uint16_t spi_dma_slave_get_non_active_buffer_size(uint8_t skip_bytes);



void sda_wait_for_slave_select_state(uint8_t state);
void sda_update_how_many_bytes_received_async_dma();

void sds_stop_async_dma_rx();
uint16_t spi_dma_slave_get_active_buffer_size(uint8_t skip_bytes);

void sds_handle_slave_select();

void sds_clear_residual_data();