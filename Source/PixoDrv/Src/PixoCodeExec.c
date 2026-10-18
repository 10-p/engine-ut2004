/*=============================================================================
	PixoCodeExec.c: executable JIT code-buffer allocator override.

	Pixomatic is a JIT rasterizer: it generates x86 machine code for the
	current render state into buffers from PixoAllocCodeBuffer(), then executes
	it (DrawPrimitive jumps into the generated code). The closed pixomatic_linux.a
	allocates those buffers with malloc, and Linux maps the heap NX, so the
	generated code would fault on instruction fetch.

	These PixoAllocCodeBuffer/PixoFreeCodeBuffer mmap() READ|WRITE|EXEC pages
	instead. This object is linked ahead of pixomatic_linux.a (PixoDrv is
	whole-archived; the archive is a later PRIVATE dep) and the link uses
	-Wl,--allow-multiple-definition, so these definitions win over the archive's.
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
	/* Intentionally a no-op (a leak): Pixomatic's JIT code bakes absolute addresses to state and
	   stream buffers and tolerates stale references into freed ones, which munmap() would turn into
	   faults. Code buffers are few and small. */
	(void)buf;
}

#endif /* __LINUX_X86__ */
