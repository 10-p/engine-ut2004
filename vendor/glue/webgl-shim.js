// webgl-shim.js — the thin --js-library every engine bakes over one function of Emscripten's own library.
// ============================================================================
// Vendored from @ufront/glue (bin/vendor-engine.mjs) like the ABI shims, but it forwards nothing to the host: it
// replaces Emscripten's $setCanvasElementSizeCallingThread (src/lib/libhtml5.js of the emsdk the engines build
// with) by the same code with one read guarded. A --js-library given on the link line overrides a library function
// of the same name. Like Emscripten's own libraries, this file is preprocessed at link time: the #if lines and the
// {{{ }}} macros are expanded there.
//
// A lost WebGL context answers getParameter with null. Emscripten's version reads the GL_VIEWPORT it gets back as an
// array to decide whether the viewport follows the canvas, so a canvas resize after a loss throws a TypeError on the
// engine thread there: SDL's resize handler and SDL_DestroyWindow (on an error exit) both resize it. Here, with no
// viewport to read, the viewport is left as it is, as for a viewport that does not cover the canvas.
//
// test/webgl-shim.test.cjs checks this copy against the emsdk's function, so a toolchain change that edits it fails
// there instead of shipping the older code.
mergeInto(LibraryManager.library, {
#if PTHREADS
  $setCanvasElementSizeCallingThread__deps: [
#if OFFSCREENCANVAS_SUPPORT
    '$setOffscreenCanvasSizeOnTargetThread',
#endif
    '$findCanvasEventTarget'],
  $setCanvasElementSizeCallingThread: (target, width, height) => {
    var canvas = findCanvasEventTarget(target);
    if (!canvas) return {{{ cDefs.EMSCRIPTEN_RESULT_UNKNOWN_TARGET }}};

#if OFFSCREENCANVAS_SUPPORT
    if (canvas.canvasSharedPtr) {
      {{{ makeSetValue('canvas.canvasSharedPtr', 0, 'width', 'i32') }}};
      {{{ makeSetValue('canvas.canvasSharedPtr', 4, 'height', 'i32') }}};
    }

    if (canvas.offscreenCanvas || !canvas.controlTransferredOffscreen) {
      if (canvas.offscreenCanvas) canvas = canvas.offscreenCanvas;
#else
    if (!canvas.controlTransferredOffscreen) {
#endif
      var autoResizeViewport = false;
      if (canvas.GLctxObject?.GLctx) {
        var prevViewport = canvas.GLctxObject.GLctx.getParameter(0xBA2 /* GL_VIEWPORT */);
        // A lost context returns null here: no viewport to follow the canvas.
        autoResizeViewport = (!!prevViewport && prevViewport[0] === 0 && prevViewport[1] === 0 && prevViewport[2] === canvas.width && prevViewport[3] === canvas.height);
#if GL_DEBUG
        dbg(`Resizing canvas from ${canvas.width}x${canvas.height} to ${width}x${height}. Previous GL viewport size was ${prevViewport}, so autoResizeViewport=${autoResizeViewport}`);
#endif
      }
      canvas.width = width;
      canvas.height = height;
      if (autoResizeViewport) {
#if GL_DEBUG
        dbg(`Automatically resizing GL viewport to cover whole render target ${width}x${height}`);
#endif
        canvas.GLctxObject.GLctx.viewport(0, 0, width, height);
      }
#if OFFSCREENCANVAS_SUPPORT
    } else if (canvas.canvasSharedPtr) {
      var targetThread = {{{ makeGetValue('canvas.canvasSharedPtr', 8, '*') }}};
      setOffscreenCanvasSizeOnTargetThread(targetThread, target, width, height);
      return {{{ cDefs.EMSCRIPTEN_RESULT_DEFERRED }}};
#endif
    } else {
#if GL_DEBUG
      dbg('canvas.controlTransferredOffscreen but we do not own the canvas, and do not know who has (no canvas.canvasSharedPtr present, an internal bug?)!\n');
#endif
      return {{{ cDefs.EMSCRIPTEN_RESULT_UNKNOWN_TARGET }}};
    }
#if OFFSCREEN_FRAMEBUFFER
    if (canvas.GLctxObject) GL.resizeOffscreenFramebuffer(canvas.GLctxObject);
#endif
    return {{{ cDefs.EMSCRIPTEN_RESULT_SUCCESS }}};
  },
#endif
});
