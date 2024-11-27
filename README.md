# STM32-SD logger
![IMG_4903](https://github.com/user-attachments/assets/645b17e6-43e6-43a9-9442-c1b80d204d96)

SPI slave that acts as an interface to an SD card. Max SPI speed 780Kb/s with DMA mode and speed of 390Kb/s using bit bang mode. It supports low level commands for control of the SD card and has high level commands that just set it up for logging.

SD cards with more than 4GB of storage need to be partitioned to use FAT32 or something to make it work. Same deal as with 3D printers. SDXC are not supported. Use ChatGpt to figure out how to do that

The current version supports DMA SPI slave functionality that lets the data be written to the SD card while it is also received at the same time from the master to the slave buffers.

The old version used bit bang spi slave functionality. Interrupts were triggered on clk pin and slave select pin to receive and transmit data. This approach actually worked well but I was uncertain it could handle my future needs of higher demand systems of logging.


An example of how the transfer looks like when transferring 50 bytes in async mode from the master to the slave:
![SPI transfer example](./images/logger_max_async_speed_analyzer_dma.png)

An example of the data written to the SD Card:
![DMA functionality sd card data](./images/dma_functionality_sd_card.png)


You may also experience the SD card not writing all data if it is sent rapidly, this is because of the fatfs library slowing things down and not writing for some random periods. The slow down looks like this:
![Write slow down](./images/drawbacks_of_fatfs_library.png)


## Some examples from the bit bang SPI functionality

This is also some example of the old functionality of the bit bang spi slave:
![SPI transfer example](./images/logger_max_async_speed_analyzer.png)

An example of how the transfer look like when transferring 120 bytes:

![SPI transfer example](./images/logger_max_speed_logic_analyzer.png)

How the sd card folder looks like:

![Log files example in sd card](./images/log_files_example.png)

How the file looks like with quadcopter data i logged:

![Log file content example](./images/file_contents.png)
