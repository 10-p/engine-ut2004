/*
 * ue_proxynet_abi.h — the optional "proxynet" ABI module (C face).  PROXYNET_ABI_VERSION 1.
 * ============================================================================
 * An optional module, versioned on its own. It is not part of the main thin-shim ABI (../ue_abi.h): a
 * change here does not move UE_ABI_VERSION, ../ue_abi.h and the vendored net-shim.js know nothing of it,
 * and an engine that does not vendor this header neither knows nor cares that it exists (ADR-16).
 *
 * What it is for: "Proxy networking", a web client joining an ordinary native game server through a
 * WSS<->UDP game proxy. Experimental, and detachable by construction: the engine side is its own package
 * (ProxyNetDrv), the JS side its own library (packages/glue/src/proxynet/), and this module is the only
 * thing the two share. Without all three, P2P networking is unchanged.
 *
 * Absent module = absent feature, never a boot failure. The vendored proxynet-shim.js asserts
 * `Module.UE.proxynet.version === 1` on first use; a host that injects no `Module.UE.proxynet`
 * makes ue_proxynet_connect() return -1.
 *
 * Same conventions as ../ue_abi.h: UE_IMPORT = the engine calls, JS provides. Every boundary
 * function stays on one line: the parity test (test/abi-proxynet-parity.test.cjs) keys off the markers.
 * Pointers cross as wasm addresses; strings in are NUL-terminated UTF-8; (ptr,max) pairs are
 * caller-owned buffers the callee fills up to max. Everything is non-blocking and polled: no Asyncify.
 */
#ifndef UE_PROXYNET_ABI_H
#define UE_PROXYNET_ABI_H

#define UE_PROXYNET_ABI_VERSION 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef UE_IMPORT
#define UE_IMPORT extern
#endif

/* ue_proxynet_state() return values. */
#define PROXYNET_STATE_INVALID    0   /* no such connection */
#define PROXYNET_STATE_CONNECTING 1   /* proxy reached or queued; the UDP socket is not open yet */
#define PROXYNET_STATE_OPEN       2   /* datagrams flow */
#define PROXYNET_STATE_CLOSED     3   /* ended: refused, lost past the resume window, or closed */

/* ue_proxynet_http_poll() status values. */
#define PROXYNET_HTTP_PENDING     0   /* no response yet */
#define PROXYNET_HTTP_RECEIVING   1   /* headers in; *total is the body size, or -1 if unknown */
#define PROXYNET_HTTP_DONE        2   /* whole body delivered to the read queue */
#define PROXYNET_HTTP_FAILED      3   /* refused / unreachable / over budget — fall back to the in-band download */

/* ===================== ABI-PARITY: imports (C->JS) ========================= *
 * The datagram pipe. connect() returns a new connection id each time (>= 0), or -1 when the module is
 * absent or proxy networking is not configured, so, unlike the P2P transport's single client slot, an
 * old and a new connection coexist across a server map change as two UDP sockets do.
 * send/recv are the hot path: not proxied to the main thread, served from the shared packet rings.
 */
UE_IMPORT int  ue_proxynet_connect(const char* host, int port);
UE_IMPORT int  ue_proxynet_state(int connId);
UE_IMPORT int  ue_proxynet_send(int connId, const void* data, int len);
UE_IMPORT int  ue_proxynet_recv(int connId, void* buffer, int maxLen);
UE_IMPORT void ue_proxynet_close(int connId);
/* Redirect downloads (a server's DLMGR names IpDrv.HTTPDownload). open() returns a handle >= 0 or -1.
 * poll() fills *status (PROXYNET_HTTP_*), *received (bytes delivered so far) and *total. read() copies
 * up to maxLen queued body bytes and returns the count (0 = nothing queued right now). */
UE_IMPORT int  ue_proxynet_http_open(int connId, const char* url);
UE_IMPORT int  ue_proxynet_http_poll(int handle, int* status, int* received, int* total);
UE_IMPORT int  ue_proxynet_http_read(int handle, void* buffer, int maxLen);
UE_IMPORT void ue_proxynet_http_close(int handle);
/* The in-game server browser's list: the chosen proxy's /servers, as one text blob (the web build has no
 * sockets, so the engine cannot ask a master itself). open() returns a handle >= 0, or -1 when proxy networking is
 * dormant (P2P mode, or no proxy). poll() fills *status (PROXYNET_HTTP_PENDING / DONE / FAILED) and *count (rows).
 * read() returns the blob's size and copies it only when it fits in maxLen (call again with a bigger buffer);
 * 0 = not ready. Blob: "#proxynet-list 1 ping=<ms>\n", then one row per line, tab-separated:
 * host, port (game), name, map, gameType, players, maxPlayers, version, minVersion, password (0/1); -1 = unknown. */
UE_IMPORT int  ue_proxynet_list_open(void);
UE_IMPORT int  ue_proxynet_list_poll(int handle, int* status, int* count);
UE_IMPORT int  ue_proxynet_list_read(int handle, void* buffer, int maxLen);
UE_IMPORT void ue_proxynet_list_close(int handle);
/* ===================== ABI-PARITY: end imports =========================== */

#ifdef __cplusplus
}
#endif

#endif /* UE_PROXYNET_ABI_H */
