// webrtc-shim.js — the thin --js-library the engine bakes (step 1.3).
// ============================================================================
// VENDORED from @ufront/glue at ABI_VERSION 1 (docs/02, docs/05 §2). It defines the
// 19 webrtc_* C-imports the engine WASM calls and FORWARDS each to the runtime-injected
// Module.UE.net.* (the host sets Module.UE before main()). There is NO heavy logic here
// — FetchFS/WebRTC/IDB logic lives in @ufront/glue ESM (createWebRtc), injected at
// runtime. This is the entire engine-side cost of the WebRTC extraction.
//
// $ueNet asserts Module.UE is injected at the right ABI version and returns the transport.
// It is pulled in via each forwarder's __deps (the wasm imports the webrtc_* symbols).

mergeInto(LibraryManager.library, {
  $ueNet: function() {
    var ue = Module['UE'];
    if (!ue || ue.abiVersion !== 1) {
      throw new Error('[ue-abi] Module.UE.net not injected or ABI mismatch (host=' +
        (ue && ue.abiVersion) + ', shim expects 1) — the host must set Module.UE before main()');
    }
    return ue.net;
  },

  // --- transport ---
  webrtc_init_listen__deps: ['$ueNet'],
  webrtc_init_listen: function(roomNamePtr) { return ueNet().webrtc_init_listen(roomNamePtr); },

  webrtc_connect__deps: ['$ueNet'],
  webrtc_connect: function(peerIdPtr) { return ueNet().webrtc_connect(peerIdPtr); },

  webrtc_send__deps: ['$ueNet'],
  webrtc_send: function(connId, dataPtr, len) { return ueNet().webrtc_send(connId, dataPtr, len); },

  webrtc_recv__deps: ['$ueNet'],
  webrtc_recv: function(connId, bufferPtr, maxLen) { return ueNet().webrtc_recv(connId, bufferPtr, maxLen); },

  webrtc_accept_connection__deps: ['$ueNet'],
  webrtc_accept_connection: function() { return ueNet().webrtc_accept_connection(); },

  webrtc_close__deps: ['$ueNet'],
  webrtc_close: function(connId) { return ueNet().webrtc_close(connId); },

  webrtc_destroy__deps: ['$ueNet'],
  webrtc_destroy: function() { return ueNet().webrtc_destroy(); },

  webrtc_get_connection_state__deps: ['$ueNet'],
  webrtc_get_connection_state: function(connId) { return ueNet().webrtc_get_connection_state(connId); },

  webrtc_check_pending_connect__deps: ['$ueNet'],
  webrtc_check_pending_connect: function(bufferPtr, maxLen) { return ueNet().webrtc_check_pending_connect(bufferPtr, maxLen); },

  webrtc_set_listen_status__deps: ['$ueNet'],
  webrtc_set_listen_status: function(namePtr, mapPtr, players, maxPlayers, gametypePtr) {
    return ueNet().webrtc_set_listen_status(namePtr, mapPtr, players, maxPlayers, gametypePtr);
  },

  // --- discovery / server browser ---
  webrtc_discover_servers__deps: ['$ueNet'],
  webrtc_discover_servers: function(roomPtr) { return ueNet().webrtc_discover_servers(roomPtr); },

  webrtc_get_server_count__deps: ['$ueNet'],
  webrtc_get_server_count: function() { return ueNet().webrtc_get_server_count(); },

  webrtc_is_discovery_done__deps: ['$ueNet'],
  webrtc_is_discovery_done: function() { return ueNet().webrtc_is_discovery_done(); },

  webrtc_get_server_info__deps: ['$ueNet'],
  webrtc_get_server_info: function(index, namePtr, nameMax, joinKeyPtr, joinKeyMax) {
    return ueNet().webrtc_get_server_info(index, namePtr, nameMax, joinKeyPtr, joinKeyMax);
  },

  webrtc_get_server_map__deps: ['$ueNet'],
  webrtc_get_server_map: function(index, bufPtr, maxLen) { return ueNet().webrtc_get_server_map(index, bufPtr, maxLen); },

  webrtc_get_server_gametype__deps: ['$ueNet'],
  webrtc_get_server_gametype: function(index, bufPtr, maxLen) { return ueNet().webrtc_get_server_gametype(index, bufPtr, maxLen); },

  webrtc_get_server_players__deps: ['$ueNet'],
  webrtc_get_server_players: function(index) { return ueNet().webrtc_get_server_players(index); },

  webrtc_get_server_maxplayers__deps: ['$ueNet'],
  webrtc_get_server_maxplayers: function(index) { return ueNet().webrtc_get_server_maxplayers(index); },

  webrtc_get_server_ping__deps: ['$ueNet'],
  webrtc_get_server_ping: function(index) { return ueNet().webrtc_get_server_ping(index); },
});
