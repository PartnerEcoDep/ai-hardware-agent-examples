#include <assert.h>
#include <stdio.h>
#include "../src/platforms/goldieos/components/convai/ws63/delivery_diag.h"

int main(void) {
    ws63_delivery_diag_t d = {0};
    /* A two-byte WS header read can leave the rest of a TLS record cached.
     * WRITE readiness must not hide the absence of READ readiness. */
    ws63_delivery_read(&d, 100, 2, 2, false, 1022);
    ws63_delivery_poll(&d, true, false, true);
    assert(d.window.pending_wait == 1 && d.window.read_ready == 0);
    assert(d.window.write_ready == 1 && d.window.plain_rx == 2);
    ws63_delivery_poll(&d, false, false, true);
    ws63_delivery_poll(&d, true, true, false);
    assert(d.window.pending_wait == 1 && d.window.polls == 3);
    assert(!ws63_delivery_due(&d, 999) && ws63_delivery_due(&d, 1000));
    ws63_delivery_reported(&d, 1000);
    assert(d.window.polls == 0 && d.window.plain_rx == 0);
    assert(d.read_seen && d.pending == 1022 && d.read_at == 100);
    ws63_delivery_poll(&d, true, false, false);
    assert(d.window.pending_wait == 1); /* Quiet windows retain the snapshot. */
    ws63_delivery_read(&d, 1100, 1, 1022, false, 0);
    ws63_delivery_poll(&d, true, false, false);
    assert(d.window.pending_wait == 1 && d.pending == 0);
    ws63_delivery_read(&d, 1200, 3, -123, true, 0);
    ws63_delivery_read(&d, 1300, 7, -456, false, 0);
    ws63_delivery_read(&d, 1400, 0, 0, false, 0);
    assert(d.window.reads == 4 && d.window.wants == 1 && d.window.errors == 2);
    assert(d.window.plain_rx == 1022 && d.window.read_max_ms == 7);
    /* Rate limiting must survive the 32-bit monotonic clock wrapping. */
    d.report_at = UINT32_MAX - 499;
    assert(!ws63_delivery_due(&d, 499));
    assert(ws63_delivery_due(&d, 500));
    puts("WS63 TLS delivery counters and quiet/wrap windows passed");
}
