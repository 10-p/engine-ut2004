
#include "xForceFeedback.h"

// IFC is Immersion's force-feedback SDK, a Windows-only library that is not in this tree, so only an
// MSVC build with Epic's project compiles this.
#if defined(WIN32) && defined(_MSC_VER)
#define __IFC__
#endif

#ifdef _XBOX
#undef __IFC__
#endif

// !!! FIXME: Remove this if a Win64 version of IFC ever shows up.  --ryan.
#if _WIN64
#undef __IFC__
#endif

int GForceFeedbackAvailable = 0;

#ifdef __IFC__
#include "IFC.h"
static CImmDevice *ImmDevice = NULL;
static CImmProject *ImmProject = NULL;
static CImmDevices *ImmDevices = NULL;

static const char *PickFile( CImmDevice *Device )
{
    // Would be nice to manually set effect set
    const char *FileName = "../ForceFeedback/other.ifr";
    
    if( Device )
    {
        char ProductName[256];
        if( Device->GetProductName( ProductName, sizeof(ProductName) ) )
        {
			if (Device->GetDeviceType() == IMM_DEVICETYPE_MOUSE)
			{
				if
				(   !stricmp( ProductName, "Logitech iFeel Mouse" )
				||  !stricmp( ProductName, "Logitech iFeel MouseMan" )
				){
					FileName = "../ForceFeedback/ifeel.ifr";
				}
			}
			else
			{
				if (Device->GetDeviceType() == IMM_WHEEL || Device->GetDeviceType() == IMM_GAMEPAD)
				{
					FileName = "../ForceFeedback/gamepad.ifr";
				}
				else
				{
					FileName = "../ForceFeedback/joystick.ifr";
				}
			}
        }
    }
    
    return FileName;
}
#endif


// Make sure this is re-entrant!
void InitForceFeedback(void* hInstance, void* hWnd)
{
#ifdef __IFC__
    if( ImmDevice )
        return;

	ImmDevices = new CImmDevices();
	if( ImmDevices && ImmDevices->CreateDevices( (HINSTANCE)hInstance, (HWND)hWnd, -1, IMM_ENUMERATE_ALL, IMM_PREFER_DX_DEVICES ) > 0 )
		ImmDevice = ImmDevices->GetDevice( 0 );

    if( !ImmDevice )
        return;

    ImmProject = new CImmProject;
    if
	(	!ImmProject 
	||	!ImmProject->OpenFile( PickFile( ImmDevice ), ImmDevice )
	){
        ExitForceFeedback();
        return;
    }

    PrecacheForceFeedback();  // move elsewhere?

    GForceFeedbackAvailable = 1;
#else
	hInstance;
	hWnd;
    GForceFeedbackAvailable = 0;
#endif
}

void ExitForceFeedback()
{
#ifdef __IFC__
    if( ImmProject )
    {
        delete ImmProject;
        ImmProject = NULL;
    }

    ImmDevice = NULL;

	if( ImmDevices )
	{
		delete ImmDevices;
		ImmDevices = NULL;
	}
#endif
	GForceFeedbackAvailable = 0;
}

void PlayFeedbackEffect( const char* EffectName )
{
#ifdef __IFC__
    if( ImmProject )
        ImmProject->Start( EffectName );
#else
    EffectName;
#endif
}

// Pass NULL to stop all
void StopFeedbackEffect( const char* EffectName )
{
#ifdef __IFC__
    if( ImmProject )
        ImmProject->Stop( EffectName );
#else
    EffectName;
#endif
}

void PrecacheForceFeedback()
{
#ifdef __IFC__
    // Cache all compound effects
    for
    (   int i = 0, NumEffects = ImmProject->GetNumEffects()
    ;   i < NumEffects
    ;   i++
    ){
        if( IMM_EFFECTTYPE_COMPOUND == ImmProject->GetEffectType( ImmProject->GetEffectName( i ) ) )
        {
            ImmProject->CreateEffectByIndex( i );
        }
    }
#endif
}

void ChangeBaseParamsForceFeedback( const char* EffectName, int DirectionX, int DirectionY, unsigned int Gain )
{
#ifdef __IFC__
	if( ImmProject )
	{
		CImmCompoundEffect *pICE = ImmProject->GetEffect( EffectName );
		if( pICE )
		{
			if (DirectionX < 10000 || DirectionX > 10000)
				DirectionX = IMM_EFFECT_DONT_CHANGE;
			if (DirectionY < 10000 || DirectionY > 10000)
				DirectionY = IMM_EFFECT_DONT_CHANGE;
			if (Gain < 0 || Gain > 10000)
				Gain = IMM_EFFECT_DONT_CHANGE;

			// assumes component effects in same direction
			for (int i = 0; i < pICE->GetNumberOfContainedEffects(); i++)
			{
				CImmEffect *pIE = pICE->GetContainedEffect( (long)i );
				if( pIE )
				{
					pIE->ChangeBaseParams
					(	DirectionX
					,	DirectionY
					,	IMM_EFFECT_DONT_CHANGE
					,	(LPIMM_ENVELOPE) IMM_EFFECT_DONT_CHANGE_PTR
					,	IMM_EFFECT_DONT_CHANGE
					,	Gain
					);
				}

				GUID guid = pIE->GetGUID();
				// Conditions have different avenue to change direction
				if( DirectionX != IMM_EFFECT_DONT_CHANGE
				&&	DirectionY != IMM_EFFECT_DONT_CHANGE
				){
					if( IsEqualGUID(guid, GUID_Imm_Spring)
					||	IsEqualGUID(guid, GUID_Imm_DeviceSpring)
					||	IsEqualGUID(guid, GUID_Imm_Damper)
					||	IsEqualGUID(guid, GUID_Imm_Inertia)
					||	IsEqualGUID(guid, GUID_Imm_Friction)
					||	IsEqualGUID(guid, GUID_Imm_Texture)
					||	IsEqualGUID(guid, GUID_Imm_Grid)
					){
						CImmCondition *pIC = (CImmCondition *)pIE;
						pIC->ChangeConditionParams
						(	IMM_EFFECT_DONT_CHANGE
						,	IMM_EFFECT_DONT_CHANGE
						,	IMM_EFFECT_DONT_CHANGE
						,	IMM_EFFECT_DONT_CHANGE
						,	IMM_EFFECT_DONT_CHANGE
						,	IMM_EFFECT_DONT_CHANGE_POINT
						,	DirectionX
						,	DirectionY
						);
					}
				}
			}
		}
	}
#else
    EffectName;
    DirectionX;
    DirectionY;
    Gain;
#endif
}

void ChangeSpringFeedbackEffect( const char* EffectName, int CenterX, int CenterY )
{
#ifdef __IFC__
	if( ImmProject )
	{
		CImmCompoundEffect *pICE = ImmProject->GetEffect( EffectName );
		if( pICE )
		{
			CImmEffect *pIE = pICE->GetContainedEffect( (long)0 );
			if( pIE )
			{
				GUID guid = GUID_Feelit_Spring;
				if (pIE->GetIsCompatibleGUID(guid))
				{
					POINT p = {CenterX, CenterY};
					BOOL bRes = ((CImmCondition*)pIE)->ChangeCenter( p );
					bRes = bRes;
				}
			}
		}
	}
#else
    EffectName;
    CenterX;
    CenterY;
#endif
}
