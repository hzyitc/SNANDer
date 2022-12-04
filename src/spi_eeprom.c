/*
 * Copyright (C) 2022 McMCC <mcmcc@mail.ru>
 * spi_eeprom.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "timer.h"
#include "spi_eeprom.h"
#include "spi_controller.h"

extern char eepromname[12];
struct spi_eeprom seeprom_info;
int seepromsize = 0;

static void wait_ready(void)
{
	uint8_t data[1];

	while (1) {
		SPI_CONTROLLER_Chip_Select_Low();
		SPI_CONTROLLER_Write_One_Byte(SEEP_RDSR_CMD);
		SPI_CONTROLLER_Read_NByte(data, 1, SPI_CONTROLLER_SPEED_SINGLE);
		SPI_CONTROLLER_Chip_Select_High();

		if ((data[0] & 0x01) == 0) break;
		usleep(1);
	}
}

static void write_enable(void)
{
	uint8_t data[1];

	while (1) {
		SPI_CONTROLLER_Chip_Select_Low();
		SPI_CONTROLLER_Write_One_Byte(SEEP_WREN_CMD);
		SPI_CONTROLLER_Chip_Select_High();
		usleep(1);

		SPI_CONTROLLER_Chip_Select_Low();
		SPI_CONTROLLER_Write_One_Byte(SEEP_RDSR_CMD);
		SPI_CONTROLLER_Read_NByte(data, 1, SPI_CONTROLLER_SPEED_SINGLE);
		SPI_CONTROLLER_Chip_Select_High();

		if (data[0] == 0x02) break;
		usleep(1);
	}
}

static void eeprom_write_byte(struct spi_eeprom *dev, uint16_t address, uint8_t data)
{
	uint8_t buf[4];

	write_enable();

	buf[0] = SEEP_WRITE_CMD;
	if (dev->addr_bits == 9 && address > 0xff)
		buf[0] = buf[0] | 0x08;

	SPI_CONTROLLER_Chip_Select_Low();
	if (dev->addr_bits < 10) {
		buf[1] = (address & 0xFF);
		buf[2] = data;
		SPI_CONTROLLER_Write_NByte(buf, 3, SPI_CONTROLLER_SPEED_SINGLE);
	} else {
		buf[1] = (address & 0x0FF00) >> 8;
		buf[2] = (address & 0x0FF);
		buf[3] = data;
		SPI_CONTROLLER_Write_NByte(buf, 4, SPI_CONTROLLER_SPEED_SINGLE);
	}
	SPI_CONTROLLER_Chip_Select_High();

	wait_ready();
}

static uint8_t eeprom_read_byte(struct spi_eeprom *dev, uint16_t address)
{
	uint8_t buf[3];
	uint8_t data;
	buf[0] = SEEP_READ_CMD;

	if (dev->addr_bits == 9 && address > 0xff)
		buf[0] = buf[0] | 0x08;

	SPI_CONTROLLER_Chip_Select_Low();
	if (dev->addr_bits < 10) {
		buf[1] = (address & 0xFF);
		SPI_CONTROLLER_Write_NByte(buf, 2, SPI_CONTROLLER_SPEED_SINGLE);
		SPI_CONTROLLER_Read_NByte(buf, 1, SPI_CONTROLLER_SPEED_SINGLE);
		data = buf[0];
	} else {
		buf[1] = (address & 0x0FF00) >> 8;
		buf[2] = (address & 0x0FF);
		SPI_CONTROLLER_Write_NByte(buf, 3, SPI_CONTROLLER_SPEED_SINGLE);
		SPI_CONTROLLER_Read_NByte(buf, 1, SPI_CONTROLLER_SPEED_SINGLE);
		data = buf[0];
	}
	SPI_CONTROLLER_Chip_Select_High();

	return data;
}

int32_t parseSEEPsize(char *seepromname, struct spi_eeprom *seeprom)
{
	int i;

	for (i = 0; seepromlist[i].total_bytes; i++) {
		if (strstr(seepromlist[i].name, seepromname)) {
			memcpy(seeprom, &(seepromlist[i]), sizeof(struct spi_eeprom));
			return (seepromlist[i].total_bytes);
		}
	}

	return -1;
}

int spi_eeprom_read(unsigned char *buf, unsigned long from, unsigned long len)
{
	unsigned char *pbuf, ebuf[MAX_SEEP_SIZE];
	int i;

	if (len == 0)
		return -1;

	timer_start();
	memset(ebuf, 0, sizeof(ebuf));
	pbuf = ebuf;

	for (i = 0; i < seepromsize; i++)
		pbuf[i] = eeprom_read_byte(&seeprom_info, (uint16_t)i);

	memcpy(buf, pbuf + from, len);

	printf("Read [%d] bytes from [%s] EEPROM address 0x%08lu\n", (int)len, eepromname, from);
	timer_end();

	return (int)len;
}

int spi_eeprom_erase(unsigned long offs, unsigned long len)
{
	unsigned char *pbuf, ebuf[MAX_SEEP_SIZE];
	int i;

	if (len == 0)
		return -1;

	timer_start();
	memset(ebuf, 0xff, sizeof(ebuf));
	pbuf = ebuf;

	if (offs || len < seepromsize) {
		for (i = 0; i < seepromsize; i++)
			pbuf[i] = eeprom_read_byte(&seeprom_info, (uint16_t)i);
		memset(pbuf + offs, 0xff, len);
	}

	for (i = 0; i < seepromsize; i++)
		eeprom_write_byte(&seeprom_info, (uint16_t)i, pbuf[i]);

	printf("Erased [%d] bytes of [%s] EEPROM address 0x%08lu\n", (int)len, eepromname, offs);
	timer_end();

	return 0;
}

int spi_eeprom_write(unsigned char *buf, unsigned long to, unsigned long len)
{
	unsigned char *pbuf, ebuf[MAX_SEEP_SIZE];
	int i;

	if (len == 0)
		return -1;

	timer_start();
	memset(ebuf, 0xff, sizeof(ebuf));
	pbuf = ebuf;

	if (to || len < seepromsize) {
		for (i = 0; i < seepromsize; i++)
			pbuf[i] = eeprom_read_byte(&seeprom_info, (uint16_t)i);
	}
	memcpy(pbuf + to, buf, len);

	for (i = 0; i < seepromsize; i++)
		eeprom_write_byte(&seeprom_info, (uint16_t)i, pbuf[i]);

	printf("Wrote [%d] bytes to [%s] EEPROM address 0x%08lu\n", (int)len, eepromname, to);
	timer_end();

	return (int)len;
}

long spi_eeprom_init(void)
{
	if (seepromsize <= 0) {
		printf("SPI EEPROM Not Detected!\n");
		return -1;
	}

	printf("SPI EEPROM chip: %s, Size: %d bytes\n", eepromname, seepromsize);

	return (long)seepromsize;
}

void support_spi_eeprom_list(void)
{
	int i;

	printf("SPI EEPROM Support List:\n");
	for ( i = 0; i < (sizeof(seepromlist)/sizeof(struct spi_eeprom)); i++)
	{
		if (!seepromlist[i].total_bytes)
			break;
		printf("%03d. %s\n", i + 1, seepromlist[i].name);
	}
}
/* End of [spi_eeprom.c] package */
