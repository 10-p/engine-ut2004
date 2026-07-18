/*=============================================================================
	UPS2ConvertCommandlet.cpp: ADPCM sound compression utility.
	Copyright 2000 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Brandon Reinhart
=============================================================================*/

#include "EditorPrivate.h"
#include <stdlib.h>
/*-----------------------------------------------------------------------------
	FDXTDecompressor.
-----------------------------------------------------------------------------*/
class FDXTDecompressor
{
public:

	FDXTDecompressor();
	virtual ~FDXTDecompressor();

	BYTE	* m_pCompBytes;		// compressed image bytes
	BYTE	* m_pDecompBytes;

	ETextureFormat	m_CompFormat;

	int	m_nWidth;	// in pixels of uncompressed image 
	int m_nHeight;

	bool LoadFromFile( char * filename );		// true if success

	void AllocateDecompBytes();

	void Decompress();
	
	void DecompressDXT1();
	void DecompressDXT2();
	void DecompressDXT3();
	void DecompressDXT4();
	void DecompressDXT5();

	void SaveAsRaw();			// save decompressed bits
};

#pragma pack(push,1)
struct FBitmapFileHeader
{
    _WORD bfType;
    DWORD bfSize;
    _WORD bfReserved1;
    _WORD bfReserved2;
    DWORD bfOffBits;
	friend FArchive& operator<<( FArchive& Ar, FBitmapFileHeader& H )
	{
		guard(FBitmapFileHeader<<);
		Ar << H.bfType << H.bfSize << H.bfReserved1 << H.bfReserved2 << H.bfOffBits;
		return Ar;
		unguard;
	}
};
#pragma pack(pop)

// .BMP subheader.
#pragma pack(push,1)
struct FBitmapInfoHeader
{
    DWORD biSize;
    DWORD biWidth;
    DWORD biHeight;
    _WORD biPlanes;
    _WORD biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    DWORD biXPelsPerMeter;
    DWORD biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
	friend FArchive& operator<<( FArchive& Ar, FBitmapInfoHeader& H )
	{
		guard(FBitmapInfoHeader<<);
		Ar << H.biSize << H.biWidth << H.biHeight;
		Ar << H.biPlanes << H.biBitCount;
		Ar << H.biCompression << H.biSizeImage;
		Ar << H.biXPelsPerMeter << H.biYPelsPerMeter;
		Ar << H.biClrUsed << H.biClrImportant;
		return Ar;
		unguard;
	}
};
#pragma pack(pop)

/*-----------------------------------------------------------------------------
	UPS2ConvertCommandlet
-----------------------------------------------------------------------------*/

class UPS2ConvertCommandlet : public UCommandlet
{
	DECLARE_CLASS(UPS2ConvertCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(UPS2ConvertCommandlet::StaticConstructor);

		LogToStdout     = 0;
		IsClient        = 1;
		IsEditor        = 1;
		IsServer        = 1;
		LazyLoad        = 1;
		ShowErrorCount  = 1;

		unguard;
	}
	INT Main( const TCHAR* Parms )
	{
		guard(UPS2ConvertCommandlet::Main);
		FString WildCard;
		if( !ParseToken(Parms,WildCard,0) )
			appErrorf(TEXT("Package search mask not specified."));
		UClass* Class = FindObjectChecked<UClass>( ANY_PACKAGE, TEXT("Sound") );
		UClass* TexClass = FindObjectChecked<UClass>( ANY_PACKAGE, TEXT("Texture") );
		UClass* StaticMeshClass = FindObjectChecked<UClass>( ANY_PACKAGE, TEXT("StaticMesh") );
		TArray<FString> FilesFound = GFileManager->FindFiles( *WildCard, 1, 0 );

		GLazyLoad = 0;
		for (INT i=0; i<FilesFound.Num(); i++)
		{
			FString Pkg = FilesFound(i);
			GWarn->Logf( TEXT("Package %s..."), *Pkg );
			GWarn->Logf( TEXT("  Loading") );

			UObject* Package = NULL;
			if (appStrstr(*Pkg, TEXT(".ut2")))
			{
				UClass* EditorEngineClass = UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail | LOAD_DisallowFiles, NULL );
				GEditor  = ConstructObject<UEditorEngine>( EditorEngineClass );
				GEditor->UseSound = 0;
				GEditor->Init();
				GIsRequestingExit = 1; // Causes ctrl-c to immediately exit.

				GEditor->Exec( *FString::Printf(TEXT("MAP LOAD FILE=\"%s\""), *Pkg ) );
				Package = CastChecked<UPackage>(GEditor->Level->GetOuter());
			}
			else
			{
				Package = LoadPackage(NULL,*Pkg,LOAD_NoWarn);
			}
			if (Package != NULL)
			{
				GWarn->Logf( TEXT("  Converting") );
				for( TObjectIterator<UObject> It; It; ++It )
				{
					if( It->IsA(Class) && It->IsIn(Package) )
					{
						USound* Sound = (USound*) *It;
						Sound->PS2Convert();
					}
					else if( It->IsA(StaticMeshClass) && It->IsIn(Package) )
					{
						UStaticMesh* Mesh = (UStaticMesh*) *It;

						GWarn->Logf(TEXT("Rebuilding..."));
						INT Temp = Mesh->InternalVersion;
						Mesh->InternalVersion = -1;
						Mesh->Build();
						Mesh->InternalVersion = Temp;
						GWarn->Logf(TEXT("Rebuilt..."));
					}
					else if( It->IsA(TexClass) && It->IsIn(Package) )
					{
						UTexture* Texture = (UTexture*) *It;
						UBOOL DoMips = false;

						if (appStrstr(Texture->GetName(), TEXT("alpha")) || appStrstr(Texture->GetName(), TEXT("Alpha")))
							continue;
						if (Texture->Format == TEXF_DXT1 || Texture->Format == TEXF_DXT3 || Texture->Format == TEXF_DXT5)
						{
							FDXTDecompressor Decompress;

							if (Texture->Mips.Num() > 1)
								DoMips = 1;

							Decompress.m_nWidth = Texture->Mips(0).USize;
							Decompress.m_nHeight = Texture->Mips(0).VSize;
							Decompress.m_CompFormat = (ETextureFormat)Texture->Format;
							Decompress.m_pCompBytes = &Texture->Mips(0).DataArray(0);

							Decompress.AllocateDecompBytes();
							GWarn->Logf(TEXT("Decompressing... [%d, %d]"), Decompress.m_nWidth, Decompress.m_nHeight);
							Decompress.Decompress();

							Decompress.SaveAsRaw();


							// delete all compressed mips
//							Texture->CompMips.Empty();
//							Texture->bHasComp = 0;
//							Texture->CompFormat = TEXF_NODATA;
							Texture->Format = TEXF_RGBA8;

							INT TextureSize = Decompress.m_nWidth * Decompress.m_nHeight * 4;
						//	// Fill out this mipmap
							Texture->Mips(0).DataArray.Empty();
							Texture->Mips(0).DataArray.Add(TextureSize);
							for (INT i=0; i<TextureSize; i++)
							{
								// For some reason, I need to swap the red and blue, so here we go!
								if ((i % 4) == 0)
									Texture->Mips(0).DataArray(i) = Decompress.m_pDecompBytes[i + 2];
								else if ((i % 4) == 2)
									Texture->Mips(0).DataArray(i) = Decompress.m_pDecompBytes[i - 2];
								else
									Texture->Mips(0).DataArray(i) = Decompress.m_pDecompBytes[i];
							}

							Texture->CreateMips(DoMips, 1); // Take out if we do the RGBA thing below
						}
/*
						if (Texture->Format == TEXF_RGBA8)
						{

							GWarn->Logf(TEXT("Color reducing %s..."), Texture->GetName());
							UExporter::ExportToFile(Texture, NULL, TEXT("temp.pcx"));
							system("ImageMagick\\convert -colors 256 temp.pcx out.tga");
							system("ImageMagick\\convert out.tga out.pcx");

//							TArray<BYTE> Data;
//							if( !appLoadFileToArray( Data, TEXT("out.bmp" ) ) )
//							{
//								GWarn->Logf(TEXT("  ERROR: Failed to read reduced texture!"));
//								continue;
//							}
//
//							Data.AddItem( 0 );
//							BYTE* Buffer = &Data( 0 );
//							BYTE* BufferEnd = Buffer + Data.Num() - 1;
							FArchive* PCXReader = GFileManager->CreateFileReader(TEXT("out.pcx"));
							INT PCXSize = PCXReader->TotalSize();
							BYTE* PCXBuffer = (BYTE*)appMalloc(PCXSize, TEXT("PCX"));
							PCXReader->Serialize(PCXBuffer, PCXSize);
							delete PCXReader;

							if (PCXBuffer[3] != 8)
							{
								appFree(PCXBuffer);

								system("ImageMagick\\convert -depth 256 temp.pcx out.pcx");
								PCXReader = GFileManager->CreateFileReader(TEXT("out.pcx"));
								PCXSize = PCXReader->TotalSize();
								PCXBuffer = (BYTE*)appMalloc(PCXSize, TEXT("PCX"));
								PCXReader->Serialize(PCXBuffer, PCXSize);
								delete PCXReader;

								if (PCXBuffer[3] != 8)
								{
									GWarn->Logf(TEXT("ERROR: Texture %s couldn't be made into 8-bit properly!"), Texture->GetName());
									continue;
								}
							}


							BYTE* Buffer = PCXBuffer;
							BYTE* BufferEnd = Buffer + PCXSize;
//							Result = Factory->FactoryCreateBinary( Class, InOuter, Name, Flags, NULL, appFExt(Filename), Ptr, Ptr+Data.Num()-1, Warn );

							DoMips |= Texture->Mips.Num() > 1;
							Texture->Format = TEXF_P8;
							Texture->Init(Texture->Mips(0).USize, Texture->Mips(0).VSize);
							Texture->PostLoad();

							// Import it.
							BYTE* DestPtr	= &Texture->Mips(0).DataArray(0);
							BYTE* DestEnd	= DestPtr + Texture->Mips(0).USize * Texture->Mips(0).VSize;//Texture->Mips(0).DataArray.Num();
							Buffer += 128;

							while( DestPtr < DestEnd )
							{
								BYTE Color = *Buffer++;
								if( (Color & 0xc0) == 0xc0 )
								{
									INT RunLength = Color & 0x3f;
									Color     = *Buffer++;
									appMemset( DestPtr, Color, Min(RunLength,(INT)(DestEnd - DestPtr)) );
									DestPtr  += RunLength;
								}
								else *DestPtr++ = Color;
							}

							// Do the palette.
							UPalette* Palette = new( Package, NAME_None, RF_Public )UPalette;
							BYTE* PCXPalette = (BYTE *)(BufferEnd - NUM_PAL_COLORS * 3);
							Palette->Colors.Empty();
							for( INT i=0; i<NUM_PAL_COLORS; i++ )
								new(Palette->Colors)FColor(PCXPalette[i*3+0],PCXPalette[i*3+1],PCXPalette[i*3+2],255);
							Texture->Palette = Palette;//->ReplaceWithExisting();

							Texture->CreateMips(DoMips, 1);
						}
*/					}
//					else
//						debugf(TEXT(" Object of %s skipped"), It->GetFullName());
				}

				GWarn->Logf( TEXT("  Saving") );
				

				if (appStrstr(*Pkg, TEXT(".ut2")))
				{
					// Save the updated map
					GEditor->Exec( *FString::Printf(TEXT("MAP SAVE FILE=\"%s\""), *Pkg ) );
				}
				else
					SavePackage( Package, NULL, RF_Standalone, *Pkg, GError, NULL );
				UObject::CollectGarbage(RF_Native);
			}
		}

		GIsRequestingExit=1;
		return 0;
		unguard;
	}
};
IMPLEMENT_CLASS(UPS2ConvertCommandlet)












#define LOW_5	0x001F;
#define MID_6	0x07E0;
#define HIGH_5	0xF800;

#define MID_555	0x03E0;
#define HI_555	0x7C00;





#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/////////////////////////////////////
// should be in ddraw.h

#ifndef MAKEFOURCC
    #define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
                ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |   \
                ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 ))
#endif //defined(MAKEFOURCC)

/////////////////////////////////////
// Defined in this module:

INT GetNumberOfBits( DWORD dwMask );



//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

FDXTDecompressor::FDXTDecompressor()
{
	m_pCompBytes = NULL;
	m_pDecompBytes = NULL;
}

FDXTDecompressor::~FDXTDecompressor()
{
	appFree( m_pDecompBytes );
}


//////////////////////////////////////////////////////////////////////


void FDXTDecompressor::SaveAsRaw()
{
	TCHAR Name[256];
	appSprintf(Name, TEXT("%dx%d.raw"), m_nHeight, m_nWidth);
	FArchive* RAWWriter = GFileManager->CreateFileWriter(Name);
	RAWWriter->Serialize(m_pDecompBytes, m_nHeight * m_nWidth * 4);
	delete RAWWriter;
/*
	// save decompressed bits

	FILE * pf = fopen( "decom.raw", "wb" );


	if( pf == NULL )
	{
		GWarn->Logf("Couldn't open file\n");
		return;
	}


	// writes only 32 bit format.

	fwrite( m_pDecompBytes, m_nHeight * m_nWidth * 4, sizeof( byte ), pf );

	
	fclose( pf );
	pf = NULL;
*/
}



bool FDXTDecompressor::LoadFromFile( char * filename )
{
	return( true );
}


void FDXTDecompressor::AllocateDecompBytes()
{
	if( m_pDecompBytes != NULL )
	{
		appFree( m_pDecompBytes );
		m_pDecompBytes = NULL;
	}


	// Allocate for 32 bit surface:

	m_pDecompBytes = (BYTE*) appMalloc( m_nWidth * m_nHeight * 4, TEXT("DXT"));
	if( m_pDecompBytes == NULL )
		GWarn->Logf(TEXT("Error allocating decompressed byte storage\n"));

	appMemset(m_pDecompBytes, 0, m_nWidth * m_nHeight * 4);
}


void FDXTDecompressor::Decompress()
{
	check( m_pCompBytes != NULL );
	check( m_pDecompBytes != NULL );		// must already have allocated memory


	switch( m_CompFormat )
	{
		case TEXF_DXT1 :
			GWarn->Logf(TEXT("Decompressing image format:  DXT1\n"));
			DecompressDXT1();
			break;

		case TEXF_DXT3 :
			GWarn->Logf(TEXT("Decompressing image format:  DXT3\n"));
			DecompressDXT3();	
			break;

		case TEXF_DXT5 :
			GWarn->Logf(TEXT("Decompressing image format:  DXT5\n"));
			DecompressDXT5();		
			break;
	}
}


struct DXTColBlock
{
	_WORD col0;
	_WORD col1;

	// no bit fields - use bytes
	BYTE row[4];
};

struct DXTAlphaBlockExplicit
{
	_WORD row[4];
};

struct DXTAlphaBlock3BitLinear
{
	BYTE alpha0;
	BYTE alpha1;

	BYTE stuff[6];
};



// use cast to struct instead of RGBA_MAKE as struct is
//  much
struct Color8888
{
	BYTE r;		// change the order of names to change the 
	BYTE g;		//  order of the output ARGB or BGRA, etc...
	BYTE b;		//  Last one is MSB, 1st is LSB.
	BYTE a;
};


struct Color565
{
	unsigned nBlue  : 5;		// order of names changes
	unsigned nGreen : 6;		//  byte order of output to 32 bit
	unsigned nRed	: 5;
};






inline void GetColorBlockColors( DXTColBlock * pBlock, Color8888 * col_0, Color8888 * col_1, 
													 Color8888 * col_2, Color8888 * col_3,
													 _WORD & wrd  )
{
	// There are 4 methods to use - see the Time_ functions.
	// 1st = shift = does normal approach per byte for color comps
	// 2nd = use freak variable bit field color565 for component extraction
	// 3rd = use super-freak DWORD adds BEFORE shifting the color components
	//  This lets you do only 1 add per color instead of 3 BYTE adds and
	//  might be faster
	// Call RunTimingSession() to run each of them & output result to txt file

 
	// freak variable bit structure method
	// normal math
	// This method is fastest

	Color565 * pCol;

	pCol = (Color565*) & (pBlock->col0 );

	col_0->a = 0xff;
	col_0->r = pCol->nRed;
	col_0->r <<= 3;				// shift to full precision
	col_0->g = pCol->nGreen;
	col_0->g <<= 2;
	col_0->b = pCol->nBlue;
	col_0->b <<= 3;

	pCol = (Color565*) & (pBlock->col1 );
	col_1->a = 0xff;
	col_1->r = pCol->nRed;
	col_1->r <<= 3;				// shift to full precision
	col_1->g = pCol->nGreen;
	col_1->g <<= 2;
	col_1->b = pCol->nBlue;
	col_1->b <<= 3;


	if( pBlock->col0 > pBlock->col1 )
	{
		// Four-color block: derive the other two colors.    
		// 00 = color_0, 01 = color_1, 10 = color_2, 11 = color_3
		// These two bit codes correspond to the 2-bit fields 
		// stored in the 64-bit block.

		wrd = ((_WORD)col_0->r * 2 + (_WORD)col_1->r )/3;
											// no +1 for rounding
											// as bits have been shifted to 888
		col_2->r = (BYTE)wrd;

		wrd = ((_WORD)col_0->g * 2 + (_WORD)col_1->g )/3;
		col_2->g = (BYTE)wrd;

		wrd = ((_WORD)col_0->b * 2 + (_WORD)col_1->b )/3;
		col_2->b = (BYTE)wrd;
		col_2->a = 0xff;

		wrd = ((_WORD)col_0->r + (_WORD)col_1->r *2 )/3;
		col_3->r = (BYTE)wrd;

		wrd = ((_WORD)col_0->g + (_WORD)col_1->g *2 )/3;
		col_3->g = (BYTE)wrd;

		wrd = ((_WORD)col_0->b + (_WORD)col_1->b *2 )/3;
		col_3->b = (BYTE)wrd;
		col_3->a = 0xff;

	}
	else
	{
		// Three-color block: derive the other color.
		// 00 = color_0,  01 = color_1,  10 = color_2,  
		// 11 = transparent.
		// These two bit codes correspond to the 2-bit fields 
		// stored in the 64-bit block. 

		// explicit for each component, unlike some refrasts...
		
		// GWarn->Logf("block has alpha\n");

		wrd = ((_WORD)col_0->r + (_WORD)col_1->r )/2;
		col_2->r = (BYTE)wrd;
		wrd = ((_WORD)col_0->g + (_WORD)col_1->g )/2;
		col_2->g = (BYTE)wrd;
		wrd = ((_WORD)col_0->b + (_WORD)col_1->b )/2;
		col_2->b = (BYTE)wrd;
		col_2->a = 0xff;

		col_3->r = 0x00;		// random color to indicate alpha
		col_3->g = 0xff;
		col_3->b = 0xff;
		col_3->a = 0x00;

	}
}			//  Get color block colors (...)



inline void DecodeColorBlock( DWORD * pImPos, DXTColBlock * pColorBlock, int width,
								DWORD * col_0,
								DWORD * col_1, DWORD * col_2, DWORD * col_3 )
{
	// width is width of image in pixels


	DWORD bits;
	int r,n;

	// bit masks = 00000011, 00001100, 00110000, 11000000
	const DWORD masks[] = { 3, 12, 3 << 4, 3 << 6 };
	const int   shift[] = { 0, 2, 4, 6 };

	// r steps through lines in y
	for( r=0; r < 4; r++, pImPos += width-4 )	// no width*4 as DWORD ptr inc will *4
	{

		// width * 4 bytes per pixel per line
		// each j dxtc row is 4 lines of pixels

		// pImPos = (DWORD*)((DWORD)pBase + i*16 + (r+j*4) * m_nWidth * 4 );

		// n steps through pixels
		for( n=0; n < 4; n++ )
		{
			bits =		pColorBlock->row[r] & masks[n];
			bits >>=	shift[n];

			switch( bits )
			{
			case 0 :
				*pImPos = *col_0;
				pImPos++;		// increment to next DWORD
				break;
			case 1 :
				*pImPos = *col_1;
				pImPos++;
				break;
			case 2 :
				*pImPos = *col_2;
				pImPos++;
				break;
			case 3 :
				*pImPos = *col_3;
				pImPos++;
				break;
			default:
				GWarn->Logf(TEXT("Your logic is jacked! bits == 0x%x\n"), bits );
				pImPos++;
				break;
			}
		}
	}
}



inline void  DecodeAlphaExplicit( DWORD * pImPos, DXTAlphaBlockExplicit * pAlphaBlock,
								  int width, DWORD alphazero )
{
	// alphazero is a bit mask that when & with the image color
	//  will zero the alpha bits, so if the image DWORDs  are
	//  ARGB then alphazero will be 0x00ffffff or if
	//  RGBA then alphazero will be 0xffffff00
	//  alphazero constructed automaticaly from field order of Color8888 structure

	// decodes to 32 bit format only


	int row, pix;

	_WORD wrd;

	Color8888 col;
	col.r = col.g = col.b = 0;


	//GWarn->Logf("\n");

	for( row=0; row < 4; row++, pImPos += width-4 )
	{
		// pImPow += pImPos += width-4 moves to next row down

		wrd = pAlphaBlock->row[ row ];

		// GWarn->Logf("0x%.8x\t\t", wrd);

		for( pix = 0; pix < 4; pix++ )
		{
			// zero the alpha bits of image pixel
			*pImPos &= alphazero;

			col.a = wrd & 0x000f;		// get only low 4 bits
//			col.a <<= 4;				// shift to full byte precision
										// NOTE:  with just a << 4 you'll never have alpha
										// of 0xff,  0xf0 is max so pure shift doesn't quite
										// cover full alpha range.
										// It's much cheaper than divide & scale though.
										// To correct for this, and get 0xff for max alpha,
										//  or the low bits back in after left shifting
			col.a = col.a | (col.a << 4 );	// This allows max 4 bit alpha to be 0xff alpha
											//  in final image, and is crude approach to full 
											//  range scale

			*pImPos |= *((DWORD*)&col);	// or the bits into the prev. nulled alpha

			wrd >>= 4;		// move next bits to lowest 4

			pImPos++;		// move to next pixel in the row

		}
	}
}




BYTE		gBits[4][4];
_WORD		gAlphas[8];
Color8888	gACol[4][4];


inline void DecodeAlpha3BitLinear( DWORD * pImPos, DXTAlphaBlock3BitLinear * pAlphaBlock,
									int width, DWORD alphazero)
{

	gAlphas[0] = pAlphaBlock->alpha0;
	gAlphas[1] = pAlphaBlock->alpha1;

	
	// 8-alpha or 6-alpha block?    

	if( gAlphas[0] > gAlphas[1] )
	{
		// 8-alpha block:  derive the other 6 alphas.    
		// 000 = alpha_0, 001 = alpha_1, others are interpolated

		gAlphas[2] = ( 6 * gAlphas[0] +     gAlphas[1]) / 7;	// bit code 010
		gAlphas[3] = ( 5 * gAlphas[0] + 2 * gAlphas[1]) / 7;	// Bit code 011    
		gAlphas[4] = ( 4 * gAlphas[0] + 3 * gAlphas[1]) / 7;	// Bit code 100    
		gAlphas[5] = ( 3 * gAlphas[0] + 4 * gAlphas[1]) / 7;	// Bit code 101
		gAlphas[6] = ( 2 * gAlphas[0] + 5 * gAlphas[1]) / 7;	// Bit code 110    
		gAlphas[7] = (     gAlphas[0] + 6 * gAlphas[1]) / 7;	// Bit code 111
	}    
	else
	{
		// 6-alpha block:  derive the other alphas.    
		// 000 = alpha_0, 001 = alpha_1, others are interpolated

		gAlphas[2] = (4 * gAlphas[0] +     gAlphas[1]) / 5;	// Bit code 010
		gAlphas[3] = (3 * gAlphas[0] + 2 * gAlphas[1]) / 5;	// Bit code 011    
		gAlphas[4] = (2 * gAlphas[0] + 3 * gAlphas[1]) / 5;	// Bit code 100    
		gAlphas[5] = (    gAlphas[0] + 4 * gAlphas[1]) / 5;	// Bit code 101
		gAlphas[6] = 0;										// Bit code 110
		gAlphas[7] = 255;									// Bit code 111
	}


	// Decode 3-bit fields into array of 16 BYTES with same value

	// first two rows of 4 pixels each:
	// pRows = (Alpha3BitRows*) & ( pAlphaBlock->stuff[0] );
	const DWORD mask = 0x00000007;		// bits = 00 00 01 11

	DWORD bits = *( (DWORD*) & ( pAlphaBlock->stuff[0] ));

	gBits[0][0] = (BYTE)( bits & mask );
	bits >>= 3;
	gBits[0][1] = (BYTE)( bits & mask );
	bits >>= 3;
	gBits[0][2] = (BYTE)( bits & mask );
	bits >>= 3;
	gBits[0][3] = (BYTE)( bits & mask );
	bits >>= 3;
	gBits[1][0] = (BYTE)( bits & mask );
	bits >>= 3;
	gBits[1][1] = (BYTE)( bits & mask );
	bits >>= 3;
	gBits[1][2] = (BYTE)( bits & mask );
	bits >>= 3;
	gBits[1][3] = (BYTE)( bits & mask );

	// now for last two rows:

	bits = *( (DWORD*) & ( pAlphaBlock->stuff[3] ));		// last 3 bytes

	gBits[2][0] = (BYTE)( bits & mask );
	bits >>= 3;
	gBits[2][1] = (BYTE)( bits & mask );
	bits >>= 3;
	gBits[2][2] = (BYTE)( bits & mask );
	bits >>= 3;
	gBits[2][3] = (BYTE)( bits & mask );
	bits >>= 3;
	gBits[3][0] = (BYTE)( bits & mask );
	bits >>= 3;
	gBits[3][1] = (BYTE)( bits & mask );
	bits >>= 3;
	gBits[3][2] = (BYTE)( bits & mask );
	bits >>= 3;
	gBits[3][3] = (BYTE)( bits & mask );


	// decode the codes into alpha values
	int row, pix;


	for( row = 0; row < 4; row++ )
	{
		for( pix=0; pix < 4; pix++ )
		{
			gACol[row][pix].a = (BYTE) gAlphas[ gBits[row][pix] ];

			check( gACol[row][pix].r == 0 );
			check( gACol[row][pix].g == 0 );
			check( gACol[row][pix].b == 0 );
		}
	}



	// Write out alpha values to the image bits

	for( row=0; row < 4; row++, pImPos += width-4 )
	{
		// pImPow += pImPos += width-4 moves to next row down

		for( pix = 0; pix < 4; pix++ )
		{
			// zero the alpha bits of image pixel
			*pImPos &=  alphazero;

			*pImPos |=  *((DWORD*) &(gACol[row][pix]));	// or the bits into the prev. nulled alpha
			pImPos++;
		}
	}
}



void FDXTDecompressor::DecompressDXT1()
{
	// This was hacked up pretty quick & slopily
	// decompresses to 32 bit format 0xARGB

	int xblocks, yblocks;

#if 0 // sjs merge_hack _DEBUG
	if( (ddsd.dwWidth % 4 ) != 0 )
	{
		GWarn->Logf("****** warning width not div by 4!  %d\n", ddsd.dwWidth );
	}
	if( (ddsd.dwHeight %4 ) != 0 )
	{
		GWarn->Logf("****** warning Height not div by 4! %d\n", ddsd.dwHeight );
	}
	GWarn->Logf("end check\n");
#endif 

	xblocks = m_nWidth / 4;
	yblocks = m_nHeight / 4;

	
	int i,j;

	DWORD * pBase  = (DWORD*)  m_pDecompBytes;
	DWORD * pImPos = (DWORD*)  pBase;			// pos in decompressed data

	DXTColBlock * pBlock;

	Color8888 col_0, col_1, col_2, col_3;


	_WORD wrd;

	/*
	// yes, ptr++ does work!  duh! =) 
	DWORD * pptr = 0;
	for( j=0; j < 10; j++ )
	{

		GWarn->Logf(" %x\n", pptr );
		pptr++;
	}
	*/



	//	GWarn->Logf("blocks: x: %d    y: %d\n", xblocks, yblocks );



	for( j=0; j < yblocks; j++ )
	{
		// 8 bytes per block
		pBlock = (DXTColBlock*) ( (PTRINT)m_pCompBytes + j * xblocks * 8 );


		for( i=0; i < xblocks; i++, pBlock++ )
		{

			// inline func:
			GetColorBlockColors( pBlock, &col_0, &col_1, &col_2, &col_3, wrd );


			// now decode the color block into the bitmap bits
			// inline func:

			pImPos = (DWORD*)((PTRINT)pBase + i*16 + (j*4) * m_nWidth * 4 );


			DecodeColorBlock( pImPos, pBlock, m_nWidth, (DWORD*)&col_0, (DWORD*)&col_1,
								(DWORD*)&col_2, (DWORD*)&col_3 );


			// Set to RGB test pattern
			//	pImPos = (DWORD*)((DWORD)pBase + i*4 + j*m_nWidth*4);
			//	*pImPos = ((i*4) << 16) | ((j*4) << 8 ) | ( (63-i)*4 );

			// checkerboard of only col_0 and col_1 basis colors:
			//	pImPos = (DWORD*)((DWORD)pBase + i*8 + j*m_nWidth*8);
			//	*pImPos = *((DWORD*)&col_0);
			//	pImPos += 1 + m_nWidth;
			//	*pImPos = *((DWORD*)&col_1);

		}
	}
}




void FDXTDecompressor::DecompressDXT3()
{
	int xblocks, yblocks;

#if 0 // sjs merge_hack DEBUG
	if( (ddsd.dwWidth % 4 ) != 0 )
	{
		GWarn->Logf("****** warning width not div by 4!  %d\n", ddsd.dwWidth );
	}
	if( (ddsd.dwHeight %4 ) != 0 )
	{
		GWarn->Logf("****** warning Height not div by 4! %d\n", ddsd.dwHeight );
	}
	GWarn->Logf("end check\n");
#endif 

	xblocks = m_nWidth / 4;
	yblocks = m_nHeight / 4;

	
	int i,j;

	DWORD * pBase  = (DWORD*)  m_pDecompBytes;
	DWORD * pImPos = (DWORD*)  pBase;			// pos in decompressed data

	DXTColBlock				* pBlock;
	DXTAlphaBlockExplicit	* pAlphaBlock;

	Color8888 col_0, col_1, col_2, col_3;


	_WORD wrd;

	// fill alphazero with appropriate value to zero out alpha when
	//  alphazero is ANDed with the image color 32 bit DWORD:
	col_0.a = 0;
	col_0.r = col_0.g = col_0.b = 0xff;
	DWORD alphazero = *((DWORD*) &col_0);



	//	GWarn->Logf("blocks: x: %d    y: %d\n", xblocks, yblocks );

	for( j=0; j < yblocks; j++ )
	{
		// 8 bytes per block
		// 1 block for alpha, 1 block for color

		pBlock = (DXTColBlock*) ( (PTRINT)m_pCompBytes + j * xblocks * 16 );

		for( i=0; i < xblocks; i++, pBlock ++ )
		{

			// inline
			// Get alpha block

			pAlphaBlock = (DXTAlphaBlockExplicit*) pBlock;

			// inline func:
			// Get color block & colors
			pBlock++;
			GetColorBlockColors( pBlock, &col_0, &col_1, &col_2, &col_3, wrd );

			// Decode the color block into the bitmap bits
			// inline func:

			pImPos = (DWORD*)((PTRINT)pBase + i*16 + (j*4) * m_nWidth * 4 );


			DecodeColorBlock( pImPos, pBlock, m_nWidth, (DWORD*)&col_0, (DWORD*)&col_1,
								(DWORD*)&col_2, (DWORD*)&col_3 );

			// Overwrite the previous alpha bits with the alpha block
			//  info
			// inline func:
			DecodeAlphaExplicit( pImPos, pAlphaBlock, m_nWidth, alphazero );


		}
	}
}




void FDXTDecompressor::DecompressDXT5()
{

	int xblocks, yblocks;

#if 0 //def DEBUG
	if( (ddsd.dwWidth % 4 ) != 0 )
	{
		GWarn->Logf("****** warning width not div by 4!  %d\n", ddsd.dwWidth );
	}
	if( (ddsd.dwHeight %4 ) != 0 )
	{
		GWarn->Logf("****** warning Height not div by 4! %d\n", ddsd.dwHeight );
	}
	GWarn->Logf("end check\n");
#endif 

	xblocks = m_nWidth / 4;
	yblocks = m_nHeight / 4;

	
	int i,j;

	DWORD * pBase  = (DWORD*)  m_pDecompBytes;
	DWORD * pImPos = (DWORD*)  pBase;			// pos in decompressed data

	DXTColBlock				* pBlock;
	DXTAlphaBlock3BitLinear * pAlphaBlock;

	Color8888 col_0, col_1, col_2, col_3;
	_WORD wrd;

	// fill alphazero with appropriate value to zero out alpha when
	//  alphazero is ANDed with the image color 32 bit DWORD:
	col_0.a = 0;
	col_0.r = col_0.g = col_0.b = 0xff;
	DWORD alphazero = *((DWORD*) &col_0);

	////////////////////////////////
		GWarn->Logf(TEXT("blocks: x: %d    y: %d\n"), xblocks, yblocks );

	for( j=0; j < yblocks; j++ )
	{
		// 8 bytes per block
		// 1 block for alpha, 1 block for color

		pBlock = (DXTColBlock*) ( (PTRINT)m_pCompBytes + j * xblocks * 16 );

		for( i=0; i < xblocks; i++, pBlock ++ )
		{

			// inline
			// Get alpha block

			pAlphaBlock = (DXTAlphaBlock3BitLinear*) pBlock;

			// inline func:
			// Get color block & colors
			pBlock++;

			 //GWarn->Logf(TEXT("pBlock:   0x%.8x\n"), pBlock );

			GetColorBlockColors( pBlock, &col_0, &col_1, &col_2, &col_3, wrd );

			// Decode the color block into the bitmap bits
			// inline func:

			pImPos = (DWORD*)((PTRINT)pBase + i*16 + (j*4) * m_nWidth * 4 );


			DecodeColorBlock( pImPos, pBlock, m_nWidth, (DWORD*)&col_0, (DWORD*)&col_1,
								(DWORD*)&col_2, (DWORD*)&col_3 );

			// Overwrite the previous alpha bits with the alpha block
			//  info

			DecodeAlpha3BitLinear( pImPos, pAlphaBlock, m_nWidth, alphazero );


		}
	}
}				// dxt5


inline void GetColorBlockColors_m2( DXTColBlock * pBlock, Color8888 * col_0, Color8888 * col_1, 
													 Color8888 * col_2, Color8888 * col_3,
													 _WORD & wrd  )
{
	// method 2
	// freak variable bit structure method
	// normal math

	Color565 * pCol;

	pCol = (Color565*) & (pBlock->col0 );

	col_0->a = 0xff;
	col_0->r = pCol->nRed;
	col_0->r <<= 3;				// shift to full precision
	col_0->g = pCol->nGreen;
	col_0->g <<= 2;
	col_0->b = pCol->nBlue;
	col_0->b <<= 3;

	pCol = (Color565*) & (pBlock->col1 );
	col_1->a = 0xff;
	col_1->r = pCol->nRed;
	col_1->r <<= 3;				// shift to full precision
	col_1->g = pCol->nGreen;
	col_1->g <<= 2;
	col_1->b = pCol->nBlue;
	col_1->b <<= 3;


	if( pBlock->col0 > pBlock->col1 )
	{
		// Four-color block: derive the other two colors.    
		// 00 = color_0, 01 = color_1, 10 = color_2, 11 = color_3
		// These two bit codes correspond to the 2-bit fields 
		// stored in the 64-bit block.

		wrd = ((_WORD)col_0->r * 2 + (_WORD)col_1->r )/3;
											// no +1 for rounding
											// as bits have been shifted to 888
		col_2->r = (BYTE)wrd;

		wrd = ((_WORD)col_0->g * 2 + (_WORD)col_1->g )/3;
		col_2->g = (BYTE)wrd;

		wrd = ((_WORD)col_0->b * 2 + (_WORD)col_1->b )/3;
		col_2->b = (BYTE)wrd;
		col_2->a = 0xff;

		wrd = ((_WORD)col_0->r + (_WORD)col_1->r *2 )/3;
		col_3->r = (BYTE)wrd;

		wrd = ((_WORD)col_0->g + (_WORD)col_1->g *2 )/3;
		col_3->g = (BYTE)wrd;

		wrd = ((_WORD)col_0->b + (_WORD)col_1->b *2 )/3;
		col_3->b = (BYTE)wrd;
		col_3->a = 0xff;

	}
	else
	{
		// Three-color block: derive the other color.
		// 00 = color_0,  01 = color_1,  10 = color_2,  
		// 11 = transparent.
		// These two bit codes correspond to the 2-bit fields 
		// stored in the 64-bit block. 

		// explicit for each component, unlike some refrasts...
		
		// GWarn->Logf("block has alpha\n");

		wrd = ((_WORD)col_0->r + (_WORD)col_1->r )/2;
		col_2->r = (BYTE)wrd;
		wrd = ((_WORD)col_0->g + (_WORD)col_1->g )/2;
		col_2->g = (BYTE)wrd;
		wrd = ((_WORD)col_0->b + (_WORD)col_1->b )/2;
		col_2->b = (BYTE)wrd;
		col_2->a = 0xff;

		col_3->r = 0x00;		// random color to indicate alpha
		col_3->g = 0xff;
		col_3->b = 0xff;
		col_3->a = 0x00;

	}
}



inline void GetColorBlockColors_m3( DXTColBlock * pBlock, Color8888 * col_0, Color8888 * col_1, 
													 Color8888 * col_2, Color8888 * col_3,
													 _WORD & wrd  )
{
	// method 3
	//////////////////////////////////////////////////////
	// super-freak variable bit structure with
	//  Cool Math Trick (tm)

	// Do 2/3 1/3 math BEFORE bit shift on the whole DWORD
	// as the fields will NEVER carry into the next
	//  or overflow!! =) 

	Color565 * pCol;

	pCol = (Color565*) & (pBlock->col0 );

	col_0->a = 0x00;			// must set to 0 to avoid overflow in DWORD add
	col_0->r = pCol->nRed;
	col_0->g = pCol->nGreen;
	col_0->b = pCol->nBlue;

	pCol = (Color565*) & (pBlock->col1 );
	col_1->a = 0x00;
	col_1->r = pCol->nRed;
	col_1->g = pCol->nGreen;
	col_1->b = pCol->nBlue;

	if( pBlock->col0 > pBlock->col1 )
	{
		*((DWORD*)col_2) = ( (*((DWORD*)col_0)) * 2 + (*((DWORD*)col_1))  );

		*((DWORD*)col_3) = ( (*((DWORD*)col_0)) + (*((DWORD*)col_1)) * 2  );

		// now shift to appropriate precision & divide by 3.
		col_2->r = ((_WORD)col_2->r << 3) / (_WORD)3;
		col_2->g = ((_WORD)col_2->g << 2) / (_WORD)3;
		col_2->b = ((_WORD)col_2->b << 3) / (_WORD)3;

		col_3->r = ((_WORD)col_3->r << 3) / (_WORD)3;
		col_3->g = ((_WORD)col_3->g << 2) / (_WORD)3;
		col_3->b = ((_WORD)col_3->b << 3) / (_WORD)3;


		col_0->a = 0xff;		// now set appropriate alpha
		col_1->a = 0xff;
		col_2->a = 0xff;
		col_3->a = 0xff;
	}
	else
	{
		*((DWORD*)col_2) = ( (*((DWORD*)col_0)) + (*((DWORD*)col_1)) );

		// now shift to appropriate precision & divide by 2.
		// << 3 ) / 2 == << 2
		// << 2 ) / 2 == << 1
		col_2->r = ((_WORD)col_2->r << 2);
		col_2->g = ((_WORD)col_2->g << 1);
		col_2->b = ((_WORD)col_2->b << 2);

		col_2->a = 0xff;

		col_3->a = 0x00;	// 
		col_3->r = 0x00;	// random color to indicate alpha
		col_3->g = 0xff;
		col_3->b = 0xff;
	}

	// now shift orig color components
	col_0->r <<= 3;
	col_0->g <<= 2;
	col_0->b <<= 3;

	col_1->r <<= 3;
	col_1->g <<= 2;
	col_1->b <<= 3;
}


inline void GetColorBlockColors_m4( DXTColBlock * pBlock, Color8888 * col_0, Color8888 * col_1, 
													 Color8888 * col_2, Color8888 * col_3,
													 _WORD & wrd  )
{

	// m1 color extraction from 5-6-5
	// m3 color math on DWORD before bit shift to full precision


	wrd = pBlock->col0;
	col_0->a = 0x00;			// must set to 0 to avoid possible overflow & carry to next field in DWORD add

	// extract r,g,b bits
	col_0->b = (unsigned char) wrd & 0x1f;			// 0x1f = 0001 1111  to mask out upper 3 bits
	wrd >>= 5;
	col_0->g = (unsigned char) wrd & 0x3f;			// 0x3f = 0011 1111  to mask out upper 2 bits
	wrd >>= 6;
	col_0->r = (unsigned char) wrd & 0x1f;


	// same for col # 2:
	wrd = pBlock->col1;
	col_1->a = 0x00;			// must set to 0 to avoid possible overflow in DWORD add

	// extract r,g,b bits
	col_1->b = (unsigned char) wrd & 0x1f;
	wrd >>= 5;
	col_1->g = (unsigned char) wrd & 0x3f;
	wrd >>= 6;
	col_1->r = (unsigned char) wrd & 0x1f;


	if( pBlock->col0 > pBlock->col1 )
	{
		*((DWORD*)col_2) = ( (*((DWORD*)col_0)) * 2 + (*((DWORD*)col_1))  );

		*((DWORD*)col_3) = ( (*((DWORD*)col_0)) + (*((DWORD*)col_1)) * 2  );

		// shift to appropriate precision & divide by 3.
		col_2->r = ((_WORD)col_2->r << 3) / (_WORD)3;
		col_2->g = ((_WORD)col_2->g << 2) / (_WORD)3;
		col_2->b = ((_WORD)col_2->b << 3) / (_WORD)3;

		col_3->r = ((_WORD)col_3->r << 3) / (_WORD)3;
		col_3->g = ((_WORD)col_3->g << 2) / (_WORD)3;
		col_3->b = ((_WORD)col_3->b << 3) / (_WORD)3;


		col_0->a = 0xff;		// set appropriate alpha
		col_1->a = 0xff;
		col_2->a = 0xff;
		col_3->a = 0xff;
	}
	else
	{
		*((DWORD*)col_2) = ( (*((DWORD*)col_0)) + (*((DWORD*)col_1)) );

		// shift to appropriate precision & divide by 2.
		// << 3 ) / 2 == << 2
		// << 2 ) / 2 == << 1
		col_2->r = ((_WORD)col_2->r << 2);
		col_2->g = ((_WORD)col_2->g << 1);
		col_2->b = ((_WORD)col_2->b << 2);

		col_2->a = 0xff;

		col_3->a = 0x00;	// 
		col_3->r = 0x00;	// random color to indicate alpha
		col_3->g = 0xff;
		col_3->b = 0xff;
	}

	// shift orig color components to full precision
	col_0->r <<= 3;
	col_0->g <<= 2;
	col_0->b <<= 3;

	col_1->r <<= 3;
	col_1->g <<= 2;
	col_1->b <<= 3;
}


inline void GetColorBlockColors_m1( DXTColBlock * pBlock, Color8888 * col_0, Color8888 * col_1, 
													 Color8888 * col_2, Color8888 * col_3,
													 _WORD & wrd  )
{

	// Method 1:
	// Shifty method

	wrd = pBlock->col0;
	col_0->a = 0xff;

	// extract r,g,b bits
	col_0->b = (unsigned char) wrd;
	col_0->b <<= 3;		// shift to full precision
	wrd >>= 5;
	col_0->g = (unsigned char) wrd;
	col_0->g <<= 2;		// shift to full precision
	wrd >>= 6;
	col_0->r = (unsigned char) wrd;
	col_0->r <<= 3;		// shift to full precision


	// same for col # 2:
	wrd = pBlock->col1;
	col_1->a = 0xff;

	// extract r,g,b bits
	col_1->b = (unsigned char) wrd;
	col_1->b <<= 3;		// shift to full precision
	wrd >>= 5;
	col_1->g = (unsigned char) wrd;
	col_1->g <<= 2;		// shift to full precision
	wrd >>= 6;
	col_1->r = (unsigned char) wrd;
	col_1->r <<= 3;		// shift to full precision



	// use this for all but the super-freak math method

	if( pBlock->col0 > pBlock->col1 )
	{
		// Four-color block: derive the other two colors.    
		// 00 = color_0, 01 = color_1, 10 = color_2, 11 = color_3
		// These two bit codes correspond to the 2-bit fields 
		// stored in the 64-bit block.

		wrd = ((_WORD)col_0->r * 2 + (_WORD)col_1->r )/3;
											// no +1 for rounding
											// as bits have been shifted to 888
		col_2->r = (BYTE)wrd;

		wrd = ((_WORD)col_0->g * 2 + (_WORD)col_1->g )/3;
		col_2->g = (BYTE)wrd;

		wrd = ((_WORD)col_0->b * 2 + (_WORD)col_1->b )/3;
		col_2->b = (BYTE)wrd;
		col_2->a = 0xff;

		wrd = ((_WORD)col_0->r + (_WORD)col_1->r *2 )/3;
		col_3->r = (BYTE)wrd;

		wrd = ((_WORD)col_0->g + (_WORD)col_1->g *2 )/3;
		col_3->g = (BYTE)wrd;

		wrd = ((_WORD)col_0->b + (_WORD)col_1->b *2 )/3;
		col_3->b = (BYTE)wrd;
		col_3->a = 0xff;

	}
	else
	{
		// Three-color block: derive the other color.
		// 00 = color_0,  01 = color_1,  10 = color_2,  
		// 11 = transparent.
		// These two bit codes correspond to the 2-bit fields 
		// stored in the 64-bit block. 

		// explicit for each component, unlike some refrasts...
		
		// GWarn->Logf("block has alpha\n");

		wrd = ((_WORD)col_0->r + (_WORD)col_1->r )/2;
		col_2->r = (BYTE)wrd;
		wrd = ((_WORD)col_0->g + (_WORD)col_1->g )/2;
		col_2->g = (BYTE)wrd;
		wrd = ((_WORD)col_0->b + (_WORD)col_1->b )/2;
		col_2->b = (BYTE)wrd;
		col_2->a = 0xff;

		col_3->r = 0x00;		// random color to indicate alpha
		col_3->g = 0xff;
		col_3->b = 0xff;
		col_3->a = 0x00;

	}
}			//  Get color block colors (...)









//-----------------------------------------------------------------------------
// Name: GetNumberOfBits()
// Desc: Returns the number of bits set in a DWORD mask
//	from microsoft mssdk d3dim sample "Compress"
//-----------------------------------------------------------------------------
INT GetNumberOfBits( DWORD dwMask )
{
	INT wBits;
    for( wBits = 0; dwMask; wBits++ )
        dwMask = dwMask & ( dwMask - 1 );  

    return wBits;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

