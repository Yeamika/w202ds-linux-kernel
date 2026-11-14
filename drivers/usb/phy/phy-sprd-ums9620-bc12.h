/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef SPRD_UMS9620_BC12_H
#define SPRD_UMS9620_BC12_H

/* UMP9620 masks and redetect sequence from the vendor sprd-bc1p2 driver.
 * The caller owns DP/DM and keeps the USB data path disconnected throughout.
 * Never accept an inherited DONE result: require clear -> completed first.
 */
#define UMP9620_BC12_STATUS 0x239c
#define UMP9620_BC12_MUX 0x23a0
#define UMP9620_BC12_CTRL 0x243c
#define UMP9620_BC12_MUX_MASK (BIT(14) | BIT(0) | GENMASK(7, 4))

static int ums9620_bc12_sink_present(struct regmap *map)
{
	u32 state, debug;
	int ret = regmap_read(map, 0x39c, &state);

	if (!ret)
		ret = regmap_read(map, 0x3e0, &debug);
	if (ret)
		return ret;
	return (state & 0x1f) == 2 && (debug & BIT(8));
}

static int ums9620_bc12_redetect(struct regmap *map, u32 *detected)
{
	u32 mux, control, status;
	int ret, restore_control, restore_mux, i;

	*detected = 0;
	ret = ums9620_bc12_sink_present(map);
	if (ret <= 0)
		return ret ?: -ENODEV;
	ret = regmap_read(map, UMP9620_BC12_MUX, &mux);
	if (!ret)
		ret = regmap_read(map, UMP9620_BC12_CTRL, &control);
	if (ret)
		return ret;
	if (control & BIT(0))
		return -EBUSY;
	ret = regmap_update_bits(map, UMP9620_BC12_MUX,
				UMP9620_BC12_MUX_MASK, 15 << 4);
	if (!ret)
		ret = regmap_update_bits(map, UMP9620_BC12_CTRL, BIT(0), BIT(0));
	if (ret)
		goto restore;
	msleep(20);
	ret = regmap_read(map, UMP9620_BC12_STATUS, &status);
	if (ret)
		goto restore;
	if (!(status & BIT(2)) || (status & BIT(11))) {
		ret = -EAGAIN;
		goto restore;
	}
	ret = -ETIMEDOUT;
	for (i = 0; i < 110; i++) {
		msleep(20);
		ret = ums9620_bc12_sink_present(map);
		if (ret <= 0) {
			ret = ret ?: -ENODEV;
			break;
		}
		ret = regmap_read(map, UMP9620_BC12_STATUS, &status);
		if (ret)
			break;
		if (!(status & BIT(2))) {
			ret = -ENODEV;
			break;
		}
		if (status & BIT(11)) {
			*detected = status;
			break;
		}
		ret = -ETIMEDOUT;
	}
restore:
	/* Attempt both restores even if one fails; no valid result on any error. */
	restore_control = regmap_update_bits(map, UMP9620_BC12_CTRL, BIT(0), control);
	restore_mux = regmap_update_bits(map, UMP9620_BC12_MUX,
					UMP9620_BC12_MUX_MASK, mux);
	if (restore_control || restore_mux)
		ret = restore_control ?: restore_mux;
	if (ret)
		*detected = 0;
	return ret;
}

#endif
