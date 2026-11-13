/* SPDX-License-Identifier: GPL-2.0-only */
/* Boot-only audio adaptation of LK-selected DT. Include beside the existing GPU
 * fixup in setup.c, then call before early_init_dt_scan(). Never a live overlay.
 * Generated from w202ds-alsa-preserve-dp.dtsi; unrelated properties are retained.
 */
#include <linux/sizes.h>
/* OF properties retain pointers into this blob after init memory is freed. */
static unsigned char w202_audio_fdt[SZ_1M] __aligned(8);

static int __init w202_audio_node(void *fdt, const char *path)
{
	int node = fdt_path_offset(fdt, path);
	if (node < 0)
		panic("W202DS audio DT missing %s: %d", path, node);
	return node;
}

static void __init w202_audio_property(void *fdt, const char *path,
		const char *name, const void *value, int length)
{
	int ret = fdt_setprop(fdt, w202_audio_node(fdt, path), name, value, length);
	if (ret)
		panic("W202DS audio DT %s/%s: %d", path, name, ret);
}

static void __init w202_audio_delete(void *fdt, const char *path, const char *name)
{
	int ret = fdt_delprop(fdt, w202_audio_node(fdt, path), name);
	if (ret && ret != -FDT_ERR_NOTFOUND)
		panic("W202DS audio DT delete %s/%s: %d", path, name, ret);
}

static void __init w202_audio_phandle(void *fdt, const char *path, u32 expected)
{
	if (fdt_get_phandle(fdt, w202_audio_node(fdt, path)) != expected)
		panic("W202DS audio DT unexpected phandle at %s", path);
}

static void __init w202_audio_reservation(void *fdt, const char *path, u32 base, u32 size)
{
	int len;
	const fdt32_t *reg = fdt_getprop(fdt, w202_audio_node(fdt, path), "reg", &len);
	if (!reg || len != 16 || fdt32_to_cpu(reg[0]) || fdt32_to_cpu(reg[1]) != base ||
	    fdt32_to_cpu(reg[2]) || fdt32_to_cpu(reg[3]) != size)
		panic("W202DS audio DT unexpected reservation at %s", path);
}

static void *__init w202ds_audio_boot_fixup(void *original, phys_addr_t *physical)
{
	void *fdt = w202_audio_fdt;
	int node, ret;
	u32 largest = 0, phandle;
	fdt32_t amp_phandle;
	if (!original || fdt_check_header(original) ||
	    fdt_node_check_compatible(original, 0, "sprd,ums9620"))
		panic("W202DS audio requires the matched LK board DT");
	ret = fdt_open_into(original, fdt, sizeof(w202_audio_fdt));
	if (ret)
		panic("W202DS audio DT copy: %d", ret);
	w202_audio_phandle(fdt, "/audcp-iram@65006400", 0xf9);
	w202_audio_phandle(fdt, "/reserved-memory/audio-mem@af700000", 0xfa);
	w202_audio_phandle(fdt, "/reserved-memory/audiodsp-mem@afa00000", 0xfb);
	w202_audio_phandle(fdt, "/soc/agcp/dma-controller@56650000", 0xf8);
	w202_audio_phandle(fdt, "/soc/agcp/vbc@56510000", 0x10d);
	w202_audio_phandle(fdt, "/sprd-pcm-audio", 0x109);
	w202_audio_phandle(fdt, "/soc/aon/gpio@170000", 0xc);
	w202_audio_phandle(fdt, "/soc/syscon@64900000", 0x2);
	w202_audio_phandle(fdt, "/soc/syscon@64910000", 0x4);
	w202_audio_phandle(fdt, "/soc/clock-controller@56200000", 0xbb);
	w202_audio_phandle(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@34", 0x10f);
	w202_audio_phandle(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@35", 0x110);
	w202_audio_phandle(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@36", 0x111);
	w202_audio_phandle(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@37", 0x112);
	w202_audio_phandle(fdt, "/soc/aon/spi@400000/pmic@0/audio-codec@1000", 0x10e);
	w202_audio_phandle(fdt, "/soc/agcp/audio-codec@56360000", 0x7f);
	w202_audio_reservation(fdt, "/reserved-memory/audio-mem@af700000", 0xaf700000, 0x300000);
	w202_audio_reservation(fdt, "/reserved-memory/audiodsp-mem@afa00000", 0xafa00000, 0x600000);
	for (node = fdt_next_node(fdt, -1, NULL); node >= 0;
	     node = fdt_next_node(fdt, node, NULL)) {
		phandle = fdt_get_phandle(fdt, node);
		if (phandle > largest)
			largest = phandle;
	}
	if (node != -FDT_ERR_NOTFOUND || largest >= U32_MAX - 1)
		panic("W202DS audio DT cannot allocate amplifier phandle");
	amp_phandle = cpu_to_fdt32(largest + 1);
	ret = fdt_add_subnode(fdt, 0, "amplifier-bank");
	if (ret < 0)
		panic("W202DS audio DT adding /amplifier-bank: %d", ret);
	w202_audio_property(fdt, "/amplifier-bank", "phandle", &amp_phandle, sizeof(amp_phandle));
	w202_audio_property(fdt, "/amplifier-bank", "compatible", "\x7a\x74\x65\x2c\x77\x32\x30\x32\x64\x73\x2d\x61\x6d\x70\x6c\x69\x66\x69\x65\x72\x2d\x62\x61\x6e\x6b\x00", 26);
	w202_audio_property(fdt, "/amplifier-bank", "#power-domain-cells", "\x00\x00\x00\x00", 4);
	w202_audio_property(fdt, "/amplifier-bank", "reset-gpios", "\x00\x00\x00\x0c\x00\x00\x00\x3d\x00\x00\x00\x01", 12);
	ret = fdt_add_subnode(fdt, 0, "alsa-speakers");
	if (ret < 0)
		panic("W202DS audio DT adding /alsa-speakers: %d", ret);
	w202_audio_property(fdt, "/alsa-speakers", "compatible", "\x7a\x74\x65\x2c\x77\x32\x30\x32\x64\x73\x2d\x73\x70\x65\x61\x6b\x65\x72\x73\x00", 20);
	w202_audio_property(fdt, "/alsa-speakers", "zte,audio-cpu", "\x00\x00\x01\x0d", 4);
	w202_audio_property(fdt, "/alsa-speakers", "zte,audio-platform", "\x00\x00\x01\x09", 4);
	w202_audio_property(fdt, "/alsa-speakers", "zte,audio-codecs", "\x00\x00\x01\x0f\x00\x00\x01\x10\x00\x00\x01\x11\x00\x00\x01\x12", 16);
	w202_audio_property(fdt, "/alsa-speakers", "zte,audio-mic", "\x00\x00\x01\x0e", 4);
	w202_audio_property(fdt, "/reserved-memory/audio-mem@af700000", "no-map", "", 0);
	w202_audio_property(fdt, "/reserved-memory/audiodsp-mem@afa00000", "no-map", "", 0);
	w202_audio_property(fdt, "/audcp-iram@65006400", "compatible", "\x7a\x74\x65\x2c\x77\x32\x30\x32\x64\x73\x2d\x61\x75\x64\x69\x6f\x2d\x64\x73\x70\x00", 21);
	w202_audio_property(fdt, "/audcp-iram@65006400", "#power-domain-cells", "\x00\x00\x00\x00", 4);
	w202_audio_property(fdt, "/audcp-iram@65006400", "memory-region", "\x00\x00\x00\xfb\x00\x00\x00\xfa", 8);
	w202_audio_property(fdt, "/audcp-iram@65006400", "zte,ipc-offset", "\x00\x28\x00\x00", 4);
	w202_audio_property(fdt, "/audcp-iram@65006400", "mboxes", "\x00\x00\x00\xf7\x00\x00\x00\x05\x00\x00\x00\x00", 12);
	w202_audio_property(fdt, "/audcp-iram@65006400", "mbox-names", "\x61\x75\x64\x63\x70\x00", 6);
	w202_audio_property(fdt, "/audcp-iram@65006400", "sysshutdown", "\x00\x00\x00\x04\x00\x00\x03\xd0\x02\x00\x00\x00", 12);
	w202_audio_property(fdt, "/audcp-iram@65006400", "coreshutdown", "\x00\x00\x00\x04\x00\x00\x03\xd4\x02\x00\x00\x00", 12);
	w202_audio_property(fdt, "/audcp-iram@65006400", "deepsleep", "\x00\x00\x00\x04\x00\x00\x08\x18\x00\x00\x00\x08", 12);
	w202_audio_property(fdt, "/audcp-iram@65006400", "corereset", "\x00\x00\x00\x04\x00\x00\x0b\x88\x00\x00\x00\x01", 12);
	w202_audio_property(fdt, "/audcp-iram@65006400", "sysreset", "\x00\x00\x00\x04\x00\x00\x0b\x98\x00\x40\x00\x00", 12);
	w202_audio_property(fdt, "/audcp-iram@65006400", "reset_sel", "\x00\x00\x00\x04\x00\x00\x0b\xa8\xff\xff\xff\xff", 12);
	w202_audio_property(fdt, "/audcp-iram@65006400", "sysstatus", "\x00\x00\x00\x04\x00\x00\x05\x44\x00\x00\x1f\x00", 12);
	w202_audio_property(fdt, "/audcp-iram@65006400", "corestatus", "\x00\x00\x00\x04\x00\x00\x05\x44\x00\x1f\x00\x00", 12);
	w202_audio_property(fdt, "/audcp-iram@65006400", "sleepstatus", "\x00\x00\x00\x04\x00\x00\x08\x60\x00\x00\xf0\x00", 12);
	w202_audio_property(fdt, "/audcp-iram@65006400", "bootprotect", "\x00\x00\x00\x02\x00\x00\x00\x78\x00\x00\xff\xff", 12);
	w202_audio_property(fdt, "/audcp-iram@65006400", "bootvector", "\x00\x00\x00\x02\x00\x00\x01\x40\xff\xff\xff\xff", 12);
	w202_audio_property(fdt, "/audcp-iram@65006400", "bootaddress_sel", "\x00\x00\x00\x02\x00\x00\x01\x44\x00\x00\x00\x01", 12);
	w202_audio_property(fdt, "/audcp-iram@65006400", "ap-access", "\x00\x00\x00\x02\x00\x00\x01\x4c\x00\x00\x00\x20", 12);
	w202_audio_property(fdt, "/audcp-iram@65006400", "status", "\x6f\x6b\x61\x79\x00", 5);
	w202_audio_property(fdt, "/soc/clock-controller@56200000", "power-domains", "\x00\x00\x00\xf9", 4);
	w202_audio_property(fdt, "/soc/agcp/dma-controller@56650000", "power-domains", "\x00\x00\x00\xf9", 4);
	w202_audio_property(fdt, "/soc/agcp/dma-controller@56650000", "interrupts", "\x00\x00\x00\x00\x00\x00\x00\x38\x00\x00\x00\x04", 12);
	w202_audio_property(fdt, "/soc/agcp/dma-controller@56650000", "status", "\x6f\x6b\x61\x79\x00", 5);
	w202_audio_property(fdt, "/soc/agcp/vbc@56510000", "compatible", "\x7a\x74\x65\x2c\x77\x32\x30\x32\x64\x73\x2d\x76\x62\x63\x00", 15);
	w202_audio_property(fdt, "/soc/agcp/vbc@56510000", "#sound-dai-cells", "\x00\x00\x00\x00", 4);
	w202_audio_property(fdt, "/soc/agcp/vbc@56510000", "zte,audio-dsp", "\x00\x00\x00\xf9", 4);
	w202_audio_property(fdt, "/soc/agcp/vbc@56510000", "power-domains", "\x00\x00\x00\xf9", 4);
	w202_audio_property(fdt, "/soc/agcp/vbc@56510000", "clocks", "\x00\x00\x00\xbb\x00\x00\x00\x08\x00\x00\x00\xbb\x00\x00\x00\x0f\x00\x00\x00\xbb\x00\x00\x00\x15", 24);
	w202_audio_property(fdt, "/soc/agcp/vbc@56510000", "clock-names", "\x76\x62\x63\x00\x76\x62\x63\x2d\x32\x34\x6d\x00\x76\x62\x63\x2d\x61\x70\x00", 19);
	w202_audio_property(fdt, "/soc/agcp/vbc@56510000", "status", "\x6f\x6b\x61\x79\x00", 5);
	w202_audio_property(fdt, "/sprd-pcm-audio", "compatible", "\x7a\x74\x65\x2c\x77\x32\x30\x32\x64\x73\x2d\x70\x63\x6d\x00", 15);
	w202_audio_property(fdt, "/sprd-pcm-audio", "power-domains", "\x00\x00\x00\xf9", 4);
	w202_audio_property(fdt, "/sprd-pcm-audio", "dmas", "\x00\x00\x00\xf8\x00\x00\x00\x01\x00\x00\x00\xf8\x00\x00\x00\x02\x00\x00\x00\xf8\x00\x00\x00\x03\x00\x00\x00\xf8\x00\x00\x00\x04", 32);
	w202_audio_property(fdt, "/sprd-pcm-audio", "dma-names", "\x6e\x6f\x72\x6d\x61\x6c\x5f\x70\x5f\x6c\x00\x6e\x6f\x72\x6d\x61\x6c\x5f\x70\x5f\x72\x00\x6e\x6f\x72\x6d\x61\x6c\x5f\x63\x5f\x6c\x00\x6e\x6f\x72\x6d\x61\x6c\x5f\x63\x5f\x72\x00", 44);
	w202_audio_property(fdt, "/sprd-pcm-audio", "status", "\x6f\x6b\x61\x79\x00", 5);
	w202_audio_property(fdt, "/soc/aon/spi@400000/pmic@0/audio-codec@1000", "compatible", "\x7a\x74\x65\x2c\x77\x32\x30\x32\x64\x73\x2d\x6d\x69\x63\x2d\x63\x6f\x64\x65\x63\x00", 21);
	w202_audio_property(fdt, "/soc/aon/spi@400000/pmic@0/audio-codec@1000", "#sound-dai-cells", "\x00\x00\x00\x00", 4);
	w202_audio_property(fdt, "/soc/aon/spi@400000/pmic@0/audio-codec@1000", "power-domains", "\x00\x00\x00\xf9", 4);
	w202_audio_property(fdt, "/soc/aon/spi@400000/pmic@0/audio-codec@1000", "clocks", "\x00\x00\x00\xbb\x00\x00\x00\x0d\x00\x00\x00\xbb\x00\x00\x00\x0e", 16);
	w202_audio_property(fdt, "/soc/aon/spi@400000/pmic@0/audio-codec@1000", "clock-names", "\x61\x75\x64\x00\x61\x75\x64\x69\x66\x00", 10);
	w202_audio_property(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@34", "compatible", "\x61\x77\x69\x6e\x69\x63\x2c\x61\x77\x38\x38\x31\x36\x36\x00", 15);
	w202_audio_property(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@34", "#sound-dai-cells", "\x00\x00\x00\x00", 4);
	w202_audio_delete(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@34", "reset-gpio");
	w202_audio_delete(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@34", "sync-load");
	w202_audio_delete(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@34", "sound-channel");
	w202_audio_property(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@34", "power-domains", &amp_phandle, sizeof(amp_phandle));
	w202_audio_property(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@34", "awinic,audio-channel", "\x00\x00\x00\x00", 4);
	w202_audio_property(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@34", "awinic,sync-flag", "", 0);
	w202_audio_property(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@35", "compatible", "\x61\x77\x69\x6e\x69\x63\x2c\x61\x77\x38\x38\x31\x36\x36\x00", 15);
	w202_audio_property(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@35", "#sound-dai-cells", "\x00\x00\x00\x00", 4);
	w202_audio_delete(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@35", "sync-load");
	w202_audio_delete(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@35", "sound-channel");
	w202_audio_property(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@35", "power-domains", &amp_phandle, sizeof(amp_phandle));
	w202_audio_property(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@35", "awinic,audio-channel", "\x00\x00\x00\x01", 4);
	w202_audio_property(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@35", "awinic,sync-flag", "", 0);
	w202_audio_property(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@36", "compatible", "\x61\x77\x69\x6e\x69\x63\x2c\x61\x77\x38\x38\x31\x36\x36\x00", 15);
	w202_audio_property(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@36", "#sound-dai-cells", "\x00\x00\x00\x00", 4);
	w202_audio_delete(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@36", "sync-load");
	w202_audio_delete(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@36", "sound-channel");
	w202_audio_property(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@36", "power-domains", &amp_phandle, sizeof(amp_phandle));
	w202_audio_property(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@36", "awinic,audio-channel", "\x00\x00\x00\x02", 4);
	w202_audio_property(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@36", "awinic,sync-flag", "", 0);
	w202_audio_property(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@37", "compatible", "\x61\x77\x69\x6e\x69\x63\x2c\x61\x77\x38\x38\x31\x36\x36\x00", 15);
	w202_audio_property(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@37", "#sound-dai-cells", "\x00\x00\x00\x00", 4);
	w202_audio_delete(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@37", "sync-load");
	w202_audio_delete(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@37", "sound-channel");
	w202_audio_property(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@37", "power-domains", &amp_phandle, sizeof(amp_phandle));
	w202_audio_property(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@37", "awinic,audio-channel", "\x00\x00\x00\x03", 4);
	w202_audio_property(fdt, "/soc/ap-ahb/i2c@22c0000/aw883xx_smartpa@37", "awinic,sync-flag", "", 0);
	w202_audio_property(fdt, "/sound@0", "status", "\x64\x69\x73\x61\x62\x6c\x65\x64\x00", 9);
	w202_audio_property(fdt, "/sound@1", "status", "\x64\x69\x73\x61\x62\x6c\x65\x64\x00", 9);
	w202_audio_property(fdt, "/audio-mem-mgr", "status", "\x64\x69\x73\x61\x62\x6c\x65\x64\x00", 9);
	w202_audio_property(fdt, "/soc/agcp/agdsp-access", "status", "\x64\x69\x73\x61\x62\x6c\x65\x64\x00", 9);
	ret = fdt_pack(fdt);
	if (ret)
		panic("W202DS audio DT packing: %d", ret);
	*physical = __pa_symbol(w202_audio_fdt);
	pr_info("W202DS: boot-only speaker/microphone DT enabled; DP and other LK nodes preserved\n");
	return fdt;
}
