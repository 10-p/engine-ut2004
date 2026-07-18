//=============================================================================
// Copyright 2001 Digital Extremes - All Rights Reserved.
// Confidential.
//=============================================================================

#include "XInterface.h"

IMPLEMENT_CLASS(AHudBase);

void AHudBase::execDrawSpriteWidget( FFrame& Stack, RESULT_DECL )
{
	guard(AHudBase::execDrawSpriteWidget);

    P_GET_OBJECT(UCanvas,Canvas);
    P_GET_STRUCT_REF(FSpriteWidget,W)
	P_FINISH;

	DrawSpriteWidget(Canvas, W);
	unguardexec;
}

void AHudBase::DrawSpriteWidget( UCanvas* Canvas, FSpriteWidget* Widget )
{
	guard(AHudBase::DrawSpriteWidget);

    if( !Widget || !Widget->WidgetTexture )
		return;

    float TotalScaleX, TotalScaleY;
    float TextureX1, TextureY1;
    float TextureDX, TextureDY;
    float ScreenDX, ScreenDY;
    float ScreenOffsetX, ScreenOffsetY;
    float ScreenX, ScreenY;

    TotalScaleX = Widget->TextureScale * HudCanvasScale * ResScaleX * HudScale;
    TotalScaleY = Widget->TextureScale * HudCanvasScale * ResScaleY * HudScale;

    TextureX1 = Widget->TextureCoords.X1;
    TextureY1 = Widget->TextureCoords.Y1;

    TextureDX = Widget->TextureCoords.X2 - Widget->TextureCoords.X1 + 1.0f;
    TextureDY = Widget->TextureCoords.Y2 - Widget->TextureCoords.Y1 + 1.0f;

    if ((TextureDX == 0.0) || (TextureDY == 0.0))
        return;

    ScreenDX = Abs(TextureDX) * TotalScaleX;
    ScreenDY = Abs(TextureDY) * TotalScaleY;

    ScreenOffsetX = Widget->OffsetX * TotalScaleX;
    ScreenOffsetY = Widget->OffsetY * TotalScaleY;

    ScreenX = (Widget->PosX * HudCanvasScale * Canvas->SizeX) + (((1.0f - HudCanvasScale) * 0.5f) * Canvas->SizeX);
    ScreenY = (Widget->PosY * HudCanvasScale * Canvas->SizeY) + (((1.0f - HudCanvasScale) * 0.5f) * Canvas->SizeY);

    ScreenX += ScreenOffsetX;
    ScreenY += ScreenOffsetY;

    CalcPivotCoords( (EDrawPivot)(Widget->DrawPivot), ScreenX, ScreenY, ScreenDX, ScreenDY );

    //if ((Widget->Scale < 0.0f) || (Widget->Scale > 1.0f))
    //    debugf( NAME_Error, TEXT("DrawSpriteWidget() -- Widget->Scale out of range"));
	Widget->Scale = Clamp<FLOAT>(Widget->Scale,0.f,1.f);

    switch (Widget->ScaleMode)
    {
        case SM_None:
            break;

        case SM_Up:
            ScreenY += (1.0f - Widget->Scale) * ScreenDY;
            ScreenDY *= Widget->Scale;
            TextureY1 += (1.0f - Widget->Scale) * TextureDY;
            TextureDY *= Widget->Scale;
            break;

        case SM_Down:
            ScreenDY *= Widget->Scale;
            TextureDY *= Widget->Scale;
            break;

        case SM_Left:
            ScreenX += (1.0f - Widget->Scale) * ScreenDX;
            ScreenDX *= Widget->Scale;
            TextureX1 += (1.0f - Widget->Scale) * TextureDX;
            TextureDX *= Widget->Scale;
            break;

        case SM_Right:
            ScreenDX *= Widget->Scale;
            TextureDX *= Widget->Scale;
            break;
    }

    Canvas->CurX = ScreenX;
	Canvas->CurY = ScreenY;
	Canvas->Style = Widget->RenderStyle;
    Canvas->Color = Widget->Tints[TeamIndex];

    if ((PassStyle != STY_None) && (Widget->RenderStyle != PassStyle))
        debugf(NAME_Error,TEXT("DrawSpriteWidget() draw style mis-match (%d was supposed to be %d)"), Widget->RenderStyle, PassStyle );

	Canvas->DrawTile
    (
        Widget->WidgetTexture,
        Canvas->OrgX+Canvas->CurX,
        Canvas->OrgY+Canvas->CurY,
        ScreenDX,
        ScreenDY,
        TextureX1,
        TextureY1,
        TextureDX,
        TextureDY,
        Canvas->Z,
        Canvas->Color.Plane(),
		FPlane(0.0f,0.0f,0.0f,0.0f)
    );

    unguard;
}

//native simulated function DrawNumericWidget (Canvas C, out NumericWidget W, out DigitSet D);
void AHudBase::execDrawNumericWidget( FFrame& Stack, RESULT_DECL )
{
	guard(AHudBase::execDrawNumericWidget);

    P_GET_OBJECT(UCanvas,Canvas);
    P_GET_STRUCT_REF(FNumericWidget,W)
    P_GET_STRUCT_REF(FDigitSet,D)
	P_FINISH;

	DrawNumericWidget( Canvas, W, D );
	unguardexec;
}

void AHudBase::DrawNumericWidget( UCanvas* Canvas, FNumericWidget* Widget, FDigitSet* D )
{
	guard(AHudBase::DrawNumericWidget);

    int Digits [32];

    if( !D->DigitTexture )
    {
        debugf( NAME_Warning, TEXT("DrawNumericWidget called with no texture!") );
        return;
    }

    int DigitCount;
    float ScreenX, ScreenY;

    float TotalScaleX, TotalScaleY;

    TotalScaleX = Widget->TextureScale * HudCanvasScale * ResScaleX * HudScale;
    TotalScaleY = Widget->TextureScale * HudCanvasScale * ResScaleY * HudScale;

    int AbsValue;
    int Digit;

    float TextureDX, TextureDY;
    float DigitDX, DigitDY;

    float ScreenDX, ScreenDY;

    float ScreenOffsetX, ScreenOffsetY;

    int MinDigitCount;

    AbsValue = Abs(Widget->Value);
    DigitCount = 0;

    TextureDX = 0;
    TextureDY = D->TextureCoords[0].Y2 - D->TextureCoords[0].Y1 + 1.0f;

    do
    {
        Digit = AbsValue % 10;
        Digits[DigitCount] = Digit;
        AbsValue /= 10;
        DigitCount++;

        DigitDX = D->TextureCoords[Digit].X2 - D->TextureCoords[Digit].X1 + 1.0f;
        DigitDY = D->TextureCoords[Digit].Y2 - D->TextureCoords[Digit].Y1 + 1.0f;

        if (DigitDY != TextureDY)
            debugf(NAME_Error,TEXT("DrawNumericWidget() -- DigitSet with uneven height detected [%d]"), Digit );

        TextureDX += DigitDX;

    } while (AbsValue != 0);

    if (Widget->bPadWithZeroes)
    {
        if (Widget->Value < 0)
            MinDigitCount = Widget->MinDigitCount - 1;
        else
            MinDigitCount = Widget->MinDigitCount;

        Digit = 0;

        DigitDX = D->TextureCoords[Digit].X2 - D->TextureCoords[Digit].X1 + 1.0f;
        DigitDY = D->TextureCoords[Digit].Y2 - D->TextureCoords[Digit].Y1 + 1.0f;

        while (DigitCount < MinDigitCount)
        {
            Digits[DigitCount] = Digit;
            DigitCount++;
            TextureDX += DigitDX;
        }

        if (Widget->Value < 0)
        {
            Digit = 10; // The 10th digit is the negative sign
            Digits[DigitCount] = Digit;
            DigitCount++;

            DigitDX = D->TextureCoords[Digit].X2 - D->TextureCoords[Digit].X1 + 1.0f;
            DigitDY = D->TextureCoords[Digit].Y2 - D->TextureCoords[Digit].Y1 + 1.0f;

            if (DigitDY != TextureDY)
                debugf(NAME_Error,TEXT("DrawNumericWidget() -- DigitSet with uneven height detected [-]"));

            TextureDX += DigitDX;
        }
    }
    else
    {
        MinDigitCount = Widget->MinDigitCount;

        if (Widget->Value < 0)
        {
            Digit = 10; // The 10th digit is the negative sign
            Digits[DigitCount] = Digit;
            DigitCount++;

            DigitDX = D->TextureCoords[Digit].X2 - D->TextureCoords[Digit].X1 + 1.0f;
            DigitDY = D->TextureCoords[Digit].Y2 - D->TextureCoords[Digit].Y1 + 1.0f;

            if (DigitDY != TextureDY)
                debugf(NAME_Error,TEXT("DrawNumericWidget() -- DigitSet with uneven height detected [-]"));

            TextureDX += DigitDX;
        }

        Digit = 0;

        DigitDX = D->TextureCoords[Digit].X2 - D->TextureCoords[Digit].X1 + 1;
        DigitDY = D->TextureCoords[Digit].Y2 - D->TextureCoords[Digit].Y1 + 1;

        while (DigitCount < MinDigitCount)
        {
            Digits[DigitCount] = -1;
            DigitCount++;
            TextureDX += DigitDX;
        }
    }

    ScreenDX = TextureDX * TotalScaleX;
    ScreenDY = TextureDY * TotalScaleY;

    ScreenOffsetX = Widget->OffsetX * TotalScaleX;
    ScreenOffsetY = Widget->OffsetY * TotalScaleY;

    ScreenX = (Widget->PosX * HudCanvasScale * Canvas->SizeX) + (((1.0f - HudCanvasScale) * 0.5f) * Canvas->SizeX);
    ScreenY = (Widget->PosY * HudCanvasScale * Canvas->SizeY) + (((1.0f - HudCanvasScale) * 0.5f) * Canvas->SizeY);

    ScreenX += ScreenOffsetX;
    ScreenY += ScreenOffsetY;

    CalcPivotCoords( (EDrawPivot)(Widget->DrawPivot), ScreenX, ScreenY, ScreenDX, ScreenDY );

	Canvas->Style = Widget->RenderStyle;

    if ((PassStyle != STY_None) && (Widget->RenderStyle != PassStyle))
        debugf(NAME_Error,TEXT("DrawNumericWidget() draw style mis-match (%d was supposed to be %d)"), Widget->RenderStyle, PassStyle );

    Canvas->Color = Widget->Tints[TeamIndex];

    do
    {
        DigitCount--;

        int Digit = Digits[DigitCount];

        if (Digit >= 0)
        {
            float DigitDX = D->TextureCoords[Digit].X2 - D->TextureCoords[Digit].X1 + 1.0f;
            float DigitDY = D->TextureCoords[Digit].Y2 - D->TextureCoords[Digit].Y1 + 1.0f;

            float ScreenDX = DigitDX * TotalScaleX;
            float ScreenDY = DigitDY * TotalScaleY;

            Canvas->CurX = ScreenX;
            Canvas->CurY = ScreenY;

            Canvas->DrawTile
            (
                D->DigitTexture,
                Canvas->OrgX+Canvas->CurX,
                Canvas->OrgY+Canvas->CurY,
                ScreenDX,
                ScreenDY,
                D->TextureCoords[Digit].X1,
                D->TextureCoords[Digit].Y1,
                DigitDX,
                DigitDY,
                Canvas->Z,
                Canvas->Color.Plane(),
	            FPlane(0.0f,0.0f,0.0f,0.0f)
            );

            ScreenX += ScreenDX;
        }
        else
        {
            float DigitDX = D->TextureCoords[0].X2 - D->TextureCoords[0].X1 + 1.0f;
            float ScreenDX = DigitDX * TotalScaleX;
            ScreenX += ScreenDX;
        }

    } while (DigitCount != 0);

    unguard;
}

UBOOL AHudBase::MemoryIsZero( const void* Memory, size_t Size )
{
	guard(AHudBase::MemoryIsZero);

    const BYTE* Ptr = (const BYTE*)Memory;

    while( Size > 0 )
    {
        if( *Ptr != 0x00 )
            return( false );

        Size--;
        Ptr++;
    }    

    return( true );

    unguard;
}

void AHudBase::CalcPivotCoords( EDrawPivot DrawPivot, FLOAT& ScreenX, FLOAT& ScreenY, FLOAT ScreenDX, FLOAT ScreenDY )
{
	guardSlow(AHudBase::CalcPivotCoords);

    switch (DrawPivot)
    {
        case DP_UpperLeft:
            break;

        case DP_UpperMiddle:
            ScreenX -= ScreenDX * 0.5f;
            break;

        case DP_UpperRight:
            ScreenX -= ScreenDX;
            break;

        case DP_MiddleRight:
            ScreenX -= ScreenDX;
            ScreenY -= ScreenDY * 0.5f;
            break;

        case DP_LowerRight:
            ScreenX -= ScreenDX;
            ScreenY -= ScreenDY;
            break;

        case DP_LowerMiddle:
            ScreenX -= ScreenDX * 0.5f;
            ScreenY -= ScreenDY;
            break;

        case DP_LowerLeft:
            ScreenY -= ScreenDY;
            break;

        case DP_MiddleLeft:
            ScreenY -= ScreenDY * 0.5f;
            break;

        case DP_MiddleMiddle:
            ScreenX -= ScreenDX * 0.5f;
            ScreenY -= ScreenDY * 0.5f;
            break;

        default:
            debugf( NAME_Error, TEXT("CalcPivotCoords() -- Unknown DrawPivot"));
            break;
    }

    unguardSlow;
}
