#pragma once

#include "stdio.h"
#include "stm32f4xx_hal.h"

void spi_bit_bang_ss_interrupt();
void spi_bit_bang_clk_interrupt();
uint8_t spi_bit_bang_initialize(GPIO_TypeDef* SS_port, uint16_t SS_pin, GPIO_TypeDef* CLK_port, uint16_t CLK_pin, GPIO_TypeDef* MOSI_port, uint16_t MOSI_pin, GPIO_TypeDef* MISO_port, uint16_t MISO_pin);
uint8_t spi_bit_bang_receive(uint8_t * receive_data, uint16_t receive_data_size, uint16_t timeout_ms);
uint8_t spi_bit_bang_transmit(uint8_t * transmit_data, uint16_t transmit_data_size, uint16_t timeout_ms);
