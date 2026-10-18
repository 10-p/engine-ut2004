// proxynet-shim.js — the thin --js-library for the optional "proxynet" ABI module.
// ============================================================================
// Vendored from @ufront/glue at PROXYNET_ABI_VERSION 1, only by the engines that ship a ProxyNetDrv (their target
// lists `proxynet` in publish.engine.abiModules). It is a separate file from net-shim.js: that shim stays
// byte-identical in every engine repo, the main ABI does not move when this module does, and an engine that does
// not vendor this file has no `ue_proxynet_*` imports at all (docs/12 ADR-16).
//
// It defines the thirteen imports and forwards each to the runtime-injected `Module.UE.proxynet.*`. No logic.
//
// Absent module = absent feature, never a boot failure. Unlike $ueNet, $ueProxyNet does not throw when the host
// injected nothing: it returns null and every forwarder answers "not available" (connect -> -1, state -> CLOSED,
// the rest -> 0), so a page without Proxy networking, or in P2P mode, costs nothing and breaks nothing. A module
// that is present at the wrong version is a real mismatch, and that one throws.
//
// Threading (-sPROXY_TO_PTHREAD), the same split as net-shim.js and for the same reason:
//   * ue_proxynet_send / ue_proxynet_recv are not proxied. They are the per-packet path; on the engine thread
//     `Module.UE.proxynet` is the ring client @ufront/glue installs there (installWorkerProxyNet), so the call
//     never leaves the thread.
//   * all the others are `__proxy: 'sync'`: per-tick or per-action, and the WebSocket, the session token and
//     fetch() all live on the main thread. Pthreads share memory, so a proxied call reads the string at the
//     pointer and fills the out-buffers in the same heap, with no marshalling.
mergeInto(LibraryManager.library, {
    $ueProxyNet: function () {
        var ue = Module['UE'];
        var m = ue && ue.proxynet;
        if (!m) return null;
        if (m.version !== 1) {
            throw new Error('[ue-abi] Module.UE.proxynet version mismatch (host=' + m.version + ', shim expects 1)');
        }
        return m;
    },

    // --- the pipe ---
    ue_proxynet_connect__deps: ['$ueProxyNet'],
    ue_proxynet_connect__proxy: 'sync',
    ue_proxynet_connect: function (hostPtr, port) {
        var m = ueProxyNet();
        return m ? m.ue_proxynet_connect(hostPtr, port) : -1;
    },

    ue_proxynet_state__deps: ['$ueProxyNet'],
    ue_proxynet_state__proxy: 'sync',
    ue_proxynet_state: function (connId) {
        var m = ueProxyNet();
        return m ? m.ue_proxynet_state(connId) : 3;
    },

    // --- the packet path: not proxied. Answered on the engine thread by the shared rings. ---
    ue_proxynet_send__deps: ['$ueProxyNet'],
    ue_proxynet_send: function (connId, dataPtr, len) {
        var m = ueProxyNet();
        return m ? m.ue_proxynet_send(connId, dataPtr, len) : 0;
    },

    ue_proxynet_recv__deps: ['$ueProxyNet'],
    ue_proxynet_recv: function (connId, bufferPtr, maxLen) {
        var m = ueProxyNet();
        return m ? m.ue_proxynet_recv(connId, bufferPtr, maxLen) : 0;
    },

    ue_proxynet_close__deps: ['$ueProxyNet'],
    ue_proxynet_close__proxy: 'sync',
    ue_proxynet_close: function (connId) {
        var m = ueProxyNet();
        if (m) m.ue_proxynet_close(connId);
    },

    // --- redirect downloads ---
    ue_proxynet_http_open__deps: ['$ueProxyNet'],
    ue_proxynet_http_open__proxy: 'sync',
    ue_proxynet_http_open: function (connId, urlPtr) {
        var m = ueProxyNet();
        return m ? m.ue_proxynet_http_open(connId, urlPtr) : -1;
    },

    ue_proxynet_http_poll__deps: ['$ueProxyNet'],
    ue_proxynet_http_poll__proxy: 'sync',
    ue_proxynet_http_poll: function (handle, statusPtr, receivedPtr, totalPtr) {
        var m = ueProxyNet();
        return m ? m.ue_proxynet_http_poll(handle, statusPtr, receivedPtr, totalPtr) : -1;
    },

    ue_proxynet_http_read__deps: ['$ueProxyNet'],
    ue_proxynet_http_read__proxy: 'sync',
    ue_proxynet_http_read: function (handle, bufferPtr, maxLen) {
        var m = ueProxyNet();
        return m ? m.ue_proxynet_http_read(handle, bufferPtr, maxLen) : 0;
    },

    ue_proxynet_http_close__deps: ['$ueProxyNet'],
    ue_proxynet_http_close__proxy: 'sync',
    ue_proxynet_http_close: function (handle) {
        var m = ueProxyNet();
        if (m) m.ue_proxynet_http_close(handle);
    },

    // --- the in-game server browser's list ---
    ue_proxynet_list_open__deps: ['$ueProxyNet'],
    ue_proxynet_list_open__proxy: 'sync',
    ue_proxynet_list_open: function () {
        var m = ueProxyNet();
        return m && m.ue_proxynet_list_open ? m.ue_proxynet_list_open() : -1;
    },

    ue_proxynet_list_poll__deps: ['$ueProxyNet'],
    ue_proxynet_list_poll__proxy: 'sync',
    ue_proxynet_list_poll: function (handle, statusPtr, countPtr) {
        var m = ueProxyNet();
        return m && m.ue_proxynet_list_poll ? m.ue_proxynet_list_poll(handle, statusPtr, countPtr) : -1;
    },

    ue_proxynet_list_read__deps: ['$ueProxyNet'],
    ue_proxynet_list_read__proxy: 'sync',
    ue_proxynet_list_read: function (handle, bufferPtr, maxLen) {
        var m = ueProxyNet();
        return m && m.ue_proxynet_list_read ? m.ue_proxynet_list_read(handle, bufferPtr, maxLen) : 0;
    },

    ue_proxynet_list_close__deps: ['$ueProxyNet'],
    ue_proxynet_list_close__proxy: 'sync',
    ue_proxynet_list_close: function (handle) {
        var m = ueProxyNet();
        if (m && m.ue_proxynet_list_close) m.ue_proxynet_list_close(handle);
    },
});
