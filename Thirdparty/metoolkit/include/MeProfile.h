#ifndef _MEPROFILE_H /* -*- mode: C; -*- */
#define _MEPROFILE_H

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/15 11:56:32 $ - Revision: $Revision: 1.27.2.4 $

   This software and its accompanying manuals have been developed
   by MathEngine PLC ("MathEngine") and the copyright and all other
   intellectual property rights in them belong to MathEngine. All
   rights conferred by law (including rights under international
   copyright conventions) are reserved to MathEngine. This software
   may also incorporate information which is confidential to
   MathEngine.

   Save to the extent permitted by law, or as otherwise expressly
   permitted by MathEngine, this software and the manuals must not
   be copied (in whole or in part), re-arranged, altered or adapted
   in any way without the prior written consent of the Company. In
   addition, the information contained in the software may not be
   disseminated without the prior written consent of MathEngine.

 */

/** @file
 * Public profiling API
 */

#include <MePrecision.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Profiling output styles */
typedef enum
{
    kMeProfileOutputAverage,    /**< Compact frame average display (default) */
    kMeProfileOutputEvent,      /**< Compact frame and event average display */
    kMeProfileOutputNormal,     /**< Lists everything sequentially */
    kMeProfileOutputGnuplot     /**< Produces a few files in Gnuplot format */
}
MeProfileOutput_enum;

/** Counter modes. These are only relevant for PS2 and IRIX */
typedef enum
{
    /** Counts floating point operations */
    kMeProfileCounterModeFlops,
    /** Counts cache misses */
    kMeProfileCounterModeCache
}
MeProfileCounterModes_enum;

/** Logging modes. */
typedef enum
{
    /** Don't log, keep only the data for the latest frame */
    kMeProfileDontLog,
    /** Log each section for each frame, takes lots of memory */
    kMeProfileLogAll,
    /** Log only the cumulative numbers for each section across all frames */
    kMeProfileLogTotals
}
MeProfileLogModes_enum;

typedef struct
{
    MeU64                       cpuCycles;
    MeU64                       count0;
    MeU64                       count1;
}
MeProfileTimerResult;

/** Timer mode. These settings are only relevant for PS2 */
typedef struct
{
    unsigned                    granularity;    /**< Granularity */
    MeProfileCounterModes_enum  counterMode;    /**< Counter mode */
    const char                  **count0Label;  /**< Label for 'count0' */
    const char                  **count1Label;  /**< Label for 'count1' */
}
MeProfileTimerMode;

/** Output style parameters */
typedef struct
{
    MeProfileOutput_enum        style;          /**< Output style */
    float                       jaggedness;     /**< For gnuplot */
    unsigned                    settletime;     /**< For gnuplot */
}
MeProfileOutput;


/** Initialises profiling */
MEPUBLIC
void              MEAPI MeProfileStartTiming(MeProfileTimerMode mode,
                            MeProfileLogModes_enum logging);

/** Cleans up profiling */
MEPUBLIC
void              MEAPI MeProfileStopTiming();

/** Indicates where in the code a frame is said to have started. */
MEPUBLIC
void              MEAPI MeProfileStartFrame();

/** Indicates that the end of a frame has been reached. */
MEPUBLIC
void              MEAPI MeProfileEndFrame();

/** Start timing a section of code.
 *  @param codeSection Name of the section to be timed.
 *  @param autoStop If 1 then the timing of this section of code will
 *      stop automatically at the next call to %MeProfileStartSection().
 *      If 0 then an explicit call to %MeProfileEndSection() will have
 *      to be made. This allows for concurrent sections of timed code.
 */
#ifdef _ME_NOPROFILING
#   define MeProfileStartSection(x,y)
#else
#   define MeProfileStartSection(x,y) MeProfileStartSectionFn((x),(y))
MEPUBLIC
void              MEAPI MeProfileStartSectionFn(const char *codeSection,
                            char unsigned autoStop);
#endif

/** Explicitly stops timing the specified section of code.
 *  You should not call this if you set autostop to 1 in
 *  MeProfileStartSection().
 */
#ifdef _ME_NOPROFILING
#   define MeProfileEndSection(x)
#else
#   define MeProfileEndSection(x) MeProfileEndSectionFn(x)
MEPUBLIC
void              MEAPI MeProfileEndSectionFn(const char *codeSection);
#endif

/** Gets timer value */
MEPUBLIC
void              MEAPI MeProfileGetTimerValue(MeProfileTimerResult
                            *const result);

/** Can be called just before the end of a frame
 *  to make sure all timers are stopped.
 */
MEPUBLIC
void              MEAPI MeProfileStopTimers();

/** Output profile results in the currently selected style. */
MEPUBLIC
void              MEAPI MeProfileOutputResults();

MEPUBLIC
MeReal            MEAPI MeProfileGetSectionTime(const char *codeSection);

MEPUBLIC
MeReal            MEAPI MeProfileGetAllSectionTime();

MEPUBLIC
MeU64             MEAPI MeProfileGetClockSpeed();

/** Sets the ouput mode for profiling information. */
MEPUBLIC
void              MEAPI MeProfileSetOutputParameters(MeProfileOutput output);

/** Low overhead timer for internal PS2 profiling */

#ifdef PS2
    void          MEAPI ticInit();
    void          MEAPI tic(const char *name);
    void          MEAPI ticOutputResults(const char *filename);
    void          MEAPI ticCleanup();
#else
#   define ticInit(countType0,countType1)
#   define tic(name)
#   define ticOutputResults(filename)
#   define ticCleanup()
#endif

#ifdef __cplusplus
}
#endif

#endif
