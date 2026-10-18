/*
 * ue_abi.h — the engine↔glue thin-shim ABI (C face).  ABI_VERSION 2.
 * ============================================================================
 * The C declaration of the boundary between an engine's WASM and its JavaScript host
 * (docs/02). abi.ts is the TypeScript face; both live in packages/glue/abi/, the single
 * source of truth. test/abi-parity.test.cjs fails if the two faces, or the vendored
 * net-shim.js, disagree on any {name, arity, direction}, on the state enum, or on the
 * version.
 *
 * Every engine repo vendors this header with the shims in vendor/glue/, written by
 * packages/glue/bin/vendor-engine.mjs (docs/05 §2). A change to the import/export set
 * bumps UE_ABI_VERSION, and the engines and glue move together: each engine re-vendors
 * and re-links. A mismatch fails loudly, at build (the artifact's meta.json abiVersion is
 * read from this file) and at boot (the shims assert Module.UE.abiVersion before main()).
 *
 *  - UE_IMPORT : a C function the engine CALLS and JS PROVIDES. The vendored net-shim.js
 *                forwards each to the host-injected Module.UE.net.*; the WASM imports these.
 *  - UE_EXPORT : a C function the engine PROVIDES (EMSCRIPTEN_KEEPALIVE) and JS CALLS.
 *
 * The parity test keys off the UE_IMPORT / UE_EXPORT markers, so keep every boundary
 * function on ONE line in the marked blocks below.
 *
 * Part of the contract, but not C functions of the engine's own, so not declared here:
 *   - the filesystem. Engines are built -sWASMFS; the vendored wasmfs-shim.js forwards the
 *     WasmFS JSImpl backend's hooks to Module.UE.fs.backend and defines
 *     ufront_wasmfs_register / ufront_wasmfs_set_pending, which the engine exports for the
 *     host to call (abi.ts, the "wasmfs" parity block). Reads are synchronous on the engine
 *     thread; the engine's own Asyncify use is its main loop, not this boundary.
 *   - the optional ABI modules in the sibling folders: each has its own header, shim and
 *     version, and is vendored only by the engines whose target descriptor lists it
 *     (publish.engine.abiModules). This header knows none of their names, on purpose.
 */
#ifndef UE_ABI_H
#define UE_ABI_H

#define UE_ABI_VERSION 2

#ifdef __cplusplus
extern "C" {
#endif

#ifndef UE_IMPORT
#define UE_IMPORT extern
#endif
#ifndef UE_EXPORT
#define UE_EXPORT
#endif

/* ue_net_get_connection_state() return values (ABI state convention). */
#define UE_NET_STATE_INVALID    0
#define UE_NET_STATE_CONNECTING 1
#define UE_NET_STATE_OPEN       2
#define UE_NET_STATE_CLOSED     3

/* ===================== ABI-PARITY: imports (C→JS) ========================= *
 * Engine calls; the baked shim provides (forwarding to Module.UE.net.*).
 * Pointers cross as wasm addresses; (char*,int) pairs are caller-owned buffers
 * the callee fills/reads up to the given length. Strings in are NUL-terminated
 * UTF-8 at the given pointer. See README.md for per-function ownership.
 */
UE_IMPORT void ue_net_init_listen(const char* roomName);
UE_IMPORT void ue_net_connect(const char* peerId);
UE_IMPORT int  ue_net_send(int connId, const void* data, int len);
UE_IMPORT int  ue_net_recv(int connId, void* buffer, int maxLen);
UE_IMPORT int  ue_net_accept_connection(void);
UE_IMPORT void ue_net_close(int connId);
UE_IMPORT void ue_net_destroy(void);
UE_IMPORT int  ue_net_get_connection_state(int connId);
UE_IMPORT void ue_net_set_listen_status(const char* name, const char* map, int players, int maxPlayers, const char* gametype);
UE_IMPORT void ue_net_discover_servers(const char* room);
UE_IMPORT int  ue_net_get_server_count(void);
UE_IMPORT int  ue_net_is_discovery_done(void);
UE_IMPORT int  ue_net_get_server_info(int index, char* name, int nameMax, char* joinKey, int joinKeyMax);
UE_IMPORT int  ue_net_get_server_map(int index, char* buf, int maxLen);
UE_IMPORT int  ue_net_get_server_gametype(int index, char* buf, int maxLen);
UE_IMPORT int  ue_net_get_server_players(int index);
UE_IMPORT int  ue_net_get_server_maxplayers(int index);
UE_IMPORT int  ue_net_get_server_ping(int index);
/* ===================== ABI-PARITY: end imports =========================== */

/* ===================== ABI-PARITY: exports (JS→C) ======================== *
 * Engine provides (EMSCRIPTEN_KEEPALIVE); JS calls via ccall/cwrap.
 */
UE_EXPORT char* ue_exec_command(const char* cmd);   /* run a console command, RETURN its captured output (ADR-10). */
/* ===================== ABI-PARITY: end exports =========================== */

#ifdef __cplusplus
}
#endif

#endif /* UE_ABI_H */
