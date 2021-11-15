/*
 * monitor.c <z64.me>
 *
 * a simple test program for monitoring
 * the mouse's connection status
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "harpoon.h"

static void onConnect(void *udata)
{
	struct harpoon *hp = udata;
	int index = 1;
	int precision = 1000;
	unsigned color = -1;
	
	fprintf(stderr, "onConnect\n");
	
	harpoon_send(hp, harpoonPacket_dpiconfig(
		index
		, precision /* x, y */
		, precision
		, color >> 16 /* r, g, b */
		, color >> 8
		, color
	));
	harpoon_send(hp, harpoonPacket_dpimode(index)); /* use new mode */
	harpoon_send(hp, harpoonPacket_dpisetenabled(
		false
		, false
		, false
		, false
		, false
		, false
	));
}

static void onDisconnect(void *udata)
{
	fprintf(stderr, "onDisconnect\n");
	
	(void)udata;
}

int main(void)
{
	struct harpoon *hp;
	
	hp = harpoon_new();
	
	harpoon_set_onDisconnect(hp, onDisconnect, hp);
	harpoon_set_onConnect(hp, onConnect, hp);
	
	while (1)
	{
		system("sleep 0.1s");
		harpoon_monitor(hp);
	}
	
	harpoon_delete(hp);
	
	return 0;
}

