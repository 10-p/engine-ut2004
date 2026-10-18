// wasmfs-shim.js — the thin --js-library the engine bakes for the WasmFS FS backend.
// ============================================================================
// Vendored from @ufront/glue, like net-shim.js and for the same reason: the engine bakes only a forwarder,
// while all real logic lives in @ufront/glue ESM and is injected by the host at runtime (Module.UE.fs.backend).
// There is no manifest, download, cache, budget or eviction logic here.
//
// Under -sWASMFS the VFS lives in wasm, so a C open() does not route through the JS FS.open, and a JS-side
// interception of FS.open would never fire. The seam is a WasmFS JSImpl backend: file operations are
// implemented in JS and called from wasm, which is the on-demand hook FetchFS needs, and the bytes live in JS
// typed arrays rather than in a std::vector inside linear memory, which keeps a multi-GB resident set out of
// the 2 GB wasm32 address space.
//
// The host stands the backend up entirely from JS via EXPORTED_FUNCTIONS on wasmfs_create_jsimpl_backend and
// wasmfs_create_file, so this file plus a few link flags is the whole per-engine cost: no C++ in any engine
// repo.
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
  // The worker side.
  //
  // Under -sPROXY_TO_PTHREAD the engine runs on a pthread, and WasmFS calls the JSImpl hooks THERE:
  // Emscripten's libwasmfs_jsimpl.js carries no __proxy, so a hook executes on whichever thread performs
  // the FS op and looks up wasmFS$backends[backend], a per-thread object. The host registers the backend
  // on main (allocFile binds file->path on the creating thread, and the wasm runtime is not up in a
  // pthread when this code runs), so without the block below the worker's table would be empty and the
  // first read would die with "Cannot read properties of undefined (reading 'getSize')".
  //
  // This file stays a forwarder. On a pthread it:
  //   1. takes the channel main postMessages (addEventListener coexists with Emscripten's onmessage),
  //   2. replaces wasmFS$backends with a Proxy that lazily builds the table for whatever backend pointer
  //      is asked about, so main's pointer arrives as the lookup key and nothing is handshaken,
  //   3. importScripts the glue bundle (synchronously, which is what a sync FS hook needs) and lets it
  //      build the worker's backend.
  //
  // __postset survives only if the symbol is referenced: an unused $symbol is dead-stripped silently, with
  // no error and no bootstrap. It is force-included via -sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE in the link
  // flags, which must stay as long as this block does.
  $ueWorker: {
    // The whole bootstrap message main posted, kept and forwarded verbatim: the glue owns its shape, so a
    // new channel needs no edit to this file vendored into every engine repo.
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
      // ue_exec_command runs on this thread: called on the browser main thread, engine code would find no
      // GL context and no engine state (it dies in glClearColor). Main posts the command and it runs here,
      // between frames, on the thread that owns the engine.
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
