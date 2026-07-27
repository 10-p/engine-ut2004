/*=============================================================================
	UnCanvas.cpp: Unreal canvas rendering.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney
	* 31/3/99 Updated revision history - Jack Porter
=============================================================================*/

#include "EnginePrivate.h"

#define MAX_SCREEN_LIGHTS 8
static FDynamicLight screenLights[MAX_SCREEN_LIGHTS];
/*-----------------------------------------------------------------------------
	UCanvas scaled sprites.
-----------------------------------------------------------------------------*/

//
// Draw arbitrary aligned rectangle.
//
void UCanvas::DrawTile
(
	UMaterial*		Material,
	FLOAT			X,
	FLOAT			Y,
	FLOAT			XL,
	FLOAT			YL,
	FLOAT			U,
	FLOAT			V,
	FLOAT			UL,
	FLOAT			VL,
	FLOAT			Z,
	FPlane			Color,
	FPlane			Fog
)
{
	guard(UCanvas::DrawTile);

    if ( !pCanvasUtil || !Material ) // gam
        return;

    Color *= ColorModulate; // sjs
    UMaterial* PrevMaterial = Material; // sjs

	if (bForceAlpha)
	{
		Color.W = ForcedAlpha;
		Style = 5;
	}

	if( Cast<UTexture>(Material) )
	{
		DECLARE_STATIC_UOBJECT( UFinalBlend, FinalBlend, {} );

        // gam --- Hack city!
        static BYTE LastStyle = STY_None;
        static UMaterial *LastMaterial = NULL;

        if( (Style != LastStyle) || (Material != LastMaterial) )
        {
            pCanvasUtil->Flush();
            LastStyle = Style;
            LastMaterial = Material;
        }
        // --- gam

		if ( !FinalBlend )
			return;

		FinalBlend->Material = Material;
		FinalBlend->TwoSided = 0;
		FinalBlend->ZWrite = 0;
		FinalBlend->ZTest = 0;

		switch( Style )
		{
		case STY_Masked:
			FinalBlend->FrameBufferBlending = FB_Overwrite;
			FinalBlend->AlphaTest = 1;
			FinalBlend->AlphaRef = 127;
			Material = FinalBlend;				
			break;
		case STY_Translucent:
			FinalBlend->FrameBufferBlending = FB_Translucent;
			FinalBlend->AlphaTest = 0;
			Material = FinalBlend;				
			break;
		case STY_Modulated:
			FinalBlend->FrameBufferBlending = FB_Modulate;
			FinalBlend->AlphaTest = 0;
			Material = FinalBlend;				
			break;
        // gam ---
		case STY_Alpha:
			FinalBlend->FrameBufferBlending = FB_AlphaBlend;

			FinalBlend->AlphaTest = 1;
			FinalBlend->AlphaRef = 0;
			Material = FinalBlend;				
			break;
		case STY_Additive:
			FinalBlend->FrameBufferBlending = FB_Brighten;
			FinalBlend->AlphaTest = 1;
			FinalBlend->AlphaRef = 0;
			Material = FinalBlend;				
			break;
		case STY_Subtractive:
			FinalBlend->FrameBufferBlending = FB_Darken;
			FinalBlend->AlphaTest = 0;
			Material = FinalBlend;				
			break;
        // --- gam
		}

        if( PrevMaterial && (Style == 255 || PrevMaterial->IsA(UCombiner::StaticClass()) || PrevMaterial->IsA(UShader::StaticClass()) || PrevMaterial->IsA(UFinalBlend::StaticClass())) ) // sjs
            Material = PrevMaterial;

		//!!MAT
		//if     ( Actor->bSelected              ) PolyFlags |= PF_Selected;
	}

	pCanvasUtil->DrawTile(X,Y,X + XL,Y + YL,U,V,U + UL,V + VL,0.0f,Material,Color);

	unguard;
}

//
// Draw titling pattern.
//
void UCanvas::DrawPattern
(
	UMaterial*		Material,
	FLOAT			X,
	FLOAT			Y,
	FLOAT			XL,
	FLOAT			YL,
	FLOAT			Scale,
	FLOAT			OrgX,
	FLOAT			OrgY,
	FLOAT			Z,
	FPlane			Color,
	FPlane			Fog
)
{
	guard(UCanvas::DrawPattern);
	DrawTile( Material, X, Y, XL, YL, (X-OrgX)*Scale + Material->MaterialUSize(), (Y-OrgY)*Scale + Material->MaterialVSize(), XL*Scale, YL*Scale, Z, Color, Fog );
	unguard;
}

//
// Draw a scaled sprite.  Takes care of clipping.
// XSize and YSize are in pixels.
//
void UCanvas::DrawIcon
(
	UMaterial*			Material,
	FLOAT				ScreenX, 
	FLOAT				ScreenY, 
	FLOAT				XSize, 
	FLOAT				YSize,
	FLOAT				Z,
	FPlane				Color,
	FPlane				Fog
)
{
	guard(UCanvas::DrawIcon);
	DrawTile( Material, ScreenX, ScreenY, XSize, YSize, 0, 0, Material->MaterialUSize(), Material->MaterialVSize(), Z, Color, Fog );
	unguard;
}

/*-----------------------------------------------------------------------------
	Clip window.
-----------------------------------------------------------------------------*/

void UCanvas::SetClip( INT X, INT Y, INT XL, INT YL )
{
	guard(UCanvas::SetClip);

	CurX  = 0;
	CurY  = 0;
	OrgX  = X;
	OrgY  = Y;
	ClipX = XL;
	ClipY = YL;

	unguard;
}

/*-----------------------------------------------------------------------------
	UCanvas basic text functions.
-----------------------------------------------------------------------------*/

//
// Draw a string of characters.
// - returns pixels drawn
//
static INT DrawString
(
	UCanvas*		Canvas, 
	UFont*			Font, 
    FLOAT           ScaleX, // gam
    FLOAT           ScaleY, // gam
	INT				DrawX, 
	INT				DrawY,
	const TCHAR*	Text, 
	FPlane			Color, 
	UBOOL			bClip, 
	UBOOL			bHandleApersand
)
{
	guardSlow(DrawString);

	ScaleX *= Font->ScaleFactor;
	ScaleY *= Font->ScaleFactor;

    if ( !Canvas->pCanvasUtil ) // sjs
        return 0;

    int i;

	// Draw all characters in string.
	INT LineX = 0;
	INT bDrawUnderline = 0;
	INT UnderlineWidth = 0;
	for( i=0; Text[i]; i++ )
	{
		// STX/ETX control codes
		// Used to indicate the beginning & end of hyperlinks
		if ( Text[i] == 2 || Text[i] == 3 )
			continue;

		if ( int(Text[i]) != 27 )
		{

			INT bUnderlineNext = 0;
			INT Ch = (TCHARU)Font->RemapChar(Text[i]);

			// Handle ampersand underlining.
			if( bHandleApersand )
			{
				if( bDrawUnderline )
					Ch = (TCHARU)Font->RemapChar('_');

				if( Text[i]==4 )
				{
					if( !Text[i+1] )
						break; 
//					if( Text[i+1]!='&' )
//					{
						bUnderlineNext = 1;
						Ch = (TCHARU)Font->RemapChar(Text[i+1]);
//					}
				}
			}

			// Process character if it's valid.
			if( Ch < Font->Characters.Num() )
			{
				FFontCharacter& Char = Font->Characters(Ch);
				UTexture* Tex;
				if( Char.TextureIndex < Font->Textures.Num() && (Tex=Font->Textures(Char.TextureIndex))!=NULL )
				{
					// Compute character width.
					FLOAT CharWidth;
					if( bDrawUnderline )
						CharWidth = Min(UnderlineWidth, Char.USize);
					else
						CharWidth = Char.USize;


					// Prepare for clipping.
					FLOAT X      = LineX + DrawX;
					FLOAT Y      = DrawY;
					FLOAT CU     = Char.StartU;
					FLOAT CV     = Char.StartV;
					FLOAT CUSize = CharWidth;
					FLOAT CVSize = Char.VSize ;

					// Draw if it passes clip test.
					if
					(	(!bClip)
					||	( (X +(CUSize*ScaleX)) > 0 && X <= Canvas->ClipX && (Y + (CVSize*ScaleY)) >0 && Y <= Canvas->ClipY) )
					{
						if( bClip )
						{
							if( X        < 0.f           ) { CU-=X; CUSize+=X; X=0;  }
							if( Y        < 0.f           ) { CV-=Y; CVSize+=Y; Y=0;  }
							if( X+(CUSize*ScaleX) > Canvas->ClipX ) { CUSize=(INT) (Canvas->ClipX-X)/ScaleX; }
							if( Y+(CVSize*ScaleY) > Canvas->ClipY ) { CVSize=(INT) (Canvas->ClipY-Y)/ScaleY; } 
						}

						FLOAT		TileX = Canvas->OrgX + X,
									TileY = Canvas->OrgY + Y;

						// gam ---
						Canvas->DrawTile( Tex, appRound(TileX), appRound(TileY), CUSize*ScaleX, CVSize*ScaleY,
										CU, CV, CUSize, CVSize, 0.0F, Color, FPlane(0,0,0,0) );
						// --- gam
					}
					// Update underline status.
					if( bDrawUnderline )
						CharWidth = UnderlineWidth;

					if( !bUnderlineNext )
						LineX += (INT)((FLOAT)(CharWidth + Canvas->SpaceX + Font->Kerning) * ScaleX); // gam
					else
						UnderlineWidth = Char.USize * ScaleX;

					bDrawUnderline = bUnderlineNext;
				}
			}
		}
		else
		{
			if ( Text[i+1] )
				Color.X = FLOAT(Text[i+1]) / 255.0;
			if ( Text[i+2] )
				Color.Y = FLOAT(Text[i+2]) / 255.0;
			if ( Text[i+3] )
				Color.Z = FLOAT(Text[i+3]) / 255.0;

			i = i+3;
			Color.W = 1.0;
		}

	}

	return LineX;

	unguardSlow;
}

//
// Get a character's dimensions.
//
static inline void GetCharSize( UFont* Font, TCHAR InCh, INT& Width, INT& Height )
{
	guardSlow(GetCharSize);
	INT Ch    = (TCHARU)Font->RemapChar(InCh);

	if( Ch < Font->Characters.Num() )
	{
		FFontCharacter& Char = Font->Characters(Ch);
		Width = Char.USize;
		Height = Char.VSize;
	}
	else
	{
		Width = 0;
		Height = 0;
	}

	unguardSlow;
}

// Notice: This will actually append to the array for continous wrapping, does not support tabs yet
void UCanvas::WrapStringToArray( const TCHAR *Text, TArray<FString> *OutArray, float Width, UFont *pFont, TCHAR EOL)
{
	check(Text != NULL);
	check(OutArray != NULL);

	const TCHAR *LastText;

	if (*Text == '\0')
		return;

	if (pFont == NULL)
		pFont = this->Font;

	TCHAR CurrColour[5] = { 0, 0, 0, 0, 0 }; // default to black
	UBOOL UseColour = 0;

	do
	{
		while (*Text == EOL)
		{
			new(*OutArray) FString(TEXT(""));
			Text++;
		}
		if (*Text==0)
			break;

		LastText = Text;
		FString ColourStart(UseColour?CurrColour:TEXT(""));

		INT iCleanWordEnd=0, iTestWord;
		INT TestXL=0, CleanXL=0, Skip=0;
		UBOOL GotWord=0;
		for( iTestWord=0; Text[iTestWord]!=0 && Text[iTestWord]!=EOL; )
		{
			INT ChW, ChH;

			if ( Text[iTestWord] == 2 || Text[iTestWord] == 3 )
			{
//				debugf(NAME_DebugRon, TEXT("WrapStringToArray found text control character %i in %s"), Text[iTestWord], Text);
				iTestWord++;
				continue;
			}
			
			if ( Text[iTestWord] == 27 )		// Skip over color codes
				Skip=4;

			if (!Skip)
			{
				GetCharSize(pFont, Text[iTestWord], ChW, ChH);
				ChW*=pFont->ScaleFactor;
				ChH*=pFont->ScaleFactor;

				TestXL              += ChW + SpaceX + pFont->Kerning; // gam
		
				if( TestXL>Width )
					break;
			}
			else
			{
				// Read colour code.
				CurrColour[4-Skip] = Text[iTestWord];
				UseColour = 1;

				Skip--;
			}


			iTestWord++;
			UBOOL WordBreak = Text[iTestWord]==' ' || Text[iTestWord]==EOL || Text[iTestWord]==0;
			if( WordBreak || !GotWord )
			{
				iCleanWordEnd = iTestWord;
				CleanXL       = TestXL;
				GotWord       = GotWord || WordBreak;
			}
		}

		if( iCleanWordEnd==0 )
		{
			if (*Text != 0)
			{
				if ( Text == LastText )
				{
					new(*OutArray) FString( ColourStart + Text );
					Text++;
				}
				else
					new(*OutArray) FString(TEXT(""));
			}
		}
		else
		{
			FString TextLine(Text);
			new(*OutArray) FString(ColourStart + TextLine.Left(iCleanWordEnd));
		}
		Text += iCleanWordEnd;

		// Skip whitespace after word wrap.
		while( *Text==' ' )
			Text++;

		if (*Text == EOL)
			Text++;
	}
	while( *Text );
}

void UCanvas::execWrapStringToArray( FFrame& Stack, RESULT_DECL )
{
	guard(UCanvas::execWrapStringToArray);
	P_GET_STR(InText);
	P_GET_TARRAY_REF(OutArray, FString);
	P_GET_FLOAT(Width);
	P_GET_STR_OPTX(EOL, TEXT("|"));
	P_FINISH;

	WrapStringToArray(*InText, OutArray, Width, NULL, EOL[0]);

	unguardexec;
}
IMPLEMENT_FUNCTION( UCanvas, -1, execWrapStringToArray );

//
// Compute size and optionally print text with word wrap.
//!!For the next generation, redesign to ignore CurX,CurY.
//
void UCanvas::WrappedPrint( ERenderStyle Style, INT& XL, INT& YL, UFont* Font, FLOAT ScaleX, FLOAT ScaleY, UBOOL Center, const TCHAR* Text ) // gam
{
	guard(UCanvas::WrappedPrint);

	if( ClipX<0 || ClipY<0 || !Font )
		return;
	check(Font);
	FPlane DrawColor = Color.Plane();

	// Process each word until the current line overflows.
	XL = YL = 0;
	do
	{
		INT iCleanWordEnd=0, iTestWord;
		INT TestXL=(INT)CurX, CleanXL=0;
		INT TestYL=0,    CleanYL=0;
		UBOOL GotWord=0;
		for( iTestWord=0; Text[iTestWord]!=0 && Text[iTestWord]!='\n'; )
		{

			INT ChW, ChH;
			GetCharSize(Font, Text[iTestWord], ChW, ChH);
			ChW *= Font->ScaleFactor;
			ChH *= Font->ScaleFactor;

			TestXL              += (INT) ((FLOAT)(ChW + SpaceX + Font->Kerning) * ScaleX); // gam
			TestYL               = (INT) ((FLOAT)(Max( TestYL, ChH + (INT)SpaceY)) * ScaleY); // gam
			if( TestXL>ClipX )
				break;
			iTestWord++;
			UBOOL WordBreak = Text[iTestWord]==' ' || Text[iTestWord]=='\n' || Text[iTestWord]==0;
			if( WordBreak || !GotWord )
			{
				iCleanWordEnd = iTestWord;
				CleanXL       = TestXL;
				CleanYL       = TestYL;
				GotWord       = GotWord || WordBreak;
			}
		}
		if( iCleanWordEnd==0 )
			break;

		// Sucessfully split this line, now draw it.
		if( Style!=STY_None && OrgY+CurY<Viewport->SizeY && OrgY+CurY+CleanYL>0 )
		{
			FString TextLine(Text);
			INT LineX = Center ? (INT) (CurX+(ClipX-CleanXL)/2) : (INT) (CurX);
			LineX += DrawString( this, Font, ScaleX, ScaleY, LineX, (INT) CurY, *(TextLine.Left(iCleanWordEnd)), DrawColor, 0, 0 );
			CurX = LineX;
		}

		// Update position.
		CurX  = 0;
		CurY += CleanYL;
		YL   += CleanYL;
		XL    = Max(XL,CleanXL);
		Text += iCleanWordEnd;

		// Skip whitespace after word wrap.
		while( *Text==' ' )
			Text++;
	}
	while( *Text );

	unguardf(( TEXT("(%s)"), Text ));
}

/*-----------------------------------------------------------------------------
	UCanvas derived text functions.
-----------------------------------------------------------------------------*/

//
// Calculate the size of a string built from a font, word wrapped
// to a specified region.
//
void VARARGS UCanvas::WrappedStrLenf( UFont* Font, INT& XL, INT& YL, const TCHAR* Fmt, ... )
{
	TCHAR Text[4096];
	GET_VARARGS( Text, ARRAY_COUNT(Text), Fmt, Fmt );

	guard(UCanvas::WrappedStrLenf);
	WrappedPrint( STY_None, XL, YL, Font, 1.0F, 1.0F, 0, Text ); // gam
	unguard;
}

//
// Wrapped printf.
//
void VARARGS UCanvas::WrappedPrintf( UFont* Font, UBOOL Center, const TCHAR* Fmt, ... )
{
	TCHAR Text[4096];
	GET_VARARGS( Text, ARRAY_COUNT(Text), Fmt, Fmt );

	guard(UCanvas::WrappedPrintf);
	INT XL=0, YL=0;
	WrappedPrint( STY_Normal, XL, YL, Font, 1.0F, 1.0F, Center, Text ); // gam
	unguard;
}

// gam ---
//
// Calculate the size of a string built from a font, word wrapped
// to a specified region.
//
void VARARGS UCanvas::WrappedStrLenf( UFont* Font, FLOAT ScaleX, FLOAT ScaleY, INT& XL, INT& YL, const TCHAR* Fmt, ... ) // gam
{
	TCHAR Text[4096];
	GET_VARARGS( Text, ARRAY_COUNT(Text), Fmt, Fmt );

	guard(UCanvas::WrappedStrLenf);
	WrappedPrint( STY_None, XL, YL, Font, ScaleX, ScaleY, 0, Text ); // gam
	unguard;
}

//
// Wrapped printf.
//
void VARARGS UCanvas::WrappedPrintf( UFont* Font, FLOAT ScaleX, FLOAT ScaleY, UBOOL Center, const TCHAR* Fmt, ... ) // gam
{
	TCHAR Text[4096];
	GET_VARARGS( Text, ARRAY_COUNT(Text), Fmt, Fmt );

	guard(UCanvas::WrappedPrintf);
	INT XL=0, YL=0;
	WrappedPrint( STY_Normal, XL, YL, Font, ScaleX, ScaleY, Center, Text ); // gam
	unguard;
}

void UCanvas::ClippedStrLen( UFont* Font, FLOAT ScaleX, FLOAT ScaleY, FLOAT& XL, FLOAT& YL, const TCHAR* Text )
{
	guard(UCanvas::ClippedStrLen)
#if 1 // rjp - this method is a HUGE bottleneck for the GUI.  new version isn't *much* faster, 
	  // but this function is executed hundreds times per second, so every bit counts

	if ( !Font )
		Font = this->Font;

	if ( !Text || Font==NULL )
		return;

	ScaleX *= Font->ScaleFactor;
	ScaleY *= Font->ScaleFactor;

	register INT CharIncrement = ((INT) SpaceX) + Font->Kerning;
	register const TCHAR* T = Text;
	register INT Skip = 0;
	register INT W = 0;
	register INT H = 0;
	register FLOAT regXL = 0.0f;
	register FLOAT regYL = 0.0f;

	while ( *T )
	{
		if ( *T == 27 )
			Skip += 4;
		
		if ( Skip )
			Skip--;

		else
		{
			GetCharSize(Font, *T, W, H);
			if ( *(T + 1) )
				W += CharIncrement;

			regXL += W;
			if ( regYL < H )
				regYL = H;
		}

		T++;
	}

	XL = regXL * ScaleX;
	YL = regYL * ScaleY;

#else // rjp - old version
	XL = 0;
	YL = 0;

	if (Font == NULL)
		Font = this->Font;

	if (Font == NULL)
		return;

	INT Skip=0;

	for( INT i=0; Text[i]; i++)
	{
    	INT W, H;

		if (int(Text[i])==27)
			Skip=4;

		if (!Skip)
		{

			GetCharSize( Font, Text[i], W, H );

			// gam ---
			if( Text[i + 1] )
				W = (INT)((FLOAT)(W + SpaceX + Font->Kerning) * ScaleX);
			else
				W = (INT)((FLOAT)(W) * ScaleX);

			H = (INT)((FLOAT)(H) * ScaleY);
			// --- gam
		
			XL += W;
			if(YL < H)
				YL = H;	
		}
		else
			Skip--;

	}
#endif

	unguard;
}

void UCanvas::ClippedPrint( UFont* Font, FLOAT ScaleX, FLOAT ScaleY, UBOOL Center, const TCHAR* Text )
{
	guard(UCanvas::ClippedPrint);

	FPlane DrawColor = Color.Plane();
	DrawString( this, Font, ScaleX, ScaleY, (INT) CurX, (INT) CurY, Text, DrawColor, 1, 0 );

	unguard;
}

// --- gam

/*-----------------------------------------------------------------------------
	UCanvas object functions.
-----------------------------------------------------------------------------*/

void UCanvas::Init( UViewport* InViewport )
{
	guard(UCanvas::Init);
	Viewport = InViewport;

	// Load default fonts
	TinyFont	= Cast<UFont>( StaticLoadObject( UFont::StaticClass(), NULL, *TinyFontName, NULL, LOAD_NoWarn, NULL ) );
	if( !TinyFont )
	{
		GWarn->Logf(TEXT("Could not load stock Tiny font %s"), *TinyFontName);
		TinyFont=FindObjectChecked<UFont>(ANY_PACKAGE,TEXT("DefaultFont"));
	}
	SmallFont	= Cast<UFont>( StaticLoadObject( UFont::StaticClass(), NULL, *SmallFontName, NULL, LOAD_NoWarn, NULL ) );
	if( !SmallFont )
	{
		GWarn->Logf(TEXT("Could not load stock Small font %s"), *SmallFontName);
		SmallFont=FindObjectChecked<UFont>(ANY_PACKAGE,TEXT("DefaultFont"));
	}
	MedFont		= Cast<UFont>( StaticLoadObject( UFont::StaticClass(), NULL, *MedFontName, NULL, LOAD_NoWarn, NULL ) );
	if( !MedFont )
	{
		GWarn->Logf(TEXT("Could not load stock Medium font %s"), *MedFontName);
		MedFont=FindObjectChecked<UFont>(ANY_PACKAGE,TEXT("DefaultFont"));
	}
	unguard;
}
void UCanvas::Update()
{
	guard(UCanvas::Update);

	// Call UnrealScript to reset.
	eventReset();

	// Copy size parameters from viewport.
	ClipX = Viewport->SizeX;
	SizeX = (INT) ClipX;
	ClipY = Viewport->SizeY;
	SizeY = (INT) ClipY;

    // sjs ---
    if ( !pCanvasUtil )
        pCanvasUtil = new FCanvasUtil(&Viewport->RenderTarget,Viewport->RI);

	if( GIsOpenGL )
		pCanvasUtil->CanvasToScreen = FTranslationMatrix(FVector(-Viewport->SizeX / 2.0f,-Viewport->SizeY / 2.0f,0.0f)) * FScaleMatrix(FVector(2.0f / Viewport->SizeX,-2.0f / Viewport->SizeY,1.0f));
	else
		pCanvasUtil->CanvasToScreen = FTranslationMatrix(FVector(-Viewport->SizeX / 2.0f - 0.5f,-Viewport->SizeY / 2.0f - 0.5f,0.0f)) * FScaleMatrix(FVector(2.0f / Viewport->SizeX,-2.0f / Viewport->SizeY,1.0f));
	pCanvasUtil->ScreenToCanvas = pCanvasUtil->CanvasToScreen.Inverse();
    pCanvasUtil->RI = Viewport->RI;
    // --- sjs

	unguard;
}

void UCanvas::Destroy()
{
	guard(UCanvas::Destroy);

	if ( pCanvasUtil )
		delete pCanvasUtil;

	pCanvasUtil = NULL;
	Super::Destroy();

	unguard;
}

/*-----------------------------------------------------------------------------
	UCanvas natives.
-----------------------------------------------------------------------------*/

void UCanvas::execStrLen( FFrame& Stack, RESULT_DECL )
{
	guard(UCanvas::execStrLen);

	P_GET_STR(InText);
	P_GET_FLOAT_REF(XL);
	P_GET_FLOAT_REF(YL);
	P_FINISH;

	INT XLi, YLi;
	INT OldCurX, OldCurY;
	OldCurX = (INT) CurX;
	OldCurY = (INT) CurY;
	CurX = 0;
	CurY = 0;
	WrappedStrLenf( Font, FontScaleX, FontScaleY, XLi, YLi, TEXT("%s"), *InText ); // gam
	CurY = OldCurY;
	CurX = OldCurX;
	*XL = XLi;
	*YL = YLi;

	unguardexec;
}
IMPLEMENT_FUNCTION( UCanvas, 464, execStrLen );

void UCanvas::execDrawText( FFrame& Stack, RESULT_DECL )
{
	guard(UCanvas::execDrawText);
	P_GET_STR(InText);
	P_GET_UBOOL_OPTX(CR,1);
	P_FINISH;

	if( Style == STY_None ) // gam
        return;

	if( !Font )
	{
		Stack.Logf( NAME_Warning, TEXT("DrawText: No font") ); // gam
		return;
	}
	INT XL=0, YL=0;
//	unclock(GScriptCycles);
//	clock(GStats.DWORDStats(GEngineStats.STATS_Game_CanvasCycles));
	if( Style!=STY_None )
		WrappedPrint( (ERenderStyle)Style, XL, YL, Font, FontScaleX, FontScaleY, bCenter, *InText ); // gam
    
//    unclock(GStats.DWORDStats(GEngineStats.STATS_Game_CanvasCycles));
//	clock(GScriptCycles);
	CurX += XL;
	CurYL = Max(CurYL,(FLOAT)YL);
	if( CR )
	{
		CurX  = 0;
		CurY += CurYL;
		CurYL = 0;
	}

	unguardexec;
}
IMPLEMENT_FUNCTION( UCanvas, 465, execDrawText );

void UCanvas::execDrawTile( FFrame& Stack, RESULT_DECL )
{
	guard(UCanvas::execDrawTile);
	P_GET_OBJECT(UMaterial,Mat);
	P_GET_FLOAT(XL);
	P_GET_FLOAT(YL);
	P_GET_FLOAT(U);
	P_GET_FLOAT(V);
	P_GET_FLOAT(UL);
	P_GET_FLOAT(VL);
	P_FINISH;
	if( !Mat )
	{
		Stack.Logf( TEXT("DrawTile: Missing Material") );
		return;
	}
//	unclock(GScriptCycles);
//	clock(GStats.DWORDStats(GEngineStats.STATS_Game_CanvasCycles));
	if( Style!=STY_None ) DrawTile
	(
		Mat,
		OrgX+CurX,
		OrgY+CurY,
		XL,
		YL,
		U,
		V,
		UL,
		VL,
		Z,
		Color.Plane(),
		FPlane(0,0,0,0)
	);
//	unclock(GStats.DWORDStats(GEngineStats.STATS_Game_CanvasCycles));
//	clock(GScriptCycles);
	CurX += XL + SpaceX;
	CurYL = Max(CurYL,YL);
	unguardexec;
}
IMPLEMENT_FUNCTION( UCanvas, 466, execDrawTile );

void UCanvas::DrawActor( AActor* Actor, UBOOL WireFrame, UBOOL ClearZ, FLOAT DisplayFOV )
{
	// AutoFOV, viewmodel pass (step 2.16). This is the first-person weapon path — Weapon.uc:936 calls
	// Canvas.DrawActor(self,false,false,DisplayFOV) with its OWN var(FirstPerson) DisplayFOV (default 90),
	// which becomes a SEPARATE FActorSceneNode below. So unlike UE1 (where the viewmodel shares the world
	// frame and inherits the correction for free) UE2 must correct it explicitly: without this the world
	// would widen to 106 deg at 16:9 while the weapon stayed at a stock 90, making it look bigger and
	// vertically cropped — the exact complaint AutoFOV exists to fix. AutoFOVWeapon=0 opts back out.
	DisplayFOV = ufrontWeaponFov( DisplayFOV, ufrontEffectiveFov( DisplayFOV, Viewport->SizeX, Viewport->SizeY ) );

    guard(UCanvas::DrawActor)

    if ( pCanvasUtil ) // sjs
        pCanvasUtil->Flush();

	AActor*		CameraActor = Viewport->Actor;
	FVector		CameraLocation = Viewport->Actor->Location;
	FRotator	CameraRotation = Viewport->Actor->Rotation;

    if (Actor) //amb
	Viewport->Actor->eventPlayerCalcView(CameraActor,CameraLocation,CameraRotation);

//	unclock(GScriptCycles);
//	clock(GStats.DWORDStats(GEngineStats.STATS_Game_CanvasCycles));
	INT OldRendMap;
	OldRendMap = Viewport->Actor->RendMap;
	if( WireFrame )
		Viewport->Actor->RendMap = REN_Wire;
	if( ClearZ )
		Viewport->RI->Clear(0,FColor(0,0,0),1,1.0f,0);
    if (Actor) //amb
	    FActorSceneNode(Viewport,&Viewport->RenderTarget,Actor,CameraActor,CameraLocation,CameraRotation,DisplayFOV).Render(Viewport->RI);
	Viewport->Actor->RendMap = OldRendMap;
//	unclock(GStats.DWORDStats(GEngineStats.STATS_Game_CanvasCycles));
//	clock(GScriptCycles);

    unguard;
}

void UCanvas::execDrawActor( FFrame& Stack, RESULT_DECL )
{
	guard(UCanvas::execDrawActor);

	P_GET_OBJECT(AActor, Actor);
	P_GET_UBOOL(WireFrame);
	P_GET_UBOOL_OPTX(ClearZ, 0);
	P_GET_FLOAT_OPTX(DisplayFOV,Viewport->Actor->FovAngle);
	P_FINISH;

    DrawActor( Actor, WireFrame, ClearZ, DisplayFOV ); // gam

	unguardexec;
}

IMPLEMENT_FUNCTION( UCanvas, 467, execDrawActor );

// execDrawActorCLipped - An excercise in blind block copy :)

void UCanvas::execDrawActorClipped( FFrame& Stack, RESULT_DECL )
{
	guard(UCanvas::execDrawActorClipped);

	P_GET_OBJECT(AActor, Actor);
	P_GET_UBOOL(WireFrame);
	P_GET_FLOAT(Left);
	P_GET_FLOAT(Top);
	P_GET_FLOAT(Width);
	P_GET_FLOAT(Height);
	P_GET_UBOOL_OPTX(ClearZ, 0);
	P_GET_FLOAT_OPTX(DisplayFOV,Viewport->Actor->FovAngle);
	P_FINISH;

    if ( pCanvasUtil ) // sjs
        pCanvasUtil->Flush();

	FAuxRenderTarget RT(Width, Height, TEXF_RGBA8);

	Viewport->RI->SetViewport(Left,Top,Width,Height);

	AActor*		CameraActor = Viewport->Actor;
	FVector		CameraLocation = Viewport->Actor->Location;
	FRotator	CameraRotation = Viewport->Actor->Rotation;

    if (Actor) //amb
	Viewport->Actor->eventPlayerCalcView(CameraActor,CameraLocation,CameraRotation);

	INT OldRendMap;
	OldRendMap = Viewport->Actor->RendMap;
	if( WireFrame )
		Viewport->Actor->RendMap = REN_Wire;
	if( ClearZ )
		Viewport->RI->Clear(0,FColor(0,0,0),1,1.0f,0);
    if (Actor) //amb
	    FActorSceneNode(Viewport,&RT,Actor,CameraActor,CameraLocation,CameraRotation,DisplayFOV).Render(Viewport->RI);

	Viewport->Actor->RendMap = OldRendMap;

	Viewport->RI->SetViewport(0,0,Viewport->SizeX, Viewport->SizeY);

	unguardexec;
}
IMPLEMENT_FUNCTION( UCanvas, -1, execDrawActorClipped );


void UCanvas::execDrawTileClipped( FFrame& Stack, RESULT_DECL )
{
	guard(UCanvas::execDrawTileClipped);
	P_GET_OBJECT(UMaterial,Mat);
	P_GET_FLOAT(XL);
	P_GET_FLOAT(YL);
	P_GET_FLOAT(U);
	P_GET_FLOAT(V);
	P_GET_FLOAT(UL);
	P_GET_FLOAT(VL);
	P_FINISH;

	if( !Mat )
	{
		Stack.Logf( TEXT("DrawTileClipped: Missing Material") );
		return;
	}


	// Clip to ClipX and ClipY
	if( XL > 0 && YL > 0 )
	{		
		if( CurX<0 )
			{FLOAT C=CurX*UL/XL; U-=C; UL+=C; XL+=CurX; CurX=0;}
		if( CurY<0 )
			{FLOAT C=CurY*VL/YL; V-=C; VL+=C; YL+=CurY; CurY=0;}
		if( XL>ClipX-CurX )
			{UL+=(ClipX-CurX-XL)*UL/XL; XL=ClipX-CurX;}
		if( YL>ClipY-CurY )
			{VL+=(ClipY-CurY-YL)*VL/YL; YL=ClipY-CurY;}
	
//		unclock(GScriptCycles);
//		clock(GStats.DWORDStats(GEngineStats.STATS_Game_CanvasCycles));
		if( Style!=STY_None ) 
			DrawTile
			(
				Mat,
				OrgX+CurX,
				OrgY+CurY,
				XL,
				YL,
				U,
				V,
				UL,
				VL,
				Z,
				Color.Plane(),
				FPlane(0,0,0,0)
			);
//		unclock(GStats.DWORDStats(GEngineStats.STATS_Game_CanvasCycles));
//		clock(GScriptCycles);

		CurX += XL + SpaceX;
		CurYL = Max(CurYL,YL);
	}

	unguardexec;
}
IMPLEMENT_FUNCTION( UCanvas, 468, execDrawTileClipped );

void UCanvas::execDrawTextClipped( FFrame& Stack, RESULT_DECL )
{
	guard(UCanvas::execDrawTextClipped);
	P_GET_STR(InText);
	P_GET_UBOOL_OPTX(CheckHotKey, 0);
	P_FINISH;

	if( !Font )
	{
		Stack.Logf( NAME_Warning, TEXT("DrawTextClipped: No font") ); // gam
		return;
	}

	check(Font);

	FPlane DrawColor = Color.Plane();
//	unclock(GScriptCycles);
//	clock(GStats.DWORDStats(GEngineStats.STATS_Game_CanvasCycles));
	DrawString( this, Font, FontScaleX, FontScaleY, (INT) CurX, (INT) CurY, *InText, DrawColor, 1, CheckHotKey ); // gam
//	unclock(GStats.DWORDStats(GEngineStats.STATS_Game_CanvasCycles));
//	clock(GScriptCycles);

	unguardexec;
}
IMPLEMENT_FUNCTION( UCanvas, 469, execDrawTextClipped );

void UCanvas::execTextSize( FFrame& Stack, RESULT_DECL )
{
	guard(UCanvas::execTextSize);
	P_GET_STR(InText);
	P_GET_FLOAT_REF(XL);
	P_GET_FLOAT_REF(YL);
	P_FINISH;

	if( !Font )
	{
		Stack.Logf( NAME_Warning, TEXT("TextSize: No font") ); // gam
		return;
	}

	ClippedStrLen( Font, FontScaleX, FontScaleY, *XL, *YL, *InText );
	
//	*XL = XLi;
//	*YL = YLi;

	unguardexec;
}
IMPLEMENT_FUNCTION( UCanvas, 470, execTextSize );

// execDrawPortal
// Written by Andrew Scheidecker
// Edited by Brandon Reinhart
void UCanvas::execDrawPortal( FFrame& Stack, RESULT_DECL )
{
	guard(UCanvas::execDrawPortal);
	P_GET_INT(X);
	P_GET_INT(Y);
	P_GET_INT(Width);
	P_GET_INT(Height);
	P_GET_OBJECT(AActor,CamActor);
	P_GET_VECTOR(CamLocation);
	P_GET_ROTATOR(CamRotation);
	P_GET_INT_OPTX(FOV, 90);
	P_GET_UBOOL_OPTX(ClearZ, 1);
	P_FINISH;

	// Save state.

//	unclock(GScriptCycles);
//	clock(GStats.DWORDStats(GEngineStats.STATS_Game_CanvasCycles));
	Viewport->RI->PushState();

	FLOAT	SavedFovAngle = Viewport->Actor->FovAngle;

	// Clear the portal viewport.

	FCanvasUtil(&Viewport->RenderTarget,Viewport->RI).DrawTile(
		X,Y,
		X + Width,Y + Height,
		0,0,
		0,0,
		1,
		NULL,
//!!MAT		PF_NoZTest,
		FColor(0,0,0)
		);

	Viewport->RI->SetViewport(X,Y,Width,Height);

	// Render the scene.

	FCameraSceneNode	SceneNode(Viewport,&Viewport->RenderTarget,CamActor,CamLocation,CamRotation,FOV);

	Viewport->Actor->FovAngle = FOV;

	SceneNode.Render(Viewport->RI);

	// Restore state.

	Viewport->Actor->FovAngle = SavedFovAngle;
	Viewport->RI->PopState();
//	unclock(GStats.DWORDStats(GEngineStats.STATS_Game_CanvasCycles));
//	clock(GScriptCycles);

	unguardexec;
}
IMPLEMENT_FUNCTION( UCanvas, 480, execDrawPortal );

void UCanvas::execWorldToScreen( FFrame& Stack, RESULT_DECL ) // sjs
{
	guard(UCanvas::execWorldToScreen);
	P_GET_VECTOR(Location);							// Location holds the vector to project
	P_FINISH;

	UViewport* View = Viewport;
	
	if (!View)
        return;

	APlayerController*	CameraActor = View->Actor;
    FVector CameraLocation = CameraActor->Location;
    FRotator CameraRotation = CameraActor->Rotation;
	AActor*		ViewActor = NULL;

	CameraActor->eventPlayerCalcView(ViewActor, CameraLocation, CameraRotation);

	FCameraSceneNode	SceneNode(View,&View->RenderTarget,ViewActor, CameraLocation, CameraRotation, CameraActor->FovAngle);
	FCanvasUtil			CanvasUtil(&View->RenderTarget,View->RI);

	*(FVector*)Result = CanvasUtil.ScreenToCanvas.TransformFVector(SceneNode.Project(Location));
	unguardexec;
}
IMPLEMENT_FUNCTION( UCanvas, -1, execWorldToScreen );

void UCanvas::execGetCameraLocation( FFrame& Stack, RESULT_DECL )
{
	guard(UCanvas::execGetCameraLocation);

	P_GET_VECTOR_REF(CameraLocation);
	P_GET_ROTATOR_REF(CameraRotation);
	P_FINISH;
	
	*CameraLocation = FVector(0, 0, 0);
	*CameraRotation = FRotator(0, 0, 0);

	UViewport* View = Viewport;
	if (!View)
        return;

	APlayerController*	CameraActor = View->Actor;
    *CameraLocation = CameraActor->Location;
    *CameraRotation = CameraActor->Rotation;
	AActor*		ViewActor = NULL;

	CameraActor->eventPlayerCalcView(ViewActor, *CameraLocation, *CameraRotation);
	
	unguardexec;
}
IMPLEMENT_FUNCTION( UCanvas, -1, execGetCameraLocation );

// gam ---

void UCanvas::SetScreenLight( INT Index, const FVector& Position, FColor Color, FLOAT Radius )
{
	guard(UCanvas::SetScreenLight);
	
    FVector ColorVect;

    ColorVect.X = Color.R / 255.0f;
    ColorVect.Y = Color.G / 255.0f;
    ColorVect.Z = Color.B / 255.0f;

    if ( Index > ARRAY_COUNT( screenLights ) -1 )
        return;

    screenLights[Index].Position = Position;
    screenLights[Index].Color = ColorVect;
    screenLights[Index].Radius = Radius;
    screenLights[Index].Direction = FVector(1.0f,0.0f,0.0f); // unused

    unguard;
}

void UCanvas::execSetScreenLight( FFrame& Stack, RESULT_DECL )
{
	guard(UCanvas::execSetScreenLight);
	P_GET_INT(Index);
	P_GET_VECTOR(Position);
    P_GET_STRUCT(FColor, Color);
	P_GET_FLOAT(Radius);
	P_FINISH;

    SetScreenLight( Index, Position, Color, Radius ); // gam
    
	unguardexec;
}
IMPLEMENT_FUNCTION( UCanvas, -1, execSetScreenLight );

void UCanvas::execSetScreenProjector( FFrame& Stack, RESULT_DECL )
{
	guard(UCanvas::execSetScreenProjector);
	P_GET_INT(index);
	P_GET_VECTOR(position);
	P_GET_VECTOR(colorVect);
	P_GET_FLOAT(radius);
	P_GET_OBJECT(UTexture,pTex);
	P_FINISH;

	// TODO: 

	unguardexec;
}
IMPLEMENT_FUNCTION( UCanvas, -1, execSetScreenProjector );

void UCanvas::DrawScreenActor( AActor* Actor, UBOOL WireFrame, UBOOL ClearZ, FLOAT DisplayFOV )
{
    guard(UCanvas::DrawScreenActor)
    
    if( !Actor )
        return;

    if ( pCanvasUtil )
        pCanvasUtil->Flush();

	AActor*		CameraActor = Viewport->Actor;
	FVector		CameraLocation = FVector(0,0,0);
	FRotator	CameraRotation = FRotator(0,0,0);

//	unclock(GScriptCycles);
//	clock(GStats.DWORDStats(GEngineStats.STATS_Game_CanvasCycles));

	INT OldRendMap = Viewport->Actor->RendMap;

	if( WireFrame )
		Viewport->Actor->RendMap = REN_Wire;
    else
        Viewport->Actor->RendMap = REN_ScreenActor;
    
	if( ClearZ )
		Viewport->RI->Clear(0,FColor(0,0,0),1,1.0f,0);

    Actor->bHidden = 0;
    for( INT i = 0; i < Actor->Attached.Num(); i++ )
        Actor->Attached(i)->bHidden = 0;

    FSphere BoundingSphere(0);

    if( Actor->Mesh )
        BoundingSphere = Actor->Mesh->BoundingSphere;
    else if( Actor->StaticMesh )
        BoundingSphere = Actor->StaticMesh->BoundingSphere;

	FSphere	LightingSphere( (BoundingSphere - Actor->Location), BoundingSphere.W );
	Viewport->RI->EnableLighting( 1, 0, 1, NULL, 0, LightingSphere );

    FLOAT Modulate = Min( ColorModulate.X, Min( ColorModulate.Y, Min( ColorModulate.Z, ColorModulate.W ) ) );
    
	Viewport->RI->SetAmbientLight
	(
	    FColor
	    (
	        Actor->AmbientGlow * Modulate,
	        Actor->AmbientGlow * Modulate,
	        Actor->AmbientGlow * Modulate,
	        Actor->AmbientGlow * Modulate
	    )
	);

    for( int i=0; i<MAX_SCREEN_LIGHTS; i++ )
    {
        if ( screenLights[i].Radius == 0.0f )
            Viewport->RI->SetLight( i, NULL );
        else
        {
            FPlane OriginalColor = screenLights[i].Color; 

            screenLights[i].Color.X *= Modulate;
            screenLights[i].Color.Y *= Modulate;
            screenLights[i].Color.Z *= Modulate;
            screenLights[i].Color.W *= Modulate;
            
            Viewport->RI->SetLight( i, &screenLights[i] );
            screenLights[i].Color = OriginalColor; 
        }
    }

	FActorSceneNode(Viewport,&Viewport->RenderTarget,Actor,CameraActor,CameraLocation,CameraRotation,DisplayFOV).Render(Viewport->RI);
	Viewport->Actor->RendMap = OldRendMap;

    Actor->bHidden = 1;
    for( INT i = 0; i < Actor->Attached.Num(); i++ )
        Actor->Attached(i)->bHidden = 1;

//	unclock(GStats.DWORDStats(GEngineStats.STATS_Game_CanvasCycles));
//	clock(GScriptCycles);

    unguard;
}

void UCanvas::execDrawScreenActor( FFrame& Stack, RESULT_DECL )
{
	guard(UCanvas::execDrawScreenActor);

	P_GET_OBJECT(AActor, Actor);
	P_GET_FLOAT_OPTX(DisplayFOV, Viewport->Actor->FovAngle);
	P_GET_UBOOL_OPTX(WireFrame, 0);
	P_GET_UBOOL_OPTX(ClearZ, 0);
	P_FINISH;

    DrawScreenActor( Actor, WireFrame, ClearZ, DisplayFOV ); // gam

	unguardexec;
}
IMPLEMENT_FUNCTION( UCanvas, -1, execDrawScreenActor );

void UCanvas::execClear( FFrame& Stack, RESULT_DECL )
{
	guard(UCanvas::execClear);
    P_GET_UBOOL_OPTX(ClearRGB,1);
    P_GET_UBOOL_OPTX(ClearZ,1);
	P_FINISH;
    if ( pCanvasUtil )
        pCanvasUtil->Flush();
	Viewport->RI->Clear(ClearRGB,FColor(0,0,0),ClearZ,1.0f);
	unguardexec;
}
IMPLEMENT_FUNCTION( UCanvas, -1, execClear );

void UCanvas::execWrapText( FFrame& Stack, RESULT_DECL )
{
	guard(UCanvas::execWrapText);
    P_GET_STR_REF(pText);
    P_GET_STR_REF(pLine);
    P_GET_FLOAT(DX);
    P_GET_OBJECT(UFont,F);
    P_GET_FLOAT(FontScaleX);
	P_FINISH;

	FontScaleX *= F->ScaleFactor;

    FString& Text = *pText;
    FString& Line = *pLine;

    if( !F )
    {
        debugf( NAME_Error, TEXT("WrapText: Can't Wrap w/o a font!") );
        Line = Text;
        Text.Empty();
        return;
    }
    
    int Index = 0;
    int LastWordIndex = 0;
    FLOAT Width = 0;
    int Length = Text.Len();
    
    for(;;)
    {
        Index++;
        
        if( Width >= DX )
        {
            check( Index > 1 );
            
            if( LastWordIndex == 0 )
                LastWordIndex = Index;
            
            Line = Text.Left( LastWordIndex );
            
            while( appIsSpace( (*Text)[LastWordIndex]) )
                LastWordIndex++;
                
            Text = Text.Right( Length - LastWordIndex );
            return;
        }

        if( Index == Length )
        {
            Line = Text;
            Text.Empty();
            return;
        }

        TCHAR c = (*Text)[Index];
        TCHAR d = (*Text)[Index + 1];
        
        if( appIsSpace(c) )
            LastWordIndex = Index;
        
    	INT W, H;

		GetCharSize( F, c, W, H );

		if( d )
	        Width += (FLOAT)(W + F->Kerning) * FontScaleX;
		else
			Width += (FLOAT)(W) * FontScaleX;
    }

	unguardexec;
}
IMPLEMENT_FUNCTION( UCanvas, -1, execWrapText );

void UCanvas::execDrawTilePartialStretched( FFrame& Stack, RESULT_DECL )
{
	guard(UCanvas::execDrawTilePartialStretched);
	P_GET_OBJECT(UMaterial, Mat);
	P_GET_FLOAT(XL);
	P_GET_FLOAT(YL);
	P_FINISH;

	DrawTileStretchedOrScaled(Mat, CurX, CurY, XL, YL, INDEX_NONE, INDEX_NONE );

	unguardexec;
}
IMPLEMENT_FUNCTION( UCanvas, INDEX_NONE, execDrawTilePartialStretched );

void UCanvas::execDrawTileStretched( FFrame& Stack, RESULT_DECL )
{
	guard(UCanvas::execDrawTileStretched);

	P_GET_OBJECT(UMaterial,Mat);
	P_GET_FLOAT(AWidth);
	P_GET_FLOAT(AHeight);
	P_FINISH;

	DrawTileStretched(Mat,CurX, CurY, AWidth, AHeight);

	unguardexec;

}
IMPLEMENT_FUNCTION( UCanvas, -1, execDrawTileStretched );

void UCanvas::DrawTileStretched(UMaterial* Mat, FLOAT Left, FLOAT Top, FLOAT AWidth, FLOAT AHeight)
{

	guard(UCanvas::DrawTileStretched);

	CurX = Left;
	CurY = Top;

	if( !Mat )
	{
		debugf( TEXT("DrawTileStretched: Missing Material") );
		return;
	}
	
	if (Style==NULL)
		return;

	// Get the size of the image

	FLOAT mW = Mat->MaterialUSize();
	FLOAT mH = Mat->MaterialVSize();

	// Get the midpoints of the image

	FLOAT MidX = appFloor(mW/2);
	FLOAT MidY = appFloor(mH/2);

	// Grab info about the scaled image

	FLOAT SmallTileW = AWidth - mW;
	FLOAT SmallTileH = AHeight - mH;
	FLOAT fX, fY;		// Used to shrink

	// Draw the spans first

		// Top and Bottom

	if (mW<AWidth)
	{
		fX = MidX;

		if (mH>AHeight)
			fY = AHeight/2;
		else
			fY = MidY;

		DrawTile(Mat, CurX+fX,	CurY,					SmallTileW,		fY,		MidX,	0,			1,		fY,	0, Color, FPlane(0,0,0,0));
		DrawTile(Mat, CurX+fX,	CurY+AHeight-fY,		SmallTileW,		fY,		MidX,	mH-fY,		1,		fY,	0, Color, FPlane(0,0,0,0));
	}
	else
		fX = AWidth / 2;

		// Left and Right

	if (mH<AHeight)
	{

		fY = MidY;

		DrawTile(Mat, CurX,				CurY+fY,	fX,	SmallTileH,		0,		fY, fX,	1,	0,	Color,	FPlane(0,0,0,0));
		DrawTile(Mat, CurX+AWidth-fX,	CurY+fY,	fX,	SmallTileH,		mW-fX,	fY, fX,	1,	0,	Color,	FPlane(0,0,0,0));

	}
	else
		fY = AHeight / 2; 

		// Center

	if ( (mH<AHeight) && (mW<AWidth) )
		DrawTile(Mat, CurX+fX, CurY+fY, SmallTileW, SmallTileH, fX, fY, 1, 1, 0, Color, FPlane(0,0,0,0));

	// Draw the 4 corners.

	DrawTile(Mat, CurX,				CurY,				fX, fY, 0,		0,		fX, fY, 0,Color,FPlane(0,0,0,0));
	DrawTile(Mat, CurX+AWidth-fX,	CurY,				fX, fY,	mW-fX,	0,		fX, fY, 0,Color,FPlane(0,0,0,0));
	DrawTile(Mat, CurX,				CurY+AHeight-fY,	fX, fY, 0,		mH-fY,	fX, fY, 0,Color,FPlane(0,0,0,0));
	DrawTile(Mat, CurX+AWidth-fX,	CurY+AHeight-fY,	fX, fY,	mW-fX,	mH-fY,	fX, fY, 0,Color,FPlane(0,0,0,0));

	unguard;
}


void UCanvas::DrawTileStretchedOrScaled( UMaterial* Mat, FLOAT Left, FLOAT Top, FLOAT AWidth, FLOAT AHeight, FLOAT UL, FLOAT VL)
{
	guard(UCanvas::DrawTileStretchedOrScaled);

	CurX = Left;
	CurY = Top;

	if( !Mat )
	{
		debugf( TEXT("DrawTileStretchedOrScaled: Missing Material") );
		return;
	}
	
	if (Style==NULL)
		return;

	// Get the size of the image

	FLOAT mW = Mat->MaterialUSize();
	FLOAT mH = Mat->MaterialVSize();

	if ( UL == INDEX_NONE )
		UL = 1;

	if ( VL == INDEX_NONE )
		VL = 1;

	// Get the midpoints of the image

	FLOAT MidX = appFloor(mW/2);
	FLOAT MidY = appFloor(mH/2);

	// Grab info about the scaled image

	FLOAT SmallTileW = AWidth - mW;
	FLOAT SmallTileH = AHeight - mH;
	FLOAT fX = MidX, fY = MidY;		// Used to shrink

	// Draw the spans first

	// Top and Bottom
	if (mW<AWidth)   // Need to stretch material horizontally
	{
		if (mH>AHeight)
			fY = AHeight/2;
		else
			fY = MidY;

		DrawTile(Mat,         CurX+fX,            CurY,	SmallTileW,      fY,  MidX,    0,   1, MidY, 0, Color, FPlane(0,0,0,0));
		DrawTile(Mat,         CurX+fX, CurY+AHeight-fY, SmallTileW,	     fY,  MidX, MidY,   1, MidY, 0, Color, FPlane(0,0,0,0));
	}
	else
		fX = AWidth / 2;

	// Left and Right
	if (mH<AHeight)   // Need to stretch material vertically
	{
		DrawTile(Mat, CurX,				       CurY+fY,	       fX,SmallTileH,    0,	MidY, MidX,	  1, 0, Color, FPlane(0,0,0,0));
		DrawTile(Mat, CurX+AWidth-fX,	       CurY+fY,	       fX,SmallTileH, MidX, MidY, MidX,	  1, 0, Color, FPlane(0,0,0,0));
	}
	else
		fY = AHeight / 2; 

	// Center
	if ( (mH<AHeight) && (mW<AWidth) )// If we had to stretch the material both ways, draw filler in the middle
		DrawTile(Mat,        CurX+fX,          CurY+fY,SmallTileW,SmallTileH,   fX,   fY,    1,   1, 0, Color, FPlane(0,0,0,0));

	// Draw the 4 corners - each quadrant is scaled if its area is smaller than the destination area
	DrawTile(Mat, CurX,				CurY,				fX, fY, 0,		0,		MidX, MidY, 0,Color,FPlane(0,0,0,0)); //Upper Left
	DrawTile(Mat, CurX+AWidth-fX,	CurY,				fX, fY,	MidX,	0,		MidX, MidY, 0,Color,FPlane(0,0,0,0)); // Upper Right
	DrawTile(Mat, CurX,				CurY+AHeight-fY,	fX, fY, 0,		MidY,	MidX, MidY, 0,Color,FPlane(0,0,0,0)); // Lower Left
	DrawTile(Mat, CurX+AWidth-fX,	CurY+AHeight-fY,	fX, fY,	MidX,	MidY,	MidX, MidY, 0,Color,FPlane(0,0,0,0)); // Lower Right

	unguard;
}

void UCanvas::execDrawTileScaled( FFrame& Stack, RESULT_DECL )
{
	guard(UCanvas::execDrawTileStretched);

	P_GET_OBJECT(UMaterial,Mat);
	P_GET_FLOAT(NewXScale);
	P_GET_FLOAT(NewYScale);
	P_FINISH;

	DrawTileScaled(Mat,CurX,CurY,NewXScale,NewYScale);

	unguardexec;

}

IMPLEMENT_FUNCTION( UCanvas, -1, execDrawTileScaled );


void UCanvas::DrawTileBound(UMaterial* Mat, FLOAT Left, FLOAT Top, FLOAT Width, FLOAT Height)
{
	guard(UCanvas::DrawTileBound);

	if (!Mat)
		debugf(TEXT("DrawTileBound: Missing Material"));

	if (Style==NULL)
		return;

	DrawTile( Mat, Left, Top, Width, Height, 0, 0, Width, Height, 0, Color, FPlane(0,0,0,0));

	unguard;
}

void UCanvas::DrawTileScaleBound(UMaterial* Mat, FLOAT Left, FLOAT Top, FLOAT Width, FLOAT Height)
{
	guard(UCanvas::DrawTileScaleBound);

	if (!Mat)
		debugf(TEXT("DrawTileScaleBound: Missing Material"));

	if (Style==NULL)
		return;

	FLOAT mW = Mat->MaterialUSize();
	FLOAT mH = Mat->MaterialVSize();

	DrawTile( Mat, Left, Top, Width, Height, 0, 0, mW, mH, 0, Color, FPlane(0,0,0,0));

	unguard;
}

IMPLEMENT_FUNCTION( UCanvas, -1, execDrawTileJustified );

void UCanvas::execDrawTileJustified( FFrame& Stack, RESULT_DECL )
{
	guard(UCanvas::execDrawTileJustified);
	P_GET_OBJECT(UMaterial,Mat);
	P_GET_BYTE(Just);
	P_GET_FLOAT(AWidth);
	P_GET_FLOAT(AHeight);
	P_FINISH;

	DrawTileJustified(Mat, CurX, CurY, AWidth, AHeight, Just);

	unguardexec;
}


// Justification is: 0 = left/top, 1 = Center, 2 = bottom/right
void UCanvas::DrawTileJustified(UMaterial* Mat, FLOAT Left, FLOAT Top, FLOAT Width, FLOAT Height, BYTE Justification)
{
	guard(UCanvas::DrawTileJustified);

	if (!Mat)
		debugf(TEXT("DrawTileJustified: Missing Material"));

	if (Style==NULL)
		return;

	FLOAT mW = Mat->MaterialUSize();
	FLOAT mH = Mat->MaterialVSize();

	if (mW <= 0.0 || mH <= 0.0)
		return;

	// Find scaling proportions
	FLOAT MatProps = mH/mW;
	FLOAT BndProps = Height/Width;

	if (MatProps == BndProps)
		DrawTile( Mat, Left, Top, Width, Height, 0, 0, mW, mH, 0, Color, FPlane(0,0,0,0));
	else if (MatProps > BndProps)	// Stretch to fit Height
	{
	FLOAT NewWidth = Width * BndProps / MatProps;
	FLOAT NewLeft = Left;

		if (Justification == 1)			// Centered
			NewLeft += ((Width - NewWidth) / 2.0);
		else if (Justification == 2)	// RightBottom
			NewLeft += Width - NewWidth;

		DrawTile( Mat, NewLeft, Top, NewWidth, Height, 0, 0, mW, mH, 0, Color, FPlane(0,0,0,0));
	}
	else							// Stretch to fit Width
	{
	FLOAT NewHeight = Height * MatProps / BndProps;
	FLOAT NewTop = Top;

		if (Justification == 1)			// Centered
			NewTop += ((Height - NewHeight) / 2.0);
		else if (Justification == 2)	// RightBottom
			NewTop += Height - NewHeight;

		DrawTile( Mat, Left, NewTop, Width, NewHeight, 0, 0, mW, mH, 0, Color, FPlane(0,0,0,0));
	}
	unguard;
}

void UCanvas::DrawTileScaled(UMaterial* Mat, FLOAT Left, FLOAT Top, FLOAT NewXScale, FLOAT NewYScale)
{
	guard(UCanvas::DrawTileScaled);

	if( !Mat )
	{
		debugf( TEXT("DrawTileScaled: Missing Material") );
		return;
	}
	
	if (Style==NULL)
		return;

	FLOAT mW = Mat->MaterialUSize();
	FLOAT mH = Mat->MaterialVSize();

	DrawTile( Mat, Left, Top, mW*NewXScale, mH*NewYScale, 0, 0, mW, mH, 0, Color, FPlane(0,0,0,0));

	unguard;
}

IMPLEMENT_FUNCTION( UCanvas, -1, execDrawTextJustified );

void UCanvas::execDrawTextJustified( FFrame& Stack, RESULT_DECL )
{
	guard(UCanvas::execDrawTextJustified);
	P_GET_STR(InText);
	P_GET_BYTE(Just);
	P_GET_FLOAT(X1);
	P_GET_FLOAT(Y1);
	P_GET_FLOAT(X2);
	P_GET_FLOAT(Y2);
	P_FINISH;

	DrawTextJustified(Just,X1,Y1,X2,Y2, TEXT("%s"),*InText);

	unguardexec;
}


void VARARGS UCanvas::DrawTextJustified(BYTE Justification, FLOAT x1, FLOAT y1, FLOAT x2, FLOAT y2, const TCHAR* Fmt, ... )
{
	guard(UCanvas::DrawTextJustified);
	
	TCHAR Text[4096];
	GET_VARARGS( Text, ARRAY_COUNT(Text), Fmt, Fmt );

	FLOAT XL,YL;
	CurX = 0;
	CurY = 0;
	ClippedStrLen(  Font, 1.0,1.0, XL, YL, Text );

	// Adjust the Y so the Font is centered in the bounding box
	CurY = ((y2-y1) / 2) - (YL/2);

	if (Justification == 0)						// Left
		CurX = 0;
	else if (Justification == 1)				// Center
	{
		if( XL > x2-x1 ) // align left when there's no room
			CurX = 0;
		else
			CurX = ((x2-x1) / 2) - (XL/2);
	}
	else if (Justification == 2)				// Right
		CurX = (x2-x1) - XL;

	FLOAT OldClipX = ClipX;
	FLOAT OldClipY = ClipY;
	FLOAT OldOrgX = OrgX;
	FLOAT OldOrgY = OrgY;

	// Clip to box
	OrgX = x1;
	OrgY = y1;
	ClipX = x2-x1;
	ClipY = y2-y1;

	ClippedPrint(Font, 1.0, 1.0, false, Text);

	ClipX = OldClipX;
	ClipY = OldClipY;
	OrgX = OldOrgX;
	OrgY = OldOrgY;


	unguard;

}

// --- gam

IMPLEMENT_CLASS(UCanvas);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

