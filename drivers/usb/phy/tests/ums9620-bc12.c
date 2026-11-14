// SPDX-License-Identifier: GPL-2.0-only
/* Execute the production PMIC transaction with delayed results and I/O faults. */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define BIT(n) (1U << (n))
#define GENMASK(h, l) ((~0U >> (31 - (h))) & (~0U << (l)))
typedef uint32_t u32;
struct regmap { u32 mux, control; };
static struct regmap map;
static unsigned int calls, fail_at, elapsed, status_reads, restore_attempts;
static bool stuck_done, never_done, unplugged, no_vbus, status_absent;
static unsigned int unplug_at;

static int regmap_read(struct regmap *m, u32 reg, u32 *value)
{
	if (++calls == fail_at)
		return -EIO;
	switch (reg) {
	case 0x39c: *value = unplugged || (unplug_at && elapsed >= unplug_at) ? 5 : 2; break;
	case 0x3e0: *value = no_vbus ? 0 : BIT(8); break;
	case 0x23a0: *value = m->mux; break;
	case 0x243c: *value = m->control; break;
	case 0x239c:
		status_reads++;
		*value = status_absent ? 0 : 4;
		if (stuck_done || (!never_done && elapsed >= 960))
			*value |= 0x840;
		break;
	default: assert(false);
	}
	return 0;
}

static int regmap_update_bits(struct regmap *m, u32 reg, u32 mask, u32 value)
{
	u32 *dest = reg == 0x23a0 ? &m->mux : &m->control;

	if (status_reads || elapsed)
		restore_attempts++;
	if (++calls == fail_at)
		return -EIO;
	*dest = (*dest & ~mask) | (value & mask);
	return 0;
}
static void msleep(unsigned int ms) { elapsed += ms; }
#include "../phy-sprd-ums9620-bc12.h"

static void reset(void)
{
	map = (struct regmap){ .mux = 0x4201, .control = 0xa0 };
	calls = fail_at = elapsed = status_reads = restore_attempts = unplug_at = 0;
	stuck_done = never_done = unplugged = no_vbus = status_absent = false;
}

int main(void)
{
	u32 result;
	unsigned int count, i;

	reset();
	assert(!ums9620_bc12_redetect(&map, &result));
	count = calls;
	assert(result == 0x844 && elapsed == 960);
	assert(map.mux == 0x4201 && map.control == 0xa0 && restore_attempts == 2);
	for (i = 1; i <= count; i++) {
		reset(); fail_at = i;
		assert(ums9620_bc12_redetect(&map, &result) == -EIO);
		assert(!result);
		/* A failed restore cannot publish a charger classification. The
		 * other restore still executes, including when the first fails.
		 */
		if (i < count - 1)
			assert(map.mux == 0x4201 && map.control == 0xa0);
		if (i == count - 1)
			assert(map.mux == 0x4201 && restore_attempts == 2);
		if (i == count)
			assert(map.control == 0xa0 && restore_attempts == 2);
	}
	reset(); stuck_done = true;
	assert(ums9620_bc12_redetect(&map, &result) == -EAGAIN && !result);
	reset(); never_done = true;
	assert(ums9620_bc12_redetect(&map, &result) == -ETIMEDOUT && !result && elapsed == 2220);
	reset(); unplug_at = 440;
	assert(ums9620_bc12_redetect(&map, &result) == -ENODEV && !result && elapsed == 440);
	assert(map.mux == 0x4201 && map.control == 0xa0);
	reset(); unplugged = true;
	assert(ums9620_bc12_redetect(&map, &result) == -ENODEV && !result && !elapsed);
	reset(); no_vbus = true;
	assert(ums9620_bc12_redetect(&map, &result) == -ENODEV && !result && !elapsed);
	reset(); status_absent = true;
	assert(ums9620_bc12_redetect(&map, &result) == -EAGAIN && !result);
	reset(); map.control |= 1;
	assert(ums9620_bc12_redetect(&map, &result) == -EBUSY && !result && !elapsed);
	assert(map.mux == 0x4201 && map.control == 0xa1);
	printf("PASS: fresh detection, all %u I/O failures, both restores, stale DONE, timeout and unplug\n", count);
	return 0;
}
