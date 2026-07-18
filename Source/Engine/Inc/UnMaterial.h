
#if SUPPORTS_PRAGMA_PACK
#pragma pack(push,4)
#endif

#include "UnForcePacking_begin.h"

//
// Forward declarations
//
class	UMaterial;
class		URenderedMaterial;
class			UBitmapMaterial;
class				UTexture;

class	UModifier;

class FTexture;
class FCubemap;


//
// EFillMode
//
enum EFillMode
{
	FM_Wireframe	= 0,
	FM_FlatShaded	= 1,
	FM_Solid		= 2,
};

enum EMaterialType
{
	MT_Modifier				=	1,	
	MT_Combiner				=	2,
	MT_Shader				=	4,
	MT_ColorModifier		=	8,
	MT_FinalBlend			=	16,
	MT_OpacityModifier		=	32,
	MT_BitmapMaterial		=	64,
	MT_ProjectorMaterial	=	128,
	MT_TerrainMaterial		=	256,
	MT_ParticleMaterial		=	512,
	MT_ConstantMaterial		=	1024,
	MT_VertexColor			=	2048,
	MT_TexModifier			=	4096,
	MT_Texture				=	8192
};

/*-----------------------------------------------------------------------------
	UMaterial.
-----------------------------------------------------------------------------*/

class ENGINE_API UMaterial : public UObject
{
	DECLARE_ABSTRACT_CLASS(UMaterial,UObject,0,Engine)

	UMaterial*	FallbackMaterial;
	UMaterial*	DefaultMaterial;
	BITFIELD	UseFallback:1		GCC_PACK(4);
    BITFIELD	Validated:1;
    BYTE        SurfaceType  		GCC_PACK(4); // sjs
   	INT			MaterialType;

	// Constructor.
	UMaterial();

	// UObject interface
	void Serialize( FArchive& Ar );

	// UMaterial interface
	virtual UBOOL CheckCircularReferences( TArray<UMaterial*>& History );
	virtual UBOOL GetValidated() { return Validated; }
	virtual void SetValidated( UBOOL InValidated ) { Validated = InValidated; }
	virtual void PreSetMaterial(FLOAT TimeSeconds) {};

	// Getting information about a combined material:
	virtual INT MaterialUSize() { return 0; }
	virtual INT MaterialVSize() { return 0; }
	virtual UBOOL RequiresSorting() { return 0; }
	virtual UBOOL IsTransparent() { return 0; }
	virtual BYTE RequiredUVStreams() { return 1; }
	virtual UBOOL RequiresNormal() { return 0; }

	// Fallback handling
	static void ClearFallbacks();
	virtual UMaterial* CheckFallback();
	virtual UBOOL HasFallback() { return FallbackMaterial != NULL; }

	//!! OLDVER
	UMaterial* ConvertPolyFlagsToMaterial( UMaterial* InMaterial, DWORD InPolyFlags );

	DECLARE_FUNCTION(execMaterialUSize)
	DECLARE_FUNCTION(execMaterialVSize)
};

/*-----------------------------------------------------------------------------
	URenderedMaterial.
-----------------------------------------------------------------------------*/

class ENGINE_API URenderedMaterial : public UMaterial
{
	DECLARE_ABSTRACT_CLASS(URenderedMaterial,UMaterial,0,Engine)
    INT renderedmaterial_dummy;
};

/*-----------------------------------------------------------------------------
	UBitmapMaterial.
-----------------------------------------------------------------------------*/
class ENGINE_API UBitmapMaterial : public URenderedMaterial
{
	DECLARE_ABSTRACT_CLASS(UBitmapMaterial,URenderedMaterial,0,Engine)

	BYTE		Format;				// ETextureFormat.
	BYTE		UClampMode;			// Texture U clamp mode
	BYTE		VClampMode;			// Texture V clamp mode

	BYTE		UBits, VBits;		// # of bits in USize, i.e. 8 for 256.
	INT			USize, VSize;		// Size, must be power of 2.
	INT			UClamp, VClamp;		// Clamped width, must be <= size.
	


	// UBitmapMaterial interface.
	virtual FBaseTexture* GetRenderInterface() = 0;
	virtual UBitmapMaterial* Get( FTime Time, UViewport* Viewport ) { return this; }

	// UMaterial Interface
	virtual INT MaterialUSize() { return USize; }
	virtual INT MaterialVSize() { return VSize; }
};

/*-----------------------------------------------------------------------------
	UProxyBitmapMaterial
-----------------------------------------------------------------------------*/
class ENGINE_API UProxyBitmapMaterial : public UBitmapMaterial
{
	DECLARE_CLASS(UProxyBitmapMaterial,UBitmapMaterial,0,Engine);

private:

	FBaseTexture*	TextureInterface;

public:

	// UProxyBitmapMaterial interface.
	void SetTextureInterface(FBaseTexture* InTextureInterface)
	{
		TextureInterface = InTextureInterface;
		Format = TextureInterface->GetFormat();
		UClampMode = TextureInterface->GetUClamp();
		VClampMode = TextureInterface->GetVClamp();
		UClamp = USize = TextureInterface->GetWidth();
		VClamp = VSize = TextureInterface->GetHeight();
		UBits = appCeilLogTwo(UClamp);
		VBits = appCeilLogTwo(VClamp);
	}

	// UBitmapMaterial interface.
	virtual FBaseTexture* GetRenderInterface() { return TextureInterface; }
	virtual UBitmapMaterial* Get( FTime Time, UViewport* Viewport ) { return this; }
};

/*-----------------------------------------------------------------------------
	UTexCoordMaterial
-----------------------------------------------------------------------------*/
class ENGINE_API UTexCoordMaterial : public URenderedMaterial
{
    DECLARE_CLASS(UTexCoordMaterial,URenderedMaterial,0,Engine)

	class UBitmapMaterial* Texture;
    class UTexCoordGen* TextureCoords;

	// UMaterial interface
	virtual INT MaterialUSize() { return Texture ? Texture->MaterialUSize() : 0; }
	virtual INT MaterialVSize() { return Texture ? Texture->MaterialVSize() : 0; }
};


#include "UnForcePacking_end.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/


