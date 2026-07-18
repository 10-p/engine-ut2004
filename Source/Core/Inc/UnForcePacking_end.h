
// Please see my comments in UnForcePacking_begin.h.  --ryan.

#if FORCE_EXTREME_PACKING
  #ifndef EXTREME_PACKING_ENFORCED
    #error Extreme packing mojo not in effect. Something is confused.
  #endif

  #undef EXTREME_PACKING_ENFORCED

  #if SUPPORTS_PRAGMA_PACK
    #pragma pack(pop)
  #else
    #pragma pack()
  #endif

  #ifdef GCC_PACK_TMP
    #undef GCC_PACK
    #define GCC_PACK(x) GCC_PACK_TMP(x)
    #undef GCC_PACK_TMP
  #endif
#endif

// end of UnForcePacking_end.h ...

