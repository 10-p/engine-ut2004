#ifndef _MEVIEWER_H
#define _MEVIEWER_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

    $Date: 2002/04/04 15:29:39 $ $Revision: 1.77.2.4 $

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

#include <MeViewerTypes.h>
#include <MeMessage.h>
#include <MeMath.h>
#include <MeSimpleFile.h>
#include <MeProfile.h>
#include <MeCommandLine.h>
#include <MeStream.h>

#ifdef __cplusplus
extern "C"
{
#endif
void              MEAPI RParseText(RRender *rc, char *text_in, char *text_out,
                                   int outbuffersize);

int               MEAPI RBmpLoad(RRender* rc, char *filename, RImage *p_image,
                                 int bRequireBGR);

/* RRender */
RRender *         MEAPI RRenderContextCreate2(MeCommandLineOptions* options,
                            MeCommandLineOptions* overrideoptions,
                            int eat,void *pWnd,RCreateCallback cb);

/* for backwards compatability */
#define                 RRenderContextCreate(x,y,z) \
                            RRenderContextCreate2((x),(y),(z),NULL,NULL)

int               MEAPI RRenderContextDestroy( RRender *rc );
void              MEAPI RRenderQuit( RRender *rc );
void              MEAPI RRenderSetPause( RRender *rc, int p);
void              MEAPI RRenderUpdateProjectionMatrices( RRender *rc );
void              MEAPI RRenderUpdateGraphicMatrices( RRender *rc );

int               MEAPI RRenderTextureCreate(RRender *rc, const char *filename);

void              MEAPI RRenderToggleUserHelp( RRender *const rc );
void              MEAPI RRenderCreateUserHelp(RRender *const rc,
                            const char *const help[],const int arraySize);

void              MEAPI RRenderSkydomeCreate(RRender *const rc,
                            const char * name,int tileU, int tileV);

void              MEAPI RRenderSetAppName(RRender* rc, const char* appname);

void              MEAPI RRenderSetBackgroundColor(RRender* rc,  const float color[4]);

void              MEAPI RRenderSetSphereResolution(RRender* r, int res);
int               MEAPI RRenderGetSphereResolution(RRender* r);


/* RGraphic */
RGraphic *        MEAPI RGraphicCreate(RRender *rc, char *filename,
                                       AcmeReal xScale,
                                       AcmeReal yScale,
                                       AcmeReal zScale,
                                       const float color[4],
                                       MeMatrix4Ptr matrix,
                                       MeBool is2D, MeBool bKeepAspectRatio);

RGraphic *        MEAPI RGraphicCreateEmpty( int numVertices );

RGraphic *        MEAPI RGraphicLoadMEG( char *filename );

/* ASE Loading */
RGraphic *        MEAPI RGraphicLoadASE(RRender *rc, char* filename,
                                        AcmeReal xScale,
                                        AcmeReal yScale,
                                        AcmeReal zScale,
                                        const float color[4],
                                        MeMatrix4Ptr matrix);

RGraphic *        MEAPI RGraphicLoadASEwithOffset(RRender *rc, char* filename,
                                        AcmeReal xScale,
                                        AcmeReal yScale,
                                        AcmeReal zScale,
                                        const float color[4],
                                        MeVector3Ptr offset,
                                        MeMatrix4Ptr matrix);

RGraphic *        MEAPI RGraphicLoadASENoMCD(RRender *rc, char* filename,
                                        AcmeReal xScale, 
                                        AcmeReal yScale, 
                                        AcmeReal zScale,
                                        const float color[4], 
                                        MeMatrix4Ptr matrix);

RGraphic *        MEAPI RGraphicLoadASEwithOffsetNoMcd(RRender *rc, char* filename,
                                                  AcmeReal xScale,
                                                  AcmeReal yScale,
                                                  AcmeReal zScale,
                                                  const float color[4],
                                                  MeVector3Ptr offset,
                                                  MeMatrix4Ptr matrix);

RGraphic *        MEAPI RGraphicLoadASEwithOffsetNoMcdFromStream(RRender *rc, MeStream str,
                                                       AcmeReal xScale,
                                                       AcmeReal yScale,
                                                       AcmeReal zScale,
                                                       const float color[4],
                                                       MeVector3Ptr offset,
                                                       MeMatrix4Ptr matrix);

int               MEAPI RGraphicSave( RGraphic *rg, char *filename );

void              MEAPI RGraphicDestroy(RGraphic *rg);
void              MEAPI RGraphicDelete(RRender *rc, RGraphic *rg, int is2D);

RGraphic*         MEAPI RGraphicCombine(RRender* rc, RGraphic* a, RGraphic* b, 
                                        MeMatrix4Ptr transform);


/*must call after object is created (sorts by alpha) */
void              MEAPI RGraphicAddToList(RRender *rc, RGraphic *rg, int is2D);
void              MEAPI RGraphicRemoveFromList(RRender *rc, RGraphic *rg,
                                               int is2D);
int               MEAPI RGraphicSetTexture(RRender *rc, RGraphic *rg,
                                           const char *filename);
/* called by RGraphicLoad */
void              MEAPI RGraphicFillObjectBuffer( char *filename );
void              MEAPI RGraphicNormalize( RGraphic *rg, int bKeepAspectRatio);
void              MEAPI RGraphicScale(RGraphic *rg, AcmeReal xScale,
                                      AcmeReal yScale, AcmeReal zScale);

/* Primitives creation */
RGraphic *        MEAPI RGraphic2DRectangleCreate(RRender *rc,
                                                  AcmeReal orig_x,
                                                  AcmeReal orig_y,
                                                  AcmeReal width,
                                                  AcmeReal height,
                                                  const float color[4]);

RGraphic *        MEAPI RGraphicBoxCreate(RRender *rc, AcmeReal width,
                                          AcmeReal height, AcmeReal depth,
                                          const float color[4],
                                          MeMatrix4Ptr matrix);

RGraphic *        MEAPI RGraphicConeCreate(RRender *rc, AcmeReal radius,
                                           AcmeReal upper_height,
                                           AcmeReal lower_height,
                                           const float color[4],
                                           MeMatrix4Ptr matrix);

RGraphic *        MEAPI RGraphicCylinderCreate(RRender *rc, AcmeReal radius,
                                               AcmeReal height,
                                               const float color[4],
                                               MeMatrix4Ptr matrix);

RGraphic *        MEAPI RGraphicGroundPlaneCreate(RRender *rc,
                                                  AcmeReal side_length,
                                                  int triangles_per_side,
                                                  const float color[4],
                                                  AcmeReal y_position);

RGraphic *        MEAPI RGraphicLineCreate(RRender *rc, AcmeReal origin[3],
                                           AcmeReal end[3],
                                           const float color[4],
                                           MeMatrix4Ptr matrix);

RGraphic *        MEAPI RGraphicSphereCreate(RRender *rc,
                                             AcmeReal radius,
                                             const float color[4],
                                             MeMatrix4Ptr matrix);

RGraphic *        MEAPI RGraphicSquareCreate(RRender *rc,
                                             AcmeReal side,
                                             const float color[4],
                                             MeMatrix4Ptr matrix);

RGraphic *        MEAPI RGraphicTextCreate(RRender *rc,  char *text,
                                           AcmeReal orig_x, AcmeReal orig_y,
                                           const float color[4]);

RGraphic *        MEAPI RGraphicTorusCreate(RRender *rc, AcmeReal innerRadius,
                                            AcmeReal outerRadius,
                                            const float color[4],
                                            MeMatrix4Ptr matrix);

RGraphic *        MEAPI RGraphicFrustumCreate(RRender *rc,
                                              AcmeReal bottomRadius,
                                              AcmeReal topRadius,
                                              AcmeReal bottom,
                                              AcmeReal top, int sides,
                                              const float color[4],
                                              MeMatrix4Ptr matrix);

RGraphic *        MEAPI RGraphicSphylCreate(RRender *rc, AcmeReal radius,
                                            AcmeReal height,
                                            const float color[4],
                                            MeMatrix4Ptr matrix);

/* Utilities functions for manipulating RGraphics */
int               MEAPI RGraphicLineMoveEnds(RGraphic *lineG,
                                             AcmeReal *origin, AcmeReal *end);
void              MEAPI RGraphicSetTransformPtr(RGraphic *g,
                                                MeMatrix4Ptr matrix);
void              MEAPI RGraphicSetColor(RGraphic *g, const float color[4]);
void              MEAPI RGraphicGetColor(RGraphic *g, float color[4]);
void              MEAPI RConvertTriStripToTriList(RGraphic* rg,
                                                  RObjectVertex* strips,
                                                  int* stripSize,
                                                  int* stripStart,
                                                  int numStrips);
void              MEAPI RGraphicSetWireframe(RGraphic *g, MeBool b);
MeBool            MEAPI RGraphicIsWireframe(RGraphic *g);
void              MEAPI RGraphicCalcOGLDisplayList(RGraphic *rg);
void              MEAPI RGraphicTranslateVertices(RRender *rc,RGraphic *rg, MeVector3Ptr offset);

/* Debug Line Functionality */

void              MEAPI RLineAdd(RRender*const rc, 
                                 const AcmeVector3 start, const AcmeVector3 end, 
                                 const AcmeReal r, const AcmeReal g, const AcmeReal b);
void              MEAPI RLineRemoveAll(RRender* rc);

/* Native Lines */
void MEAPI RNLineAddToList(RRender *rc, RNativeLine *line);
void MEAPI RNLineRemoveFromList(RRender *rc, RNativeLine *line);
void MEAPI RNLineDestroy(RNativeLine *line);
void MEAPI RNLineDelete(RRender *rc, RNativeLine *line);
RNativeLine* MEAPI RNLineCreate( RRender*const rc,
    const AcmeReal start[3], const AcmeReal end[3],
    const AcmeReal color[4], const MeVector4 *const matrix);
void MEAPI RNLineSetColor( RNativeLine* line, AcmeReal color[4] );
void MEAPI RNLineMoveEnds( RNativeLine*const line,
    const AcmeReal start[3], const AcmeReal end[3] );

/* Lights */

void MEAPI RLightSwitchOn(RRender* rc, RRenderLight light);
void MEAPI RLightSwitchOff(RRender* rc, RRenderLight light);

/* Camera functions */
void              MEAPI RCameraGetLookAt(RRender *rc, AcmeVector3 camlookat);
void              MEAPI RCameraGetPosition(RRender *rc, AcmeVector3 pos );
void              MEAPI RCameraGetLookDir(RRender *rc, AcmeVector3 dir);
AcmeReal          MEAPI RCameraGetDistance(RRender *rc);
AcmeReal          MEAPI RCameraGetElevation(RRender *rc);
AcmeReal          MEAPI RCameraGetRotation(RRender *rc);
void              MEAPI RCameraGetUp(RRender *rc, AcmeVector3 up);
void              MEAPI RCameraSetRotation(RRender *rc, const MeReal rotation);
void              MEAPI RCameraSetElevation(RRender* rc, const MeReal elevation);
void              MEAPI RCameraSetOffset(RRender* rc, const MeReal offset);

void              MEAPI RCameraSetLookAt( RRender *rc, const AcmeVector3 lookAt);
void              MEAPI RCameraSetLookAtAndPosition(RRender *rc,
                                                    const MeVector3 lookAt,
                                                    const MeVector3 position);
void              MEAPI RCameraSetView( RRender *rc, AcmeReal dist,
                                       AcmeReal angle, AcmeReal elevation );
void              MEAPI RCameraSetMatrix( RRender *rc, MeMatrix4Ptr m );
void              MEAPI RCameraSetFov( RRender *rc, AcmeReal fov );

void              MEAPI RCameraPanX( RRender *rc, AcmeReal dist );
void              MEAPI RCameraPanY( RRender *rc, AcmeReal dist );
void              MEAPI RCameraPanZ( RRender *rc, AcmeReal dist );
void              MEAPI RCameraRotateAngle( RRender *rc, AcmeReal d_theta );
void              MEAPI RCameraRotateElevation( RRender *rc, AcmeReal d_phi );
void              MEAPI RCameraZoom( RRender *rc, AcmeReal dist );
void              MEAPI RCameraUpdate(RRender *rc);
void              MEAPI RCameraSetOrthographicView(RRender* rc, AcmeReal zoomFactor, AcmeReal viewDepth);
void              MEAPI RCameraSetFrontView(RRender* rc, AcmeReal zoomFactor, AcmeReal viewDepth);
void              MEAPI RCameraSetBackView(RRender* rc, AcmeReal zoomFactor, AcmeReal viewDepth);
void              MEAPI RCameraSetLeftView(RRender* rc, AcmeReal zoomFactor, AcmeReal viewDepth);
void              MEAPI RCameraSetRightView(RRender* rc, AcmeReal zoomFactor, AcmeReal viewDepth); 
void              MEAPI RCameraSetTopView(RRender* rc, AcmeReal zoomFactor, AcmeReal viewDepth);
void              MEAPI RCameraSetBottomView(RRender* rc, AcmeReal zoomFactor, AcmeReal viewDepth); 


/* Callbacks */
void              MEAPI RRenderSetActionNCallBackWithText(RRender *rc, int N,
                                                  RButtonPressCallBack func,
                                                  void *userdata, char *text);
void              MEAPI RRenderSetActionNKey(RRender* rc, const unsigned int N,
                                             const char key);
void              MEAPI RRenderSetDown2CallBackWithText(RRender *rc,
                                                RButtonPressCallBack func,
                                                void *userdata, char *text );
void              MEAPI RRenderSetDownCallBackWithText(RRender *rc,
                                               RButtonPressCallBack func,
                                               void *userdata, char *text );
void              MEAPI RRenderSetJoystickCallBackWithText(RRender *rc,
                                                   RJoystickCallBack func,
                                                   void *userdata, char *text );
void              MEAPI RRenderSetLeft2CallBackWithText(RRender *rc,
                                                RButtonPressCallBack func,
                                                void *userdata, char *text );
void              MEAPI RRenderSetLeftCallBackWithText(RRender *rc,
                                               RButtonPressCallBack func,
                                               void *userdata, char *text );
void              MEAPI RRenderSetMouseCallBackWithText(RRender *rc,
                                                RMouseCallBack func,
                                                void *userdata, char *text );
void              MEAPI RRenderSetRight2CallBackWithText(RRender *rc,
                                                 RButtonPressCallBack func,
                                                 void *userdata, char *text );
void              MEAPI RRenderSetRightCallBackWithText(RRender *rc,
                                                RButtonPressCallBack func,
                                                void *userdata, char *text );
void              MEAPI RRenderSetUp2CallBackWithText(RRender *rc,
                                              RButtonPressCallBack func,
                                              void *userdata, char *text );
void              MEAPI RRenderSetUpCallBackWithText(RRender *rc,
                                             RButtonPressCallBack func,
                                             void *userdata, char *text );
/* For backwards compatability */
#define RRenderSetUpCallBack(rc,f,u)       RRenderSetUpCallBackWithText(rc,f,u,0)
#define RRenderSetDownCallBack(rc,f,u)     RRenderSetDownCallBackWithText(rc,f,u,0)
#define RRenderSetLeftCallBack(rc,f,u)     RRenderSetLeftCallBackWithText(rc,f,u,0)
#define RRenderSetRightCallBack(rc,f,u)    RRenderSetRightCallBackWithText(rc,f,u,0)
#define RRenderSetUp2CallBack(rc,f,u)      RRenderSetUp2CallBackWithText(rc,f,u,0)
#define RRenderSetDown2CallBack(rc,f,u)    RRenderSetDown2CallBackWithText(rc,f,u,0)
#define RRenderSetLeft2CallBack(rc,f,u)    RRenderSetLeft2CallBackWithText(rc,f,u,0)
#define RRenderSetRight2CallBack(rc,f,u)   RRenderSetRight2CallBackWithText(rc,f,u,0)
#define RRenderSetMouseCallBack(rc,f,u)    RRenderSetMouseCallBackWithText(rc,f,u,0)
#define RRenderSetJoystickCallBack(rc,f,u) RRenderSetJoystickCallBackWithText(rc,f,u,0)
#define RRenderSetActionNCallBack(rc,n, f,u)  RRenderSetActionNCallBackWithText(rc, n,f,u,0)

/* Performance measurement and UI */
RGraphic*               RPerformanceBarCreateTicks( RRender *rc );
RPerformanceBar * MEAPI RPerformanceBarCreate(RRender *rc);
void              MEAPI RPerformanceBarUpdate(RRender *rc, AcmeReal coltime,
                                              AcmeReal dyntime,
                                              AcmeReal rentime,
                                              AcmeReal idletime);

void              MEAPI RRenderDisplayFps(RRender *rc, AcmeReal fps);

/* Particle Systems */
RParticleSystem * MEAPI RParticleSystemNew(int numParticles,
                                           MeVector3 *positions,
                                           int tex_id, const float color[4],
                                           AcmeReal size ); /* internal */

RParticleSystem * MEAPI RParticleSystemCreate(RRender *rc, int numParticles,
                                              MeVector3 *positions,
                                              char *tex_filename,
                                              const float color[4],
                                              AcmeReal tri_size );

void              MEAPI RParticleSystemDestroy(RParticleSystem *ps);
void              MEAPI RParticleSystemAddToList(RRender *rc,
                                                 RParticleSystem *ps);
void              MEAPI RParticleSystemRemoveFromList(RRender *rc,
                                                      RParticleSystem *ps );

/* MeViewer Menu System */
RMenu*            MEAPI RMenuCreate(RRender* rc, const char* name);
void              MEAPI RMenuDestroy(RMenu* rm);
void              MEAPI RRenderSetDefaultMenu(RRender *rc, RMenu* menu);
void              MEAPI RMenuDisplay(RMenu* rm);

void              MEAPI RMenuAddToggleEntry(RMenu* rm, const char * name,
                                            RMenuToggleCallback func,
                                            MeBool defaultValue);
void              MEAPI RMenuAddValueEntry(RMenu* rm, const char * name,
                                           RMenuValueCallback func,
                                           MeReal hi, MeReal lo,
                                           MeReal increment,
                                           MeReal defaultValue);
void              MEAPI RMenuAddSubmenuEntry(RMenu* rm, const char * name,
                                             RMenu* submenu);

/* Utils */

void              MEAPI RCalculatePortalCoordinates(RRender* rc, AcmeReal width, AcmeReal height,
                                                    int x,int y,AcmeReal* portalx,AcmeReal* portaly);

/* Internal */

void              MEAPI RDisplayBanner(void);
void              MEAPI RDisplayCommandLineHelp(void);

#ifdef __cplusplus
}
#endif

#endif
