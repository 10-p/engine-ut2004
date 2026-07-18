/*=============================================================================
	UWebNative.h: Native function lookup table for static libraries.
	Copyright 2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Brandon Reinhart
=============================================================================*/

#ifndef UWEBNATIVE_H
#define UWEBNATIVE_H

#if __STATIC_LINK

DECLARE_NATIVE_TYPE(UWeb,UWebResponse);
DECLARE_NATIVE_TYPE(UWeb,UWebRequest);

#define AUTO_INITIALIZE_REGISTRANTS_UWEB \
	UWebResponse::StaticClass(); \
	UWebRequest::StaticClass();

#endif

#endif
