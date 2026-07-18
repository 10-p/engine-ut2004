//=============================================================================
// Multipass Texture Shader implementation
// Copyright 2001 Digital Extremes - All Rights Reserved.
// Confidential.
//=============================================================================

#include "EnginePrivate.h"
#include "xTexShader.h"

MultiTextureShader::MultiTextureShader( FLevelSceneNode* inSceneNode, FRenderInterface* inRI, FVector& inLightVector )
    : SceneNode( inSceneNode ), RI( inRI ), LightVector( inLightVector ), PassesRequired( 0 )
{

}

void MultiTextureShader::SetFrameBlend( TexBlend blend )
{
    frameBlend = blend;
}

void MultiTextureShader::AddPass( UTexture* pTex, TexBlend blend, ETexCoordSource texGen, int uvSource, FMatrix& texTransform, DWORD Flags )
{
    if ( PassesRequired >= MAX_PASSES || pTex == NULL )
        return;

    // calc spherical or cubic envmapping
    if ( texGen == TC_CameraReflectionVector && !pTex->GetRenderInterface()->GetCubemapInterface() )
        texGen = TC_SphereMapCoords;

    AddFTexturePass( pTex->Get( SceneNode->Viewport->CurrentTime )->GetRenderInterface(), blend, texGen, uvSource, texTransform, Flags );

    Passes[PassesRequired-1].MipBias = 0.5f;//pTex->MipMult - 1.5f;
    
    if ( texGen == TC_CameraReflectionVector )
    {
        Passes[PassesRequired-1].TexTransform = FMatrix(FPlane(1,0,0,0), FPlane(0,1,0,0), FPlane(0,0,1,0), FPlane(0,0,0,1) );
        /*
		switch( pTex->EnvMapTransformType )
		{
			case EMTT_WorldSpace:
				
				break;
			case EMTT_ViewSpace:
				Passes[PassesRequired-1].TexTransform = SceneNode->WorldToCamera;
				break;
			case EMTT_LightSpace:
				Passes[PassesRequired-1].TexTransform = FInverseRotationMatrix(LightVector.Rotation());
				break;
            default:
                check( false );
		}*/
        Passes[PassesRequired-1].ProjectedUV = Passes[PassesRequired-1].Texture->GetCubemapInterface() ? 0 : 1;
    }
}

void MultiTextureShader::AddFTexturePass( FBaseTexture* pTex, TexBlend blend, ETexCoordSource texGen, int uvSource, FMatrix& texTransform, DWORD Flags )
{
    if ( PassesRequired >= MAX_PASSES || pTex == NULL )
        return;

    FTexturePass& texPass = Passes[PassesRequired];

    texPass.Texture = pTex;
    texPass.Blend = blend;
    texPass.TexGen = texGen;
    texPass.TexCoordSource = uvSource;
    texPass.TexTransform = texTransform;
    texPass.MipBias = -0.5f;
    texPass.ProjectedUV = 0;
    texPass.Flags = Flags;

    if ( Flags & TS_ProjectUV )
        texPass.ProjectedUV = 1;
    if ( Flags & TS_ClampUV )
        texPass.ClampUV = 1;

    PassesRequired++;
}

void MultiTextureShader::AddProjectorPass( AProjector* Projector )
{
    /*
    if ( !Projector->ProjectionMap )
        return;

    RI->CalcProjectionMatrix( Projector );
	DWORD PolyFlags = CalcBlendFlags( Projector->MultiCombine, NULL );
    if ( Projector->MultiCombine == STY_Modulated )
        PolyFlags = PF_Modulated | PF_Memorized;
    
    UTexture* pWorking = Projector->ProjectionMap->Get( SceneNode->Viewport->CurrentTime );

    pWorking->UClampMode = TC_Clamp;
    pWorking->VClampMode = TC_Clamp;
    DWORD Flags = 0;
    if ( !Projector->bOrthographic )
        Flags = MultiTextureShader::TS_ProjectUV;
    AddFTexturePass( pWorking->GetRenderInterface(),  PolyFlags, TC_WorldCoords, -1, Projector->TextureMatrix, Flags );
    */
}

void MultiTextureShader::AddProjectors( TList<FDynamicLight*>* Lights )
{
    int MAProjectors = 2;
    int NumProjectors = 0;

    for(TList<FDynamicLight*>* LightList = Lights;LightList;LightList = LightList->Next)
	{
        if(LightList->Element->Actor->GetClass() == AProjector::StaticClass() )
		{
            AddProjectorPass( (AProjector*)LightList->Element->Actor );
            NumProjectors++;
		}
        if ( NumProjectors == MAProjectors )
            break;
	}
}

void MultiTextureShader::AddProjectors( AProjector* ProjArray[MAX_ACTOR_PROJECTORS], int numProjectors )
{
    for ( int i=0; i<numProjectors; i++ )
    {
        AddProjectorPass( ProjArray[i] );
    }
}

void MultiTextureShader::DrawPrimitive( EPrimitiveType PrimitiveType,INT FirstIndex,INT NumPrimitives,INT MinIndex,INT MaxIndex )
{
    INT CurrentStage = 0;
    INT CurrentPass = 0;
    INT MaxStages = RI->GetMaxSimultaneousTextures();

    RI->PushState();

    for( CurrentPass = 0; CurrentPass < PassesRequired; CurrentPass++ )
    {
        if (CurrentStage >= MaxStages || ((Passes[CurrentPass].Flags & TS_LinkToNext) && (CurrentStage+1 >= MaxStages)) )
        {
		    RI->DrawPrimitive
            (
                PrimitiveType,
                FirstIndex,
                NumPrimitives,
                MinIndex,
                MaxIndex
            );
            CurrentStage = 0;
        }

        if( CurrentStage == 0)
        {
            if ( CurrentPass == 0)
            {
                RI->SetFrameBlending( frameBlend );
                RI->SetTextureBlending( 0, Passes[CurrentPass].Blend ); // sets material-texture combination
            }
            else
            {
                RI->SetFrameBlending( Passes[CurrentPass].Blend );
                RI->SetTextureBlending( 0, TB_NoDiffuseMod ); // don't modulate multipass with vertex colors!
            }
        }
        else
        {
            RI->SetTextureBlending( CurrentStage, Passes[CurrentPass].Blend);
        }

        RI->SetTextureTransform( CurrentStage, Passes[CurrentPass].TexTransform, Passes[CurrentPass].TexGen, Passes[CurrentPass].ProjectedUV, Passes[CurrentPass].TexCoordSource );
        RI->SetTexture( CurrentStage, Passes[CurrentPass].Texture );
        RI->SetMipBias( CurrentStage, Passes[CurrentPass].MipBias );

        CurrentStage++;
    }
    if (CurrentStage > 0)
    {
        for (CurrentPass = CurrentStage; CurrentPass < MaxStages; CurrentPass++)
        {
            RI->SetTextureBlending( CurrentPass, TB_Disable ); // disable upper texture blending //RI->SetTexture( CurrentPass, NULL );
        }
        RI->SetTextureBlending( 0, Passes[0].Blend ); // disable upper texture blending //RI->SetTexture( CurrentPass, NULL );
		RI->DrawPrimitive
        (
            PrimitiveType,
            FirstIndex,
            NumPrimitives,
            MinIndex,
            MaxIndex
        );
    }

    RI->PopState();
    RI->SetMaterial( NULL );

    // Cleanup render state
    for (CurrentPass = 0; CurrentPass < MaxStages; CurrentPass++)
    {
        RI->SetTexture( CurrentPass, NULL );
        //RI->SetTextureBlending( CurrentPass, TB_Disable );
        RI->SetTextureTransform( CurrentPass, FMatrix::Identity, TC_UVStream, 0 );
    }

    for (CurrentPass = 1; CurrentPass < MaxStages; CurrentPass++)
    {
        RI->SetTextureBlending( CurrentPass, TB_Disable );
    }
}

