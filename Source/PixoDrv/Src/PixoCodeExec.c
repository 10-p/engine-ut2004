/*=============================================================================
	PixoCodeExec.c: executable JIT code-buffer allocator override (ufront 2.12).

	Pixomatic (2002) is a JIT rasterizer: it generates x86 machine code for the
	current render state into buffers from PixoAllocCodeBuffer(), then executes
	it (DrawPrimitive jumps into the generated code). The closed pixomatic_linux.a
	allocates those buffers with malloc -- fine in 2002 when the heap was
	executable, but modern Linux enforces NX (no-execute) on the heap, so running
	the JIT'd rasterizer SIGSEGVs (observed: fault PC inside DrawPrimitive's
	generated code at a heap address).

	Fix: provide our OWN PixoAllocCodeBuffer/PixoFreeCodeBuffer that mmap()s
	READ|WRITE|EXEC pages, so the generated code is executable. This object is
	linked ahead of pixomatic_linux.a (PixoDrv is whole-archived; the archive is a
	later PRIVATE dep) and the link uses -Wl,--allow-multiple-definition, so these
	definitions win over the archive's.
=============================================================================*/

#if defined(__LINUX_X86__)

#include "pixomatic_linux.h"   /* PIXO_CODEBUFFER, PIXO_U32, PIXO_DYNLINK */
#include <sys/mman.h>
#include <stddef.h>

/* 16-byte header stores the total mmap length (for munmap) and keeps the returned
   code pointer 16-byte aligned (SSE-generated code likes alignment). */
PIXO_DYNLINK(PIXO_CODEBUFFER *) PixoAllocCodeBuffer(PIXO_U32 bytes)
{
	size_t total = (size_t)bytes + 16;
	void *p = mmap(0, total, PROT_READ | PROT_WRITE | PROT_EXEC,
	               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED)
		return (PIXO_CODEBUFFER *)0;
	*((size_t *)p) = total;
	return (PIXO_CODEBUFFER *)((char *)p + 16);
}

PIXO_DYNLINK(void) PixoFreeCodeBuffer(PIXO_CODEBUFFER *buf)
{
	/* Intentionally a NO-OP (leak). The stock malloc-based allocator left freed buffers MAPPED, so
	   Pixomatic's JIT code (which bakes absolute addresses to state/stream buffers) tolerated stale
	   references. munmap()ing here turns those stale references into hard faults (observed: JIT code
	   loading from a freed buffer's address SIGSEGVs). Code buffers are few + small, so leaking is fine. */
	(void)buf;
}

#endif /* __LINUX_X86__ */
