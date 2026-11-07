/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __DW_DP__
#define __DW_DP__

#include <linux/device.h>

struct drm_encoder;
struct dw_dp;
struct phy;

/* dw_dp_plat_data.pixel_mode: 0 keeps the mainline default (quad pixel) */
enum dw_dp_plat_pixel_mode {
	DW_DP_PLAT_PIXEL_MODE_DEFAULT = 0,
	DW_DP_PLAT_PIXEL_MODE_SINGLE = 1,
	DW_DP_PLAT_PIXEL_MODE_DUAL = 2,
	DW_DP_PLAT_PIXEL_MODE_QUAD = 3,
};

/*
 * Every field after max_link_rate is opt-in: zero/NULL/false keeps the mainline behaviour.
 *
 * @phy: used instead of devm_of_phy_get(dev, dev->of_node, NULL); the caller keeps the reference.
 * @ctrl_phy_rate_eq: the controller drives rate and equalisation over its PHY interface (Unisoc): rate code in
 *	PHYIF_CTRL[5:4], SSC_DIS from the sink's DPCD, per-lane PHY_TX_EQ (0xa04), PHY BUSY waited for around the
 *	P3/P0 transitions. phy_configure() is still called (set_rate/set_lanes and set_voltages).
 * @hpd_status_bit8: HPD level from HPD_STATUS bit 8 instead of HPD_STATE[11:9] == 7.
 * @optional_clks_rst: the "apb", "aux", "i2s", "spdif", "hdcp" clocks and the reset are optional (absent: NULL).
 * @aux_poll: AUX completion polled (GENERAL_INTERRUPT AUX_REPLY_EVENT latched, or AUX_STATUS AUX_TIMEOUT; after
 *	10 ms without either the reply status decides), AUX_REPLY_EVENT_EN left off.
 * @full_reset: every controller reset is SOFT_RESET_CTRL 0x1d (controller, HDCP, audio, AUX), not just bit 0.
 * @post_reset: called after every controller (re)initialisation: in dw_dp_bind(), after each reset (atomic_disable),
 *	and in dw_dp_hw_reinit(). Interrupt enables are already written when it runs.
 * @hw_get/@hw_put: bracket every register access that is not part of an atomic commit or of the caller's own
 *	dw_dp_hw_reinit(): detect, edid_read, the AUX transfer (also through /dev/drm_dp_aux*), the HPD work, link
 *	retraining, and the initialisation in dw_dp_bind(). hw_get may nest and must be refcounted by the caller.
 *	A nonzero return aborts the access: detect reports disconnected, edid_read returns NULL, the AUX transfer
 *	returns -EIO, the HPD work and bind's initialisation are skipped (call dw_dp_hw_reinit() after power-up).
 *	With @hw_get set, dw_dp_bind() requests the IRQ masked (IRQF_NO_AUTOEN): the caller enable_irq()s it while
 *	the controller is powered and disable_irq()s it before power-down.
 * @priv: argument of @post_reset, @hw_get and @hw_put.
 */
struct dw_dp_plat_data {
	u32 max_link_rate;
	enum dw_dp_plat_pixel_mode pixel_mode;
	struct phy *phy;
	bool ctrl_phy_rate_eq;
	bool hpd_status_bit8;
	bool optional_clks_rst;
	bool aux_poll;
	bool full_reset;
	void *priv;
	void (*post_reset)(void *priv);
	int (*hw_get)(void *priv);
	void (*hw_put)(void *priv);
};

struct dw_dp *dw_dp_bind(struct device *dev, struct drm_encoder *encoder,
			 const struct dw_dp_plat_data *plat_data);

/*
 * After the controller lost its state (power domain cycled): IRQ masked, controller reset (@full_reset depth),
 * interrupt enables, @post_reset, pending GENERAL_INTERRUPT and HPD_STATUS events cleared, link training state
 * forgotten, IRQ unmasked. The caller holds the hardware powered; @hw_get is not called.
 */
int dw_dp_hw_reinit(struct dw_dp *dp);

/*
 * Retrain the link and restart the video stream of an output whose CRTC is still active, from the
 * committed bridge state. For sinks that pulse HPD low while userspace keeps the same configuration:
 * no new commit arrives, so nothing else would re-enable the stream. Takes all modeset locks.
 * Returns -ENOLINK when the output is off or HPD is low.
 */
int dw_dp_restore_stream(struct dw_dp *dp);
#endif /* __DW_DP__ */
