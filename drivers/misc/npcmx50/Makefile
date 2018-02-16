#
# Makefile for Nuvoton Miscellaneous optional drivers
#
obj-$(CONFIG_NPCMX50_GPIO)	         += aess_gpiodrv.o
obj-$(CONFIG_NPCMX50_KCS)	         += aess_kcsdrv.o
obj-$(CONFIG_NPCMX50_BIOS_POST_CODE) += aess_biospostdrv.o
obj-$(CONFIG_NPCMX50_PSPI)	         += aess_sspidrv.o
npcmx50_vdm-y := vdm_module.o vdm_common.o CircularBuffer.o ndelay-loop.o
obj-$(CONFIG_NPCMX50_VDM)	         += npcmx50_vdm.o
