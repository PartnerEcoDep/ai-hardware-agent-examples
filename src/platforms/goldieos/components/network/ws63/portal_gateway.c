/* BSP ships DHCP with LWIP_DHCPS_GW disabled in a precompiled archive.
 * Supply option 3 on AP DHCP replies, without changing vendor binaries. */
#include "dhcp_gateway.h"
#include "lwip/netifapi.h"
#include "lwip/pbuf.h"

static netif_output_fn downstream;
static struct netif *owner;
static err_t output(struct netif *netif, struct pbuf *p, const ip4_addr_t *dst) {
    unsigned char head[20], ports[4];
    if (pbuf_copy_partial(p,head,20,0)==20 && (head[0]>>4)==4 && head[9]==17 &&
        pbuf_copy_partial(p,ports,4,(head[0]&15)*4)==4 &&
        ports[0]==0 && ports[1]==67 && ports[2]==0 && ports[3]==68) {
        struct pbuf *copy=pbuf_alloc(PBUF_LINK,p->tot_len,PBUF_RAM);
        if (!copy) return ERR_MEM;
        err_t rc=pbuf_copy(copy,p);
        if (rc==ERR_OK && copy->len==copy->tot_len) {
            ws_dhcp_add_gateway(copy->payload,copy->tot_len,
                (const uint8_t *)&netif_ip4_addr(netif)->addr);
            rc=downstream(netif,copy,dst);
        } else if (rc==ERR_OK) rc=ERR_BUF;
        pbuf_free(copy);
        return rc;
    }
    return downstream(netif,p,dst);
}
static err_t install(struct netif *netif) {
    if (owner || !netif->output) return ERR_IF;
    downstream=netif->output; owner=netif; netif->output=output;
    return ERR_OK;
}
static err_t uninstall(struct netif *netif) {
    if (owner==netif) {
        if (netif->output==output) netif->output=downstream;
        owner=NULL;
    }
    return ERR_OK;
}
int ws_portal_gateway_start(struct netif *netif) {
    return netifapi_netif_common(netif,NULL,install)==ERR_OK;
}
void ws_portal_gateway_stop(struct netif *netif) { netifapi_netif_common(netif,NULL,uninstall); }
