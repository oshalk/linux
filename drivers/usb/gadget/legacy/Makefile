#
# USB gadget drivers
#
ifndef RUN_ONLY_ONCE
 USB_GADGET_DIR := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
 $(shell ln -s -f $(USB_GADGET_DIR)/mass_storage.c $(USB_GADGET_DIR)/mass_storage1.c )
 $(shell ln -s -f $(USB_GADGET_DIR)/mass_storage.c $(USB_GADGET_DIR)/mass_storage2.c )
 RUN_ONLY_ONCE := 1
endif

ccflags-y			:= -I$(srctree)/drivers/usb/gadget/
ccflags-y			+= -I$(srctree)/drivers/usb/gadget/udc/
ccflags-y			+= -I$(srctree)/drivers/usb/gadget/function/

g_zero-y			:= zero.o
g_audio-y			:= audio.o
g_ether-y			:= ether.o
g_serial-y			:= serial.o
g_midi-y			:= gmidi.o
gadgetfs-y			:= inode.o
g_mass_storage-y		:= mass_storage.o
g_mass_storage1-y		:= mass_storage1.o
g_mass_storage2-y		:= mass_storage2.o
g_edm_kbdmouse-y		:= edmKbdMouse.o
g_printer-y			:= printer.o
g_cdc-y				:= cdc2.o
g_multi-y			:= multi.o
g_hid-y				:= hid.o
g_dbgp-y			:= dbgp.o
g_nokia-y			:= nokia.o
g_webcam-y			:= webcam.o
g_ncm-y				:= ncm.o
g_acm_ms-y			:= acm_ms.o
g_tcm_usb_gadget-y		:= tcm_usb_gadget.o

obj-$(CONFIG_USB_ZERO)		+= g_zero.o
obj-$(CONFIG_USB_AUDIO)		+= g_audio.o
obj-$(CONFIG_USB_ETH)		+= g_ether.o
obj-$(CONFIG_USB_GADGETFS)	+= gadgetfs.o
obj-$(CONFIG_USB_FUNCTIONFS)	+= g_ffs.o
obj-$(CONFIG_USB_MASS_STORAGE)	+= g_mass_storage.o
obj-$(CONFIG_USB_MASS_STORAGE)	+= g_mass_storage1.o
obj-$(CONFIG_USB_MASS_STORAGE)	+= g_mass_storage2.o
obj-$(CONFIG_USB_EDM_KBD_MOUSE)	+= g_edm_kbdmouse.o
obj-$(CONFIG_USB_G_SERIAL)	+= g_serial.o
obj-$(CONFIG_USB_G_PRINTER)	+= g_printer.o
obj-$(CONFIG_USB_MIDI_GADGET)	+= g_midi.o
obj-$(CONFIG_USB_CDC_COMPOSITE) += g_cdc.o
obj-$(CONFIG_USB_G_HID)		+= g_hid.o
obj-$(CONFIG_USB_G_DBGP)	+= g_dbgp.o
obj-$(CONFIG_USB_G_MULTI)	+= g_multi.o
obj-$(CONFIG_USB_G_NOKIA)	+= g_nokia.o
obj-$(CONFIG_USB_G_WEBCAM)	+= g_webcam.o
obj-$(CONFIG_USB_G_NCM)		+= g_ncm.o
obj-$(CONFIG_USB_G_ACM_MS)	+= g_acm_ms.o
obj-$(CONFIG_USB_GADGET_TARGET)	+= tcm_usb_gadget.o
