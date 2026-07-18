#ifndef _MCDPROFILE_H
#define _MCDPROFILE_H

#ifndef PROFILE_MCD
#   define PROFILE_MCD 0
#endif

#if (!PROFILE_MCD)
#   define McdProfileStart(s)   ((void) 0)
#   define McdProfileEnd(s)     ((void) 0)
#else
#   include <MeProfile.h>
#   if 1
#       define McdProfileStart(s)   MeProfileStartSection(s,0)
#       define McdProfileEnd(s)     MeProfileEndSection(s)
#   elif (__GNUC__)
        static __inline void McdProfileStart(const char *const s)
        { MeProfileStartSection(s,0); }
        static __inline void McdProfileEnd(const char *const s)
        { MeProfileEndSection(s); }
#   elif (_MSC_VER)
        static _inline void McdProfileStart(const char *const s)
        { MeProfileStartSection(s,0); }
        static _inline void McdProfileEnd(const char *const s)
        { MeProfileEndSection(s); }
#   else
#       define McdProfileStart(s)   MeProfileStartSection(s,0)
#       define McdProfileEnd(s)     MeProfileEndSection(s)
#   endif
#endif

#endif
