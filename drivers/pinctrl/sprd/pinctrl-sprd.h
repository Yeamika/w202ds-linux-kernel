/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Driver header file for pin controller driver
 * Copyright (C) 2017 Spreadtrum  - http://www.spreadtrum.com
 */

#ifndef __PINCTRL_SPRD_H__
#define __PINCTRL_SPRD_H__

struct platform_device;

/*
 * The UMS9620 (qogirn6pro) has global control registers beyond index 15 and
 * bit offsets up to 28, so the register index takes 5 bits and the bit offset
 * 8 (Unisoc's 5.4 layout); the pin number stays below 2^12 and the whole
 * value within 32 bits.
 */
#define NUM_OFFSET	(22)
#define TYPE_OFFSET	(18)
#define BIT_OFFSET	(10)
#define WIDTH_OFFSET	(6)

#define SPRD_PIN_INFO(num, type, offset, width, reg)	\
		(((num) & 0xFFF) << NUM_OFFSET |	\
		 ((type) & 0xF) << TYPE_OFFSET |	\
		 ((offset) & 0xFF) << BIT_OFFSET |	\
		 ((width) & 0xF) << WIDTH_OFFSET |	\
		 ((reg) & 0x1F))

#define SPRD_PINCTRL_PIN(pin)	SPRD_PINCTRL_PIN_DATA(pin, #pin)

#define SPRD_PINCTRL_PIN_DATA(a, b)				\
	{							\
		.name = b,					\
		.num = (((a) >> NUM_OFFSET) & 0xfff),		\
		.type = (((a) >> TYPE_OFFSET) & 0xf),		\
		.bit_offset = (((a) >> BIT_OFFSET) & 0xff),	\
		.bit_width = ((a) >> WIDTH_OFFSET & 0xf),	\
		.reg = ((a) & 0x1f)				\
	}

enum pin_type {
	GLOBAL_CTRL_PIN,
	COMMON_PIN,
	MISC_PIN,
};

struct sprd_pins_info {
	const char *name;
	unsigned int num;
	enum pin_type type;

	/* for global control pins configuration */
	unsigned long bit_offset;
	unsigned long bit_width;
	unsigned int reg;
};

int sprd_pinctrl_core_probe(struct platform_device *pdev,
			    struct sprd_pins_info *sprd_soc_pin_info,
			    int pins_cnt, u32 common_pin_offset,
			    u32 misc_pin_offset);
void sprd_pinctrl_remove(struct platform_device *pdev);
void sprd_pinctrl_shutdown(struct platform_device *pdev);
int sprd_pinctrl_get_pin_raw(struct platform_device *pdev, const char *name,
			     u32 *val);
int sprd_pinctrl_set_pin_func(struct platform_device *pdev, const char *name,
			      unsigned int func);

#endif /* __PINCTRL_SPRD_H__ */
