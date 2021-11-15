/*
 * harpoon.c <z64.me>
 *
 * a wrapper for interfacing
 * with a Corsair Harpoon mouse
 *
 */

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <libusb-1.0/libusb.h>

#include "harpoon.h"

struct harpoon
{
	libusb_device_handle *device;
	libusb_context *context;
	void (*onConnect)(void *udata);
	void (*onDisconnect)(void *udata);
	void *onConnect_udata;
	void *onDisconnect_udata;
};

/* device info */
#define idVendor   0x1b1c
#define idProduct  0x1b3c

/* output interface */
#define out_bInterfaceNumber  1
#define out_bEndpointAddress  0x02 /* EP 2 OUT */
#define out_wMaxPacketSize    0x0040

/*
 *
 * private
 *
 */

static void (*harpoonPacket__defer)(struct harpoon *hp) = 0;

/* fatal error message */
static void die(const char *fmt, ...)
{
	va_list ap;
	
	if (!fmt)
		exit(EXIT_FAILURE);
		
	fprintf(stderr, "[!] ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	
	exit(EXIT_FAILURE);
}

/* this deferred function gives the mouse time to restart before reconnecting */
static void harpoonPacket__defer_pollrate(struct harpoon *hp)
{
	harpoon_disconnect(hp);
	
#ifdef HARPOON_NO_MAIN_LOOP /* program has no main loop, so wait here */
	const char *errstr;
	
	system("sleep 2s");
	
	/* changing the mouse's polling rate causes it to be restarted,
	 * and therefore the connection is lost; attempt reconnecting!
	 */
	fprintf(stderr, "reconnecting...\n");
	if ((errstr = harpoon_connect(hp)))
		die("%s", errstr);
#endif
}

/*
 *
 * public
 *
 */

/* construct LED color packet */
const harpoonPacket *harpoonPacket_color(uint8_t r, uint8_t g, uint8_t b)
{
	static harpoonPacket out[out_wMaxPacketSize];
	uint8_t tmp[out_wMaxPacketSize] =
	{
		0x07, 0x22, 0x01, 0x01, 0x03, r, g, b
	};
	
	return memcpy(out, tmp, out_wMaxPacketSize);
}

/* construct a polling rate packet */
const harpoonPacket *harpoonPacket_pollrate(uint8_t msec)
{
	static harpoonPacket out[out_wMaxPacketSize];
	uint8_t tmp[out_wMaxPacketSize] =
	{
		0x07, 0x0a, 0x00, 0x00, msec
	};
	
	/* changing the mouse's polling rate causes it to be restarted;
	 * this deferred function helps reconnect to it afterwards
	 */
	harpoonPacket__defer = harpoonPacket__defer_pollrate;
	
	return memcpy(out, tmp, out_wMaxPacketSize);
}

/* construct a DPI mode switch packet */
const harpoonPacket *harpoonPacket_dpimode(uint8_t index)
{
	static harpoonPacket out[out_wMaxPacketSize];
	uint8_t tmp[out_wMaxPacketSize] =
	{
		0x07, 0x13, 0x02, 0x00, index
	};
	
	return memcpy(out, tmp, out_wMaxPacketSize);
}

/* construct a DPI configuration packet */
const harpoonPacket *harpoonPacket_dpiconfig(uint8_t index, unsigned x, unsigned y, uint8_t r, uint8_t g, uint8_t b)
{
	static harpoonPacket out[out_wMaxPacketSize];
	uint8_t tmp[out_wMaxPacketSize] =
	{
		0x07, 0x13, 0xd0 | index, 0x00, 0x00
		, (x & 0xff) << 8, (x & 0xff00) >> 8 /* XXX ensure Little Endian byte order */
		, (y & 0xff) << 8, (y & 0xff00) >> 8
		, r, g, b
	};
	
	return memcpy(out, tmp, out_wMaxPacketSize);
}

/* construct a packet indicating which DPI modes are enabled */
const harpoonPacket *harpoonPacket_dpisetenabled(bool m0, bool m1, bool m2, bool m3, bool m4, bool m5)
{
	static harpoonPacket out[out_wMaxPacketSize];
	uint8_t tmp[out_wMaxPacketSize] =
	{
		0x07, 0x13, 0x05, 0x00
		, m0
			| (m1 << 1)
			| (m2 << 2)
			| (m3 << 3)
			| (m4 << 4)
			| (m5 << 5)
	};
	
	return memcpy(out, tmp, out_wMaxPacketSize);
}

void harpoon_delete(struct harpoon *hp)
{
	if (!hp)
		return;
	
	/* cleanup */
	libusb_release_interface(hp->device, out_bInterfaceNumber);
	
	harpoon_disconnect(hp);
	
	libusb_exit(hp->context);
	free(hp);
}

void harpoon_disconnect(struct harpoon *hp)
{
	if (hp->device)
		libusb_close(hp->device);
	hp->device = 0;
	
	if (hp->onDisconnect)
		hp->onDisconnect(hp->onDisconnect_udata);
}

const char *harpoon_connect(struct harpoon *hp)
{
	int errcode;
	
	assert(hp);
	
	/* reinitialize to zero */
	if (hp->device)
		libusb_close(hp->device);
	hp->device = 0;
	
	/* fetch device */
	if (!(hp->device = libusb_open_device_with_vid_pid(hp->context, idVendor, idProduct)))
		return "libusb_open_device_with_vid_pid failed; is device plugged in?";
	
	/* tell libusb to automatically detach kernel driver when
	 * interface is claimed, and reattach when interface is released
	 */
	if ((errcode = libusb_set_auto_detach_kernel_driver(hp->device, true)))
		return "libusb_set_auto_detach_kernel_driver failed";
	
	/* now attempt to claim the interface */
	if ((errcode = libusb_claim_interface(hp->device, out_bInterfaceNumber)))
		return "libusb_claim_interface failed";
	
	if (hp->onConnect)
		hp->onConnect(hp->onConnect_udata);
	
	return 0;
}

struct harpoon *harpoon_new(void)
{
	struct harpoon *hp = 0; /* misc */
	int errcode = 0;
	
	if (!(hp = calloc(1, sizeof(*hp))))
		die("memory error");
	
	/* initialize libusb context */
	if ((errcode = libusb_init(&hp->context)))
		die("libusb_init failed");
#ifndef NDEBUG
	libusb_set_option(hp->context, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_INFO);
#endif
	
	return hp;
}

int harpoon_send(struct harpoon *hp, const harpoonPacket *sig)
{
#define RETURN(X) { rval = X; goto L_return; }
	int errcode;
	int sent;
	int rval = 0;
	
	assert(hp);
	assert(sig);
	
	if (!hp->device)
		RETURN(1);
	
	/* transfer color code to mouse */
	if ((errcode = libusb_bulk_transfer(
			hp->device
			, out_bEndpointAddress | LIBUSB_ENDPOINT_OUT
			, (void*)sig
			, out_wMaxPacketSize
			, &sent
			, 0 /* no timeout */
		))
		|| sent != out_wMaxPacketSize
	)
	{
		RETURN(1);
	}
	
	if (harpoonPacket__defer)
		harpoonPacket__defer(hp);
	
L_return:
	harpoonPacket__defer = 0;
	return rval;
}

void harpoon_set_onConnect(struct harpoon *hp, void onConnect(void *udata), void *udata)
{
	assert(hp);
	
	hp->onConnect = onConnect;
	hp->onConnect_udata = udata;
}

void harpoon_set_onDisconnect(struct harpoon *hp, void onDisconnect(void *udata), void *udata)
{
	assert(hp);
	
	hp->onDisconnect = onDisconnect;
	hp->onDisconnect_udata = udata;
}

int harpoon_isConnected(struct harpoon *hp)
{
	libusb_device *d;
	
	if (!hp)
		return 0;
	
	assert(hp);
	
	if (!hp->device)
		return 0;

	if (!(d = libusb_get_device(hp->device)))
		return 0;
	
	return libusb_get_max_packet_size(d, out_bEndpointAddress) > 0;
}

void harpoon_monitor(struct harpoon *hp)
{
	if (hp->device)
	{
		if (!harpoon_isConnected(hp))
			harpoon_disconnect(hp);
	}
	else
		harpoon_connect(hp);
}

