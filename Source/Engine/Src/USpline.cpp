//=============================================================================
// USpline.cpp - Spline Movement/Rotation support
// Copyright 2001 Digital Extremes - All Rights Reserved.
// Confidential.
//
// The tragedy of SplineA is lays in the heart of FName.
//
//=============================================================================

#include "EnginePrivate.h"

UBOOL USpline::CalcSplinePos(FLOAT t, FVector &d, FVector &v, FVector &a)
{
    return 0;
}

FLOAT USpline::smoothDeltaTime(FLOAT dt)
{
    mTotalTime -= maDeltaTimes[mIndex];
    mTotalTime += dt;

    maDeltaTimes[mIndex] = dt;
    mIndex++;
    if (mIndex >= UCONST_mMaxTimes)
        mIndex = 0;

    mCnt++;

    if (mCnt < UCONST_mMaxTimes)
        return mTotalTime/mCnt;
    else
        return mTotalTime/UCONST_mMaxTimes;

    return 0.f;
}

// native final simulated function bool   
// InitSplinePath(float t0, vector  d0, vector  v0, float t1, vector  d1, vector  v1);
void USpline::execInitSplinePath( FFrame& Stack, RESULT_DECL )
{
	guard(USpline::execInitSplinePath);
    P_GET_FLOAT(t0);
    P_GET_VECTOR(d0);
    P_GET_VECTOR(v0);
    P_GET_FLOAT(t1);
    P_GET_VECTOR(d1);
    P_GET_VECTOR(v1);
	P_FINISH;

    // Uses current loc & vel with function inputs to calculate the spline co-efficients
    if (t0 == t1)
    {
        debugf( NAME_Error, TEXT("USpline::execInitSplinePath() - Invalid time range (%f, %f)"), t0, t1 );
        *(DWORD*)Result = 0;
        return;
    }

    SplineA =  v1 +   v0 - 2*d1 + 2*d0;
    SplineB = -v1 - 2*v0 + 3*d1 - 3*d0;
    SplineC = v0;
    SplineD = d0;

    mStartT = t0;
    mEndT   = t1;

    mbInit = 1;
    mTime  = 0.f;
    mTotalTime = 0.f;
    mIndex = 0;
    mbRotatorSpline = 0;
    mCnt = 0;

    *(DWORD*)Result = 1;

	unguard;
}

// native final simulated function bool   
// InitSplineRot( float t0, rotator d0, rotator v0, float t1, rotator d1, rotator v1);
void USpline::execInitSplineRot( FFrame& Stack, RESULT_DECL )
{
	guard(USpline::execInitSplineRot);
    P_GET_FLOAT(t0);
    P_GET_ROTATOR(d0);
    P_GET_ROTATOR(v0);
    P_GET_FLOAT(t1);
    P_GET_ROTATOR(d1);
    P_GET_ROTATOR(v1);
	P_FINISH;

    // Uses current loc & vel with function inputs to calculate the spline co-efficients
    if (t0 == t1)
    {
        debugf( NAME_Error, TEXT("USpline::execInitSplineRot() - Invalid time range (%f, %f)"), t0, t1 );
        *(DWORD*)Result = 0;
        return;
    }

/* TODO: revisit SplineE -- this keeps the menu intro/outro camera spline interpolation from working
    // Rotate through the smallest angle
    // TODO: fix native code to clamp rotations all the time
    // Make the rotation components +ve
    d0.Pitch = (d0.Pitch + 65536) % 65536;
    d0.Yaw   = (d0.Yaw   + 65536) % 65536;
    d0.Roll  = (d0.Pitch + 65536) % 65536;
    d1.Pitch = (d1.Pitch + 65536) % 65536;
    d1.Yaw   = (d1.Yaw   + 65536) % 65536;
    d1.Roll  = (d1.Pitch + 65536) % 65536;

    FRotator diff = d1 - d0;

    // huh?
    if (diff.Pitch > 32768)
        d1.Pitch -= 65536;
    else if (diff.Pitch < -32768)
        d0.Pitch -= 65536;
    if (diff.Yaw > 32768)
        d1.Yaw -= 65536;
    else if (diff.Yaw < -32768)
        d0.Yaw -= 65536;
    if (diff.Roll > 32768)
        d1.Roll -= 65536;
    else if (diff.Roll < -32768)
        d0.Roll -= 65536;
*/
    SplineE =    v1 +   v0 - 2*d1 + 2*d0;
    SplineF = -1*v1 - 2*v0 + 3*d1 - 3*d0;
    SplineG = v0;
    SplineH = d0;

    mStartT = t0;
    mEndT   = t1;

    mbInit = 1;
    mTime  = 0.f;
    mTotalTime = 0.f;
    mIndex = 0;
    mbRotatorSpline = 1;
    mCnt = 0;

    *(DWORD*)Result = 1;

	unguard;
}

// native final simulated function bool   
// NextSplinePos(float dt, out vector  d, out vector  v, out vector  a, out float outdt);
void USpline::execNextSplinePos( FFrame& Stack, RESULT_DECL )
{
	guard(USpline::execNextSplinePos);
    P_GET_FLOAT(dt);
    P_GET_VECTOR_REF(d);
    P_GET_VECTOR_REF(v);
    P_GET_VECTOR_REF(a);
    P_GET_FLOAT_REF(outdt);
    P_GET_UBOOL_OPTX(bSmoothDt, 1); // gam
    P_GET_UBOOL_OPTX(bAccumDeltas, 1); 
	P_FINISH;

    if (mStartT == mEndT)
    {
        debugf( NAME_Error, TEXT("USpline::execNextSplinePos() - Invalid time range (%f, %f)"), mStartT, mEndT );
        *(DWORD*)Result = 0;
        return;
    }

    if (!mbInit)
    {
        debugf( NAME_Error, TEXT("USpline::execNextSplinePos() - Spline not initialized") );
        *(DWORD*)Result = 0;
        return;
    }

    if (bSmoothDt)
        *outdt = smoothDeltaTime(dt);
    else
        *outdt = dt;

    if (bAccumDeltas)
        mTime += *outdt;
    else
        mTime = *outdt;

    FLOAT t = (mTime - mStartT) / (mEndT - mStartT);
    
    if (Abs(t-1.f) < KINDA_SMALL_NUMBER) //amb: hack fix
        t = 1.f;

    if (t>1.f || t<0.f)
    {
        debugf( NAME_Error, TEXT("USpline::execNextSplinePos() - Invalid time t (%f)"), t );
        *(DWORD*)Result = 0;
        return;
    }

    *d = ((  SplineA*t +   SplineB)*t + SplineC)*t + SplineD;
    *v =  (3*SplineA*t + 2*SplineB)*t + SplineC;
    *a =   6*SplineA*t + 2*SplineB;

    *(DWORD*)Result = 1;

	unguard;
}

// native final simulated function bool   
// NextSplineRot(float dt, out rotator d, out rotator v, out rotator a, out float outdt);
void USpline::execNextSplineRot( FFrame& Stack, RESULT_DECL )
{
	guard(USpline::execNextSplineRot);
    P_GET_FLOAT(dt);
    P_GET_ROTATOR_REF(d);
    P_GET_ROTATOR_REF(v);
    P_GET_ROTATOR_REF(a);
    P_GET_FLOAT_REF(outdt);
    P_GET_UBOOL_OPTX(bSmoothDt, 1); // gam
    P_GET_UBOOL_OPTX(bAccumDeltas, 1); 
	P_FINISH;

    if (mStartT == mEndT)
    {
        debugf( NAME_Error, TEXT("USpline::execNextSplineRot() - Invalid time range (%f, %f)"), mStartT, mEndT );
        *(DWORD*)Result = 0;
        return;
    }

    if (!mbInit)
    {
        debugf( NAME_Error, TEXT("USpline::execNextSplineRot() - Spline not initialized") );
        *(DWORD*)Result = 0;
        return;
    }

    if (bSmoothDt)
        *outdt = smoothDeltaTime(dt);
    else
        *outdt = dt;

    if (bAccumDeltas)
        mTime += *outdt;
    else
        mTime = *outdt;

    FLOAT t = (mTime - mStartT) / (mEndT - mStartT);

    if (Abs(t-1.f) < KINDA_SMALL_NUMBER) //amb: hack fix
        t = 1.f;
    
    if (t>1.f || t<0.f)
    {
        debugf( NAME_Error, TEXT("USpline::execNextSplineRot() - Invalid time t (%f)"), t );
        *(DWORD*)Result = 0;
        return;
    }

    *d = ((  SplineE*t +   SplineF)*t + SplineG)*t + SplineH;
    *v =  (3*SplineE*t + 2*SplineF)*t + SplineG;
    *a =   6*SplineE*t + 2*SplineF;

    *(DWORD*)Result = 1;

	unguard;
}

IMPLEMENT_CLASS(USpline);

