#ifndef _INIT_D3D_H
#define _INIT_D3D_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

    $Date: 2002/04/04 15:29:39 $ $Revision: 1.3.8.2 $

   This software and its accompanying manuals have been developed
   by MathEngine PLC ("MathEngine") and the copyright and all other
   intellectual property rights in them belong to MathEngine. All
   rights conferred by law (including rights under international
   copyright conventions) are reserved to MathEngine. This software
   may also incorporate information which is confidential to
   MathEngine.

   Save to the extent permitted by law, or as otherwise expressly
   permitted by MathEngine, this software and the manuals must not
   be copied (in whole or in part), re-arranged, altered or adapted
   in any way without the prior written consent of the Company. In
   addition, the information contained in the software may not be
   disseminated without the prior written consent of MathEngine.

 */

#ifdef WITH_D3D
#include <windows.h>
#include <windowsx.h>
#include <d3d.h>
#include <ddraw.h>

#include <MeViewer.h>
#include "RMouseCam.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*
    If you do not have access to the DirectX SDK version 7 or greater,
    then you must compile MeViewer without DirectX support.

    This is achieved by compiling without WITH_D3D being #define-ed.

    Visual Studio users:
        "Project/Settings..."
        Select the MeViewer project
        "C/C++" tab
        Select "Category:" "Preprocessor:"
        Add "WITH_D3D" to "Undefined symbols"


    'makefile' users must change the following in the makefile:


            ifeq '$(PLATFORM)' 'win32'
            DEFINES   = WITH_BENCHMARK WITH_NULL WITH_OPENGL WITH_D3D

          becomes

            ifeq '$(PLATFORM)' 'win32'
            DEFINES   = WITH_BENCHMARK WITH_NULL WITH_OPENGL

*/
#if DIRECTDRAW_VERSION < 0x0700
  #error "Requires Direct Draw version 7 or higher (see Init_d3d.h for details)"
#endif
#ifdef _ME_API_DOUBLE
  #error "DirectX support unavailable in double precision build (see init_d3d.h for details on building with opengl only)"
#endif


/* Constants taken from Microsoft D3D framework */
#define D3DENUM_SOFTWAREONLY           0x00000001
#define D3DENUMERR_NOCOMPATIBLEDEVICES 0x81000004
#define D3DFW_FULLSCREEN               0x00000001
#define D3DFW_STEREO                   0x00000002
#define D3DFW_ZBUFFER                  0x00000004
#define D3DFW_NO_FPUSETUP              0x00000008

typedef struct D3DVars D3DVars;

/* Structure to hold information about enumerated devices
   as specified by Microsoft's D3D Framework */
typedef struct D3DEnum_DeviceInfo
{
    /* D3D Device info */
    TCHAR          strDesc[40];
    GUID*          pDeviceGUID;
    D3DDEVICEDESC7 ddDeviceDesc;
    BOOL           bHardware;

    /* DDraw Driver info */
    GUID*          pDriverGUID;
    DDCAPS         ddDriverCaps;
    DDCAPS         ddHELCaps;

    /* DDraw Mode Info */
    DDSURFACEDESC2 ddsdFullscreenMode;
    BOOL           bWindowed;
    BOOL           bStereo;

    GUID            guidDevice;
    GUID            guidDriver;
    DDSURFACEDESC2* pddsdModes;
    DWORD           dwNumModes;
    DWORD           dwCurrentMode;
    BOOL            bDesktopCompatible;
    BOOL            bStereoCompatible;
	D3DVars*		D3D;	
} D3DEnum_DeviceInfo;


/* Definitions for creating D3D stuff */

/* Including terminating '\0' */
#define MAX_TITLE_LENGTH 100

struct D3DVars
{
    HWND                 m_hWnd;

    BOOL                 m_bActive;
    BOOL                 m_bReady;
    BOOL                 m_bQuit;
    BOOL                 m_bIsFrameLocked;
    BOOL                 m_bDisplayFps;
    DWORD                m_dwFpsUpdate;
    unsigned int         m_uiTicksPerFrame;
    unsigned int         m_uiTicksPerSec;

    BOOL                 m_bLockAspectRatio;
    BOOL                 m_bDisplay2D;
    BOOL                 m_bDisplayPS;
    BOOL                 m_bAlphaBlend;
    BOOL                 m_bAlphaBlend2D;
    BOOL                 m_bAllowWireFrame;
    BOOL                 m_bForceWireFrame;
    BOOL                 m_bUseAntiAliasing;
    BOOL                 m_bUseTextures;
    BOOL                 m_bLinearFilter;
    BOOL                 m_bDisplayLookAt;
    BOOL                 m_bDoStep; /* for single-frame advance */

    TCHAR                m_strWindowTitle[MAX_TITLE_LENGTH];

    /* application's callback for accepting devices based on capabilities */
    HRESULT (*ConfirmDevFn)(DDCAPS*, D3DDEVICEDESC7*);

    DWORD                m_dwNumDevicesEnumerated;
    DWORD                m_dwNumDevices;
    D3DEnum_DeviceInfo   m_pDeviceList[20];

    char                 m_strDevicename[256];

    D3DEnum_DeviceInfo   *m_pDeviceInfo; /* pointer to chosen device */

    BOOL                 m_bIsFullscreen;
    DWORD                m_dwRenderWidth;
    DWORD                m_dwRenderHeight;
    RECT                 m_rcScreenRect;
    LPDIRECTDRAW7        m_pDD;
    LPDIRECT3D7          m_pD3D;
    LPDIRECT3DDEVICE7    m_pd3dDevice;
    LPDIRECTDRAWSURFACE7 m_pddsFrontBuffer;
    LPDIRECTDRAWSURFACE7 m_pddsBackBuffer;
    LPDIRECTDRAWSURFACE7 m_pddsZBuffer;
    DWORD                m_dwDeviceMemType;
    DDPIXELFORMAT        m_ddpfBackBufferPixelFormat;
    DDSURFACEDESC2       m_ddsdBackBuffer;
    BOOL                 m_bAppUseZBuffer;
    BOOL                 m_bNoVSync;

    D3DVIEWPORT7         m_vpViewPort;

    D3DMATERIAL7         m_d3dmtrl;  /* Default material */
    D3DLIGHT7            m_lgtDir1;  /* First directional light */
    D3DLIGHT7            m_lgtDir2;  /* Sencond directional light */
    D3DLIGHT7            m_lgtPt;    /* Point light */

    LPDIRECTDRAWSURFACE7 m_pddsTexture[25]; /* Texture surfaces */

    RMainLoopCallBack    callbackfunc;
    void                 *userdata;     /* callback userdata */
    RRender              *rc;

};


/* Function Prototypes */

/* sets up basic MS Windows window */
BOOL    D3D_CreateWin(D3DVars* D3D,HINSTANCE hInst);

/* initialises gD3D structure */
void    D3D_InitGlobals(RRender* rc);

/* does message loop etc */
void    D3D_RunApp(RRender *rc, RMainLoopCallBack func,void *userdata);

/* message handling */
long    WINAPI DummyD3D_WndProc(HWND win, UINT msg, WPARAM wparam, LPARAM lparam);
long    WINAPI D3D_WndProc(HWND win, UINT msg, WPARAM wparam, LPARAM lparam);

/* in case things need to be done in powersave mode */
LRESULT D3D_OnQuerySuspend( DWORD dwFlags );
LRESULT D3D_OnResumeSuspend( DWORD dwData );

/* enumerate devices, etc  */
HRESULT D3D_initD3D(D3DVars* D3D);

/* establish surfaces, etc */
HRESULT D3D_setupD3D(D3DVars* D3D);
HRESULT D3D_EnumerateDevices( D3DVars* D3D,HRESULT(*ConfirmDevFn)(DDCAPS*, D3DDEVICEDESC7*) );
BOOL WINAPI D3D_DriverEnumCallback( GUID* pGUID, TCHAR* strDesc, TCHAR* strName, VOID*, HMONITOR );
HRESULT WINAPI D3D_ModeEnumCallback( DDSURFACEDESC2* pddsd, VOID* pParentInfo );
HRESULT WINAPI D3D_DeviceEnumCallback( TCHAR* strDesc, TCHAR* strName, D3DDEVICEDESC7* pDesc, VOID* pParentInfo );

/* used as qsort callback */
int __cdecl D3D_SortModesCallback( const void* arg1, const void* arg2 );

/* callback for selecting z-buffer modes */
HRESULT WINAPI D3D_EnumZCallback( DDPIXELFORMAT* pf, void* pcon );

/* device rejection callback */
HRESULT D3D_DeviceAcceptable( DDCAPS *pddCaps, D3DDEVICEDESC7 *pd3dDevDesc );

/* determines 'best' device */
HRESULT D3D_SelectDefaultDevice( D3DVars* D3D, DWORD dwFlags );

/* does exactly what it says on the tin */
HRESULT D3D_CreateFullscreenSurfaces(D3DVars* D3D);

/* as above */
HRESULT D3D_CreateWindowSurfaces(D3DVars* D3D);

/* flips surfaces or blits in window */
HRESULT D3D_doFlip(D3DVars* D3D);

/* destroys surfaces and ddraw objects */
HRESULT D3D_CleanUpD3D(D3DVars* D3D);

/* bet you can't guess what this does */
HRESULT D3D_CreateZBuffer();

/* pop-up change device dialog */
HRESULT D3D_doDevDlg(D3DVars* D3D);

/* dialog message handler */
BOOL CALLBACK D3D_DevDlgMsgProc( HWND hDlg, UINT msg, WPARAM wparam, LPARAM lparam );

/* keeps track of pause states */
void    D3D_Pause( D3DVars* D3D,BOOL bPause );

/* brings GDI surface to front to display dialogs, etc */
HRESULT D3D_FlipToGDISurface( D3DVars* D3D,BOOL bDrawFrame );
VOID D3D_UpdateDialog( HWND hDlg, D3DVars* D3D, DWORD dwCurrentMode, BOOL bWindowed );

/* texture functions */
HRESULT D3D_LoadTextures(D3DVars* D3D);
LPDIRECTDRAWSURFACE7 D3D_CreateTextureFromBitmap( D3DVars* D3D,HBITMAP hbm, int bUseColorKey );
HRESULT CALLBACK D3D_TextureSearchCallBack( DDPIXELFORMAT* pddpf, VOID* param );

/* displays the help */
void D3D_DisplayConsoleHelp();


/* Header for d3d render */
#define DD_RED    RGB(255,0,0)
#define DD_GREEN  RGB(0,255,0)
#define DD_BLUE   RGB(0,0,255)
#define DD_YELLOW RGB(255,255,0)
#define DD_CYAN   RGB(0,255,255)
#define DD_MAGENTA  RGB(255,0,255)
#define DD_WHITE  RGB(255,255,255)
#define DD_BLACK  RGB(0,0,0)

extern MeI64            D3D_startframeclock;

/* Function Prototypes */
int        D3D_CreateRenderer(RRender *rc,void *pWnd); /* sets up window */
HRESULT      D3D_RenderD3D(RRender* rc); /* sets timer, calls DrawFrame, does frame-lock */
HRESULT      D3D_DrawText( D3DVars* D3D,TCHAR* txt, DWORD x, DWORD y, COLORREF rgb ); /* wonder what this does... */
void       D3D_fpsCalc(D3DVars* D3D,MeI64 starttime);  /* calculates and displays fps */
DOUBLE       D3D_highResClock();    /* clock for fps */
void       D3D_DrawFrame(D3DVars* D3D);   /* all drawing to back-buffer goes here */
HRESULT      D3D_SceneInit(D3DVars* D3D);   /* called before rendering starts */
HRESULT      D3D_SceneEnd(D3DVars* D3D);      /* called just before ddraw devices are destroyed */
void       D3D_CalibrateTimer(D3DVars* D3D);
void             D3D_SetWindowTitle(RRender *rc, const char * title);

#ifdef __cplusplus
}
#endif


#endif

#endif /* _D3DINIT_H */
