/*=============================================================================
	KarmaGlibcCompat.c — ufront 2.12 Part B.

	Compatibility shim for linking OLD (pre-glibc-2.3) prebuilt Karma/metoolkit
	static archives against a modern glibc. glibc <2.3 exported the ctype tables
	as the data symbols __ctype_b / __ctype_toupper / __ctype_tolower; glibc >=2.3
	replaced them with the thread-safe accessor functions __ctype_b_loc() etc. and
	dropped the old data symbols. Archives built on the old glibc (e.g. the sigmaco
	metoolkit-karma gcc3.1 Linux libs) still reference the removed data symbols, so
	we re-provide them here, initialized from the modern accessors at load time.

	Only compiled into the Karma reference build (BUILD_KARMA_REF). The in-tree
	gcc3.2 metoolkit libs were built against a newer glibc and don't need this, but
	the shim is harmless for them (the symbols simply go unreferenced).
=============================================================================*/
#include <ctype.h>

#if defined(__linux__) && defined(__GLIBC__)

/* glibc >=2.3 accessors (return a pointer to the per-thread table pointer). */
extern const unsigned short int **__ctype_b_loc(void);
extern const __int32_t **__ctype_toupper_loc(void);
extern const __int32_t **__ctype_tolower_loc(void);

/* The removed glibc <2.3 data symbols the old archives expect. */
const unsigned short int *__ctype_b;
const __int32_t *__ctype_toupper;
const __int32_t *__ctype_tolower;

/* Point the legacy tables at the current locale's tables before any static ctor
   in the Karma libs runs (constructor priority 101 = very early). */
__attribute__((constructor(101))) static void ufront_karma_ctype_compat(void)
{
	__ctype_b       = *__ctype_b_loc();
	__ctype_toupper = *__ctype_toupper_loc();
	__ctype_tolower = *__ctype_tolower_loc();
}

#endif /* __linux__ && __GLIBC__ */
