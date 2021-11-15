/*
 * harpoon.h <z64.me>
 *
 * a wrapper for interfacing
 * with a Corsair Harpoon mouse
 *
 */

#include <stdint.h>
#include <stdbool.h>

struct harpoon; /* opaque structure */
typedef uint8_t harpoonPacket;

/* signal generation */
const harpoonPacket *harpoonPacket_dpiconfig(uint8_t index, unsigned x, unsigned y, uint8_t r, uint8_t g, uint8_t b);
const harpoonPacket *harpoonPacket_dpisetenabled(bool m0, bool m1, bool m2, bool m3, bool m4, bool m5);
const harpoonPacket *harpoonPacket_color(uint8_t r, uint8_t g, uint8_t b);
const harpoonPacket *harpoonPacket_pollrate(uint8_t msec);
const harpoonPacket *harpoonPacket_dpimode(uint8_t index);

void harpoon_monitor(struct harpoon *hp);
void harpoon_set_onConnect(struct harpoon *hp, void onConnect(void *udata), void *udata);
void harpoon_set_onDisconnect(struct harpoon *hp, void onDisconnect(void *udata), void *udata);
int harpoon_send(struct harpoon *hp, const harpoonPacket *sig);
const char *harpoon_connect(struct harpoon *hp);
void harpoon_disconnect(struct harpoon *hp);
int harpoon_isConnected(struct harpoon *hp);
void harpoon_delete(struct harpoon *hp);
struct harpoon *harpoon_new(void);

