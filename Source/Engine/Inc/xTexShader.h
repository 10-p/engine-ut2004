#ifndef XTEX_SHADER_H
#define XTEX_SHADER_H

#include "UnRenDev.h"

#include "UnForcePacking_begin.h"

const int MAX_ACTOR_PROJECTORS = 4;

enum TexBlend
{
    TB_Mod,
    TB_Mod2X,
    TB_Add,
    TB_Subtract,
    TB_ModAlphaAdd,
    TB_BlendDiffuseAlpha,
    TB_NoDiffuseMod,
    TB_Translucent,
    TB_AlphaTexture,
    TB_Disable,
    TB_Mod2XFade,
    TB_MAX
};

struct FTexturePass
{
    FBaseTexture*   Texture;
    TexBlend        Blend;
    ETexCoordSource TexGen;
    FMatrix         TexTransform;
    INT             TexCoordSource;
    float           MipBias;
    UBOOL           ProjectedUV;
    UBOOL           ClampUV;
    DWORD           Flags;
};

class MultiTextureShader
{
public:
    enum { MAX_PASSES = 8 };
    enum EMTexShaderFlags
    {
        TS_ProjectUV    = 1 << 1,
        TS_ClampUV      = 1 << 2,
        TS_LinkToNext   = 1 << 3,
    };

    MultiTextureShader( FLevelSceneNode* inSceneNode, FRenderInterface* inRI, FVector& inLightVector );

    void AddPass( UTexture* pTex, TexBlend blend, ETexCoordSource texGen, int uvSource, FMatrix& texTransform, DWORD Flags=0 );
    void AddFTexturePass( FBaseTexture* pTex, TexBlend blend, ETexCoordSource texGen, int uvSource, FMatrix& texTransform, DWORD Flags=0 );
    void AddProjectors( TList<FDynamicLight*>* LightList );
    void AddProjectors( AProjector* ProjArray[MAX_ACTOR_PROJECTORS], int numProjectors );
    void DrawPrimitive( EPrimitiveType PrimitiveType,INT FirstIndex,INT NumPrimitives,INT MinIndex,INT MaxIndex );
    void AddProjectorPass( AProjector* Projector );
    void SetFrameBlend( TexBlend blend );
protected:
    FTexturePass        Passes[MAX_PASSES];
    int                 PassesRequired;
    FLevelSceneNode*	SceneNode;
	FRenderInterface*	RI;
    FVector             LightVector;
    TexBlend            frameBlend;
};

#include "UnForcePacking_end.h"

#endif//XTEX_SHADER_H
