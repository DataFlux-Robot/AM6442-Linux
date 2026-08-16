// SPDX-License-Identifier: GPL-2.0+
/*
 * FB driver for the ILI9486 LCD Controller
 *
 * Copyright (C) 2014 Noralf Tronnes
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <video/mipi_display.h>

#include "fbtft.h"

#define DRVNAME		"fb_ili9486"
#define WIDTH		320
#define HEIGHT		480

/* this init sequence matches PiScreen */
static const s16 default_init_sequence[] = {
	/* Interface Mode Control */
	-1, 0xb0, 0x0,
	-1, MIPI_DCS_EXIT_SLEEP_MODE,
	-2, 250,
	/* Interface Pixel Format */
	-1, MIPI_DCS_SET_PIXEL_FORMAT, 0x55,
	/* Power Control 3 */
	-1, 0xC2, 0x44,
	/* VCOM Control 1 */
	-1, 0xC5, 0x00, 0x00, 0x00, 0x00,
	/* PGAMCTRL(Positive Gamma Control) */
	-1, 0xE0, 0x0F, 0x1F, 0x1C, 0x0C, 0x0F, 0x08, 0x48, 0x98,
		  0x37, 0x0A, 0x13, 0x04, 0x11, 0x0D, 0x00,
	/* NGAMCTRL(Negative Gamma Control) */
	-1, 0xE1, 0x0F, 0x32, 0x2E, 0x0B, 0x0D, 0x05, 0x47, 0x75,
		  0x37, 0x06, 0x10, 0x03, 0x24, 0x20, 0x00,
	/* Digital Gamma Control 1 */
	-1, 0xE2, 0x0F, 0x32, 0x2E, 0x0B, 0x0D, 0x05, 0x47, 0x75,
		  0x37, 0x06, 0x10, 0x03, 0x24, 0x20, 0x00,
	-1, MIPI_DCS_EXIT_SLEEP_MODE,
	-1, MIPI_DCS_SET_DISPLAY_ON,
	/* end marker */
	-3
};

static void set_addr_win(struct fbtft_par *par, int xs, int ys, int xe, int ye)
{
	write_reg(par, MIPI_DCS_SET_COLUMN_ADDRESS,
		  xs >> 8, xs & 0xFF, xe >> 8, xe & 0xFF);

	write_reg(par, MIPI_DCS_SET_PAGE_ADDRESS,
		  ys >> 8, ys & 0xFF, ye >> 8, ye & 0xFF);

	write_reg(par, MIPI_DCS_WRITE_MEMORY_START);
}

static int set_var(struct fbtft_par *par)
{
	switch (par->info->var.rotate) {
	case 0:
		write_reg(par, MIPI_DCS_SET_ADDRESS_MODE,
			  0x80 | (par->bgr << 3));
		break;
	case 90:
		write_reg(par, MIPI_DCS_SET_ADDRESS_MODE,
			  0x60 | (par->bgr << 3));
		break;
	case 180:
		write_reg(par, MIPI_DCS_SET_ADDRESS_MODE,
			  0x40 | (par->bgr << 3));
		break;
	case 270:
		write_reg(par, MIPI_DCS_SET_ADDRESS_MODE,
			  0xE0 | (par->bgr << 3));
		break;
	default:
		break;
	}

	return 0;
}

static int write_vmem16_18bus8(struct fbtft_par *par, size_t offset, size_t len)
{
	u16 *vmem16 = (u16 *)(par->info->screen_buffer + offset);
	u8 *buf = par->txbuf.buf;
	size_t remain = len / 2;
	size_t i, chunk;
	int ret = 0;

	if (par->gpio.dc)
		gpiod_set_value(par->gpio.dc, 1);

	while (remain) {
		chunk = remain;
		if (chunk * 3 > par->txbuf.len)
			chunk = par->txbuf.len / 3;

		for (i = 0; i < chunk; i++) {
			u16 pixel = *vmem16++;
			/* RGB565 → RGB666 扩展（常用且兼容性较好的方式） */
			buf[i*3 + 0] = ((pixel >> 8) & 0xF8) | ((pixel >> 13) & 0x07); /* R */
			buf[i*3 + 1] = ((pixel >> 3) & 0xFC) | ((pixel >> 9)  & 0x03); /* G */
			buf[i*3 + 2] = ((pixel << 3) & 0xF8) | ((pixel >> 2)  & 0x07); /* B */
		}

		ret = par->fbtftops.write(par, buf, chunk * 3);
		if (ret < 0)
			return ret;

		remain -= chunk;
	}
	return 0;
}

static struct fbtft_display display = {
	.regwidth = 8,
	.width = WIDTH,
	.height = HEIGHT,
	.init_sequence = default_init_sequence,
	.fbtftops = {
		.set_addr_win = set_addr_win,
		.set_var = set_var,
		.write_vmem = write_vmem16_18bus8,	/* 新增：18-bit 支持 */
	},
};

FBTFT_REGISTER_DRIVER(DRVNAME, "ilitek,ili9486", &display);

MODULE_ALIAS("spi:" DRVNAME);
MODULE_ALIAS("platform:" DRVNAME);
MODULE_ALIAS("spi:ili9486");
MODULE_ALIAS("platform:ili9486");

MODULE_DESCRIPTION("FB driver for the ILI9486 LCD Controller");
MODULE_AUTHOR("Noralf Tronnes");
MODULE_LICENSE("GPL");
