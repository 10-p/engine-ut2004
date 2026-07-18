//=============================================================================
// UManifest
// Copyright 2001 Digital Extremes - All Rights Reserved.
// Confidential.
//=============================================================================

#include "EnginePrivate.h"

void UManifest::AddEntry(const FString& packageName)
{
    check(FindEntry(packageName) < 0);

    UBOOL bFound = 0;
    INT i;

    for (i=0; i<ManifestEntries.Num(); i++)
    {
        if (ManifestEntries(i) == TEXT(""))
        {
            bFound = 1;
            break;
        }
    }

    if (!bFound)
        i = ManifestEntries.AddZeroed();

    ManifestEntries(i) = packageName;

    check(FindEntry(packageName) >= 0);
}

INT UManifest::FindEntry(const FString& packageName)
{
    return ManifestEntries.FindItemIndex(packageName);
}

void UManifest::RemoveEntry(INT index)
{
    check(ManifestEntries(index) != TEXT(""));
    ManifestEntries(index) = TEXT("");
}

