/*=============================================================================
	UnBuild.h: Unreal build settings.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*-----------------------------------------------------------------------------
	Major build options.
-----------------------------------------------------------------------------*/

// NOTE: Most of these are now passed in by the compiler. -- gan

// Whether to turn off normal checks.
#ifndef DO_CHECK
#error DO_CHECK not defined!
#endif

// Whether to turn off checks in performance critical routines.
#ifndef DO_CHECK_SLOW
#error DO_CHECK_SLOW not defined!
#endif

// Whether to track call-stack errors.
#ifndef DO_GUARD
#error DO_GUARD not defined!
#endif

// Whether to track call-stack errors in performance critical routines.
#ifndef DO_GUARD_SLOW
#error DO_GUARD_SLOW not defined!
#endif

// Whether to gather performance statistics.
#ifndef DO_STAT
#error DO_STAT not defined!
#endif

// Whether to gather slow performance statistics.
#ifndef DO_STAT_SLOW
#error DO_STAT_SLOW not defined!
#endif

// Slow stats depend on normal stats
#if DO_STAT_SLOW && !DO_STAT
#error DO_STAT_SLOW without DO_STAT does not make sense!
#endif

// Whether to perform CPU-intensive timing of critical loops.
#ifndef DO_CLOCK
#error DO_CLOCK not defined!
#endif

// Whether to perform CPU-intensive timing of critical loops.
#ifndef DO_CLOCK_SLOW
#error DO_CLOCK_SLOW not defined!
#endif

// Slow clocks depend on normal clocks
#if DO_CLOCK_SLOW && !DO_CLOCK
#error DO_CLOCK_SLOW without DO_CLOCK does not make sense!
#endif

// Clocks depend on stats
#if DO_CLOCK_SLOW && !DO_STAT
#error DO_CLOCK_SLOW without DO_STAT does not make sense!
#endif

#if DO_CLOCK && !DO_STAT
#error DO_CLOCK without DO_STAT does not make sense!
#endif

// Whether to use Intel assembler code.
#ifndef ASM
#define ASM 1
#endif

// Whether to use 3DNow! assembler code.
#ifndef ASM3DNOW
#define ASM3DNOW 0
#endif

// Whether to use Katmai assembler code.
#ifndef ASMKNI
#define ASMKNI 1
#endif

// Demo version.
#ifndef DEMOVERSION
#define DEMOVERSION 0
#endif

// Whether certain mesh export and conversion restrictions apply.
#ifndef RESTRICTEXPORT
#define RESTRICTEXPORT 1
#endif

// Whether codebase is still in development - used to filter out stuff (console commands) that should not be included
// in the released binaries
// This is handled by the build script - no need to ever manually modify this
#ifndef PRERELEASE
#define PRERELEASE
#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

