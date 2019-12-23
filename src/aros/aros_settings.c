/*
    $Id$
*/

#define DEBUG 1
#include <aros/debug.h>

#include <proto/exec.h>

#include <exec/types.h>
#include <dos/dos.h>

#include <pthread.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

#include "../86box.h"
#include "../config.h"
#include "../cpu/cpu.h"
#include "../mem.h"
#include "../rom.h"
#include "../device.h"
#include "../timer.h"
#include "../nvr.h"
#include "../machine/machine.h"
#include "../game/gameport.h"
#include "../isamem.h"
#include "../isartc.h"
#include "../lpt.h"
#include "../mouse.h"
#include "../scsi/scsi.h"
#include "../scsi/scsi_device.h"
#include "../cdrom/cdrom.h"
#include "../disk/hdd.h"
#include "../disk/hdc.h"
#include "../disk/hdc_ide.h"
#include "../disk/zip.h"
#include "../floppy/fdd.h"
#include "../network/network.h"
#include "../sound/sound.h"
#include "../sound/midi.h"
#include "../sound/snd_mpu401.h"
#include "../video/video.h"
#include "../video/vid_voodoo.h"
#include "../plat.h"
#include "../plat_midi.h"
#include "../ui.h"

/* Machine category */
static int temp_machine, temp_cpu_m, temp_cpu, temp_wait_states, temp_fpu, temp_sync;
static uint32_t temp_mem_size;
#ifdef USE_DYNAREC
static int temp_dynarec;
#endif

/* Video category */
static int temp_gfxcard, temp_voodoo;

/* Input devices category */
static int temp_mouse, temp_joystick;

/* Sound category */
static int temp_sound_card, temp_midi_device, temp_mpu401, temp_SSI2001, temp_GAMEBLASTER, temp_GUS;
static int temp_float;

/* Network category */
static int temp_net_type, temp_net_card;
static char temp_pcap_dev[522];

/* Ports category */
static int temp_lpt_devices[3];
static int temp_serial[2], temp_lpt[3];

/* Other peripherals category */
static int temp_hdc, temp_scsi_card, temp_ide_ter, temp_ide_qua;
static int temp_bugger;
static int temp_isartc;
static int temp_isamem[ISAMEM_MAX];

static uint8_t temp_deviceconfig;

/* Hard disks category */
static hard_disk_t temp_hdd[HDD_NUM];

/* Floppy drives category */
static int temp_fdd_types[FDD_NUM];
static int temp_fdd_turbo[FDD_NUM];
static int temp_fdd_check_bpb[FDD_NUM];

/* Other removable devices category */
static cdrom_t temp_cdrom[CDROM_NUM];
static zip_drive_t temp_zip_drives[ZIP_NUM];

/* This does the initial read of global variables into the temporary ones. */
static void
aros_settings_init(void)
{
    int i = 0;

    /* Machine category */
    temp_machine = machine;
    temp_cpu_m = cpu_manufacturer;
    temp_wait_states = cpu_waitstates;
    temp_cpu = cpu;
    temp_mem_size = mem_size;
#ifdef USE_DYNAREC
    temp_dynarec = cpu_use_dynarec;
#endif
    temp_fpu = enable_external_fpu;
    temp_sync = time_sync;

    /* Video category */
    temp_gfxcard = gfxcard;
    temp_voodoo = voodoo_enabled;

    /* Input devices category */
    temp_mouse = mouse_type;
    temp_joystick = joystick_type;

    /* Sound category */
    temp_sound_card = sound_card_current;
    temp_midi_device = midi_device_current;
    temp_mpu401 = mpu401_standalone_enable;
    temp_SSI2001 = SSI2001;
    temp_GAMEBLASTER = GAMEBLASTER;
    temp_GUS = GUS;
    temp_float = sound_is_float;

    /* Network category */
    temp_net_type = network_type;
    memset(temp_pcap_dev, 0, sizeof(temp_pcap_dev));
#ifdef ENABLE_SETTINGS_LOG
    assert(sizeof(temp_pcap_dev) == sizeof(network_host));
#endif
    memcpy(temp_pcap_dev, network_host, sizeof(network_host));
    temp_net_card = network_card;

    /* Ports category */
    for (i = 0; i < 3; i++) {
	temp_lpt_devices[i] = lpt_ports[i].device;
	temp_lpt[i] = lpt_ports[i].enabled;
    }
    for (i = 0; i < 2; i++)
	temp_serial[i] = serial_enabled[i];

    /* Other peripherals category */
    temp_scsi_card = scsi_card_current;
    temp_hdc = hdc_current;
    temp_ide_ter = ide_ter_enabled;
    temp_ide_qua = ide_qua_enabled;
    temp_bugger = bugger_enabled;
    temp_isartc = isartc_type;
	
    /* ISA memory boards. */
    for (i = 0; i < ISAMEM_MAX; i++)
 	temp_isamem[i] = isamem_type[i];	
	
#if (0)
    mfm_tracking = xta_tracking = esdi_tracking = ide_tracking = 0;
    for (i = 0; i < 2; i++)
	scsi_tracking[i] = 0;
#endif
    /* Hard disks category */
    memcpy(temp_hdd, hdd, HDD_NUM * sizeof(hard_disk_t));
#if (0)
    for (i = 0; i < HDD_NUM; i++) {
	if (hdd[i].bus == HDD_BUS_MFM)
		mfm_tracking |= (1 << (hdd[i].mfm_channel << 3));
	else if (hdd[i].bus == HDD_BUS_XTA)
		xta_tracking |= (1 << (hdd[i].xta_channel << 3));
	else if (hdd[i].bus == HDD_BUS_ESDI)
		esdi_tracking |= (1 << (hdd[i].esdi_channel << 3));
	else if (hdd[i].bus == HDD_BUS_IDE)
		ide_tracking |= (1 << (hdd[i].ide_channel << 3));
	else if (hdd[i].bus == HDD_BUS_SCSI)
		scsi_tracking[hdd[i].scsi_id >> 3] |= (1 << ((hdd[i].scsi_id & 0x07) << 3));
    }	
#endif
	
    /* Floppy drives category */
    for (i = 0; i < FDD_NUM; i++) {
	temp_fdd_types[i] = fdd_get_type(i);
	temp_fdd_turbo[i] = fdd_get_turbo(i);
	temp_fdd_check_bpb[i] = fdd_get_check_bpb(i);
    }

    /* Other removable devices category */
    memcpy(temp_cdrom, cdrom, CDROM_NUM * sizeof(cdrom_t));
#if (0)
    for (i = 0; i < CDROM_NUM; i++) {
	if (cdrom[i].bus_type == CDROM_BUS_ATAPI)
		ide_tracking |= (2 << (cdrom[i].ide_channel << 3));
	else if (cdrom[i].bus_type == CDROM_BUS_SCSI)
		scsi_tracking[cdrom[i].scsi_device_id >> 3] |= (1 << ((cdrom[i].scsi_device_id & 0x07) << 3));
    }
#endif
    memcpy(temp_zip_drives, zip_drives, ZIP_NUM * sizeof(zip_drive_t));
#if (0)
    for (i = 0; i < ZIP_NUM; i++) {
	if (zip_drives[i].bus_type == ZIP_BUS_ATAPI)
		ide_tracking |= (4 << (zip_drives[i].ide_channel << 3));
	else if (zip_drives[i].bus_type == ZIP_BUS_SCSI)
		scsi_tracking[zip_drives[i].scsi_device_id >> 3] |= (1 << ((zip_drives[i].scsi_device_id & 0x07) << 3));
    }
#endif

    temp_deviceconfig = 0;
}


/* This returns 1 if any variable has changed, 0 if not. */
static int
aros_settings_changed(void)
{
    int i = 0;
    int j = 0;

    /* Machine category */
    i = i || (machine != temp_machine);
    i = i || (cpu_manufacturer != temp_cpu_m);
    i = i || (cpu_waitstates != temp_wait_states);
    i = i || (cpu != temp_cpu);
    i = i || (mem_size != temp_mem_size);
#ifdef USE_DYNAREC
    i = i || (temp_dynarec != cpu_use_dynarec);
#endif
    i = i || (temp_fpu != enable_external_fpu);
    i = i || (temp_sync != time_sync);

    /* Video category */
    i = i || (gfxcard != temp_gfxcard);
    i = i || (voodoo_enabled != temp_voodoo);

    /* Input devices category */
    i = i || (mouse_type != temp_mouse);
    i = i || (joystick_type != temp_joystick);

    /* Sound category */
    i = i || (sound_card_current != temp_sound_card);
    i = i || (midi_device_current != temp_midi_device);
    i = i || (mpu401_standalone_enable != temp_mpu401);
    i = i || (SSI2001 != temp_SSI2001);
    i = i || (GAMEBLASTER != temp_GAMEBLASTER);
    i = i || (GUS != temp_GUS);
    i = i || (sound_is_float != temp_float);

    /* Network category */
    i = i || (network_type != temp_net_type);
    i = i || strcmp(temp_pcap_dev, network_host);
    i = i || (network_card != temp_net_card);

    /* Ports category */
    for (j = 0; j < 3; j++) {
	i = i || (temp_lpt_devices[j] != lpt_ports[j].device);
	i = i || (temp_lpt[j] != lpt_ports[j].enabled);
    }
    for (j = 0; j < 2; j++)
	i = i || (temp_serial[j] != serial_enabled[j]);

    /* Peripherals category */
    i = i || (scsi_card_current != temp_scsi_card);
    i = i || (hdc_current != temp_hdc);
    i = i || (temp_ide_ter != ide_ter_enabled);
    i = i || (temp_ide_qua != ide_qua_enabled);
    i = i || (temp_bugger != bugger_enabled);
    i = i || (temp_isartc != isartc_type);

    /* ISA memory boards. */
    for (j = 0; j < ISAMEM_MAX; j++)
 	i = i || (temp_isamem[j] != isamem_type[j]);
	
    /* Hard disks category */
    i = i || memcmp(hdd, temp_hdd, HDD_NUM * sizeof(hard_disk_t));

    /* Floppy drives category */
    for (j = 0; j < FDD_NUM; j++) {
	i = i || (temp_fdd_types[j] != fdd_get_type(j));
	i = i || (temp_fdd_turbo[j] != fdd_get_turbo(j));
	i = i || (temp_fdd_check_bpb[j] != fdd_get_check_bpb(j));
    }

    /* Other removable devices category */
    i = i || memcmp(cdrom, temp_cdrom, CDROM_NUM * sizeof(cdrom_t));
    i = i || memcmp(zip_drives, temp_zip_drives, ZIP_NUM * sizeof(zip_drive_t));

    i = i || !!temp_deviceconfig;

    return i;
}

/* This saves the settings back to the global variables. */
static void
aros_settings_save(void)
{
    int i = 0;

    pc_reset_hard_close();

    /* Machine category */
    machine = temp_machine;
    cpu_manufacturer = temp_cpu_m;
    cpu_waitstates = temp_wait_states;
    cpu = temp_cpu;
    mem_size = temp_mem_size;
#ifdef USE_DYNAREC
    cpu_use_dynarec = temp_dynarec;
#endif
    enable_external_fpu = temp_fpu;
    time_sync = temp_sync;

    /* Video category */
    gfxcard = temp_gfxcard;
    voodoo_enabled = temp_voodoo;

    /* Input devices category */
    mouse_type = temp_mouse;
    joystick_type = temp_joystick;

    /* Sound category */
    sound_card_current = temp_sound_card;
    midi_device_current = temp_midi_device;
    mpu401_standalone_enable = temp_mpu401;
    SSI2001 = temp_SSI2001;
    GAMEBLASTER = temp_GAMEBLASTER;
    GUS = temp_GUS;
    sound_is_float = temp_float;

    /* Network category */
    network_type = temp_net_type;
    memset(network_host, '\0', sizeof(network_host));
    strcpy(network_host, temp_pcap_dev);
    network_card = temp_net_card;

    /* Ports category */
    for (i = 0; i < 3; i++) {
	lpt_ports[i].device = temp_lpt_devices[i];
	lpt_ports[i].enabled = temp_lpt[i];
    }
    for (i = 0; i < 2; i++)
	serial_enabled[i] = temp_serial[i];

    /* Peripherals category */
    scsi_card_current = temp_scsi_card;
    hdc_current = temp_hdc;
    ide_ter_enabled = temp_ide_ter;
    ide_qua_enabled = temp_ide_qua;
    bugger_enabled = temp_bugger;
    isartc_type = temp_isartc;

    /* ISA memory boards. */
    for (i = 0; i < ISAMEM_MAX; i++)
 	isamem_type[i] = temp_isamem[i];	
	
    /* Hard disks category */
    memcpy(hdd, temp_hdd, HDD_NUM * sizeof(hard_disk_t));
    for (i = 0; i < HDD_NUM; i++)
	hdd[i].priv = NULL;

    /* Floppy drives category */
    for (i = 0; i < FDD_NUM; i++) {
	fdd_set_type(i, temp_fdd_types[i]);
	fdd_set_turbo(i, temp_fdd_turbo[i]);
	fdd_set_check_bpb(i, temp_fdd_check_bpb[i]);
    }

    /* Removable devices category */
    memcpy(cdrom, temp_cdrom, CDROM_NUM * sizeof(cdrom_t));
    for (i = 0; i < CDROM_NUM; i++) {
	cdrom[i].img_fp = NULL;
	cdrom[i].priv = NULL;
	cdrom[i].ops = NULL;
	cdrom[i].image = NULL;
	cdrom[i].insert = NULL;
	cdrom[i].close = NULL;
	cdrom[i].get_volume = NULL;
	cdrom[i].get_channel = NULL;
    }
    memcpy(zip_drives, temp_zip_drives, ZIP_NUM * sizeof(zip_drive_t));
    for (i = 0; i < ZIP_NUM; i++) {
	zip_drives[i].f = NULL;
	zip_drives[i].priv = NULL;
    }

    /* Mark configuration as changed. */
    config_changed = 1;

    pc_reset_hard_init();
}
