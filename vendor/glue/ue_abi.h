/*
 * ue_abi.h — the engine↔glue thin-shim ABI (C face).  ABI_VERSION 1.
 * ============================================================================
 * SINGLE SOURCE OF TRUTH lives in packages/glue/abi/ (docs/02 §2). This header
 * is the C declaration of the boundary; abi.ts is the TypeScript mirror. A
 * parity test (packages/glue/test/abi-parity.test.cjs) fails if the two faces
 * enumerate a different set of {name, arity, direction} — they can never
 * silently drift (docs/02 §2, the "Linux-kernel model").
 *
 * Each engine VENDORS this header at a pinned ABI_VERSION (docs/05 §2). Changing
 * anything here is a deliberate act: bump ABI_VERSION + re-link every engine
 * (docs/02 §5). Day-to-day glue work never touches this file.
 *
 *  - UE_IMPORT : a C function the engine CALLS and JS PROVIDES (the baked thin
 *                shim forwards it to Module.UE.*). The WASM imports these.
 *  - UE_EXPORT : a C function the engine PROVIDES and JS CALLS (EMSCRIPTEN_KEEPALIVE
 *                / ccall). The WASM exports these.
 *
 * The parity test keys off the UE_IMPORT / UE_EXPORT markers, so keep every
 * boundary function on ONE line in the marked blocks below.
 *
 * NOT enumerated here (asymmetric — they have no clean named-C-symbol pair;
 * documented in README.md instead, guarded by the 1.2 spike + E2E, not parity):
 *   - FetchFS: JS-level FS backend (FS.open/stat/close/readdir monkey-patch +
 *     whole-file first-touch). Today's on-demand open is SYNCHRONOUS (sync XHR,
 *     "D-16") — Asyncify-in-FS.open traps. __syscall_openat is in ASYNCIFY_IMPORTS
 *     but the on-demand download does not actually suspend through it; the live
 *     Asyncify use is main-loop inversion (emscripten_sleep / set_main_loop).
 *   - Status / download-progress: emitted via inline EM_ASM today (formalized to
 *     Module.UE.on when the host shell lands, step 1.6).
 */
#ifndef UE_ABI_H
#define UE_ABI_H

#define UE_ABI_VERSION 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef UE_IMPORT
#define UE_IMPORT extern
#endif
#ifndef UE_EXPORT
#define UE_EXPORT
#endif

/* webrtc_get_connection_state() return values (ABI state convention). */
#define WEBRTC_STATE_INVALID    0
#define WEBRTC_STATE_CONNECTING 1
#define WEBRTC_STATE_OPEN       2
#define WEBRTC_STATE_CLOSED     3

/* ===================== ABI-PARITY: imports (C→JS) ========================= *
 * Engine calls; the baked shim provides (forwarding to Module.UE.net.*).
 * Pointers cross as wasm addresses; (char*,int) pairs are caller-owned buffers
 * the callee fills/reads up to the given length. Strings in are NUL-terminated
 * UTF-8 at the given pointer. See README.md for per-function ownership.
 */
UE_IMPORT void webrtc_init_listen(const char* roomName);
UE_IMPORT void webrtc_connect(const char* peerId);
UE_IMPORT int  webrtc_send(int connId, const void* data, int len);
UE_IMPORT int  webrtc_recv(int connId, void* buffer, int maxLen);
UE_IMPORT int  webrtc_accept_connection(void);
UE_IMPORT void webrtc_close(int connId);
UE_IMPORT void webrtc_destroy(void);
UE_IMPORT int  webrtc_get_connection_state(int connId);
UE_IMPORT int  webrtc_check_pending_connect(char* buffer, int maxLen);
UE_IMPORT void webrtc_set_listen_status(const char* name, const char* map, int players, int maxPlayers, const char* gametype);
UE_IMPORT void webrtc_discover_servers(const char* room);
UE_IMPORT int  webrtc_get_server_count(void);
UE_IMPORT int  webrtc_is_discovery_done(void);
UE_IMPORT int  webrtc_get_server_info(int index, char* name, int nameMax, char* joinKey, int joinKeyMax);
UE_IMPORT int  webrtc_get_server_map(int index, char* buf, int maxLen);
UE_IMPORT int  webrtc_get_server_gametype(int index, char* buf, int maxLen);
UE_IMPORT int  webrtc_get_server_players(int index);
UE_IMPORT int  webrtc_get_server_maxplayers(int index);
UE_IMPORT int  webrtc_get_server_ping(int index);
/* ===================== ABI-PARITY: end imports =========================== */

/* ===================== ABI-PARITY: exports (JS→C) ======================== *
 * Engine provides (EMSCRIPTEN_KEEPALIVE); JS calls via ccall/cwrap.
 */
UE_EXPORT int   webrtc_deferred_open(void);
UE_EXPORT char* ue_exec_command(const char* cmd);   /* run a console command, RETURN captured output (ADR-10). DECLARED in v1; engine impl lands at first rebuild (step 1.3/1.4). */
/* ===================== ABI-PARITY: end exports =========================== */

#ifdef __cplusplus
}
#endif

#endif /* UE_ABI_H */
