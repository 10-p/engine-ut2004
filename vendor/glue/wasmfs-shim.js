// wasmfs-shim.js — the thin --js-library the engine bakes for the WasmFS FS backend (step 2.13.1).
// ============================================================================
// VENDORED from @ufront/glue, exactly like net-shim.js and for the same reason: the engine bakes
// only a forwarder, while all real logic lives in @ufront/glue ESM and is injected by the host at
// runtime (Module.UE.fs.backend). There is NO manifest, download, cache, budget or eviction logic here.
//
// WHY THIS EXISTS. Before 2.13, FetchFS intercepted file access by monkey-patching the *JS* FS.open,
// which worked because the legacy Emscripten filesystem implements __syscall_openat in JS. WasmFS moves
// the VFS into wasm, so a C open() no longer routes through JS FS.open and that interception is
// SILENTLY BYPASSED (probed 2026-07-14: the patch installs but never fires). The replacement seam is a
// WasmFS JSImpl backend: file operations are implemented in JS and called from wasm, which is both the
// on-demand hook FetchFS needs and — because the bytes then live in JS typed arrays rather than in a
// std::vector inside linear memory — the way to keep multi-GB residency out of the 2 GB wasm32 address
// space (the engine itself measures a flat 256 MB, so linear memory is not where assets belong).
//
// The host stands the backend up entirely from JS via EXPORTED_FUNCTIONS on wasmfs_create_jsimpl_backend
// and wasmfs_create_file, so this file plus a few link flags is the ENTIRE per-engine cost of the FS
// re-architecture — no C++ in any engine repo.
//
// $ueFs asserts Module.UE is injected at the right ABI version and returns the backend implementation.

mergeInto(LibraryManager.library, {
  $ueFs: function() {
    var ue = Module['UE'];
    if (!ue || ue.abiVersion !== 2) {
      throw new Error('[ue-abi] Module.UE.fs not injected or ABI mismatch (host=' +
        (ue && ue.abiVersion) + ', shim expects 2) — the host must set Module.UE before main()');
    }
    if (!ue.fs || !ue.fs.backend) {
      throw new Error('[ue-abi] Module.UE.fs.backend missing — this engine is built -sWASMFS and ' +
        'requires the glue WasmFS backend (@ufront/glue createFetchFs installs it)');
    }
    return ue.fs.backend;
  },

  // Registers the JS side of a JSImpl backend created by the host. wasmFS$backends maps the C++ backend
  // pointer to the hook table WasmFS calls; every hook forwards straight to the injected backend.
  // Signatures are WasmFS's, not ours (see system/lib/wasmfs/js_impl_backend.h):
  //   read/write(file, bufferPtr, length, offset) -> bytes handled, or -errno
  //   getSize(file) -> off_t ; setSize(file, size) -> 0 | -errno
  // `file` is an opaque per-file index; the host binds it to a path in allocFile (see set_pending).
  ufront_wasmfs_register__deps: ['$ueFs', '$wasmFS$backends'],
  ufront_wasmfs_register: function(backend) {
    wasmFS$backends[backend] = {
      allocFile: function(file) { return ueFs().allocFile(file); },
      freeFile: function(file) { return ueFs().freeFile(file); },
      read: function(file, buffer, length, offset) { return ueFs().read(file, buffer, length, offset); },
      write: function(file, buffer, length, offset) { return ueFs().write(file, buffer, length, offset); },
      getSize: function(file) { return ueFs().getSize(file); },
      setSize: function(file, size) { return ueFs().setSize(file, size); },
    };
  },

  // wasmfs_create_file() hands allocFile only an opaque file index, never a path — so the host stashes
  // the path it is about to create here, and allocFile binds the two. Deterministic because creation is
  // synchronous and single-threaded.
  ufront_wasmfs_set_pending__deps: ['$ueFs'],
  ufront_wasmfs_set_pending: function(pathPtr) {
    ueFs().pendingPath = pathPtr ? UTF8ToString(pathPtr) : null;
  },

  // --------------------------------------------------------------------------------------------
  // step 2.13.2 — THE WORKER SIDE.
  //
  // Under -sPROXY_TO_PTHREAD the engine runs on a pthread, and WasmFS calls the JSImpl hooks THERE:
  // Emscripten's libwasmfs_jsimpl.js carries no __proxy, so a hook executes on whichever thread performs
  // the FS op and looks up wasmFS$backends[backend] — a PER-THREAD object. The host registers the
  // backend on MAIN (it must: allocFile binds file->path on the creating thread, and the wasm runtime is
  // not even up in a pthread when this code runs), so without the block below the worker's table is
  // empty and the first read dies with "Cannot read properties of undefined (reading 'getSize')".
  //
  // The fix keeps this file a forwarder. On a pthread we:
  //   1. take the channel main postMessages us (addEventListener coexists with Emscripten's onmessage),
  //   2. replace wasmFS$backends with a PROXY that lazily builds the table for whatever backend pointer
  //      is asked about — so main's pointer arrives as the lookup KEY and nothing is handshaken,
  //   3. importScripts the real glue bundle (synchronously — which is what a sync FS hook needs) and let
  //      it build the worker's backend.
  //
  // ⚠ __postset only survives if the symbol is REFERENCED: an unused $symbol is dead-stripped silently,
  // no error and no bootstrap. It is force-included via -sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE in the link
  // flags — do not remove that without removing this.
  $ueWorker: {
    // step 2.13.4: the WHOLE bootstrap message main posted, kept verbatim and forwarded verbatim. It used
    // to be destructured into named fields here, which meant every new channel needed an edit to a file
    // vendored into every engine repo + a rebuild + a re-gate of all of them. The glue owns the shape now;
    // this file only has to deliver it.
    boot: null,
    table: null,
  },

  $ueWorkerBoot__deps: ['$ueWorker', '$wasmFS$backends'],
  $ueWorkerBoot__postset: 'ueWorkerBoot();',
  $ueWorkerBoot: function() {
    if (typeof ENVIRONMENT_IS_PTHREAD === 'undefined' || !ENVIRONMENT_IS_PTHREAD) return;

    addEventListener('message', function(e) {
      if (!e || !e.data) return;
      if (e.data.ufrontChannel) {
        ueWorker.boot = e.data;
      }
      // step 2.13.2 — ue_exec_command must run ON THIS THREAD. The frozen ABI is unchanged; what changes
      // is routing: the host used to call Module._ue_exec_command directly, but under PROXY_TO_PTHREAD
      // that executes engine code on the browser main thread, where there is no GL context and no engine
      // state — it dies in glClearColor. Main posts the command instead and we run it here, between
      // frames, on the thread that owns the engine.
      if (typeof e.data.ufrontExec === 'string') {
        console.log('[ufront/worker] exec received: ' + e.data.ufrontExec +
          ' (export=' + (typeof Module['_ue_exec_command']) + ')');
      }
      if (typeof e.data.ufrontExec === 'string' && Module['_ue_exec_command']) {
        var cmd = e.data.ufrontExec;
        var len = lengthBytesUTF8(cmd) + 1;
        var saved = stackSave();
        try {
          var ptr = stackAlloc(len);
          stringToUTF8(cmd, ptr, len);
          Module['_ue_exec_command'](ptr);
        } finally {
          stackRestore(saved);
        }
      }
    });

    wasmFS$backends = new Proxy({}, {
      get: function(target, key) {
        if (!(key in target)) {
          if (!ueWorker.table) {
            if (!ueWorker.boot) {
              // Main posts the channel before callMain and the pthread is idle until then, so this is
              // not expected. Fail loudly rather than serving zero-length reads that look like a
              // corrupt install.
              throw new Error('[ufront] WasmFS hook fired on the worker before the host handed over ' +
                'the sync channel — the engine cannot read any asset');
            }
            importScripts('ufront-glue.js');
            ueWorker.table = ufrontGlue.createWorkerFs({
              Module: Module,
              boot: ueWorker.boot,
            });
          }
          target[key] = ueWorker.table;
        }
        return target[key];
      },
    });
  },
});
