// SPDX-License-Identifier: GPL-2.0
// BQ256XX Battery Charger Driver
// Copyright (C) 2020 Texas Instruments Incorporated - http://www.ti.com/

#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio/consumer.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/types.h>
#include <linux/usb/phy.h>
#include <linux/device.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/acpi.h>
#include <linux/bitfield.h>
#include <linux/devm-helpers.h>
#include <linux/extcon.h>
#include <linux/mfd/sc27xx-pmic.h>
#include <linux/of.h>
#include <linux/spi/spi.h>

#define BQ256XX_MANUFACTURER "Texas Instruments"

#define BQ256XX_INPUT_CURRENT_LIMIT		0x00
#define BQ256XX_CHARGER_CONTROL_0		0x01
#define BQ256XX_CHARGE_CURRENT_LIMIT		0x02
#define BQ256XX_PRECHG_AND_TERM_CURR_LIM	0x03
#define BQ256XX_BATTERY_VOLTAGE_LIMIT		0x04
#define BQ256XX_CHARGER_CONTROL_1		0x05
#define BQ256XX_CHARGER_CONTROL_2		0x06
#define BQ256XX_CHARGER_CONTROL_3		0x07
#define BQ256XX_CHARGER_STATUS_0		0x08
#define BQ256XX_CHARGER_STATUS_1		0x09
#define BQ256XX_CHARGER_STATUS_2		0x0a
#define BQ256XX_PART_INFORMATION		0x0b
#define BQ256XX_CHARGER_CONTROL_4		0x0c

#define BQ256XX_IINDPM_MASK		GENMASK(4, 0)
#define BQ256XX_IINDPM_STEP_uA		100000
#define BQ256XX_IINDPM_OFFSET_uA	100000
#define BQ256XX_IINDPM_MIN_uA		100000
#define BQ256XX_IINDPM_MAX_uA		3200000
#define BQ256XX_IINDPM_DEF_uA		2400000

#define BQ256XX_TS_IGNORE		BIT(6)
#define BQ256XX_TS_IGNORE_SHIFT		6

#define BQ256XX_VINDPM_MASK		GENMASK(3, 0)
#define BQ256XX_VINDPM_STEP_uV		100000
#define BQ256XX_VINDPM_OFFSET_uV	3900000
#define BQ256XX_VINDPM_MIN_uV		3900000
#define BQ256XX_VINDPM_MAX_uV		5400000
#define BQ256XX_VINDPM_DEF_uV		4500000

#define BQ256XX_VBATREG_MASK		GENMASK(7, 3)
#define BQ2560X_VBATREG_STEP_uV		32000
#define BQ2560X_VBATREG_OFFSET_uV	3856000
#define BQ2560X_VBATREG_MIN_uV		3856000
#define BQ2560X_VBATREG_MAX_uV		4624000
#define BQ2560X_VBATREG_DEF_uV		4208000
#define BQ25601D_VBATREG_OFFSET_uV	3847000
#define BQ25601D_VBATREG_MIN_uV		3847000
#define BQ25601D_VBATREG_MAX_uV		4615000
#define BQ25601D_VBATREG_DEF_uV		4199000
#define BQ2561X_VBATREG_STEP_uV		10000
#define BQ25611D_VBATREG_MIN_uV		3494000
#define BQ25611D_VBATREG_MAX_uV		4510000
#define BQ25611D_VBATREG_DEF_uV		4190000
#define BQ25618_VBATREG_MIN_uV		3504000
#define BQ25618_VBATREG_MAX_uV		4500000
#define BQ25618_VBATREG_DEF_uV		4200000
#define BQ256XX_VBATREG_BIT_SHIFT	3
#define BQ2561X_VBATREG_THRESH		0x8
#define BQ25611D_VBATREG_THRESH_uV	4290000
#define BQ25618_VBATREG_THRESH_uV	4300000

#define BQ256XX_CHG_CONFIG_MASK		BIT(4)
#define BQ256XX_CHG_CONFIG_BIT_SHIFT	4

#define BQ256XX_ITERM_MASK		GENMASK(3, 0)
#define BQ256XX_ITERM_STEP_uA		60000
#define BQ256XX_ITERM_OFFSET_uA		60000
#define BQ256XX_ITERM_MIN_uA		60000
#define BQ256XX_ITERM_MAX_uA		780000
#define BQ256XX_ITERM_DEF_uA		180000
#define BQ25618_ITERM_STEP_uA		20000
#define BQ25618_ITERM_OFFSET_uA		20000
#define BQ25618_ITERM_MIN_uA		20000
#define BQ25618_ITERM_MAX_uA		260000
#define BQ25618_ITERM_DEF_uA		60000

#define BQ256XX_IPRECHG_MASK		GENMASK(7, 4)
#define BQ256XX_IPRECHG_STEP_uA		60000
#define BQ256XX_IPRECHG_OFFSET_uA	60000
#define BQ256XX_IPRECHG_MIN_uA		60000
#define BQ256XX_IPRECHG_MAX_uA		780000
#define BQ256XX_IPRECHG_DEF_uA		180000
#define BQ25618_IPRECHG_STEP_uA		20000
#define BQ25618_IPRECHG_OFFSET_uA	20000
#define BQ25618_IPRECHG_MIN_uA		20000
#define BQ25618_IPRECHG_MAX_uA		260000
#define BQ25618_IPRECHG_DEF_uA		40000
#define BQ256XX_IPRECHG_BIT_SHIFT	4

#define BQ256XX_ICHG_MASK		GENMASK(5, 0)
#define BQ256XX_ICHG_STEP_uA		60000
#define BQ256XX_ICHG_MIN_uA		0
#define BQ256XX_ICHG_MAX_uA		3000000
#define BQ2560X_ICHG_DEF_uA		2040000
#define BQ25611D_ICHG_DEF_uA		1020000
#define BQ25618_ICHG_STEP_uA		20000
#define BQ25618_ICHG_MIN_uA		0
#define BQ25618_ICHG_MAX_uA		1500000
#define BQ25618_ICHG_DEF_uA		340000
#define BQ25618_ICHG_THRESH		0x3c
#define BQ25618_ICHG_THRESH_uA		1180000

#define BQ256XX_VBUS_STAT_MASK		GENMASK(7, 5)
#define BQ256XX_VBUS_STAT_NO_INPUT	0
#define BQ256XX_VBUS_STAT_USB_SDP	BIT(5)
#define BQ256XX_VBUS_STAT_USB_CDP	BIT(6)
#define BQ256XX_VBUS_STAT_USB_DCP	(BIT(6) | BIT(5))
#define BQ256XX_VBUS_STAT_USB_OTG	(BIT(7) | BIT(6) | BIT(5))

#define BQ256XX_CHRG_STAT_MASK		GENMASK(4, 3)
#define BQ256XX_CHRG_STAT_NOT_CHRGING	0
#define BQ256XX_CHRG_STAT_PRECHRGING	BIT(3)
#define BQ256XX_CHRG_STAT_FAST_CHRGING	BIT(4)
#define BQ256XX_CHRG_STAT_CHRG_TERM	(BIT(4) | BIT(3))

#define BQ256XX_PG_STAT_MASK		BIT(2)
#define BQ256XX_WDT_FAULT_MASK		BIT(7)
#define BQ256XX_CHRG_FAULT_MASK		GENMASK(5, 4)
#define BQ256XX_CHRG_FAULT_NORMAL	0
#define BQ256XX_CHRG_FAULT_INPUT	BIT(4)
#define BQ256XX_CHRG_FAULT_THERM	BIT(5)
#define BQ256XX_CHRG_FAULT_CST_EXPIRE	(BIT(5) | BIT(4))
#define BQ256XX_BAT_FAULT_MASK		BIT(3)
#define BQ256XX_NTC_FAULT_MASK		GENMASK(2, 0)
#define BQ256XX_NTC_FAULT_WARM		BIT(1)
#define BQ256XX_NTC_FAULT_COOL		(BIT(1) | BIT(0))
#define BQ256XX_NTC_FAULT_COLD		(BIT(2) | BIT(0))
#define BQ256XX_NTC_FAULT_HOT		(BIT(2) | BIT(1))

#define BQ256XX_NUM_WD_VAL	4
#define BQ256XX_WATCHDOG_MASK	GENMASK(5, 4)
#define BQ256XX_WATCHDOG_MAX	1600000
#define BQ256XX_WATCHDOG_DIS	0
#define BQ256XX_WDT_BIT_SHIFT	4

#define BQ256XX_REG_RST		BIT(7)

/* bq2560x/SGM41511 bits the generic code does not use (the vendor "ti,bq2560x_chg" node, see bq2560x_chg_*) */
#define BQ256XX_OTG_CONFIG		BIT(5)	/* CHARGER_CONTROL_0 */
#define BQ256XX_EN_TERM			BIT(7)	/* CHARGER_CONTROL_1 */
#define BQ256XX_IINDET_EN		BIT(7)	/* CHARGER_CONTROL_3: force input detection, clears when done */
#define BQ256XX_VBUS_STAT_NONSTD	(BIT(7) | BIT(6))
#define BQ2560X_CHG_ICHG_MAX_uA		1980000	/* what Android charges the U30 Air's 4050 mAh cell with */
#define BQ2560X_CHG_ICHG_3A_uA		3000000	/* u30air_3a: still bounded by the battery node and the chip */
#define BQ2560X_CHG_EN_TIMER		BIT(3)	/* REG05: stock driver disables the hardware safety timer */
#define BQ2560X_CHG_VBATREG_SPECIAL	0xf	/* REG04 VREG code 01111: 4.352 V, not 3.856 V + 15 x 32 mV */
#define BQ2560X_CHG_VBATREG_SPECIAL_uV	4352000
#define BQ256XX_EN_HIZ			BIT(7)	/* INPUT_CURRENT_LIMIT */
#define BQ2560X_CHG_PFM_DIS		BIT(7)	/* CHARGER_CONTROL_0 */
#define BQ2560X_CHG_PART_MASK		GENMASK(6, 3)	/* PART_INFORMATION */
#define BQ2560X_CHG_PART_BQ25601	2
#define BQ2560X_CHG_OVP_MASK		GENMASK(7, 6)	/* CHARGER_CONTROL_2: VAC over-voltage protection */
#define BQ2560X_CHG_OVP_6V5		1
#define BQ2560X_CHG_OVP_10V5		2

/*
 * The U30 Air at up to 3 A (kanoqwq's 627cacea): the fast-charge current from the battery node (its 3 A), not the
 * 1.98 A of an Android register snapshot, and a Type-C source's 3 A Rp taken as 3 A of input, not 2 A. Off by
 * default: his test read the registers back (ICHG 3 A) but did not measure 3 A into the battery or from a 3 A
 * source. Built in, so it is a kernel command-line option (bq256xx_charger.u30air_3a=1), read at probe and at
 * every input detection.
 */
static bool u30air_3a;
module_param(u30air_3a, bool, 0444);
MODULE_PARM_DESC(u30air_3a, "ZTE U30 Air: charge at up to the battery's 3 A and take a Type-C 3 A source at 3 A (default off)");

/*
 * The SGM41513 that ZTE's driver (sqc_bq2560x.c) looks for when nothing answers at the node's address: the same
 * registers at I2C 0x1a, with its own fast charge and termination current steps (sgm41513_*).
 */
#define SGM41513_I2C_ADDR		0x1a
#define SGM41513_REG_0D			0x0d
#define SGM41513_ITERM_X1		BIT(0)	/* REG0D: 1 the termination current as in the table, 0 six times it */

/**
 * struct bq256xx_init_data -
 * @ichg: fast charge current
 * @iindpm: input current limit
 * @vbatreg: charge voltage
 * @iterm: termination current
 * @iprechg: precharge current
 * @vindpm: input voltage limit
 * @ichg_max: maximum fast charge current
 * @vbatreg_max: maximum charge voltage
 * @ts_ignore: TS_IGNORE flag
 */
struct bq256xx_init_data {
	u32 ichg;
	u32 iindpm;
	u32 vbatreg;
	u32 iterm;
	u32 iprechg;
	u32 vindpm;
	u32 ichg_max;
	u32 vbatreg_max;
	bool ts_ignore;
};

/**
 * struct bq256xx_state -
 * @vbus_stat: VBUS status according to BQ256XX_CHARGER_STATUS_0
 * @chrg_stat: charging status according to BQ256XX_CHARGER_STATUS_0
 * @online: PG status according to BQ256XX_CHARGER_STATUS_0
 *
 * @wdt_fault: watchdog fault according to BQ256XX_CHARGER_STATUS_1
 * @bat_fault: battery fault according to BQ256XX_CHARGER_STATUS_1
 * @chrg_fault: charging fault according to BQ256XX_CHARGER_STATUS_1
 * @ntc_fault: TS fault according to BQ256XX_CHARGER_STATUS_1
 */
struct bq256xx_state {
	u8 vbus_stat;
	u8 chrg_stat;
	bool online;

	u8 wdt_fault;
	u8 bat_fault;
	u8 chrg_fault;
	u8 ntc_fault;
};

enum bq256xx_id {
	BQ25600,
	BQ25600D,
	BQ25601,
	BQ25601D,
	BQ25618,
	BQ25619,
	BQ25611D,
};

/**
 * struct bq256xx_device -
 * @client: i2c client structure
 * @regmap: register map structure
 * @dev: device structure
 * @charger: power supply registered for the charger
 * @battery: power supply registered for the battery
 * @lock: mutex lock structure
 *
 * @usb2_phy: usb_phy identifier
 * @usb3_phy: usb_phy identifier
 * @usb_nb: notifier block
 * @usb_work: usb work queue
 * @usb_event: usb_event code
 *
 * @model_name: i2c name string
 *
 * @init_data: initialization data
 * @chip_info: device variant information
 * @state: device status and faults
 * @watchdog_timer: watchdog timer value in milliseconds
 */
struct bq256xx_device {
	struct i2c_client *client;
	struct device *dev;
	struct power_supply *charger;
	struct power_supply *battery;
	struct mutex lock;
	struct regmap *regmap;

	struct usb_phy *usb2_phy;
	struct usb_phy *usb3_phy;
	struct notifier_block usb_nb;
	struct work_struct usb_work;
	unsigned long usb_event;

	char model_name[I2C_NAME_SIZE];

	struct bq256xx_init_data init_data;
	const struct bq256xx_chip_info *chip_info;
	struct bq256xx_state state;
	int watchdog_timer;

	/*
	 * The vendor device tree's "ti,bq2560x_chg" (ZTE U30 Air: an SGM41511, part 0010 = bq25601): no IRQ line, no
	 * input current in the DT, and the chip as Android left it. VBUS events and the charger type come from the USB
	 * PHY, which owns BC1.2 (bc12_phy, its charger notifier; bc12_ready once the chip is set up).
	 */
	bool bq2560x_chg;
	bool chg_off;
	/* a USB PD contract's input current is in force (the UMP9620's port controller, ump9620-tcpc) */
	bool pd_contract;
	int usb_type;
	struct device *pmic;
	struct usb_phy *bc12_phy;
	bool bc12_ready;
	struct notifier_block vbus_nb;
	struct delayed_work vbus_work;
	/* the W200DS's second charger and the policy over both chips (bq2560x_par_*); NULL elsewhere */
	struct bq2560x_par *par;
};

/**
 * struct bq256xx_chip_info -
 * @model_id: device instance
 *
 * @bq256xx_regmap_config: regmap configuration struct
 * @bq256xx_get_ichg: pointer to instance specific get_ichg function
 * @bq256xx_get_iindpm: pointer to instance specific get_iindpm function
 * @bq256xx_get_vbatreg: pointer to instance specific get_vbatreg function
 * @bq256xx_get_iterm: pointer to instance specific get_iterm function
 * @bq256xx_get_iprechg: pointer to instance specific get_iprechg function
 * @bq256xx_get_vindpm: pointer to instance specific get_vindpm function
 *
 * @bq256xx_set_ichg: pointer to instance specific set_ichg function
 * @bq256xx_set_iindpm: pointer to instance specific set_iindpm function
 * @bq256xx_set_vbatreg: pointer to instance specific set_vbatreg function
 * @bq256xx_set_iterm: pointer to instance specific set_iterm function
 * @bq256xx_set_iprechg: pointer to instance specific set_iprechg function
 * @bq256xx_set_vindpm: pointer to instance specific set_vindpm function
 * @bq256xx_set_charge_type: pointer to instance specific set_charge_type function
 * @bq256xx_set_ts_ignore: pointer to instance specific set_ts_ignore function
 *
 * @bq256xx_def_ichg: default ichg value in microamps
 * @bq256xx_def_iindpm: default iindpm value in microamps
 * @bq256xx_def_vbatreg: default vbatreg value in microvolts
 * @bq256xx_def_iterm: default iterm value in microamps
 * @bq256xx_def_iprechg: default iprechg value in microamps
 * @bq256xx_def_vindpm: default vindpm value in microvolts
 *
 * @bq256xx_max_ichg: maximum charge current in microamps
 * @bq256xx_max_vbatreg: maximum battery regulation voltage in microvolts
 *
 * @has_usb_detect: indicates whether device has BC1.2 detection
 */
struct bq256xx_chip_info {
	int model_id;

	const struct regmap_config *bq256xx_regmap_config;

	int (*bq256xx_get_ichg)(struct bq256xx_device *bq);
	int (*bq256xx_get_iindpm)(struct bq256xx_device *bq);
	int (*bq256xx_get_vbatreg)(struct bq256xx_device *bq);
	int (*bq256xx_get_iterm)(struct bq256xx_device *bq);
	int (*bq256xx_get_iprechg)(struct bq256xx_device *bq);
	int (*bq256xx_get_vindpm)(struct bq256xx_device *bq);

	int (*bq256xx_set_ichg)(struct bq256xx_device *bq, int ichg);
	int (*bq256xx_set_iindpm)(struct bq256xx_device *bq, int iindpm);
	int (*bq256xx_set_vbatreg)(struct bq256xx_device *bq, int vbatreg);
	int (*bq256xx_set_iterm)(struct bq256xx_device *bq, int iterm);
	int (*bq256xx_set_iprechg)(struct bq256xx_device *bq, int iprechg);
	int (*bq256xx_set_vindpm)(struct bq256xx_device *bq, int vindpm);
	int (*bq256xx_set_charge_type)(struct bq256xx_device *bq, int type);
	int (*bq256xx_set_ts_ignore)(struct bq256xx_device *bq, bool ts_ignore);

	int bq256xx_def_ichg;
	int bq256xx_def_iindpm;
	int bq256xx_def_vbatreg;
	int bq256xx_def_iterm;
	int bq256xx_def_iprechg;
	int bq256xx_def_vindpm;

	int bq256xx_max_ichg;
	int bq256xx_max_vbatreg;

	bool has_usb_detect;
};

static int bq256xx_watchdog_time[BQ256XX_NUM_WD_VAL] = {
	0, 40000, 80000, 1600000
};

static const int bq25611d_vbatreg_values[] = {
	3494000, 3590000, 3686000, 3790000, 3894000, 3990000, 4090000, 4140000,
	4190000
};

static const int bq25618_619_vbatreg_values[] = {
	3504000, 3600000, 3696000, 3800000, 3904000, 4000000, 4100000, 4150000,
	4200000
};

static const int bq25618_619_ichg_values[] = {
	1290000, 1360000, 1430000, 1500000
};

static int bq256xx_array_parse(int array_size, int val, const int array[])
{
	int i = 0;

	if (val < array[i])
		return i - 1;

	if (val >= array[array_size - 1])
		return array_size - 1;

	for (i = 1; i < array_size; i++) {
		if (val == array[i])
			return i;

		if (val > array[i - 1] && val < array[i]) {
			if (val < array[i])
				return i - 1;
			else
				return i;
		}
	}
	return -EINVAL;
}

static int bq256xx_usb_notifier(struct notifier_block *nb, unsigned long val,
				void *priv)
{
	struct bq256xx_device *bq =
			container_of(nb, struct bq256xx_device, usb_nb);

	bq->usb_event = val;
	queue_work(system_power_efficient_wq, &bq->usb_work);

	return NOTIFY_OK;
}

static void bq256xx_usb_work(struct work_struct *data)
{
	struct bq256xx_device *bq =
			container_of(data, struct bq256xx_device, usb_work);

	switch (bq->usb_event) {
	case USB_EVENT_ID:
		break;
	case USB_EVENT_NONE:
		power_supply_changed(bq->charger);
		break;
	default:
		dev_err(bq->dev, "Error switching to charger mode.\n");
		break;
	}
}

static const struct reg_default bq2560x_reg_defs[] = {
	{BQ256XX_INPUT_CURRENT_LIMIT, 0x17},
	{BQ256XX_CHARGER_CONTROL_0, 0x1a},
	{BQ256XX_CHARGE_CURRENT_LIMIT, 0xa2},
	{BQ256XX_PRECHG_AND_TERM_CURR_LIM, 0x22},
	{BQ256XX_BATTERY_VOLTAGE_LIMIT, 0x58},
	{BQ256XX_CHARGER_CONTROL_1, 0x9f},
	{BQ256XX_CHARGER_CONTROL_2, 0x66},
	{BQ256XX_CHARGER_CONTROL_3, 0x4c},
};

static const struct reg_default bq25611d_reg_defs[] = {
	{BQ256XX_INPUT_CURRENT_LIMIT, 0x17},
	{BQ256XX_CHARGER_CONTROL_0, 0x1a},
	{BQ256XX_CHARGE_CURRENT_LIMIT, 0x91},
	{BQ256XX_PRECHG_AND_TERM_CURR_LIM, 0x12},
	{BQ256XX_BATTERY_VOLTAGE_LIMIT, 0x40},
	{BQ256XX_CHARGER_CONTROL_1, 0x9e},
	{BQ256XX_CHARGER_CONTROL_2, 0xe6},
	{BQ256XX_CHARGER_CONTROL_3, 0x4c},
	{BQ256XX_PART_INFORMATION, 0x54},
	{BQ256XX_CHARGER_CONTROL_4, 0x75},
};

static const struct reg_default bq25618_619_reg_defs[] = {
	{BQ256XX_INPUT_CURRENT_LIMIT, 0x17},
	{BQ256XX_CHARGER_CONTROL_0, 0x1a},
	{BQ256XX_CHARGE_CURRENT_LIMIT, 0x91},
	{BQ256XX_PRECHG_AND_TERM_CURR_LIM, 0x12},
	{BQ256XX_BATTERY_VOLTAGE_LIMIT, 0x40},
	{BQ256XX_CHARGER_CONTROL_1, 0x9e},
	{BQ256XX_CHARGER_CONTROL_2, 0xe6},
	{BQ256XX_CHARGER_CONTROL_3, 0x4c},
	{BQ256XX_PART_INFORMATION, 0x2c},
	{BQ256XX_CHARGER_CONTROL_4, 0x75},
};

static int bq256xx_get_state(struct bq256xx_device *bq,
				struct bq256xx_state *state)
{
	unsigned int charger_status_0;
	unsigned int charger_status_1;
	int ret;

	ret = regmap_read(bq->regmap, BQ256XX_CHARGER_STATUS_0,
						&charger_status_0);
	if (ret)
		return ret;

	ret = regmap_read(bq->regmap, BQ256XX_CHARGER_STATUS_1,
						&charger_status_1);
	if (ret)
		return ret;

	state->vbus_stat = charger_status_0 & BQ256XX_VBUS_STAT_MASK;
	state->chrg_stat = charger_status_0 & BQ256XX_CHRG_STAT_MASK;
	state->online = charger_status_0 & BQ256XX_PG_STAT_MASK;

	state->wdt_fault = charger_status_1 & BQ256XX_WDT_FAULT_MASK;
	state->bat_fault = charger_status_1 & BQ256XX_BAT_FAULT_MASK;
	state->chrg_fault = charger_status_1 & BQ256XX_CHRG_FAULT_MASK;
	state->ntc_fault = charger_status_1 & BQ256XX_NTC_FAULT_MASK;

	return 0;
}

static int bq256xx_set_charge_type(struct bq256xx_device *bq, int type)
{
	int chg_config = 0;

	switch (type) {
	case POWER_SUPPLY_CHARGE_TYPE_NONE:
		chg_config = 0x0;
		break;
	case POWER_SUPPLY_CHARGE_TYPE_TRICKLE:
	case POWER_SUPPLY_CHARGE_TYPE_FAST:
		chg_config = 0x1;
		break;
	default:
		return -EINVAL;
	}

	return regmap_update_bits(bq->regmap, BQ256XX_CHARGER_CONTROL_0,
				BQ256XX_CHG_CONFIG_MASK,
				(chg_config ? 1 : 0) << BQ256XX_CHG_CONFIG_BIT_SHIFT);
}

static int bq256xx_get_ichg_curr(struct bq256xx_device *bq)
{
	unsigned int charge_current_limit;
	unsigned int ichg_reg_code;
	int ret;

	ret = regmap_read(bq->regmap, BQ256XX_CHARGE_CURRENT_LIMIT,
						&charge_current_limit);
	if (ret)
		return ret;

	ichg_reg_code = charge_current_limit & BQ256XX_ICHG_MASK;

	return ichg_reg_code * BQ256XX_ICHG_STEP_uA;
}

static int bq25618_619_get_ichg_curr(struct bq256xx_device *bq)
{
	unsigned int charge_current_limit;
	unsigned int ichg_reg_code;
	int ret;

	ret = regmap_read(bq->regmap, BQ256XX_CHARGE_CURRENT_LIMIT,
						&charge_current_limit);
	if (ret)
		return ret;

	ichg_reg_code = charge_current_limit & BQ256XX_ICHG_MASK;

	if (ichg_reg_code < BQ25618_ICHG_THRESH)
		return ichg_reg_code * BQ25618_ICHG_STEP_uA;

	return bq25618_619_ichg_values[ichg_reg_code - BQ25618_ICHG_THRESH];
}

static int bq256xx_set_ichg_curr(struct bq256xx_device *bq, int ichg)
{
	unsigned int ichg_reg_code;
	int ichg_max = bq->init_data.ichg_max;

	ichg = clamp(ichg, BQ256XX_ICHG_MIN_uA, ichg_max);
	ichg_reg_code = ichg / BQ256XX_ICHG_STEP_uA;

	return regmap_update_bits(bq->regmap, BQ256XX_CHARGE_CURRENT_LIMIT,
					BQ256XX_ICHG_MASK, ichg_reg_code);
}

static int bq25618_619_set_ichg_curr(struct bq256xx_device *bq, int ichg)
{
	int array_size = ARRAY_SIZE(bq25618_619_ichg_values);
	unsigned int ichg_reg_code;
	int ichg_max = bq->init_data.ichg_max;

	ichg = clamp(ichg, BQ25618_ICHG_MIN_uA, ichg_max);

	if (ichg <= BQ25618_ICHG_THRESH_uA) {
		ichg_reg_code = ichg / BQ25618_ICHG_STEP_uA;
	} else {
		ichg_reg_code = bq256xx_array_parse(array_size, ichg,
			bq25618_619_ichg_values) + BQ25618_ICHG_THRESH;
	}

	return regmap_update_bits(bq->regmap, BQ256XX_CHARGE_CURRENT_LIMIT,
					BQ256XX_ICHG_MASK, ichg_reg_code);
}

static int bq25618_619_get_chrg_volt(struct bq256xx_device *bq)
{
	unsigned int battery_volt_lim;
	unsigned int vbatreg_reg_code;
	int ret;

	ret = regmap_read(bq->regmap, BQ256XX_BATTERY_VOLTAGE_LIMIT,
							&battery_volt_lim);

	if (ret)
		return ret;

	vbatreg_reg_code = (battery_volt_lim & BQ256XX_VBATREG_MASK) >>
						BQ256XX_VBATREG_BIT_SHIFT;

	if (vbatreg_reg_code > BQ2561X_VBATREG_THRESH)
		return ((vbatreg_reg_code - BQ2561X_VBATREG_THRESH) *
					BQ2561X_VBATREG_STEP_uV) +
					BQ25618_VBATREG_THRESH_uV;

	return bq25618_619_vbatreg_values[vbatreg_reg_code];
}

static int bq25611d_get_chrg_volt(struct bq256xx_device *bq)
{
	unsigned int battery_volt_lim;
	unsigned int vbatreg_reg_code;
	int ret;

	ret = regmap_read(bq->regmap, BQ256XX_BATTERY_VOLTAGE_LIMIT,
							&battery_volt_lim);
	if (ret)
		return ret;

	vbatreg_reg_code = (battery_volt_lim & BQ256XX_VBATREG_MASK) >>
						BQ256XX_VBATREG_BIT_SHIFT;

	if (vbatreg_reg_code > BQ2561X_VBATREG_THRESH)
		return ((vbatreg_reg_code - BQ2561X_VBATREG_THRESH) *
					BQ2561X_VBATREG_STEP_uV) +
					BQ25611D_VBATREG_THRESH_uV;

	return bq25611d_vbatreg_values[vbatreg_reg_code];
}

static int bq2560x_get_chrg_volt(struct bq256xx_device *bq)
{
	unsigned int battery_volt_lim;
	unsigned int vbatreg_reg_code;
	int ret;

	ret = regmap_read(bq->regmap, BQ256XX_BATTERY_VOLTAGE_LIMIT,
							&battery_volt_lim);
	if (ret)
		return ret;

	vbatreg_reg_code = (battery_volt_lim & BQ256XX_VBATREG_MASK) >>
						BQ256XX_VBATREG_BIT_SHIFT;

	/* "ti,bq2560x_chg": code 0xf is the chip's special 4.352 V, as the stock driver reads it */
	if (bq->bq2560x_chg && vbatreg_reg_code == BQ2560X_CHG_VBATREG_SPECIAL)
		return BQ2560X_CHG_VBATREG_SPECIAL_uV;
	return (vbatreg_reg_code * BQ2560X_VBATREG_STEP_uV)
					+ BQ2560X_VBATREG_OFFSET_uV;
}

static int bq25601d_get_chrg_volt(struct bq256xx_device *bq)
{
	unsigned int battery_volt_lim;
	unsigned int vbatreg_reg_code;
	int ret;

	ret = regmap_read(bq->regmap, BQ256XX_BATTERY_VOLTAGE_LIMIT,
							&battery_volt_lim);
	if (ret)
		return ret;

	vbatreg_reg_code = (battery_volt_lim & BQ256XX_VBATREG_MASK) >>
						BQ256XX_VBATREG_BIT_SHIFT;

	return (vbatreg_reg_code * BQ2560X_VBATREG_STEP_uV)
					+ BQ25601D_VBATREG_OFFSET_uV;
}

static int bq25618_619_set_chrg_volt(struct bq256xx_device *bq, int vbatreg)
{
	int array_size = ARRAY_SIZE(bq25618_619_vbatreg_values);
	unsigned int vbatreg_reg_code;
	int vbatreg_max = bq->init_data.vbatreg_max;

	vbatreg = clamp(vbatreg, BQ25618_VBATREG_MIN_uV, vbatreg_max);

	if (vbatreg > BQ25618_VBATREG_THRESH_uV)
		vbatreg_reg_code = ((vbatreg -
		BQ25618_VBATREG_THRESH_uV) /
		(BQ2561X_VBATREG_STEP_uV)) + BQ2561X_VBATREG_THRESH;
	else {
		vbatreg_reg_code = bq256xx_array_parse(array_size, vbatreg,
						bq25618_619_vbatreg_values);
	}

	return regmap_update_bits(bq->regmap, BQ256XX_BATTERY_VOLTAGE_LIMIT,
				BQ256XX_VBATREG_MASK, vbatreg_reg_code <<
						BQ256XX_VBATREG_BIT_SHIFT);
}

static int bq25611d_set_chrg_volt(struct bq256xx_device *bq, int vbatreg)
{
	int array_size = ARRAY_SIZE(bq25611d_vbatreg_values);
	unsigned int vbatreg_reg_code;
	int vbatreg_max = bq->init_data.vbatreg_max;

	vbatreg = clamp(vbatreg, BQ25611D_VBATREG_MIN_uV, vbatreg_max);

	if (vbatreg > BQ25611D_VBATREG_THRESH_uV)
		vbatreg_reg_code = ((vbatreg -
		BQ25611D_VBATREG_THRESH_uV) /
		(BQ2561X_VBATREG_STEP_uV)) + BQ2561X_VBATREG_THRESH;
	else {
		vbatreg_reg_code = bq256xx_array_parse(array_size, vbatreg,
						bq25611d_vbatreg_values);
	}

	return regmap_update_bits(bq->regmap, BQ256XX_BATTERY_VOLTAGE_LIMIT,
				BQ256XX_VBATREG_MASK, vbatreg_reg_code <<
						BQ256XX_VBATREG_BIT_SHIFT);
}

static int bq2560x_set_chrg_volt(struct bq256xx_device *bq, int vbatreg)
{
	unsigned int vbatreg_reg_code;
	int vbatreg_max = bq->init_data.vbatreg_max;

	vbatreg = clamp(vbatreg, BQ2560X_VBATREG_MIN_uV, vbatreg_max);

	vbatreg_reg_code = (vbatreg - BQ2560X_VBATREG_OFFSET_uV) /
						BQ2560X_VBATREG_STEP_uV;

	return regmap_update_bits(bq->regmap, BQ256XX_BATTERY_VOLTAGE_LIMIT,
				BQ256XX_VBATREG_MASK, vbatreg_reg_code <<
						BQ256XX_VBATREG_BIT_SHIFT);
}

static int bq25601d_set_chrg_volt(struct bq256xx_device *bq, int vbatreg)
{
	unsigned int vbatreg_reg_code;
	int vbatreg_max = bq->init_data.vbatreg_max;

	vbatreg = clamp(vbatreg, BQ25601D_VBATREG_MIN_uV, vbatreg_max);

	vbatreg_reg_code = (vbatreg - BQ25601D_VBATREG_OFFSET_uV) /
						BQ2560X_VBATREG_STEP_uV;

	return regmap_update_bits(bq->regmap, BQ256XX_BATTERY_VOLTAGE_LIMIT,
				BQ256XX_VBATREG_MASK, vbatreg_reg_code <<
						BQ256XX_VBATREG_BIT_SHIFT);
}

static int bq256xx_set_ts_ignore(struct bq256xx_device *bq, bool ts_ignore)
{
	return regmap_update_bits(bq->regmap, BQ256XX_INPUT_CURRENT_LIMIT,
				BQ256XX_TS_IGNORE, (ts_ignore ? 1 : 0) << BQ256XX_TS_IGNORE_SHIFT);
}

static int bq256xx_get_prechrg_curr(struct bq256xx_device *bq)
{
	unsigned int prechg_and_term_curr_lim;
	unsigned int iprechg_reg_code;
	int ret;

	ret = regmap_read(bq->regmap, BQ256XX_PRECHG_AND_TERM_CURR_LIM,
						&prechg_and_term_curr_lim);
	if (ret)
		return ret;

	iprechg_reg_code = (prechg_and_term_curr_lim & BQ256XX_IPRECHG_MASK)
						>> BQ256XX_IPRECHG_BIT_SHIFT;

	return (iprechg_reg_code * BQ256XX_IPRECHG_STEP_uA) +
						BQ256XX_IPRECHG_OFFSET_uA;
}

static int bq256xx_set_prechrg_curr(struct bq256xx_device *bq, int iprechg)
{
	unsigned int iprechg_reg_code;

	iprechg = clamp(iprechg, BQ256XX_IPRECHG_MIN_uA,
						BQ256XX_IPRECHG_MAX_uA);

	iprechg_reg_code = ((iprechg - BQ256XX_IPRECHG_OFFSET_uA) /
			BQ256XX_IPRECHG_STEP_uA) << BQ256XX_IPRECHG_BIT_SHIFT;

	return regmap_update_bits(bq->regmap, BQ256XX_PRECHG_AND_TERM_CURR_LIM,
				BQ256XX_IPRECHG_MASK, iprechg_reg_code);
}

static int bq25618_619_get_prechrg_curr(struct bq256xx_device *bq)
{
	unsigned int prechg_and_term_curr_lim;
	unsigned int iprechg_reg_code;
	int ret;

	ret = regmap_read(bq->regmap, BQ256XX_PRECHG_AND_TERM_CURR_LIM,
						&prechg_and_term_curr_lim);
	if (ret)
		return ret;

	iprechg_reg_code = (prechg_and_term_curr_lim & BQ256XX_IPRECHG_MASK)
						>> BQ256XX_IPRECHG_BIT_SHIFT;

	return (iprechg_reg_code * BQ25618_IPRECHG_STEP_uA) +
						BQ25618_IPRECHG_OFFSET_uA;
}

static int bq25618_619_set_prechrg_curr(struct bq256xx_device *bq, int iprechg)
{
	unsigned int iprechg_reg_code;

	iprechg = clamp(iprechg, BQ25618_IPRECHG_MIN_uA,
						BQ25618_IPRECHG_MAX_uA);

	iprechg_reg_code = ((iprechg - BQ25618_IPRECHG_OFFSET_uA) /
			BQ25618_IPRECHG_STEP_uA) << BQ256XX_IPRECHG_BIT_SHIFT;

	return regmap_update_bits(bq->regmap, BQ256XX_PRECHG_AND_TERM_CURR_LIM,
				BQ256XX_IPRECHG_MASK, iprechg_reg_code);
}

static int bq256xx_get_term_curr(struct bq256xx_device *bq)
{
	unsigned int prechg_and_term_curr_lim;
	unsigned int iterm_reg_code;
	int ret;

	ret = regmap_read(bq->regmap, BQ256XX_PRECHG_AND_TERM_CURR_LIM,
						&prechg_and_term_curr_lim);
	if (ret)
		return ret;

	iterm_reg_code = prechg_and_term_curr_lim & BQ256XX_ITERM_MASK;

	return (iterm_reg_code * BQ256XX_ITERM_STEP_uA) +
						BQ256XX_ITERM_OFFSET_uA;
}

static int bq256xx_set_term_curr(struct bq256xx_device *bq, int iterm)
{
	unsigned int iterm_reg_code;

	iterm = clamp(iterm, BQ256XX_ITERM_MIN_uA, BQ256XX_ITERM_MAX_uA);

	iterm_reg_code = (iterm - BQ256XX_ITERM_OFFSET_uA) /
							BQ256XX_ITERM_STEP_uA;

	return regmap_update_bits(bq->regmap, BQ256XX_PRECHG_AND_TERM_CURR_LIM,
				BQ256XX_ITERM_MASK, iterm_reg_code);
}

static int bq25618_619_get_term_curr(struct bq256xx_device *bq)
{
	unsigned int prechg_and_term_curr_lim;
	unsigned int iterm_reg_code;
	int ret;

	ret = regmap_read(bq->regmap, BQ256XX_PRECHG_AND_TERM_CURR_LIM,
						&prechg_and_term_curr_lim);
	if (ret)
		return ret;

	iterm_reg_code = prechg_and_term_curr_lim & BQ256XX_ITERM_MASK;

	return (iterm_reg_code * BQ25618_ITERM_STEP_uA) +
						BQ25618_ITERM_OFFSET_uA;
}

static int bq25618_619_set_term_curr(struct bq256xx_device *bq, int iterm)
{
	unsigned int iterm_reg_code;

	iterm = clamp(iterm, BQ25618_ITERM_MIN_uA, BQ25618_ITERM_MAX_uA);

	iterm_reg_code = (iterm - BQ25618_ITERM_OFFSET_uA) /
							BQ25618_ITERM_STEP_uA;

	return regmap_update_bits(bq->regmap, BQ256XX_PRECHG_AND_TERM_CURR_LIM,
				BQ256XX_ITERM_MASK, iterm_reg_code);
}

static int bq256xx_get_input_volt_lim(struct bq256xx_device *bq)
{
	unsigned int charger_control_2;
	unsigned int vindpm_reg_code;
	int ret;

	ret = regmap_read(bq->regmap, BQ256XX_CHARGER_CONTROL_2,
						&charger_control_2);
	if (ret)
		return ret;

	vindpm_reg_code = charger_control_2 & BQ256XX_VINDPM_MASK;

	return (vindpm_reg_code * BQ256XX_VINDPM_STEP_uV) +
						BQ256XX_VINDPM_OFFSET_uV;
}

static int bq256xx_set_input_volt_lim(struct bq256xx_device *bq, int vindpm)
{
	unsigned int vindpm_reg_code;

	vindpm = clamp(vindpm, BQ256XX_VINDPM_MIN_uV, BQ256XX_VINDPM_MAX_uV);

	vindpm_reg_code = (vindpm - BQ256XX_VINDPM_OFFSET_uV) /
						BQ256XX_VINDPM_STEP_uV;

	return regmap_update_bits(bq->regmap, BQ256XX_CHARGER_CONTROL_2,
					BQ256XX_VINDPM_MASK, vindpm_reg_code);
}

static int bq256xx_get_input_curr_lim(struct bq256xx_device *bq)
{
	unsigned int input_current_limit;
	unsigned int iindpm_reg_code;
	int ret;

	ret = regmap_read(bq->regmap, BQ256XX_INPUT_CURRENT_LIMIT,
						&input_current_limit);
	if (ret)
		return ret;

	iindpm_reg_code = input_current_limit & BQ256XX_IINDPM_MASK;

	return (iindpm_reg_code * BQ256XX_IINDPM_STEP_uA) +
						BQ256XX_IINDPM_OFFSET_uA;
}

static int bq256xx_set_input_curr_lim(struct bq256xx_device *bq, int iindpm)
{
	unsigned int iindpm_reg_code;

	iindpm = clamp(iindpm, BQ256XX_IINDPM_MIN_uA, BQ256XX_IINDPM_MAX_uA);

	iindpm_reg_code = (iindpm - BQ256XX_IINDPM_OFFSET_uA) /
							BQ256XX_IINDPM_STEP_uA;

	return regmap_update_bits(bq->regmap, BQ256XX_INPUT_CURRENT_LIMIT,
					BQ256XX_IINDPM_MASK, iindpm_reg_code);
}

/*
 * The CV the stock driver programs for the battery's declared one (sqc_bq2560x.ko sqc_mp_set_cv, and the matching
 * sqc_v8 source): register code floor((uV - 3.856 V) / 32 mV) + 1, one step above the generic round-down, at most the
 * chip's maximum. A 4.3 V battery is code 0xe, REG04 0x70, 4.304 V; stock Android reads 0x70 on the same unit.
 * Returned in uV so that the generic path (bq2560x_set_chrg_volt, which rounds down) writes exactly that code.
 */
static int bq2560x_chg_stock_cv(int requested, int maximum)
{
	unsigned int code, max_code;

	if (requested < BQ2560X_VBATREG_MIN_uV)
		return BQ2560X_VBATREG_MIN_uV;
	max_code = (maximum - BQ2560X_VBATREG_OFFSET_uV) / BQ2560X_VBATREG_STEP_uV;
	code = min_t(unsigned int, (requested - BQ2560X_VBATREG_OFFSET_uV) / BQ2560X_VBATREG_STEP_uV + 1, max_code);
	if (code == BQ2560X_CHG_VBATREG_SPECIAL)
		return BQ2560X_CHG_VBATREG_SPECIAL_uV;
	return BQ2560X_VBATREG_OFFSET_uV + code * BQ2560X_VBATREG_STEP_uV;
}

/*
 * The W200DS (ZTE P720P01) charges with two bq25601 in parallel, as ZTE's firmware does: the "ti,bq2560x_chg2" node
 * (I2C2 0x6b, "role-slave") is a second chip on the same VBUS and battery that charges only while its enable GPIO is
 * high (GPIO 187; measured: high with CHG_CONFIG on adds 0.54 A at 5 V 0.5 A, low takes it away whatever CHG_CONFIG
 * says). ZTE's policy for it is in a userspace daemon that is not available; this one keeps to what the stock kernel
 * and device tree say (sqc_bq2560x.c, charger-manager-sqc-v8.c, zte_charger_papaya.dtsi):
 *  - the pair only with a USB PD contract above 5 V (ump9620-tcpc asks a charger for 9 V, 18 W at most: ZTE's cap),
 *    from an open-circuit voltage of 3.52 V and under 4.1 V (or with 1.2 A going into the battery anyway), with the
 *    temperature table's current at 1.2 A or more; the second chip off again after two samples in a row under 3.4 V
 *    or under 1 A into the battery (the end of constant current: the first chip finishes alone and terminates), then
 *    on again only for an open-circuit voltage under 4.1 V;
 *  - each chip half the battery's current and half the contract's input current, at most the device tree's per-chip
 *    fast charge pair (charge-fchg-current-microamp: 2.65 A charge, 1.25 A input); the second chip's CV one step
 *    (32 mV) under the first's and its termination off (as ZTE sets it), in HIZ with charging off when not charging;
 *  - the battery's temperature table (fchg-jeita-temp-table: ZTE's six are the same) whether one chip charges or two:
 *    the current of both together and the CV by temperature, no charging at 0 degC and under or from 55 degC, with
 *    ZTE's recovery temperatures and two samples in a row before a change. One chip alone takes at most its share.
 * The battery (sc27xx-fgu) is sampled every 10 s while an outside supply is on VBUS. In system suspend the second
 * chip is off and nothing is sampled: the first one charges on as the table last said.
 * Kernel-only writes of the port controller (ump9620-tcpc), besides usb_type and input_current_limit: voltage_max,
 * how high VBUS may go (the chips' VAC over-voltage protection above it), and voltage_now, the contract's voltage.
 */
#define BQ2560X_PAR_POLL_MS		10000
#define BQ2560X_PAR_ROWS		8
#define BQ2560X_PAR_MIN_uV		5500000	/* a contract above this: the pair */
#define BQ2560X_PAR_OCV_MIN_uV		3520000
#define BQ2560X_PAR_OCV_HIGH_uV		4100000
#define BQ2560X_PAR_ICHG_MIN_uA		1200000	/* the table's current, and into the battery, for the pair */
#define BQ2560X_PAR_VBAT_EXIT_uV	3400000
#define BQ2560X_PAR_IBAT_EXIT_uA	1000000
#define BQ2560X_OVP_MAX_uV		10500000

struct bq2560x_jeita {
	int temp, recovery;	/* deci-degC */
	int ichg, cv;		/* uA (both chips together), uV */
};

struct bq2560x_par {
	struct bq256xx_device *bq;
	struct bq256xx_device *slave;	/* the second chip, for the generic register helpers */
	struct gpio_desc *enable;
	struct power_supply *fgu;	/* only the work uses it */
	struct delayed_work work;
	struct bq2560x_jeita rows[BQ2560X_PAR_ROWS];
	int nrows;
	int chip_ichg, chip_iindpm;	/* the most one chip takes */
	int bat_ichg;			/* the battery's maximum */
	int user_ichg, user_cv;		/* constant_charge_current (both chips) and _voltage written from userspace */
	/* from the port controller */
	int ovp_uv, contract_uv, pd_ua;
	/* the last sample */
	bool online, sampled;
	int temp, vbat, ocv, ibat;
	/* the table's status: -1 not known yet, 0 and nrows no charging, else rows[status] */
	int status, next, count;
	bool want, done, suspended, on;
	int exit_count;
};

/* REG06 VAC_OVP for a VBUS of up to uv: 6.5 V for 5 V (ZTE's normal setting), 10.5 V for 9 V (its fast charge) */
static unsigned int bq2560x_chg_ovp(int uv)
{
	return FIELD_PREP(BQ2560X_CHG_OVP_MASK, uv > BQ2560X_PAR_MIN_uV ? BQ2560X_CHG_OVP_10V5 : BQ2560X_CHG_OVP_6V5);
}

static bool bq2560x_par_stop(const struct bq2560x_par *p)
{
	return p->status == 0 || p->status == p->nrows;
}

/* no charging: userspace said so (charge_type N/A), or the battery's temperature */
static bool bq2560x_chg_stopped(struct bq256xx_device *bq)
{
	return READ_ONCE(bq->chg_off) || (bq->par && bq2560x_par_stop(bq->par));
}

/* both chips together: the table's current at this temperature (one chip's before the first sample) and the caps */
static int bq2560x_par_ichg(const struct bq2560x_par *p)
{
	int ichg = p->status > 0 && p->status < p->nrows ? p->rows[p->status].ichg : p->chip_ichg;

	return min3(ichg, p->bat_ichg, p->user_ichg);
}

/*
 * The table's status at temperature t after prev, as charger-manager-sqc-v8.c: past the first row's temperature 1,
 * at or past a later one its number plus one, 0 under them all. A status is left upwards only from its row's
 * recovery temperature where that is above the row's own (0 to 3 degC, 15 to 18), downwards only from the recovery
 * of the row under it where that is below (55 to 52, 45 to 42).
 */
static int bq2560x_par_eval(const struct bq2560x_par *p, int t, int prev)
{
	const struct bq2560x_jeita *r;
	int i, s = 0;

	for (i = 0; i < p->nrows; i++)
		if (i ? t >= p->rows[i].temp : t > p->rows[i].temp)
			s = i + 1;
	if (prev < 0 || s == prev)
		return s;
	if (s > prev) {
		r = &p->rows[prev];
		if (r->recovery > r->temp && t < r->recovery)
			return prev;
	} else {
		r = &p->rows[prev - 1];
		if (r->recovery < r->temp && t > r->recovery)
			return prev;
	}
	return s;
}

/* the second chip off: its enable first, then charging off and HIZ (ZTE's order) */
static void bq2560x_par_slave_off(struct bq256xx_device *bq)
{
	struct bq256xx_device *s = bq->par->slave;

	gpiod_set_value_cansleep(bq->par->enable, 0);
	regmap_update_bits(s->regmap, BQ256XX_CHARGER_CONTROL_0, BQ256XX_CHG_CONFIG_MASK, 0);
	regmap_update_bits(s->regmap, BQ256XX_INPUT_CURRENT_LIMIT, BQ256XX_EN_HIZ, BQ256XX_EN_HIZ);
}

static int bq2560x_par_slave_on(struct bq256xx_device *bq, int ichg, int cv, int iindpm)
{
	struct bq256xx_device *s = bq->par->slave;
	int ret;

	ret = regmap_update_bits(s->regmap, BQ256XX_CHARGER_CONTROL_2, BQ2560X_CHG_OVP_MASK,
				 bq2560x_chg_ovp(bq->par->ovp_uv));
	if (!ret)
		ret = s->chip_info->bq256xx_set_vbatreg(s, cv);
	if (!ret)
		ret = s->chip_info->bq256xx_set_ichg(s, ichg);
	if (!ret)
		ret = s->chip_info->bq256xx_set_iindpm(s, iindpm);
	if (!ret)
		ret = regmap_update_bits(s->regmap, BQ256XX_INPUT_CURRENT_LIMIT, BQ256XX_EN_HIZ, 0);
	if (!ret)
		ret = regmap_update_bits(s->regmap, BQ256XX_CHARGER_CONTROL_0, BQ256XX_CHG_CONFIG_MASK,
					 BQ256XX_CHG_CONFIG_MASK);
	if (!ret)
		gpiod_set_value_cansleep(bq->par->enable, 1);
	return ret;
}

/*
 * Both chips as the state says (under bq->lock): the second one off first, or on last, so that the input current of
 * the two never adds up to more than the contract's.
 */
static void bq2560x_par_apply(struct bq256xx_device *bq)
{
	struct bq2560x_par *p = bq->par;
	int ichg = bq2560x_par_ichg(p), cv = bq->init_data.vbatreg_max, iindpm = 0, ret;
	unsigned int ctrl0;
	bool stop = bq2560x_chg_stopped(bq), otg, pair;

	ret = regmap_read(bq->regmap, BQ256XX_CHARGER_CONTROL_0, &ctrl0);
	otg = !ret && (ctrl0 & BQ256XX_OTG_CONFIG);
	if (p->status > 0 && p->status < p->nrows)
		cv = min(cv, bq2560x_chg_stock_cv(p->rows[p->status].cv, bq->chip_info->bq256xx_max_vbatreg));
	cv = min(cv, p->user_cv);
	pair = !ret && !otg && !stop && p->online && !p->suspended && p->want && bq->pd_contract &&
	       p->contract_uv > BQ2560X_PAR_MIN_uV && p->pd_ua > 0;
	if (pair)
		iindpm = min(p->pd_ua / 2, p->chip_iindpm);
	else
		bq2560x_par_slave_off(bq);

	if (!ret && !otg)
		ret = regmap_update_bits(bq->regmap, BQ256XX_CHARGER_CONTROL_0, BQ256XX_CHG_CONFIG_MASK,
					 stop ? 0 : BQ256XX_CHG_CONFIG_MASK);
	if (!ret && !stop)
		ret = bq->chip_info->bq256xx_set_ichg(bq, pair ? ichg / 2 : ichg);
	if (!ret && !stop)
		ret = bq->chip_info->bq256xx_set_vbatreg(bq, cv);
	if (!ret && bq->pd_contract && p->pd_ua > 0)
		ret = bq->chip_info->bq256xx_set_iindpm(bq, pair ? iindpm : p->pd_ua);
	if (ret)
		dev_warn_ratelimited(bq->dev, "charger setup: %d\n", ret);
	if (pair && !ret) {
		ret = bq2560x_par_slave_on(bq, ichg / 2, cv - BQ2560X_VBATREG_STEP_uV, iindpm);
		if (ret) {
			bq2560x_par_slave_off(bq);
			dev_warn_ratelimited(bq->dev, "second charger: %d\n", ret);
		}
	}
	pair = pair && !ret;
	if (pair != p->on) {
		p->on = pair;
		if (pair)
			dev_info(bq->dev, "second charger on: %d mA charge and %d mA input each, CV %d mV\n",
				 min(ichg / 2, p->chip_ichg) / 1000, iindpm / 1000, cv / 1000);
		else
			dev_info(bq->dev, "second charger off\n");
	}
}

/* the table's status from a temperature: a change only on two samples in a row, the first one at once */
static void bq2560x_par_temp(struct bq256xx_device *bq, int temp)
{
	struct bq2560x_par *p = bq->par;
	int s = bq2560x_par_eval(p, temp, p->status);
	bool first = p->status < 0;

	if (s == p->status) {
		p->count = 0;
		return;
	}
	if (!first) {
		if (s != p->next) {
			p->next = s;
			p->count = 0;
		}
		if (++p->count < 2)
			return;
	}
	p->status = s;
	p->count = 0;
	if (bq2560x_par_stop(p))
		dev_info(bq->dev, "battery at %s%d.%d degC: no charging\n", temp < 0 ? "-" : "", abs(temp) / 10,
			 abs(temp) % 10);
	else
		dev_info(bq->dev, "battery at %s%d.%d degC: %d mA, %d mV\n", temp < 0 ? "-" : "", abs(temp) / 10,
			 abs(temp) % 10, p->rows[s].ichg / 1000, p->rows[s].cv / 1000);
}

/* whether the pair should charge, from the last sample (ZTE's thresholds, see above) */
static void bq2560x_par_decide(struct bq256xx_device *bq)
{
	struct bq2560x_par *p = bq->par;

	if (!p->sampled || bq2560x_chg_stopped(bq) || !bq->pd_contract || p->contract_uv <= BQ2560X_PAR_MIN_uV) {
		p->want = false;
		p->exit_count = 0;
	} else if (!p->want) {
		p->want = bq2560x_par_ichg(p) >= BQ2560X_PAR_ICHG_MIN_uA && p->ocv >= BQ2560X_PAR_OCV_MIN_uV &&
			  (p->ocv < BQ2560X_PAR_OCV_HIGH_uV || (!p->done && p->ibat >= BQ2560X_PAR_ICHG_MIN_uA));
		p->exit_count = 0;
	} else if (p->vbat < BQ2560X_PAR_VBAT_EXIT_uV || p->ibat < BQ2560X_PAR_IBAT_EXIT_uA) {
		if (++p->exit_count >= 2) {
			p->want = false;
			p->done = p->ibat < BQ2560X_PAR_IBAT_EXIT_uA;
			dev_info(bq->dev, "battery %d mV, %d mA: one charger\n", p->vbat / 1000, p->ibat / 1000);
		}
	} else {
		p->exit_count = 0;
	}
}

static int bq2560x_par_get(struct power_supply *psy, enum power_supply_property psp, int *val)
{
	union power_supply_propval v;
	int ret = power_supply_get_property(psy, psp, &v);

	if (!ret)
		*val = v.intval;
	return ret;
}

/* every 10 s with an outside supply on VBUS: the battery (without bq->lock: the gauge asks the charger back) */
static void bq2560x_par_work(struct work_struct *work)
{
	struct bq2560x_par *p = container_of(to_delayed_work(work), struct bq2560x_par, work);
	struct bq256xx_device *bq = p->bq;
	int temp = 0, vbat = 0, ocv = 0, ibat = 0, status;
	unsigned int stat;
	bool ok, on, online, changed;

	if (!p->fgu)
		p->fgu = power_supply_get_by_name("sc27xx-fgu");
	ok = p->fgu && !bq2560x_par_get(p->fgu, POWER_SUPPLY_PROP_TEMP, &temp) &&
	     !bq2560x_par_get(p->fgu, POWER_SUPPLY_PROP_VOLTAGE_NOW, &vbat) &&
	     !bq2560x_par_get(p->fgu, POWER_SUPPLY_PROP_VOLTAGE_OCV, &ocv) &&
	     !bq2560x_par_get(p->fgu, POWER_SUPPLY_PROP_CURRENT_NOW, &ibat);

	mutex_lock(&bq->lock);
	status = p->status;
	on = p->on;
	online = !regmap_read(bq->regmap, BQ256XX_CHARGER_STATUS_0, &stat) && (stat & BQ256XX_PG_STAT_MASK) &&
		 (stat & BQ256XX_VBUS_STAT_MASK) != BQ256XX_VBUS_STAT_USB_OTG;
	p->online = online;
	if (!online) {
		/* unplugged: the next plug starts over */
		p->sampled = p->want = p->done = false;
		p->status = -1;
		p->count = p->exit_count = 0;
	} else if (ok) {
		p->sampled = true;
		p->temp = temp;
		p->vbat = vbat;
		p->ocv = ocv;
		p->ibat = ibat;
		bq2560x_par_temp(bq, temp);
	}
	if (online)
		bq2560x_par_decide(bq);
	bq2560x_par_apply(bq);
	changed = on != p->on || status != p->status;
	mutex_unlock(&bq->lock);

	if (changed)
		power_supply_changed(bq->charger);
	if (online)
		mod_delayed_work(system_power_efficient_wq, &p->work, msecs_to_jiffies(BQ2560X_PAR_POLL_MS));
}

/* voltage_max from the port controller: the VAC over-voltage protection of both chips above it */
static int bq2560x_chg_set_ovp(struct bq256xx_device *bq, int uv)
{
	int ret;

	if (!bq->bq2560x_chg || uv <= 0 || uv > BQ2560X_OVP_MAX_uV)
		return -EINVAL;
	mutex_lock(&bq->lock);
	ret = regmap_update_bits(bq->regmap, BQ256XX_CHARGER_CONTROL_2, BQ2560X_CHG_OVP_MASK, bq2560x_chg_ovp(uv));
	if (!ret && bq->par) {
		if (bq->par->on)
			ret = regmap_update_bits(bq->par->slave->regmap, BQ256XX_CHARGER_CONTROL_2,
						 BQ2560X_CHG_OVP_MASK, bq2560x_chg_ovp(uv));
		if (!ret && bq2560x_chg_ovp(uv) != bq2560x_chg_ovp(bq->par->ovp_uv))
			dev_info(bq->dev, "VBUS up to %d mV: over-voltage protection at %s V\n", uv / 1000,
				 uv > BQ2560X_PAR_MIN_uV ? "10.5" : "6.5");
		bq->par->ovp_uv = uv;
	}
	mutex_unlock(&bq->lock);
	return ret;
}

static void bq256xx_charger_reset(void *data)
{
	struct bq256xx_device *bq = data;

	regmap_update_bits(bq->regmap, BQ256XX_PART_INFORMATION,
					BQ256XX_REG_RST, BQ256XX_REG_RST);

	if (!IS_ERR_OR_NULL(bq->usb2_phy))
		usb_unregister_notifier(bq->usb2_phy, &bq->usb_nb);

	if (!IS_ERR_OR_NULL(bq->usb3_phy))
		usb_unregister_notifier(bq->usb3_phy, &bq->usb_nb);
}

static int bq256xx_set_charger_property(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	struct bq256xx_device *bq = power_supply_get_drvdata(psy);
	int ret = -EINVAL;

	switch (prop) {
	case POWER_SUPPLY_PROP_USB_TYPE:
		/*
		 * "ti,bq2560x_chg", from the kernel only (not writeable from sysfs): ump9620-tcpc says PD when a PD
		 * contract's current follows (INPUT_CURRENT_LIMIT), UNKNOWN when it ends; the charger's own detection
		 * then takes over again. While PD, a VBUS event leaves the input current alone.
		 */
		if (!bq->bq2560x_chg || (val->intval != POWER_SUPPLY_USB_TYPE_PD &&
					 val->intval != POWER_SUPPLY_USB_TYPE_UNKNOWN))
			return -EINVAL;
		mutex_lock(&bq->lock);
		ret = bq->pd_contract != (val->intval == POWER_SUPPLY_USB_TYPE_PD);
		WRITE_ONCE(bq->pd_contract, val->intval == POWER_SUPPLY_USB_TYPE_PD);
		if (bq->par) {
			if (!bq->pd_contract)
				bq->par->contract_uv = bq->par->pd_ua = 0;
			bq2560x_par_apply(bq);
		}
		mutex_unlock(&bq->lock);
		if (ret && !bq->pd_contract)
			mod_delayed_work(system_power_efficient_wq, &bq->vbus_work, 0);
		ret = 0;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		/* ump9620-tcpc, from the kernel only: VBUS may go up to this (before it asks a charger for more) */
		return bq2560x_chg_set_ovp(bq, val->intval);

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		/* ump9620-tcpc, from the kernel only: the PD contract's voltage, which the pair needs above 5 V */
		if (!bq->par || val->intval < 0)
			return -EINVAL;
		mutex_lock(&bq->lock);
		ret = bq->par->contract_uv != val->intval;
		bq->par->contract_uv = val->intval;
		bq2560x_par_apply(bq);
		mutex_unlock(&bq->lock);
		if (ret)
			mod_delayed_work(system_power_efficient_wq, &bq->par->work, 0);
		ret = 0;
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		/* (under the lock, so that it cannot cross bq2560x_chg_detect's own setting) */
		mutex_lock(&bq->lock);
		if (bq->par && bq->pd_contract) {
			/* the contract's, for both chips together */
			ret = val->intval > 0 ? 0 : -EINVAL;
			if (!ret) {
				bq->par->pd_ua = val->intval;
				bq2560x_par_apply(bq);
			}
		} else {
			ret = bq->chip_info->bq256xx_set_iindpm(bq, val->intval);
		}
		mutex_unlock(&bq->lock);
		if (ret)
			return ret;
		if (bq->bq2560x_chg && READ_ONCE(bq->pd_contract))
			dev_info(bq->dev, "USB PD: input current %d mA\n", val->intval / 1000);
		break;

	case POWER_SUPPLY_PROP_STATUS:
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (bq->par) {
			/* caps on what the temperature table says: the current of both chips together */
			if (val->intval <= 0)
				return -EINVAL;
			mutex_lock(&bq->lock);
			if (prop == POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT)
				bq->par->user_ichg = val->intval;
			else
				bq->par->user_cv = val->intval;
			bq2560x_par_apply(bq);
			mutex_unlock(&bq->lock);
			ret = 0;
		} else if (prop == POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT) {
			ret = bq->chip_info->bq256xx_set_ichg(bq, val->intval);
		} else {
			ret = bq->chip_info->bq256xx_set_vbatreg(bq, val->intval);
		}
		if (ret)
			return ret;
		break;

	case POWER_SUPPLY_PROP_PRECHARGE_CURRENT:
		ret = bq->chip_info->bq256xx_set_iprechg(bq, val->intval);
		if (ret)
			return ret;
		break;

	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		ret = bq->chip_info->bq256xx_set_iterm(bq, val->intval);
		if (ret)
			return ret;
		break;

	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		ret = bq->chip_info->bq256xx_set_vindpm(bq, val->intval);
		if (ret)
			return ret;
		break;

	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		ret = bq->chip_info->bq256xx_set_charge_type(bq, val->intval);
		if (ret)
			return ret;
		/* "N/A" stops charging until "Fast" (or "Trickle"), also across plugs (bq2560x_chg_detect) */
		WRITE_ONCE(bq->chg_off, val->intval == POWER_SUPPLY_CHARGE_TYPE_NONE);
		if (bq->par) {
			mutex_lock(&bq->lock);
			bq2560x_par_apply(bq);
			mutex_unlock(&bq->lock);
		}
		break;

	default:
		break;
	}

	return ret;
}


static int bq256xx_get_battery_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct bq256xx_device *bq = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		val->intval = bq->init_data.ichg_max;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		val->intval = bq->init_data.vbatreg_max;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int bq256xx_get_charger_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct bq256xx_device *bq = power_supply_get_drvdata(psy);
	struct bq256xx_state state;
	int ret = 0;

	mutex_lock(&bq->lock);
	ret = bq256xx_get_state(bq, &state);
	mutex_unlock(&bq->lock);
	if (ret)
		return ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (state.vbus_stat == BQ256XX_VBUS_STAT_NO_INPUT ||
		    state.vbus_stat == BQ256XX_VBUS_STAT_USB_OTG)
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		else if (state.chrg_stat == BQ256XX_CHRG_STAT_NOT_CHRGING)
			val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		else if (state.chrg_stat == BQ256XX_CHRG_STAT_CHRG_TERM)
			val->intval = POWER_SUPPLY_STATUS_FULL;
		else
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		break;

	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = POWER_SUPPLY_HEALTH_UNKNOWN;
		if (state.wdt_fault) {
			val->intval =
				POWER_SUPPLY_HEALTH_WATCHDOG_TIMER_EXPIRE;
		} else if (state.bat_fault) {
			val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		} else {
			switch (state.chrg_stat) {
			case BQ256XX_CHRG_FAULT_INPUT:
				val->intval =
					POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
				break;
			case BQ256XX_CHRG_FAULT_THERM:
				val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
				break;
			case BQ256XX_CHRG_FAULT_CST_EXPIRE:
				val->intval =
				POWER_SUPPLY_HEALTH_SAFETY_TIMER_EXPIRE;
				break;
			default:
				break;
			}

			switch (state.ntc_fault) {
			case BQ256XX_NTC_FAULT_WARM:
				val->intval = POWER_SUPPLY_HEALTH_WARM;
				break;
			case BQ256XX_NTC_FAULT_COOL:
				val->intval = POWER_SUPPLY_HEALTH_COOL;
				break;
			case BQ256XX_NTC_FAULT_COLD:
				val->intval = POWER_SUPPLY_HEALTH_COLD;
				break;
			case BQ256XX_NTC_FAULT_HOT:
				val->intval = POWER_SUPPLY_HEALTH_HOT;
				break;
			default:
				val->intval = POWER_SUPPLY_HEALTH_GOOD;
				break;
			}
		}
		break;

	case POWER_SUPPLY_PROP_USB_TYPE:
		if (bq->bq2560x_chg && READ_ONCE(bq->pd_contract)) {
			val->intval = POWER_SUPPLY_USB_TYPE_PD;
			break;
		}
		/* "ti,bq2560x_chg": the PMIC's charger detection, when it said something (bq2560x_chg_detect) */
		if (bq->bq2560x_chg && READ_ONCE(bq->usb_type) != POWER_SUPPLY_USB_TYPE_UNKNOWN &&
		    state.vbus_stat != BQ256XX_VBUS_STAT_NO_INPUT &&
		    state.vbus_stat != BQ256XX_VBUS_STAT_USB_OTG) {
			val->intval = READ_ONCE(bq->usb_type);
		} else if (bq->chip_info->has_usb_detect) {
			switch (state.vbus_stat) {
			case BQ256XX_VBUS_STAT_USB_SDP:
				val->intval = POWER_SUPPLY_USB_TYPE_SDP;
				break;
			case BQ256XX_VBUS_STAT_USB_CDP:
				val->intval = POWER_SUPPLY_USB_TYPE_CDP;
				break;
			case BQ256XX_VBUS_STAT_USB_DCP:
				val->intval = POWER_SUPPLY_USB_TYPE_DCP;
				break;
			case BQ256XX_VBUS_STAT_USB_OTG:
				val->intval = POWER_SUPPLY_USB_TYPE_ACA;
				break;
			default:
				val->intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
				break;
			}
		} else {
			switch (state.vbus_stat) {
			case BQ256XX_VBUS_STAT_USB_SDP:
				val->intval = POWER_SUPPLY_USB_TYPE_SDP;
				break;
			case BQ256XX_VBUS_STAT_USB_OTG:
				val->intval = POWER_SUPPLY_USB_TYPE_ACA;
				break;
			default:
				val->intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
				break;
			}
		}
		break;

	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		switch (state.chrg_stat) {
		case BQ256XX_CHRG_STAT_NOT_CHRGING:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
			break;
		case BQ256XX_CHRG_STAT_PRECHRGING:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			break;
		case BQ256XX_CHRG_STAT_FAST_CHRGING:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
			break;
		case BQ256XX_CHRG_STAT_CHRG_TERM:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			break;
		default:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
		}
		break;

	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = BQ256XX_MANUFACTURER;
		break;

	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = bq->model_name;
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = state.online;
		break;

	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		ret = bq->chip_info->bq256xx_get_vindpm(bq);
		if (ret < 0)
			return ret;
		val->intval = ret;
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = bq->chip_info->bq256xx_get_iindpm(bq);
		if (ret < 0)
			return ret;
		val->intval = ret;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = bq->chip_info->bq256xx_get_vbatreg(bq);
		if (ret < 0)
			return ret;
		val->intval = ret;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = bq->chip_info->bq256xx_get_ichg(bq);
		if (ret < 0)
			return ret;
		val->intval = ret;
		break;

	case POWER_SUPPLY_PROP_PRECHARGE_CURRENT:
		ret = bq->chip_info->bq256xx_get_iprechg(bq);
		if (ret < 0)
			return ret;
		val->intval = ret;
		break;

	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		ret = bq->chip_info->bq256xx_get_iterm(bq);
		if (ret < 0)
			return ret;
		val->intval = ret;
		break;

	default:
		return -EINVAL;
	}

	return ret;
}

static bool bq256xx_state_changed(struct bq256xx_device *bq,
				  struct bq256xx_state *new_state)
{
	struct bq256xx_state old_state;

	mutex_lock(&bq->lock);
	old_state = bq->state;
	mutex_unlock(&bq->lock);

	return memcmp(&old_state, new_state, sizeof(struct bq256xx_state)) != 0;
}

static irqreturn_t bq256xx_irq_handler_thread(int irq, void *private)
{
	struct bq256xx_device *bq = private;
	struct bq256xx_state state;
	int ret;

	ret = bq256xx_get_state(bq, &state);
	if (ret < 0)
		goto irq_out;

	if (!bq256xx_state_changed(bq, &state))
		goto irq_out;

	mutex_lock(&bq->lock);
	bq->state = state;
	mutex_unlock(&bq->lock);

	power_supply_changed(bq->charger);

irq_out:
	return IRQ_HANDLED;
}

static enum power_supply_property bq256xx_power_supply_props[] = {
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_PRECHARGE_CURRENT,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
};

static enum power_supply_property bq256xx_battery_props[] = {
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
};

static int bq256xx_property_is_writeable(struct power_supply *psy,
					 enum power_supply_property prop)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_PRECHARGE_CURRENT:
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		return true;
	default:
		return false;
	}
}

static const struct power_supply_desc bq256xx_power_supply_desc = {
	.name = "bq256xx-charger",
	.type = POWER_SUPPLY_TYPE_USB,
	.usb_types = BIT(POWER_SUPPLY_USB_TYPE_SDP) |
		     BIT(POWER_SUPPLY_USB_TYPE_CDP) |
		     BIT(POWER_SUPPLY_USB_TYPE_DCP) |
		     BIT(POWER_SUPPLY_USB_TYPE_ACA) |
		     BIT(POWER_SUPPLY_USB_TYPE_PD) |
		     BIT(POWER_SUPPLY_USB_TYPE_UNKNOWN),
	.properties = bq256xx_power_supply_props,
	.num_properties = ARRAY_SIZE(bq256xx_power_supply_props),
	.get_property = bq256xx_get_charger_property,
	.set_property = bq256xx_set_charger_property,
	.property_is_writeable = bq256xx_property_is_writeable,
};

static struct power_supply_desc bq256xx_battery_desc = {
	.name			= "bq256xx-battery",
	.type			= POWER_SUPPLY_TYPE_BATTERY,
	.get_property		= bq256xx_get_battery_property,
	.properties		= bq256xx_battery_props,
	.num_properties		= ARRAY_SIZE(bq256xx_battery_props),
	.property_is_writeable	= bq256xx_property_is_writeable,
};


static bool bq256xx_is_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case BQ256XX_INPUT_CURRENT_LIMIT:
	case BQ256XX_CHARGER_STATUS_0...BQ256XX_CHARGER_STATUS_2:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config bq25600_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = BQ256XX_PART_INFORMATION,
	.reg_defaults	= bq2560x_reg_defs,
	.num_reg_defaults = ARRAY_SIZE(bq2560x_reg_defs),
	.cache_type = REGCACHE_FLAT,
	.volatile_reg = bq256xx_is_volatile_reg,
};

static const struct regmap_config bq25611d_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = BQ256XX_CHARGER_CONTROL_4,
	.reg_defaults	= bq25611d_reg_defs,
	.num_reg_defaults = ARRAY_SIZE(bq25611d_reg_defs),
	.cache_type = REGCACHE_FLAT,
	.volatile_reg = bq256xx_is_volatile_reg,
};

static const struct regmap_config bq25618_619_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = BQ256XX_CHARGER_CONTROL_4,
	.reg_defaults	= bq25618_619_reg_defs,
	.num_reg_defaults = ARRAY_SIZE(bq25618_619_reg_defs),
	.cache_type = REGCACHE_FLAT,
	.volatile_reg = bq256xx_is_volatile_reg,
};

/*
 * "ti,bq2560x_chg": the chip arrives configured by Android, so a cache that starts from the power-on defaults would
 * write those back in every read-modify-write; and mu300-usb switches the OTG boost (CHARGER_CONTROL_0) itself.
 * No cache: every update reads the register first.
 */
static const struct regmap_config bq2560x_chg_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = BQ256XX_PART_INFORMATION,
	.cache_type = REGCACHE_NONE,
};

/* the SGM41513 has the termination current's range in REG0D */
static const struct regmap_config sgm41513_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = SGM41513_REG_0D,
	.cache_type = REGCACHE_NONE,
};

/*
 * The SGM41513's fast charge current, as ZTE's driver codes it (bq2560x_charger_get_current() in sqc_bq2560x.c):
 * the six bits are eight ranges of eight steps, 3 A at most.
 */
static const int sgm41513_ichg_base_uA[] = { 0, 40000, 130000, 300000, 540000, 1020000, 1500000, 2460000 };
static const int sgm41513_ichg_step_uA[] = { 5000, 10000, 20000, 30000, 60000, 60000, 120000, 120000 };

static int sgm41513_ichg_uA(unsigned int code)
{
	unsigned int range = (code & BQ256XX_ICHG_MASK) >> 3;

	return min(sgm41513_ichg_base_uA[range] + (int)(code & 7) * sgm41513_ichg_step_uA[range],
		   BQ256XX_ICHG_MAX_uA);
}

static int sgm41513_get_ichg_curr(struct bq256xx_device *bq)
{
	unsigned int charge_current_limit;
	int ret;

	ret = regmap_read(bq->regmap, BQ256XX_CHARGE_CURRENT_LIMIT, &charge_current_limit);
	if (ret)
		return ret;

	return sgm41513_ichg_uA(charge_current_limit);
}

/* the largest step that is not above what was asked for */
static int sgm41513_set_ichg_curr(struct bq256xx_device *bq, int ichg)
{
	unsigned int code, ichg_reg_code = 0;
	int ichg_max = bq->init_data.ichg_max;

	ichg = clamp(ichg, BQ256XX_ICHG_MIN_uA, ichg_max);
	for (code = 1; code <= BQ256XX_ICHG_MASK; code++)
		if (sgm41513_ichg_uA(code) <= ichg && sgm41513_ichg_uA(code) > sgm41513_ichg_uA(ichg_reg_code))
			ichg_reg_code = code;

	return regmap_update_bits(bq->regmap, BQ256XX_CHARGE_CURRENT_LIMIT, BQ256XX_ICHG_MASK, ichg_reg_code);
}

/*
 * Its termination current (bq2560x_charger_set_termina_cur() and ..._get_termia_cur() there): sixteen steps up to
 * 240 mA, or six times each with REG0D bit 0 clear, which is how ZTE's driver sets anything above 240 mA (the U30
 * Air's battery node asks for 300 mA).
 */
static const int sgm41513_iterm_uA[] = {
	5000, 10000, 15000, 20000, 30000, 40000, 50000, 60000,
	80000, 100000, 120000, 140000, 160000, 180000, 200000, 240000,
};

static int sgm41513_get_term_curr(struct bq256xx_device *bq)
{
	unsigned int prechg_and_term_curr_lim, reg0d;
	int ret;

	ret = regmap_read(bq->regmap, BQ256XX_PRECHG_AND_TERM_CURR_LIM, &prechg_and_term_curr_lim);
	if (!ret)
		ret = regmap_read(bq->regmap, SGM41513_REG_0D, &reg0d);
	if (ret)
		return ret;

	return sgm41513_iterm_uA[prechg_and_term_curr_lim & BQ256XX_ITERM_MASK] *
	       (reg0d & SGM41513_ITERM_X1 ? 1 : 6);
}

static int sgm41513_set_term_curr(struct bq256xx_device *bq, int iterm)
{
	const unsigned int last = ARRAY_SIZE(sgm41513_iterm_uA) - 1;
	int mul = iterm > sgm41513_iterm_uA[last] ? 6 : 1;
	unsigned int code, iterm_reg_code = 0;
	int ret;

	iterm = clamp(iterm, sgm41513_iterm_uA[0], 6 * sgm41513_iterm_uA[last]);
	for (code = 1; code <= last; code++)
		if (sgm41513_iterm_uA[code] * mul <= iterm)
			iterm_reg_code = code;

	ret = regmap_update_bits(bq->regmap, SGM41513_REG_0D, SGM41513_ITERM_X1,
				 mul == 1 ? SGM41513_ITERM_X1 : 0);
	if (ret)
		return ret;

	return regmap_update_bits(bq->regmap, BQ256XX_PRECHG_AND_TERM_CURR_LIM, BQ256XX_ITERM_MASK, iterm_reg_code);
}

/*
 * Its precharge current: ZTE's driver never writes it and has no table for it, and the bq25601's 60 mA steps are
 * not this chip's. The field stays as the chip has it.
 */
static int sgm41513_get_prechrg_curr(struct bq256xx_device *bq)
{
	return -ENODATA;
}

static int sgm41513_set_prechrg_curr(struct bq256xx_device *bq, int iprechg)
{
	return 0;
}

static const struct bq256xx_chip_info bq256xx_chip_info_tbl[] = {
	[BQ25600] = {
		.model_id = BQ25600,
		.bq256xx_regmap_config = &bq25600_regmap_config,
		.bq256xx_get_ichg = bq256xx_get_ichg_curr,
		.bq256xx_get_iindpm = bq256xx_get_input_curr_lim,
		.bq256xx_get_vbatreg = bq2560x_get_chrg_volt,
		.bq256xx_get_iterm = bq256xx_get_term_curr,
		.bq256xx_get_iprechg = bq256xx_get_prechrg_curr,
		.bq256xx_get_vindpm = bq256xx_get_input_volt_lim,
		.bq256xx_set_ts_ignore = NULL,

		.bq256xx_set_ichg = bq256xx_set_ichg_curr,
		.bq256xx_set_iindpm = bq256xx_set_input_curr_lim,
		.bq256xx_set_vbatreg = bq2560x_set_chrg_volt,
		.bq256xx_set_iterm = bq256xx_set_term_curr,
		.bq256xx_set_iprechg = bq256xx_set_prechrg_curr,
		.bq256xx_set_vindpm = bq256xx_set_input_volt_lim,
		.bq256xx_set_charge_type = bq256xx_set_charge_type,

		.bq256xx_def_ichg = BQ2560X_ICHG_DEF_uA,
		.bq256xx_def_iindpm = BQ256XX_IINDPM_DEF_uA,
		.bq256xx_def_vbatreg = BQ2560X_VBATREG_DEF_uV,
		.bq256xx_def_iterm = BQ256XX_ITERM_DEF_uA,
		.bq256xx_def_iprechg = BQ256XX_IPRECHG_DEF_uA,
		.bq256xx_def_vindpm = BQ256XX_VINDPM_DEF_uV,

		.bq256xx_max_ichg = BQ256XX_ICHG_MAX_uA,
		.bq256xx_max_vbatreg = BQ2560X_VBATREG_MAX_uV,

		.has_usb_detect = false,
	},

	[BQ25600D] = {
		.model_id = BQ25600D,
		.bq256xx_regmap_config = &bq25600_regmap_config,
		.bq256xx_get_ichg = bq256xx_get_ichg_curr,
		.bq256xx_get_iindpm = bq256xx_get_input_curr_lim,
		.bq256xx_get_vbatreg = bq2560x_get_chrg_volt,
		.bq256xx_get_iterm = bq256xx_get_term_curr,
		.bq256xx_get_iprechg = bq256xx_get_prechrg_curr,
		.bq256xx_get_vindpm = bq256xx_get_input_volt_lim,

		.bq256xx_set_ichg = bq256xx_set_ichg_curr,
		.bq256xx_set_iindpm = bq256xx_set_input_curr_lim,
		.bq256xx_set_vbatreg = bq2560x_set_chrg_volt,
		.bq256xx_set_iterm = bq256xx_set_term_curr,
		.bq256xx_set_iprechg = bq256xx_set_prechrg_curr,
		.bq256xx_set_vindpm = bq256xx_set_input_volt_lim,
		.bq256xx_set_charge_type = bq256xx_set_charge_type,
		.bq256xx_set_ts_ignore = NULL,

		.bq256xx_def_ichg = BQ2560X_ICHG_DEF_uA,
		.bq256xx_def_iindpm = BQ256XX_IINDPM_DEF_uA,
		.bq256xx_def_vbatreg = BQ2560X_VBATREG_DEF_uV,
		.bq256xx_def_iterm = BQ256XX_ITERM_DEF_uA,
		.bq256xx_def_iprechg = BQ256XX_IPRECHG_DEF_uA,
		.bq256xx_def_vindpm = BQ256XX_VINDPM_DEF_uV,

		.bq256xx_max_ichg = BQ256XX_ICHG_MAX_uA,
		.bq256xx_max_vbatreg = BQ2560X_VBATREG_MAX_uV,

		.has_usb_detect = true,
	},

	[BQ25601] = {
		.model_id = BQ25601,
		.bq256xx_regmap_config = &bq25600_regmap_config,
		.bq256xx_get_ichg = bq256xx_get_ichg_curr,
		.bq256xx_get_iindpm = bq256xx_get_input_curr_lim,
		.bq256xx_get_vbatreg = bq2560x_get_chrg_volt,
		.bq256xx_get_iterm = bq256xx_get_term_curr,
		.bq256xx_get_iprechg = bq256xx_get_prechrg_curr,
		.bq256xx_get_vindpm = bq256xx_get_input_volt_lim,

		.bq256xx_set_ichg = bq256xx_set_ichg_curr,
		.bq256xx_set_iindpm = bq256xx_set_input_curr_lim,
		.bq256xx_set_vbatreg = bq2560x_set_chrg_volt,
		.bq256xx_set_iterm = bq256xx_set_term_curr,
		.bq256xx_set_iprechg = bq256xx_set_prechrg_curr,
		.bq256xx_set_vindpm = bq256xx_set_input_volt_lim,
		.bq256xx_set_charge_type = bq256xx_set_charge_type,
		.bq256xx_set_ts_ignore = NULL,

		.bq256xx_def_ichg = BQ2560X_ICHG_DEF_uA,
		.bq256xx_def_iindpm = BQ256XX_IINDPM_DEF_uA,
		.bq256xx_def_vbatreg = BQ2560X_VBATREG_DEF_uV,
		.bq256xx_def_iterm = BQ256XX_ITERM_DEF_uA,
		.bq256xx_def_iprechg = BQ256XX_IPRECHG_DEF_uA,
		.bq256xx_def_vindpm = BQ256XX_VINDPM_DEF_uV,

		.bq256xx_max_ichg = BQ256XX_ICHG_MAX_uA,
		.bq256xx_max_vbatreg = BQ2560X_VBATREG_MAX_uV,

		.has_usb_detect = false,
	},

	[BQ25601D] = {
		.model_id = BQ25601D,
		.bq256xx_regmap_config = &bq25600_regmap_config,
		.bq256xx_get_ichg = bq256xx_get_ichg_curr,
		.bq256xx_get_iindpm = bq256xx_get_input_curr_lim,
		.bq256xx_get_vbatreg = bq25601d_get_chrg_volt,
		.bq256xx_get_iterm = bq256xx_get_term_curr,
		.bq256xx_get_iprechg = bq256xx_get_prechrg_curr,
		.bq256xx_get_vindpm = bq256xx_get_input_volt_lim,

		.bq256xx_set_ichg = bq256xx_set_ichg_curr,
		.bq256xx_set_iindpm = bq256xx_set_input_curr_lim,
		.bq256xx_set_vbatreg = bq25601d_set_chrg_volt,
		.bq256xx_set_iterm = bq256xx_set_term_curr,
		.bq256xx_set_iprechg = bq256xx_set_prechrg_curr,
		.bq256xx_set_vindpm = bq256xx_set_input_volt_lim,
		.bq256xx_set_charge_type = bq256xx_set_charge_type,
		.bq256xx_set_ts_ignore = NULL,

		.bq256xx_def_ichg = BQ2560X_ICHG_DEF_uA,
		.bq256xx_def_iindpm = BQ256XX_IINDPM_DEF_uA,
		.bq256xx_def_vbatreg = BQ2560X_VBATREG_DEF_uV,
		.bq256xx_def_iterm = BQ256XX_ITERM_DEF_uA,
		.bq256xx_def_iprechg = BQ256XX_IPRECHG_DEF_uA,
		.bq256xx_def_vindpm = BQ256XX_VINDPM_DEF_uV,

		.bq256xx_max_ichg = BQ256XX_ICHG_MAX_uA,
		.bq256xx_max_vbatreg = BQ2560X_VBATREG_MAX_uV,

		.has_usb_detect = true,
	},

	[BQ25611D] = {
		.model_id = BQ25611D,
		.bq256xx_regmap_config = &bq25611d_regmap_config,
		.bq256xx_get_ichg = bq256xx_get_ichg_curr,
		.bq256xx_get_iindpm = bq256xx_get_input_curr_lim,
		.bq256xx_get_vbatreg = bq25611d_get_chrg_volt,
		.bq256xx_get_iterm = bq256xx_get_term_curr,
		.bq256xx_get_iprechg = bq256xx_get_prechrg_curr,
		.bq256xx_get_vindpm = bq256xx_get_input_volt_lim,

		.bq256xx_set_ichg = bq256xx_set_ichg_curr,
		.bq256xx_set_iindpm = bq256xx_set_input_curr_lim,
		.bq256xx_set_vbatreg = bq25611d_set_chrg_volt,
		.bq256xx_set_iterm = bq256xx_set_term_curr,
		.bq256xx_set_iprechg = bq256xx_set_prechrg_curr,
		.bq256xx_set_vindpm = bq256xx_set_input_volt_lim,
		.bq256xx_set_charge_type = bq256xx_set_charge_type,
		.bq256xx_set_ts_ignore = bq256xx_set_ts_ignore,

		.bq256xx_def_ichg = BQ25611D_ICHG_DEF_uA,
		.bq256xx_def_iindpm = BQ256XX_IINDPM_DEF_uA,
		.bq256xx_def_vbatreg = BQ25611D_VBATREG_DEF_uV,
		.bq256xx_def_iterm = BQ256XX_ITERM_DEF_uA,
		.bq256xx_def_iprechg = BQ256XX_IPRECHG_DEF_uA,
		.bq256xx_def_vindpm = BQ256XX_VINDPM_DEF_uV,

		.bq256xx_max_ichg = BQ256XX_ICHG_MAX_uA,
		.bq256xx_max_vbatreg = BQ25611D_VBATREG_MAX_uV,

		.has_usb_detect = true,
	},

	[BQ25618] = {
		.model_id = BQ25618,
		.bq256xx_regmap_config = &bq25618_619_regmap_config,
		.bq256xx_get_ichg = bq25618_619_get_ichg_curr,
		.bq256xx_get_iindpm = bq256xx_get_input_curr_lim,
		.bq256xx_get_vbatreg = bq25618_619_get_chrg_volt,
		.bq256xx_get_iterm = bq25618_619_get_term_curr,
		.bq256xx_get_iprechg = bq25618_619_get_prechrg_curr,
		.bq256xx_get_vindpm = bq256xx_get_input_volt_lim,

		.bq256xx_set_ichg = bq25618_619_set_ichg_curr,
		.bq256xx_set_iindpm = bq256xx_set_input_curr_lim,
		.bq256xx_set_vbatreg = bq25618_619_set_chrg_volt,
		.bq256xx_set_iterm = bq25618_619_set_term_curr,
		.bq256xx_set_iprechg = bq25618_619_set_prechrg_curr,
		.bq256xx_set_vindpm = bq256xx_set_input_volt_lim,
		.bq256xx_set_charge_type = bq256xx_set_charge_type,
		.bq256xx_set_ts_ignore = bq256xx_set_ts_ignore,

		.bq256xx_def_ichg = BQ25618_ICHG_DEF_uA,
		.bq256xx_def_iindpm = BQ256XX_IINDPM_DEF_uA,
		.bq256xx_def_vbatreg = BQ25618_VBATREG_DEF_uV,
		.bq256xx_def_iterm = BQ25618_ITERM_DEF_uA,
		.bq256xx_def_iprechg = BQ25618_IPRECHG_DEF_uA,
		.bq256xx_def_vindpm = BQ256XX_VINDPM_DEF_uV,

		.bq256xx_max_ichg = BQ25618_ICHG_MAX_uA,
		.bq256xx_max_vbatreg = BQ25618_VBATREG_MAX_uV,

		.has_usb_detect = false,
	},

	[BQ25619] = {
		.model_id = BQ25619,
		.bq256xx_regmap_config = &bq25618_619_regmap_config,
		.bq256xx_get_ichg = bq25618_619_get_ichg_curr,
		.bq256xx_get_iindpm = bq256xx_get_input_curr_lim,
		.bq256xx_get_vbatreg = bq25618_619_get_chrg_volt,
		.bq256xx_get_iterm = bq25618_619_get_term_curr,
		.bq256xx_get_iprechg = bq25618_619_get_prechrg_curr,
		.bq256xx_get_vindpm = bq256xx_get_input_volt_lim,

		.bq256xx_set_ichg = bq25618_619_set_ichg_curr,
		.bq256xx_set_iindpm = bq256xx_set_input_curr_lim,
		.bq256xx_set_vbatreg = bq25618_619_set_chrg_volt,
		.bq256xx_set_iterm = bq25618_619_set_term_curr,
		.bq256xx_set_iprechg = bq25618_619_set_prechrg_curr,
		.bq256xx_set_vindpm = bq256xx_set_input_volt_lim,
		.bq256xx_set_charge_type = bq256xx_set_charge_type,
		.bq256xx_set_ts_ignore = bq256xx_set_ts_ignore,

		.bq256xx_def_ichg = BQ25618_ICHG_DEF_uA,
		.bq256xx_def_iindpm = BQ256XX_IINDPM_DEF_uA,
		.bq256xx_def_vbatreg = BQ25618_VBATREG_DEF_uV,
		.bq256xx_def_iterm = BQ25618_ITERM_DEF_uA,
		.bq256xx_def_iprechg = BQ25618_IPRECHG_DEF_uA,
		.bq256xx_def_vindpm = BQ256XX_VINDPM_DEF_uV,

		.bq256xx_max_ichg = BQ25618_ICHG_MAX_uA,
		.bq256xx_max_vbatreg = BQ25618_VBATREG_MAX_uV,

		.has_usb_detect = false,
	},
};

/*
 * The charger type the USB PHY's BC1.2 detection found (phy-sprd-ums9620-ssphy.c, ums9620-bc12.h; kanoqwq b4748534):
 * the PHY is the one owner of D+/D-, so nothing here starts a detection. UNKNOWN while it detects, without VBUS, and
 * when it found no type. The PMIC device is still looked up here for the Type-C Rp reading below.
 */
static int bq2560x_chg_pmic_type(struct bq256xx_device *bq)
{
#if IS_REACHABLE(CONFIG_MFD_SC27XX_PMIC)
	if (!bq->pmic) {
		struct device_node *np = of_find_compatible_node(NULL, NULL, "sprd,ump9620");

		if (np)
			bq->pmic = bus_find_device_by_of_node(&spi_bus_type, np);
		of_node_put(np);
	}
#endif
	if (READ_ONCE(bq->bc12_phy->chg_state) != USB_CHARGER_PRESENT)
		return POWER_SUPPLY_USB_TYPE_UNKNOWN;
	switch (READ_ONCE(bq->bc12_phy->chg_type)) {
	case SDP_TYPE:
		return POWER_SUPPLY_USB_TYPE_SDP;
	case CDP_TYPE:
		return POWER_SUPPLY_USB_TYPE_CDP;
	case DCP_TYPE:
		return POWER_SUPPLY_USB_TYPE_DCP;
	default:
		return POWER_SUPPLY_USB_TYPE_UNKNOWN;
	}
}

/*
 * The input current a Type-C source offers with its Rp (USB Type-C Current: 1.5 A or 3 A), from the UMP9620's own
 * Type-C block ("sprd,sc27xx-typec", typec@380 under the PMIC). It has no driver here, but its state machine runs by
 * itself: STATUS bits 4:0 are the state (2: attached as a sink), bits 7:6 the source's Rp (0 default, 1 1.5 A,
 * 2 3 A), as Unisoc's sc27xx_pd reads them. A hub or splitter with a charger behind it answers BC1.2 as an SDP
 * (500 mA) but offers more through its Rp. 3 A is taken as 2 A, what a DCP gets here (3 A with u30air_3a). 0 when
 * there is no answer.
 * One register read through the PMIC's regmap, as sprd_pmic_detect_charger_type() does.
 */
#define UMP9620_TYPEC_STATUS		0x1c
#define UMP9620_TYPEC_STATE		GENMASK(4, 0)
#define UMP9620_TYPEC_ATTACHWAIT_SNK	1
#define UMP9620_TYPEC_ATTACHED_SNK	2
#define UMP9620_TYPEC_RP		GENMASK(7, 6)

static int bq2560x_chg_typec_iindpm(struct bq256xx_device *bq, unsigned int *stat)
{
	struct device_node *np;
	struct regmap *pmic;
	u32 base;
	int ret;

	*stat = 0;
	if (!bq->pmic)
		return 0;
	pmic = dev_get_regmap(bq->pmic, NULL);
	np = of_get_compatible_child(bq->pmic->of_node, "sprd,sc27xx-typec");
	if (!pmic || !np) {
		of_node_put(np);
		return 0;
	}
	ret = of_property_read_u32(np, "reg", &base);
	of_node_put(np);
	if (ret || regmap_read(pmic, base + UMP9620_TYPEC_STATUS, stat))
		return 0;
	/* still debouncing the attach (seen after a USB role change): give it up to 1.5 s */
	if (FIELD_GET(UMP9620_TYPEC_STATE, *stat) == UMP9620_TYPEC_ATTACHWAIT_SNK &&
	    regmap_read_poll_timeout(pmic, base + UMP9620_TYPEC_STATUS, *stat,
				     FIELD_GET(UMP9620_TYPEC_STATE, *stat) != UMP9620_TYPEC_ATTACHWAIT_SNK,
				     50000, 1500000))
		return 0;
	if (FIELD_GET(UMP9620_TYPEC_STATE, *stat) != UMP9620_TYPEC_ATTACHED_SNK)
		return 0;
	switch (FIELD_GET(UMP9620_TYPEC_RP, *stat)) {
	case 1:
		return 1500000;
	case 2:
		return u30air_3a ? 3000000 : 2000000;
	default:
		return 0;
	}
}

static void bq2560x_chg_put_pmic(void *data)
{
	struct bq256xx_device *bq = data;

	if (bq->pmic)
		put_device(bq->pmic);
}

/*
 * At probe and whenever VBUS comes or goes or the PHY's BC1.2 detection ends: charging on (Android leaves CHG_CONFIG
 * off; unless userspace stopped it) and, with an outside supply, the input current from that detection (a USB port's
 * 500 mA while it runs or when it found nothing; the PHY tells us when it is done). Never the charger's own input
 * detection (IINDET_EN): it drives D+/D-, the USB gadget's lines (kanoqwq b4748534).
 */
static void bq2560x_chg_detect(struct bq256xx_device *bq)
{
	unsigned int ctrl0 = 0, stat = 0, typec;
	bool off;
	int ret, iindpm, rp, type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
	const char *src = "PMIC";

	/* not before the chip is set up at probe (bq256xx_hw_init); the end of the probe runs this */
	if (!READ_ONCE(bq->bc12_ready))
		return;
	mutex_lock(&bq->lock);
	off = bq2560x_chg_stopped(bq);
	/* charging on, unless the boost is on (it drives VBUS: mu300-usb host, or the otg-vbus regulator) */
	ret = regmap_read(bq->regmap, BQ256XX_CHARGER_CONTROL_0, &ctrl0);
	if (!ret && !(ctrl0 & BQ256XX_OTG_CONFIG))
		ret = regmap_update_bits(bq->regmap, BQ256XX_CHARGER_CONTROL_0, BQ256XX_CHG_CONFIG_MASK,
					 off ? 0 : BQ256XX_CHG_CONFIG_MASK);
	if (!ret)
		ret = regmap_read(bq->regmap, BQ256XX_CHARGER_STATUS_0, &stat);
	/* a contract does not outlive VBUS, whatever became of the port controller */
	if (!ret && !(stat & BQ256XX_PG_STAT_MASK)) {
		WRITE_ONCE(bq->pd_contract, false);
		if (bq->par)
			bq->par->contract_uv = bq->par->pd_ua = 0;
	}
	/* nothing on VBUS, or the chip's own boost drives it, or a PD contract set the input current */
	if (ret || (ctrl0 & BQ256XX_OTG_CONFIG) || !(stat & BQ256XX_PG_STAT_MASK) ||
	    (stat & BQ256XX_VBUS_STAT_MASK) == BQ256XX_VBUS_STAT_USB_OTG || bq->pd_contract)
		goto out;

	type = bq2560x_chg_pmic_type(bq);
	switch (type) {
	case POWER_SUPPLY_USB_TYPE_SDP:
		iindpm = 500000;
		break;
	case POWER_SUPPLY_USB_TYPE_CDP:
		iindpm = 1500000;
		break;
	case POWER_SUPPLY_USB_TYPE_DCP:
		iindpm = 2000000;
		break;
	default:
		/* still detecting, or nothing found: a USB port's current; the PHY's notifier comes back when it is done */
		iindpm = 500000;
		break;
	}
	/* a Type-C source's Rp says what it offers whatever BC1.2 found (a hub with a charger behind it: SDP) */
	rp = bq2560x_chg_typec_iindpm(bq, &typec);
	if (rp > iindpm) {
		iindpm = rp;
		src = type == POWER_SUPPLY_USB_TYPE_UNKNOWN ? "Type-C" : "PMIC + Type-C";
	}
	ret = bq->chip_info->bq256xx_set_iindpm(bq, iindpm);
	if (!ret)
		dev_info(bq->dev, "%s: usb_type %d, Type-C 0x%x, input current %d mA, charging %s\n", src, type,
			 typec, iindpm / 1000,
			 !off ? "on" : READ_ONCE(bq->chg_off) ? "off (userspace)" : "off (battery temperature)");
out:
	WRITE_ONCE(bq->usb_type, type);
	if (bq->par) {
		bq->par->online = !ret && (stat & BQ256XX_PG_STAT_MASK) && !(ctrl0 & BQ256XX_OTG_CONFIG) &&
				  (stat & BQ256XX_VBUS_STAT_MASK) != BQ256XX_VBUS_STAT_USB_OTG;
		bq2560x_par_apply(bq);
	}
	mutex_unlock(&bq->lock);
	/* the battery from now on, or (unplugged) the end of the sampling */
	if (bq->par)
		mod_delayed_work(system_power_efficient_wq, &bq->par->work, 0);
	if (ret)
		dev_warn(bq->dev, "charger setup after VBUS change failed: %d\n", ret);
	power_supply_changed(bq->charger);
}

static void bq2560x_chg_vbus_work(struct work_struct *work)
{
	bq2560x_chg_detect(container_of(to_delayed_work(work), struct bq256xx_device, vbus_work));
}

static int bq2560x_chg_vbus_notifier(struct notifier_block *nb, unsigned long event, void *ptr)
{
	struct bq256xx_device *bq = container_of(nb, struct bq256xx_device, vbus_nb);

	/* (atomic: the PHY's charger notifier) a VBUS change, or the end of a BC1.2 detection, from the PHY */
	mod_delayed_work(system_power_efficient_wq, &bq->vbus_work, 0);
	return NOTIFY_OK;
}

static void bq2560x_chg_phy_cleanup(void *data)
{
	struct bq256xx_device *bq = data;

	WRITE_ONCE(bq->bc12_ready, false);
	usb_unregister_notifier(bq->bc12_phy, &bq->vbus_nb);
	cancel_delayed_work_sync(&bq->vbus_work);
}

/*
 * The USB PHY owns BC1.2 on this board (its VBUS is the extcon-usb-gpio device, which the charger used to follow
 * itself): it tells us of VBUS changes and of each detection's end. Until it is there, the probe waits.
 */
static int bq2560x_chg_phy(struct bq256xx_device *bq)
{
	struct device_node *np = of_find_compatible_node(NULL, NULL, "sprd,qogirn6pro-ssphy");

	if (!np)
		return dev_err_probe(bq->dev, -ENODEV, "no USB PHY node for BC1.2\n");
	bq->bc12_phy = devm_usb_get_phy_by_node(bq->dev, np, NULL);
	of_node_put(np);
	if (IS_ERR(bq->bc12_phy))
		return dev_err_probe(bq->dev, PTR_ERR(bq->bc12_phy), "BC1.2 USB PHY\n");
	if (!bq->bc12_phy->charger_detect)
		return dev_err_probe(bq->dev, -ENODEV, "the USB PHY does not detect chargers\n");
	return 0;
}

static int bq256xx_power_supply_init(struct bq256xx_device *bq,
		struct power_supply_config *psy_cfg, struct device *dev)
{
	bq->charger = devm_power_supply_register(bq->dev,
						 &bq256xx_power_supply_desc,
						 psy_cfg);
	if (IS_ERR(bq->charger)) {
		dev_err(dev, "power supply register charger failed\n");
		return PTR_ERR(bq->charger);
	}

	/* the fuel gauge is the battery there (sc27xx-fgu): no second one with two properties */
	if (bq->bq2560x_chg)
		return 0;

	bq->battery = devm_power_supply_register(bq->dev,
						      &bq256xx_battery_desc,
						      psy_cfg);
	if (IS_ERR(bq->battery)) {
		dev_err(dev, "power supply register battery failed\n");
		return PTR_ERR(bq->battery);
	}
	return 0;
}

static int bq256xx_hw_init(struct bq256xx_device *bq)
{
	struct power_supply_battery_info *bat_info;
	int wd_reg_val = BQ256XX_WATCHDOG_DIS;
	int ret = 0;
	int i;

	for (i = 0; i < BQ256XX_NUM_WD_VAL; i++) {
		if (bq->watchdog_timer == bq256xx_watchdog_time[i]) {
			wd_reg_val = i;
			break;
		}
		if (i + 1 < BQ256XX_NUM_WD_VAL &&
		    bq->watchdog_timer > bq256xx_watchdog_time[i] &&
		    bq->watchdog_timer < bq256xx_watchdog_time[i + 1])
			wd_reg_val = i;
	}
	ret = regmap_update_bits(bq->regmap, BQ256XX_CHARGER_CONTROL_1,
				 BQ256XX_WATCHDOG_MASK, wd_reg_val <<
						BQ256XX_WDT_BIT_SHIFT);
	if (ret)
		return ret;

	ret = power_supply_get_battery_info(bq->charger, &bat_info);
	if (ret == -ENOMEM)
		return ret;

	if (ret) {
		dev_warn(bq->dev, "battery info missing, default values will be applied\n");

		bat_info->constant_charge_current_max_ua =
				bq->chip_info->bq256xx_def_ichg;

		bat_info->constant_charge_voltage_max_uv =
				bq->chip_info->bq256xx_def_vbatreg;

		bat_info->precharge_current_ua =
				bq->chip_info->bq256xx_def_iprechg;

		bat_info->charge_term_current_ua =
				bq->chip_info->bq256xx_def_iterm;

		bq->init_data.ichg_max =
				bq->chip_info->bq256xx_max_ichg;

		bq->init_data.vbatreg_max =
				bq->chip_info->bq256xx_max_vbatreg;
	} else {
		bq->init_data.ichg_max =
			bat_info->constant_charge_current_max_ua;

		bq->init_data.vbatreg_max =
			bat_info->constant_charge_voltage_max_uv;
	}

	if (bq->bq2560x_chg) {
		if (bq->par) {
			/* the W200DS: each chip at most its fast charge share, the battery's maximum for both */
			bq->par->bat_ichg = (ret || bat_info->constant_charge_current_max_ua <= 0) ?
				2 * bq->par->chip_ichg : bat_info->constant_charge_current_max_ua;
			bq->init_data.ichg_max = bq->par->chip_ichg;
		} else if (u30air_3a) {
			/* the battery node's current (1.98 A without one), at most 3 A and the chip's maximum */
			bq->init_data.ichg_max = min_t(u32,
				(ret || bat_info->constant_charge_current_max_ua <= 0) ?
				BQ2560X_CHG_ICHG_MAX_uA :
				bat_info->constant_charge_current_max_ua,
				min_t(u32, BQ2560X_CHG_ICHG_3A_uA, bq->chip_info->bq256xx_max_ichg));
		} else {
			bq->init_data.ichg_max = min_t(u32, bq->init_data.ichg_max, BQ2560X_CHG_ICHG_MAX_uA);
		}
		/* A boot-time register snapshot is not the stock charging policy.
		 * Use this battery's declared CV, bounded by the charger capability;
		 * fall back to the chip default only when no valid value was supplied.
		 */
		bq->init_data.vbatreg_max = !ret && bat_info->constant_charge_voltage_max_uv > 0 ?
			bq2560x_chg_stock_cv(bat_info->constant_charge_voltage_max_uv,
					     bq->chip_info->bq256xx_max_vbatreg) :
			bq->chip_info->bq256xx_def_vbatreg;
		/* Stock sqc_bq2560x probe clears REG05 EN_TIMER (0x1b08).
		 * Keep termination enabled; this does not disable TS/NTC protection.
		 */
		ret = regmap_update_bits(bq->regmap, BQ256XX_CHARGER_CONTROL_1,
					 BQ256XX_EN_TERM | BQ2560X_CHG_EN_TIMER,
					 BQ256XX_EN_TERM);
		if (ret)
			return ret;
		/* VBUS up to 5 V until the port controller says more (bq2560x_chg_set_ovp): ZTE's 6.5 V */
		ret = regmap_update_bits(bq->regmap, BQ256XX_CHARGER_CONTROL_2, BQ2560X_CHG_OVP_MASK,
					 bq2560x_chg_ovp(5000000));
		if (ret)
			return ret;
	}

	ret = bq->chip_info->bq256xx_set_vindpm(bq, bq->init_data.vindpm);
	if (ret)
		return ret;

	ret = bq->chip_info->bq256xx_set_iindpm(bq, bq->init_data.iindpm);
	if (ret)
		return ret;

	ret = bq->chip_info->bq256xx_set_ichg(bq,
				bq->bq2560x_chg && u30air_3a ? bq->init_data.ichg_max :
				bq->chip_info->bq256xx_def_ichg);
	if (ret)
		return ret;

	ret = bq->chip_info->bq256xx_set_iprechg(bq,
				bat_info->precharge_current_ua);
	if (ret)
		return ret;

	ret = bq->chip_info->bq256xx_set_vbatreg(bq,
			bq->bq2560x_chg ? bq->init_data.vbatreg_max :
			bq->chip_info->bq256xx_def_vbatreg);
	if (ret)
		return ret;

	ret = bq->chip_info->bq256xx_set_iterm(bq,
				bat_info->charge_term_current_ua);
	if (ret)
		return ret;

	if (bq->chip_info->bq256xx_set_ts_ignore) {
		ret = bq->chip_info->bq256xx_set_ts_ignore(bq, bq->init_data.ts_ignore);
		if (ret)
			return ret;
	}

	power_supply_put_battery_info(bq->charger, bat_info);

	return 0;
}

static int bq256xx_parse_dt(struct bq256xx_device *bq,
		struct power_supply_config *psy_cfg, struct device *dev)
{
	int ret = 0;

	psy_cfg->drv_data = bq;
	psy_cfg->fwnode = dev_fwnode(dev);

	ret = device_property_read_u32(bq->dev, "ti,watchdog-timeout-ms",
				       &bq->watchdog_timer);
	if (ret)
		bq->watchdog_timer = BQ256XX_WATCHDOG_DIS;

	if (bq->watchdog_timer > BQ256XX_WATCHDOG_MAX ||
	    bq->watchdog_timer < BQ256XX_WATCHDOG_DIS)
		return -EINVAL;

	ret = device_property_read_u32(bq->dev,
				       "input-voltage-limit-microvolt",
				       &bq->init_data.vindpm);
	if (ret)
		bq->init_data.vindpm = bq->chip_info->bq256xx_def_vindpm;

	ret = device_property_read_u32(bq->dev,
				       "input-current-limit-microamp",
				       &bq->init_data.iindpm);
	if (ret)
		bq->init_data.iindpm = bq->chip_info->bq256xx_def_iindpm;

	bq->init_data.ts_ignore = device_property_read_bool(bq->dev, "ti,no-thermistor");

	if (bq->bq2560x_chg) {
		u32 mv;
		static char *supplied_to[] = { "sc27xx-fgu" };

		/* a USB port's 500 mA until the input detection says more (bq2560x_chg_detect) */
		if (!device_property_present(bq->dev, "input-current-limit-microamp"))
			bq->init_data.iindpm = 500000;
		/* the vendor property, in mV; the chip has 100 mV steps: rounded up */
		if (!device_property_present(bq->dev, "input-voltage-limit-microvolt") &&
		    !device_property_read_u32(bq->dev, "vindpm-value", &mv))
			bq->init_data.vindpm = DIV_ROUND_UP(mv, 100) * 100000;
		/* the fuel gauge takes its status from the charger */
		psy_cfg->supplied_to = supplied_to;
		psy_cfg->num_supplicants = ARRAY_SIZE(supplied_to);
	}

	return 0;
}

/*
 * "ti,bq2560x_chg": the boost (OTG_CONFIG) as the DT's "otg-vbus" regulator (vddvbus, the UMP9620 PD node's
 * vbus-supply), for ump9620-tcpc. Never against another supply: with power good from outside (a computer, a charger,
 * a dock) enabling is refused, after waiting up to 100 ms for it to go (a power role swap: the partner has just
 * switched its own 5 V off). The boost and charging exclude each other: CHG_CONFIG is off while it runs and back as
 * it was (or as userspace left it) after.
 */
static int bq2560x_otg_enable(struct regulator_dev *rdev)
{
	struct bq256xx_device *bq = rdev_get_drvdata(rdev);
	unsigned int stat;
	int ret;

	mutex_lock(&bq->lock);
	ret = regmap_read_poll_timeout(bq->regmap, BQ256XX_CHARGER_STATUS_0, stat,
				       !(stat & BQ256XX_PG_STAT_MASK) ||
				       (stat & BQ256XX_VBUS_STAT_MASK) == BQ256XX_VBUS_STAT_USB_OTG,
				       10000, 100000);
	if (ret == -ETIMEDOUT) {
		dev_warn(bq->dev, "5 V out refused: an outside supply is on VBUS\n");
		ret = -EBUSY;
	}
	/* the second chip must not charge the battery from the first one's boost */
	if (!ret && bq->par) {
		bq->par->online = false;
		bq2560x_par_slave_off(bq);
	}
	if (!ret)
		ret = regmap_update_bits(bq->regmap, BQ256XX_CHARGER_CONTROL_0,
					 BQ256XX_OTG_CONFIG | BQ256XX_CHG_CONFIG_MASK, BQ256XX_OTG_CONFIG);
	mutex_unlock(&bq->lock);
	if (!ret)
		dev_info(bq->dev, "5 V out on\n");
	return ret;
}

static int bq2560x_otg_disable(struct regulator_dev *rdev)
{
	struct bq256xx_device *bq = rdev_get_drvdata(rdev);
	int ret;

	mutex_lock(&bq->lock);
	ret = regmap_update_bits(bq->regmap, BQ256XX_CHARGER_CONTROL_0,
				 BQ256XX_OTG_CONFIG | BQ256XX_CHG_CONFIG_MASK,
				 bq2560x_chg_stopped(bq) ? 0 : BQ256XX_CHG_CONFIG_MASK);
	mutex_unlock(&bq->lock);
	if (!ret) {
		dev_info(bq->dev, "5 V out off\n");
		mod_delayed_work(system_power_efficient_wq, &bq->vbus_work, 0);
	}
	return ret;
}

static int bq2560x_otg_is_enabled(struct regulator_dev *rdev)
{
	struct bq256xx_device *bq = rdev_get_drvdata(rdev);
	unsigned int ctrl0;
	int ret = regmap_read(bq->regmap, BQ256XX_CHARGER_CONTROL_0, &ctrl0);

	return ret ?: !!(ctrl0 & BQ256XX_OTG_CONFIG);
}

static const struct regulator_ops bq2560x_otg_ops = {
	.enable = bq2560x_otg_enable,
	.disable = bq2560x_otg_disable,
	.is_enabled = bq2560x_otg_is_enabled,
};

static const struct regulator_desc bq2560x_otg_desc = {
	.name = "otg-vbus",
	.of_match = "otg-vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &bq2560x_otg_ops,
	.n_voltages = 1,
	.fixed_uV = 5000000,
};

static int bq2560x_otg_register(struct bq256xx_device *bq)
{
	struct regulator_config config = { .dev = bq->dev, .driver_data = bq };
	unsigned int ctrl0;
	int ret;

	/*
	 * The chip runs on the battery and keeps its registers across a reboot: a boost still on from before it (a
	 * crash in host mode) would meet the next computer's 5 V. Off before anything can use the port.
	 */
	ret = regmap_read(bq->regmap, BQ256XX_CHARGER_CONTROL_0, &ctrl0);
	if (!ret && (ctrl0 & BQ256XX_OTG_CONFIG)) {
		ret = regmap_update_bits(bq->regmap, BQ256XX_CHARGER_CONTROL_0, BQ256XX_OTG_CONFIG, 0);
		dev_info(bq->dev, "the boost was still on from before the reboot: off\n");
	}
	if (ret)
		return ret;
	return PTR_ERR_OR_ZERO(devm_regulator_register(bq->dev, &bq2560x_otg_desc, &config));
}

static void bq2560x_par_put_client(void *data)
{
	put_device(data);
}

static void bq2560x_par_regmap_exit(void *data)
{
	regmap_exit(data);
}

static void bq2560x_par_put_fgu(void *data)
{
	struct bq2560x_par *p = data;

	if (p->fgu)
		power_supply_put(p->fgu);
}

static void bq2560x_par_remove(void *data)
{
	struct bq256xx_device *bq = data;

	mutex_lock(&bq->lock);
	bq2560x_par_slave_off(bq);
	mutex_unlock(&bq->lock);
}

/*
 * The W200DS's second charger (see bq2560x_par_apply): its I2C device and enable GPIO (low: off from the start), its
 * registers as ZTE sets them, the battery's temperature table and per-chip currents. Nothing without its node (the
 * U30 Air). The first chip charges alone when the node's chip does not answer as a bq25601 or when the battery node
 * lacks the table.
 */
static int bq2560x_par_probe(struct bq256xx_device *bq)
{
	struct device *dev = bq->dev;
	struct device_node *np, *bat;
	struct i2c_client *client;
	struct bq256xx_device *s;
	struct bq2560x_par *p;
	struct regmap *regmap;
	u32 tbl[BQ2560X_PAR_ROWS * 4], fchg[2];
	unsigned int part = 0;
	int i, n, ret;

	np = of_find_compatible_node(NULL, NULL, "ti,bq2560x_chg2");
	if (!np || !of_device_is_available(np)) {
		of_node_put(np);
		return 0;
	}
	client = of_find_i2c_device_by_node(np);
	if (!client) {
		of_node_put(np);
		return dev_err_probe(dev, -EPROBE_DEFER, "second charger: no I2C device yet\n");
	}
	ret = devm_add_action_or_reset(dev, bq2560x_par_put_client, &client->dev);
	if (ret) {
		of_node_put(np);
		return ret;
	}
	p = devm_kzalloc(dev, sizeof(*p), GFP_KERNEL);
	s = devm_kzalloc(dev, sizeof(*s), GFP_KERNEL);
	if (!p || !s) {
		of_node_put(np);
		return -ENOMEM;
	}
	p->enable = devm_fwnode_gpiod_get(dev, of_fwnode_handle(np), "enable", GPIOD_OUT_LOW, "bq2560x-chg2");
	of_node_put(np);
	if (IS_ERR(p->enable))
		return dev_err_probe(dev, PTR_ERR(p->enable), "second charger's enable GPIO\n");
	regmap = regmap_init_i2c(client, &bq2560x_chg_regmap_config);
	if (IS_ERR(regmap))
		return dev_err_probe(dev, PTR_ERR(regmap), "second charger's registers\n");
	ret = devm_add_action_or_reset(dev, bq2560x_par_regmap_exit, regmap);
	if (ret)
		return ret;
	ret = regmap_read(regmap, BQ256XX_PART_INFORMATION, &part);
	if (ret || FIELD_GET(BQ2560X_CHG_PART_MASK, part) != BQ2560X_CHG_PART_BQ25601) {
		dev_warn(dev, "second charger: no bq25601 (%d, part 0x%02x): one charger\n", ret, part);
		return 0;
	}

	bat = of_parse_phandle(dev->of_node, "monitored-battery", 0);
	n = bat ? of_property_count_u32_elems(bat, "fchg-jeita-temp-table") : -ENOENT;
	if (n < 4 || n % 4 || n > (int)ARRAY_SIZE(tbl) ||
	    of_property_read_u32_array(bat, "fchg-jeita-temp-table", tbl, n) ||
	    of_property_read_u32_array(bat, "charge-fchg-current-microamp", fchg, 2) || !fchg[0] || !fchg[1])
		n = 0;
	of_node_put(bat);
	for (i = 0; i < n / 4; i++) {
		/* temperatures in deci-degC plus 1000 */
		p->rows[i] = (struct bq2560x_jeita) {
			.temp = (int)tbl[4 * i] - 1000, .recovery = (int)tbl[4 * i + 1] - 1000,
			.ichg = tbl[4 * i + 2], .cv = tbl[4 * i + 3],
		};
		if (i && p->rows[i].temp <= p->rows[i - 1].temp)
			n = 0;
	}
	if (!n) {
		dev_warn(dev, "second charger: no temperature table or fast charge current in the battery node: one charger\n");
		return 0;
	}
	p->nrows = n / 4;
	p->chip_ichg = min_t(u32, fchg[0], BQ256XX_ICHG_MAX_uA);
	p->chip_iindpm = min_t(u32, fchg[1], BQ256XX_IINDPM_MAX_uA);
	p->bat_ichg = 2 * p->chip_ichg;
	p->user_ichg = p->user_cv = INT_MAX;
	p->ovp_uv = 5000000;
	p->status = -1;
	p->bq = bq;
	s->dev = dev;
	s->regmap = regmap;
	s->chip_info = &bq256xx_chip_info_tbl[BQ25601];
	s->bq2560x_chg = true;
	s->init_data.ichg_max = p->chip_ichg;
	s->init_data.vbatreg_max = s->chip_info->bq256xx_max_vbatreg;
	p->slave = s;

	/* off, and as ZTE sets it: no termination, watchdog or safety timer, PFM off, 6.5 V, the first one's VINDPM */
	ret = regmap_update_bits(regmap, BQ256XX_CHARGER_CONTROL_1,
				 BQ256XX_EN_TERM | BQ256XX_WATCHDOG_MASK | BQ2560X_CHG_EN_TIMER, 0);
	if (!ret)
		ret = regmap_update_bits(regmap, BQ256XX_CHARGER_CONTROL_0,
					 BQ2560X_CHG_PFM_DIS | BQ256XX_OTG_CONFIG | BQ256XX_CHG_CONFIG_MASK,
					 BQ2560X_CHG_PFM_DIS);
	if (!ret)
		ret = regmap_update_bits(regmap, BQ256XX_INPUT_CURRENT_LIMIT, BQ256XX_EN_HIZ, BQ256XX_EN_HIZ);
	if (!ret)
		ret = regmap_update_bits(regmap, BQ256XX_CHARGER_CONTROL_2, BQ2560X_CHG_OVP_MASK,
					 bq2560x_chg_ovp(p->ovp_uv));
	if (!ret)
		ret = s->chip_info->bq256xx_set_vindpm(s, bq->init_data.vindpm);
	if (ret) {
		dev_warn(dev, "second charger setup: %d: one charger\n", ret);
		return 0;
	}

	ret = devm_add_action_or_reset(dev, bq2560x_par_put_fgu, p);
	if (ret)
		return ret;
	bq->par = p;
	ret = devm_add_action_or_reset(dev, bq2560x_par_remove, bq);
	if (!ret)
		ret = devm_delayed_work_autocancel(dev, &p->work, bq2560x_par_work);
	if (ret)
		return ret;
	dev_info(dev, "second charger: bq25601 at %s, %d temperature rows, %d mA charge and %d mA input per chip\n",
		 dev_name(&client->dev), p->nrows, p->chip_ichg / 1000, p->chip_iindpm / 1000);
	return 0;
}

static int bq256xx_probe(struct i2c_client *client)
{
	const struct i2c_device_id *id = i2c_client_get_device_id(client);
	struct device *dev = &client->dev;
	struct bq256xx_device *bq;
	struct power_supply_config psy_cfg = { };
	struct i2c_client *chip = client;	/* where the registers are: 0x1a for the SGM41513 */

	int ret;

	/*
	 * MU300 (kanoqwq f59a2968): ZTE's charger node is on every board of this family, but there is a battery to
	 * charge only where the device tree has the U30 Air's charger policy service (the test of patch 0016 and
	 * mu300-device). The F50 and V50 have none: no register of either charger address is touched there, so no
	 * chip that answers at the node's own address can be set up to charge a battery that is not there.
	 */
	if (of_device_is_compatible(dev->of_node, "ti,bq2560x_chg")) {
		struct device_node *policy = of_find_node_by_path("/charger_policy_service");

		of_node_put(policy);
		if (!policy)
			return -ENODEV;
	}

	bq = devm_kzalloc(dev, sizeof(*bq), GFP_KERNEL);
	if (!bq)
		return -ENOMEM;

	bq->client = client;
	bq->dev = dev;
	bq->chip_info = i2c_get_match_data(client);

	mutex_init(&bq->lock);

	strscpy(bq->model_name, id->name, sizeof(bq->model_name));

	bq->bq2560x_chg = of_device_is_compatible(dev->of_node, "ti,bq2560x_chg");
	if (bq->bq2560x_chg) {
		struct bq256xx_chip_info *info;
		int part, err;
		struct device_node *np;
		bool battery;

		/* the bq25601 as it is, without a register cache and with its input detection */
		info = devm_kmemdup(dev, bq->chip_info, sizeof(*info), GFP_KERNEL);
		if (!info)
			return -ENOMEM;
		info->bq256xx_regmap_config = &bq2560x_chg_regmap_config;
		info->has_usb_detect = true;
		bq->chip_info = info;

		/*
		 * Nothing at the node's address: ZTE's driver then goes on at 0x1a, where an SGM41513 is, and keeps
		 * the client it has (so the device is "6-006b" under Android whichever chip it is). Here the second
		 * address is a client of its own, which also keeps i2c-dev off it. A board that has the node and
		 * neither chip (the F50) probes no further than these two reads.
		 */
		part = i2c_smbus_read_byte_data(client, BQ256XX_PART_INFORMATION);
		/*
		 * Only on a board with a battery to charge: the U30 Air, whose device tree has the charger policy
		 * service (as mu300-device tells the boards apart). The F50 has no battery, and a charger at 0x1a
		 * all the same, with its registers set (0e 1e b4 f7 c0 8f 64 40 34 00 80 00 measured): bound and
		 * told to charge, the fuel gauge reported over-voltage and the PMIC reset the board, over and over
		 * on a router's USB port (issue #225). There it stays unbound, as before v2026.10.25.
		 */
		np = of_find_node_by_path("/charger_policy_service");
		battery = np;
		of_node_put(np);
		if (part < 0 && client->addr != SGM41513_I2C_ADDR && battery) {
			err = part;
			chip = devm_i2c_new_dummy_device(dev, client->adapter, SGM41513_I2C_ADDR);
			if (IS_ERR(chip))
				return dev_err_probe(dev, -ENODEV, "no charger: %d (0x%02x not tried: %ld)\n", err,
						     SGM41513_I2C_ADDR, PTR_ERR(chip));
			part = i2c_smbus_read_byte_data(chip, BQ256XX_PART_INFORMATION);
			if (part < 0)
				return dev_err_probe(dev, -ENODEV, "no charger: %d (and %d at 0x%02x)\n", err, part,
						     SGM41513_I2C_ADDR);
			dev_info(dev, "nothing at 0x%02x (%d): the SGM41513 at 0x%02x\n", client->addr, err,
				 SGM41513_I2C_ADDR);
			strscpy(bq->model_name, "sgm41513", sizeof(bq->model_name));
			info->bq256xx_regmap_config = &sgm41513_regmap_config;
			info->bq256xx_get_ichg = sgm41513_get_ichg_curr;
			info->bq256xx_set_ichg = sgm41513_set_ichg_curr;
			info->bq256xx_get_iterm = sgm41513_get_term_curr;
			info->bq256xx_set_iterm = sgm41513_set_term_curr;
			info->bq256xx_get_iprechg = sgm41513_get_prechrg_curr;
			info->bq256xx_set_iprechg = sgm41513_set_prechrg_curr;
		} else if (part < 0) {
			return dev_err_probe(dev, -ENODEV, "no charger: %d\n", part);
		}
		dev_info(dev, "part %u, rev %u\n", (part >> 3) & 0xf, part & 0x3);

		ret = bq2560x_chg_phy(bq);
		if (ret)
			return ret;
	}

	bq->regmap = devm_regmap_init_i2c(chip,
					bq->chip_info->bq256xx_regmap_config);

	if (IS_ERR(bq->regmap)) {
		dev_err(dev, "Failed to allocate register map\n");
		return PTR_ERR(bq->regmap);
	}

	i2c_set_clientdata(client, bq);

	ret = bq256xx_parse_dt(bq, &psy_cfg, dev);
	if (ret) {
		dev_err(dev, "Failed to read device tree properties%d\n", ret);
		return ret;
	}

	ret = devm_add_action_or_reset(dev, bq256xx_charger_reset, bq);
	if (ret)
		return ret;

	if (bq->bq2560x_chg) {
		ret = bq2560x_par_probe(bq);
		if (ret)
			return ret;
	}

	/* OTG reporting (not with the extcon: it would pick up the SoC's PHY, whose events it does not know) */
	if (!bq->bq2560x_chg) {
		bq->usb2_phy = devm_usb_get_phy(dev, USB_PHY_TYPE_USB2);
		if (!IS_ERR_OR_NULL(bq->usb2_phy)) {
			INIT_WORK(&bq->usb_work, bq256xx_usb_work);
			bq->usb_nb.notifier_call = bq256xx_usb_notifier;
			usb_register_notifier(bq->usb2_phy, &bq->usb_nb);
		}

		bq->usb3_phy = devm_usb_get_phy(dev, USB_PHY_TYPE_USB3);
		if (!IS_ERR_OR_NULL(bq->usb3_phy)) {
			INIT_WORK(&bq->usb_work, bq256xx_usb_work);
			bq->usb_nb.notifier_call = bq256xx_usb_notifier;
			usb_register_notifier(bq->usb3_phy, &bq->usb_nb);
		}
	}

	/* before the power supply: a property write (charge_type, usb_type) queues it (kanoqwq b4748534) */
	if (bq->bq2560x_chg) {
		ret = devm_delayed_work_autocancel(dev, &bq->vbus_work, bq2560x_chg_vbus_work);
		if (ret)
			return ret;
	}

	ret = bq256xx_power_supply_init(bq, &psy_cfg, dev);
	if (ret) {
		dev_err(dev, "Failed to register power supply\n");
		return ret;
	}

	if (client->irq) {
		ret = devm_request_threaded_irq(dev, client->irq, NULL,
						bq256xx_irq_handler_thread,
						IRQF_TRIGGER_FALLING |
						IRQF_ONESHOT,
						dev_name(&client->dev), bq);
		if (ret < 0) {
			dev_err(dev, "get irq fail: %d\n", ret);
			return ret;
		}
	}

	ret = bq256xx_hw_init(bq);
	if (ret) {
		dev_err(dev, "Cannot initialize the chip.\n");
		return ret;
	}

	if (bq->bq2560x_chg) {
		ret = devm_add_action_or_reset(dev, bq2560x_chg_put_pmic, bq);
		if (ret)
			return ret;
		WRITE_ONCE(bq->bc12_ready, true);
		bq->vbus_nb.notifier_call = bq2560x_chg_vbus_notifier;
		ret = usb_register_notifier(bq->bc12_phy, &bq->vbus_nb);
		if (ret)
			return ret;
		ret = devm_add_action_or_reset(dev, bq2560x_chg_phy_cleanup, bq);
		if (ret)
			return ret;
		ret = bq2560x_otg_register(bq);
		if (ret)
			return dev_err_probe(dev, ret, "otg-vbus regulator\n");
		mod_delayed_work(system_power_efficient_wq, &bq->vbus_work, 0);
	}

	return ret;
}

static const struct i2c_device_id bq256xx_i2c_ids[] = {
	{ "bq25600", (kernel_ulong_t)&bq256xx_chip_info_tbl[BQ25600] },
	{ "bq25600d", (kernel_ulong_t)&bq256xx_chip_info_tbl[BQ25600D] },
	{ "bq25601", (kernel_ulong_t)&bq256xx_chip_info_tbl[BQ25601] },
	{ "bq2560x_chg", (kernel_ulong_t)&bq256xx_chip_info_tbl[BQ25601] },
	{ "bq25601d", (kernel_ulong_t)&bq256xx_chip_info_tbl[BQ25601D] },
	{ "bq25611d", (kernel_ulong_t)&bq256xx_chip_info_tbl[BQ25611D] },
	{ "bq25618", (kernel_ulong_t)&bq256xx_chip_info_tbl[BQ25618] },
	{ "bq25619", (kernel_ulong_t)&bq256xx_chip_info_tbl[BQ25619] },
	{}
};
MODULE_DEVICE_TABLE(i2c, bq256xx_i2c_ids);

static const struct of_device_id bq256xx_of_match[] = {
	{ .compatible = "ti,bq25600", .data = &bq256xx_chip_info_tbl[BQ25600] },
	{ .compatible = "ti,bq25600d", .data = &bq256xx_chip_info_tbl[BQ25600D] },
	{ .compatible = "ti,bq25601", .data = &bq256xx_chip_info_tbl[BQ25601] },
	{ .compatible = "ti,bq2560x_chg", .data = &bq256xx_chip_info_tbl[BQ25601] },
	{ .compatible = "ti,bq25601d", .data = &bq256xx_chip_info_tbl[BQ25601D] },
	{ .compatible = "ti,bq25611d", .data = &bq256xx_chip_info_tbl[BQ25611D] },
	{ .compatible = "ti,bq25618", .data = &bq256xx_chip_info_tbl[BQ25618] },
	{ .compatible = "ti,bq25619", .data = &bq256xx_chip_info_tbl[BQ25619] },
	{}
};
MODULE_DEVICE_TABLE(of, bq256xx_of_match);

static const struct acpi_device_id bq256xx_acpi_match[] = {
	{ "bq25600", (kernel_ulong_t)&bq256xx_chip_info_tbl[BQ25600] },
	{ "bq25600d", (kernel_ulong_t)&bq256xx_chip_info_tbl[BQ25600D] },
	{ "bq25601", (kernel_ulong_t)&bq256xx_chip_info_tbl[BQ25601] },
	{ "bq25601d", (kernel_ulong_t)&bq256xx_chip_info_tbl[BQ25601D] },
	{ "bq25611d", (kernel_ulong_t)&bq256xx_chip_info_tbl[BQ25611D] },
	{ "bq25618", (kernel_ulong_t)&bq256xx_chip_info_tbl[BQ25618] },
	{ "bq25619", (kernel_ulong_t)&bq256xx_chip_info_tbl[BQ25619] },
	{}
};
MODULE_DEVICE_TABLE(acpi, bq256xx_acpi_match);

/* system suspend: nothing samples the battery, so the second chip is off; the first one charges on as it was */
static int bq256xx_suspend(struct device *dev)
{
	struct bq256xx_device *bq = dev_get_drvdata(dev);

	if (!bq || !bq->par)
		return 0;
	cancel_delayed_work_sync(&bq->par->work);
	mutex_lock(&bq->lock);
	bq->par->suspended = true;
	bq2560x_par_apply(bq);
	mutex_unlock(&bq->lock);
	return 0;
}

static int bq256xx_resume(struct device *dev)
{
	struct bq256xx_device *bq = dev_get_drvdata(dev);

	if (!bq || !bq->par)
		return 0;
	mutex_lock(&bq->lock);
	bq->par->suspended = false;
	mutex_unlock(&bq->lock);
	mod_delayed_work(system_power_efficient_wq, &bq->par->work, 0);
	return 0;
}

static DEFINE_SIMPLE_DEV_PM_OPS(bq256xx_pm_ops, bq256xx_suspend, bq256xx_resume);

static struct i2c_driver bq256xx_driver = {
	.driver = {
		.name = "bq256xx-charger",
		.of_match_table = bq256xx_of_match,
		.acpi_match_table = bq256xx_acpi_match,
		.pm = pm_sleep_ptr(&bq256xx_pm_ops),
	},
	.probe = bq256xx_probe,
	.id_table = bq256xx_i2c_ids,
};
module_i2c_driver(bq256xx_driver);

MODULE_AUTHOR("Ricardo Rivera-Matos <r-rivera-matos@ti.com>");
MODULE_DESCRIPTION("bq256xx charger driver");
MODULE_LICENSE("GPL v2");
