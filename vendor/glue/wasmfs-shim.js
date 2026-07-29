// wasmfs-shim.js — the thin --js-library the engine bakes for the WasmFS FS backend (step 2.13.1).
// ============================================================================
// VENDORED from @ufront/glue, exactly like webrtc-shim.js and for the same reason: the engine bakes
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
    if (!ue || ue.abiVersion !== 1) {
      throw new Error('[ue-abi] Module.UE.fs not injected or ABI mismatch (host=' +
        (ue && ue.abiVersion) + ', shim expects 1) — the host must set Module.UE before main()');
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
});
