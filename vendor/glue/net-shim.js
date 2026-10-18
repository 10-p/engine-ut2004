// net-shim.js — the thin --js-library every engine bakes for the network imports.
// ============================================================================
// VENDORED from @ufront/glue at ABI_VERSION 2 (docs/02, docs/05 §2). It defines the 18 ue_net_* C-imports the
// engine WASM calls and FORWARDS each to the runtime-injected Module.UE.net.* (the host sets Module.UE before
// main()). There is NO logic here: the transport lives in @ufront/glue ESM (createWebRtc), injected at runtime.
//
// $ueNet asserts Module.UE is injected at the right ABI version and returns the transport. It is pulled in via
// each forwarder's __deps (the wasm imports the ue_net_* symbols).
//
// THREADING (-sPROXY_TO_PTHREAD): every forwarder is __proxy: 'sync' EXCEPT ue_net_send and ue_net_recv.
//
// The engine calls these from its pthread, but the P2P transport cannot follow it there: RTCPeerConnection is a
// Window API and is not available in workers (W3C webrtc-extensions #77; browsers ship only a transferable
// RTCDataChannel). Module.UE is injected on the main thread's Module and each pthread has its own Module scope, so
// an unproxied call finds nothing and throws "[ue-abi] Module.UE.net not injected". __proxy: 'sync' runs the call
// on the main thread and blocks the caller until it returns, which is the semantics the engine expects. Pthreads
// share memory, so a proxied call reads packet bytes at dataPtr and fills bufferPtr in the SAME heap.
//
// ue_net_send and ue_net_recv are the per-packet path and are NOT proxied. Proxied, every 512-byte datagram was a
// synchronous cross-thread round trip; a file transfer saturates the send loop, the engine thread ended up parked
// in those calls instead of draining its OWN incoming proxy queue, and the queue grew until main could no longer
// enqueue an input event:
//     Aborted(Assertion failed: false && "emscripten_proxy_async failed", .../html5/callback.c,42)
// On the engine thread the host installs a Module.UE.net of its own (@ufront/glue src/webrtc/worker.js, from the
// same bootstrap message that carries the FS channels) that reads and writes SHARED-MEMORY PACKET RINGS, so the
// unproxied forwarder finds a net object HERE and never leaves the thread. The other 16 are per-tick or
// per-user-action, not per-packet.
mergeInto(LibraryManager.library, {
  $ueNet: function() {
    var ue = Module['UE'];
    if (!ue || ue.abiVersion !== 2) {
      throw new Error('[ue-abi] Module.UE.net not injected or ABI mismatch (host=' +
        (ue && ue.abiVersion) + ', shim expects 2) — the host must set Module.UE before main()');
    }
    return ue.net;
  },

  // --- transport ---
  ue_net_init_listen__deps: ['$ueNet'],
  ue_net_init_listen__proxy: 'sync',
  ue_net_init_listen: function(roomNamePtr) { return ueNet().ue_net_init_listen(roomNamePtr); },

  ue_net_connect__deps: ['$ueNet'],
  ue_net_connect__proxy: 'sync',
  ue_net_connect: function(peerIdPtr) { return ueNet().ue_net_connect(peerIdPtr); },

  // --- the packet path: NOT proxied. Answered on the engine thread by the shared rings. ---
  ue_net_send__deps: ['$ueNet'],
  ue_net_send: function(connId, dataPtr, len) { return ueNet().ue_net_send(connId, dataPtr, len); },

  ue_net_recv__deps: ['$ueNet'],
  ue_net_recv: function(connId, bufferPtr, maxLen) { return ueNet().ue_net_recv(connId, bufferPtr, maxLen); },

  ue_net_accept_connection__deps: ['$ueNet'],
  ue_net_accept_connection__proxy: 'sync',
  ue_net_accept_connection: function() { return ueNet().ue_net_accept_connection(); },

  ue_net_close__deps: ['$ueNet'],
  ue_net_close__proxy: 'sync',
  ue_net_close: function(connId) { return ueNet().ue_net_close(connId); },

  ue_net_destroy__deps: ['$ueNet'],
  ue_net_destroy__proxy: 'sync',
  ue_net_destroy: function() { return ueNet().ue_net_destroy(); },

  ue_net_get_connection_state__deps: ['$ueNet'],
  ue_net_get_connection_state__proxy: 'sync',
  ue_net_get_connection_state: function(connId) { return ueNet().ue_net_get_connection_state(connId); },

  ue_net_set_listen_status__deps: ['$ueNet'],
  ue_net_set_listen_status__proxy: 'sync',
  ue_net_set_listen_status: function(namePtr, mapPtr, players, maxPlayers, gametypePtr) {
    return ueNet().ue_net_set_listen_status(namePtr, mapPtr, players, maxPlayers, gametypePtr);
  },

  // --- discovery / server browser ---
  ue_net_discover_servers__deps: ['$ueNet'],
  ue_net_discover_servers__proxy: 'sync',
  ue_net_discover_servers: function(roomPtr) { return ueNet().ue_net_discover_servers(roomPtr); },

  ue_net_get_server_count__deps: ['$ueNet'],
  ue_net_get_server_count__proxy: 'sync',
  ue_net_get_server_count: function() { return ueNet().ue_net_get_server_count(); },

  ue_net_is_discovery_done__deps: ['$ueNet'],
  ue_net_is_discovery_done__proxy: 'sync',
  ue_net_is_discovery_done: function() { return ueNet().ue_net_is_discovery_done(); },

  ue_net_get_server_info__deps: ['$ueNet'],
  ue_net_get_server_info__proxy: 'sync',
  ue_net_get_server_info: function(index, namePtr, nameMax, joinKeyPtr, joinKeyMax) {
    return ueNet().ue_net_get_server_info(index, namePtr, nameMax, joinKeyPtr, joinKeyMax);
  },

  ue_net_get_server_map__deps: ['$ueNet'],
  ue_net_get_server_map__proxy: 'sync',
  ue_net_get_server_map: function(index, bufPtr, maxLen) { return ueNet().ue_net_get_server_map(index, bufPtr, maxLen); },

  ue_net_get_server_gametype__deps: ['$ueNet'],
  ue_net_get_server_gametype__proxy: 'sync',
  ue_net_get_server_gametype: function(index, bufPtr, maxLen) { return ueNet().ue_net_get_server_gametype(index, bufPtr, maxLen); },

  ue_net_get_server_players__deps: ['$ueNet'],
  ue_net_get_server_players__proxy: 'sync',
  ue_net_get_server_players: function(index) { return ueNet().ue_net_get_server_players(index); },

  ue_net_get_server_maxplayers__deps: ['$ueNet'],
  ue_net_get_server_maxplayers__proxy: 'sync',
  ue_net_get_server_maxplayers: function(index) { return ueNet().ue_net_get_server_maxplayers(index); },

  ue_net_get_server_ping__deps: ['$ueNet'],
  ue_net_get_server_ping__proxy: 'sync',
  ue_net_get_server_ping: function(index) { return ueNet().ue_net_get_server_ping(index); },
});
