#ifndef X_FORCE_FEEDBACK_H
#define X_FORCE_FEEDBACK_H

extern void InitForceFeedback(void* hInstance, void* hWnd);
extern void ExitForceFeedback();
extern void PlayFeedbackEffect( const char* EffectName );
extern void StopFeedbackEffect( const char* EffectName ); // Pass NULL to stop all
extern void PrecacheForceFeedback();
extern void ChangeSpringFeedbackEffect( const char* EffectName, int CenterX, int CenterY );
extern void ChangeBaseParamsForceFeedback( const char* EffectName, int DirectionX, int DirectionY, unsigned int Gain );

extern int GForceFeedbackAvailable;

#endif  //X_FORCE_FEEDBACK_H

