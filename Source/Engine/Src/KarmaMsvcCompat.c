/*=============================================================================
	KarmaMsvcCompat.c — ufront 2026-09-12.

	Compatibility shim for linking MathEngine's prebuilt WINDOWS Karma/metoolkit
	archives (metoolkit/lib.rel/win32_single_msvcrt/*.lib) with the MinGW-w64
	cross toolchain. The Windows counterpart of KarmaGlibcCompat.c, and it exists
	for the same reason: those archives are the ORIGINAL vendor binaries and they
	reference their original compiler's support library, which is not ours.

	MSVC emits calls to a handful of helper routines that live in its CRT rather
	than in the object, and libgcc/mingwex spell every one of them differently.
	Six are actually referenced by the archives the engine pulls in — the list is
	not a guess, it is what the link reported:

	    _alldiv _aulldiv _allmul   64-bit integer math (MeGlobals/MeProfile.obj)
	    _chkstk _alloca_probe      the stack probe (many objects, any frame >4 KB)
	    _except_list               the SEH chain head (MdtKea/keaCheckCPU_sse.obj)
	    ??3@YAXPAX@Z               operator delete (McdCommon/CxSmallSort.obj)

	⚠ operator delete IS ON THIS LIST BECAUSE THE TOOLCHAIN CANNOT BE RELIED ON TO
	SUPPLY IT, and that was learned the expensive way: the link succeeded on the
	owner's box and failed in CI on the same binutils 2.45, because msvcrt.dll
	exports `??3@YAXPAX@Z` and whether MinGW's libmsvcrt.a import library lists it
	varies by distribution. Defining it here removes the question. It forwards to
	the ENGINE's own ::operator delete rather than to free(), so a pointer that
	ever does cross the boundary is released by the allocator that produced it.
	(Nothing here references operator NEW — measured across all sixteen archives —
	so there is no matching allocation side to provide.)

	⚠ THE ASSEMBLER LABELS BELOW CARRY TWO LEADING UNDERSCORES AND THAT IS NOT A
	TYPO. These names already begin with an underscore at C level, and i386-PE
	then prefixes one of its own, so the symbol in the archive is `__alldiv`.
	binutils prints its diagnostics WITHOUT the prefix — "undefined reference to
	`_alldiv'" — so a shim defining `_alldiv` looks correct next to the error
	message, links into the group, is never selected, and leaves the exact same
	error. `nm -u` on the referencing object is what settles it; the diagnostic
	cannot.

	⚠ NONE OF THESE IS OPTIONAL OR ON A DEAD PATH. _except_list in particular is
	SIMDSupport_OS(), which is how MdtKea decides at init whether it may use its
	SSE solver — so "it is only the profiler" is true of the _all* group and false
	of this one.

	⚠ THE CALLING CONVENTIONS ARE THE POINT. _alldiv/_aulldiv/_allmul take two
	64-bit arguments on the stack and POP THEM THEMSELVES (16 bytes) — MSVC's own
	convention, not cdecl — and return in EDX:EAX. _chkstk takes its size in EAX,
	returns with ESP already moved, and must preserve ECX. Getting any of that
	wrong compiles clean, links clean, and corrupts the stack at run time, so the
	three stack-arg helpers are trampolines into ordinary C (whose codegen we
	trust) rather than hand-written arithmetic, and only the genuinely
	register-conventioned _chkstk is written out in full.

	Only compiled into the legacy Karma build (BUILD_KARMA_REF) on Windows.
=============================================================================*/

#if defined(_WIN32) || defined(__MINGW32__)

/*-----------------------------------------------------------------------------
	The 64-bit helpers, as plain C. GCC lowers these to libgcc's __divdi3 /
	__udivdi3 / a mul sequence, which is precisely the codegen we want to borrow.
-----------------------------------------------------------------------------*/
long long          KarmaMsvc_alldiv ( long long a,           long long b )          { return a / b; }
unsigned long long KarmaMsvc_aulldiv( unsigned long long a,  unsigned long long b ) { return a / b; }
long long          KarmaMsvc_allmul ( long long a,           long long b )          { return a * b; }

__asm__(
/*-----------------------------------------------------------------------------
	Trampolines: MSVC's callee-pops 64-bit helpers -> the cdecl C above.

	On entry the frame is [esp]=return address, [esp+4..esp+16]=a_lo a_hi b_lo
	b_hi. Each `pushl 16(%esp)` re-reads at the SAME displacement because the
	previous push moved esp down by four — so four identical pushes copy the four
	words in the right order. `ret $16` is the callee pop.
-----------------------------------------------------------------------------*/
"	.text\n"
"	.globl	__alldiv\n"
"__alldiv:\n"
"	pushl	16(%esp)\n"
"	pushl	16(%esp)\n"
"	pushl	16(%esp)\n"
"	pushl	16(%esp)\n"
"	call	_KarmaMsvc_alldiv\n"
"	addl	$16, %esp\n"
"	ret	$16\n"
"\n"
"	.globl	__aulldiv\n"
"__aulldiv:\n"
"	pushl	16(%esp)\n"
"	pushl	16(%esp)\n"
"	pushl	16(%esp)\n"
"	pushl	16(%esp)\n"
"	call	_KarmaMsvc_aulldiv\n"
"	addl	$16, %esp\n"
"	ret	$16\n"
"\n"
"	.globl	__allmul\n"
"__allmul:\n"
"	pushl	16(%esp)\n"
"	pushl	16(%esp)\n"
"	pushl	16(%esp)\n"
"	pushl	16(%esp)\n"
"	call	_KarmaMsvc_allmul\n"
"	addl	$16, %esp\n"
"	ret	$16\n"
"\n"
/*-----------------------------------------------------------------------------
	_chkstk / _alloca_probe — MSVC's stack probe, transcribed from the documented
	chkstk.asm. MinGW's ___chkstk_ms is NOT a substitute: it probes and returns
	with ESP untouched, where this one subtracts EAX from ESP itself and the
	caller's prologue depends on that having happened.

	EAX = bytes wanted on entry. Touches one dword per 4 KB page on the way down
	so the guard page is hit in order (skipping it is how you turn a large stack
	frame into an unrecoverable fault instead of a commit). ECX is preserved;
	EAX is not, which matches the original.
-----------------------------------------------------------------------------*/
"	.globl	__chkstk\n"
"	.globl	__alloca_probe\n"
"__chkstk:\n"
"__alloca_probe:\n"
"	pushl	%ecx\n"
"	cmpl	$4096, %eax\n"
"	leal	8(%esp), %ecx\n"		/* ecx = caller's esp, before the call */
"	jb	2f\n"
"1:\n"
"	subl	$4096, %ecx\n"
"	subl	$4096, %eax\n"
"	testl	%eax, (%ecx)\n"			/* touch this page */
"	cmpl	$4096, %eax\n"
"	jae	1b\n"
"2:\n"
"	subl	%eax, %ecx\n"			/* ecx = the new esp */
"	movl	%esp, %eax\n"			/* eax = where ecx and the retaddr are saved */
"	testl	%eax, (%ecx)\n"			/* touch the last page */
"	movl	%ecx, %esp\n"
"	movl	(%eax), %ecx\n"			/* restore the caller's ecx */
"	movl	4(%eax), %eax\n"		/* the return address */
"	pushl	%eax\n"
"	ret\n"
"\n"
/*-----------------------------------------------------------------------------
	??3@YAXPAX@Z — `void __cdecl operator delete(void *)`, MSVC's mangling.

	Caller-popped and single-argument, so a bare jump is the whole trampoline: the
	engine's own `operator delete` has the identical signature and convention. The
	name is quoted because `?` is not an ordinary symbol character, and it takes NO
	leading underscore — the i386-PE prefix applies to C names, not to MSVC-mangled
	ones, which is the opposite of the two-underscore rule above. Both spellings in
	one file, and each is wrong for the other.
-----------------------------------------------------------------------------*/
"	.globl	\"??3@YAXPAX@Z\"\n"
"\"??3@YAXPAX@Z\":\n"
"	jmp	__ZdlPv\n"
"\n"
/*-----------------------------------------------------------------------------
	_except_list — not a routine. MSVC's CRT defines it as the ABSOLUTE value 0,
	so that `movl %fs:_except_list, %eax` assembles to a read of fs:[0], the head
	of the thread's SEH chain. Defining it as anything else would silently read
	the wrong TIB slot.
-----------------------------------------------------------------------------*/
"	.globl	__except_list\n"
"	.set	__except_list, 0\n"
);

#endif /* _WIN32 */
