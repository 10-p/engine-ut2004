class HudCDoubleDomination extends HudCTeamDeathMatch;

var() float REDtmpPosX, REDtmpPosY, REDtmpScaleX, REDtmpScaleY;
var() float BLUEtmpPosX, BLUEtmpPosY, BLUEtmpScaleX, BLUEtmpScaleY;

var() float NewOffsetX;

enum EPointState
{
    PS_Neutral,
    PS_HeldFriendly,
    PS_HeldEnemy,
    PS_Disabled,
};

struct FPointWidget
{
    var EPointState PointState;
    var SpriteWidget Widgets[4];
};

var() SpriteWidget SymbolGB[2];
var() SpriteWidget DomPoints[2];
var() FPointWidget Points[2];

var() SpriteWidget FlagHeldWidgets[2];
var transient xDomPoint DP1;
var transient xDomPoint DP2;

simulated function PostBeginPlay()
{
	Super.PostBeginPlay();
	SetTimer(1.0, True);
}

simulated function SpaceOutTeamInfo()
{
	local int i;

	NewOffsetX = -NewOffsetX;

	for(i = 0; i < 2; i++ )
	{
		if(i==1)
			NewOffsetX = -NewOffsetX;

		TeamScoreBackground[i].PosX = default.TeamScoreBackground[i].PosX + NewOffSetX;
		TeamScoreBackgroundDisc[i].PosX = default.TeamScoreBackgroundDisc[i].PosX + NewOffSetX; 
		ScoreTeam[i].PosX = default.ScoreTeam[i].PosX + NewOffSetX;
		TeamSymbols[i].PosX = default.TeamSymbols[i].PosX + NewOffSetX;
	}
}
// Alpha Pass ==================================================================================
simulated function ShowTeamScorePassA(Canvas C)
{
	local int i;


    Super.ShowTeamScorePassA(C);
	SpaceOutTeamInfo();

	if ( bShowPoints )
	{
		DrawSpriteWidget (C, SymbolGB[0]);
		DrawSpriteWidget (C, SymbolGB[1]);

		DrawSpriteWidget (C, DomPoints[0]);
		DrawSpriteWidget (C, DomPoints[1]);

		DomPoints[0].Tints[TeamIndex] = Points[0].Widgets[Points[0].PointState].Tints[TeamIndex];
		DomPoints[1].Tints[TeamIndex] = Points[1].Widgets[Points[1].PointState].Tints[TeamIndex];

		DomPoints[0].Tints[TeamIndex].A = 120;
		DomPoints[1].Tints[TeamIndex].A = 120;

		Points[0].Widgets[Points[0].PointState].PosY = 0.045*HUDScale;
		Points[1].Widgets[Points[1].PointState].PosY = 0.045*HUDScale;

		DrawSpriteWidget (C, Points[0].Widgets[Points[0].PointState]);
		DrawSpriteWidget (C, Points[1].Widgets[Points[1].PointState]);

		if ( DP1 != None )
		{

			C.DrawColor = HudColorHighLight;//Points[0].Widgets[Points[0].PointState].Tints[TeamIndex];
			Draw2DLocationDot(C, DP1.Location,0.5 - REDtmpPosX*HUDScale, REDtmpPosY*HUDScale, REDtmpScaleX*HUDScale, REDtmpScaleY*HUDScale);
		}
		if ( DP2 != None )
		{
			C.DrawColor = HudColorHighLight; //Points[1].Widgets[Points[1].PointState].Tints[TeamIndex];
			Draw2DLocationDot(C, DP2.Location,0.5 + BLUEtmpPosX*HUDScale, BLUEtmpPosY*HUDScale, BLUEtmpScaleX*HUDScale, BLUEtmpScaleY*HUDScale);
		}


		if ( PlayerOwner.GameReplicationInfo == None )
			return;
		for (i = 0; i < 2; i++)
		{
			if ( Points[i].PointState == PS_HeldEnemy )
				DrawSpriteWidget (C, FlagHeldWidgets[i]);
		}
	}

}

simulated function ShowVersusIcon(Canvas C) {}

simulated function UpdateTeamHud()
{
	local NavigationPoint N;

    if (DP1 == None || DP2 == None)
    {
        for (N = Level.NavigationPointList; N != None; N = N.NextNavigationPoint)
        {
	        if (!N.IsA('xDomPoint'))
                continue;

	        if (N.IsA('xDomPointA') && DP1 == None)
                DP1 = xDomPoint(N);
            else
                DP2 = xDomPoint(N);
        }
    }

    // Point A state
    if (!DP1.bControllable)
        Points[0].PointState = PS_Disabled;
    else if (DP1.ControllingTeam == None)
        Points[0].PointState = PS_Neutral;
	else if ( PlayerOwner.PlayerReplicationInfo.Team == None )
	{
		if ( DP1.ControllingTeam.TeamIndex == 0 )
			Points[0].PointState = PS_HeldFriendly; 
		else
			Points[0].PointState = PS_HeldEnemy;
	}
    else if (DP1.ControllingTeam == PawnOwnerPRI.Team)
        Points[0].PointState = PS_HeldFriendly;
	else
        Points[0].PointState = PS_HeldEnemy;
        
    // Point B state
    if (!DP2.bControllable)
        Points[1].PointState = PS_Disabled;
    else if (DP2.ControllingTeam == None)
        Points[1].PointState = PS_Neutral;
	else if ( PlayerOwner.PlayerReplicationInfo.Team == None )
	{
		if ( DP2.ControllingTeam.TeamIndex == 0 )
			Points[1].PointState = PS_HeldFriendly;
		else
			Points[1].PointState = PS_HeldEnemy;
	}
    else if (DP2.ControllingTeam == PawnOwnerPRI.Team)
	    Points[1].PointState = PS_HeldFriendly;
	else
		Points[1].PointState = PS_HeldEnemy;

	Super.UpdateTeamHUD();
}

function Timer()
{
    local NavigationPoint N;

	Super.Timer();

	if ((PlayerOwner == None) || (PawnOwner == None) || (PawnOwnerPRI == None))
		return;

    if (DP1 == None || DP2 == None)
    {
        for (N = Level.NavigationPointList; N != None; N = N.NextNavigationPoint)
        {
	        if (!N.IsA ('xDomPoint'))
                continue;

	        if (N.IsA('xDomPointA'))
                DP1 = xDomPoint(N);
            else
                DP2 = xDomPoint(N);
        }
    }

    if (DP1.ControllingTeam == DP2.ControllingTeam)
    {
        if (DP1.ControllingTeam != None)
        {
            if (PawnOwnerPRI.Team.TeamIndex == DP1.ControllingTeam.TeamIndex)
		        PlayerOwner.ReceiveLocalizedMessage (class'xDomMessage', 0);
            else
		        PlayerOwner.ReceiveLocalizedMessage (class'xDomMessage', 1);
        }
    }
}

defaultproperties
{
	REDtmpPosX=0.063
	REDtmpPosY=0.035
	REDtmpScaleX=0.025
	REDtmpScaleY=0.035

	BLUEtmpPosX=0.049
	BLUEtmpPosY=0.035
	BLUEtmpScaleX=0.025
	BLUEtmpScaleY=0.035

	NewOffsetX=0.058

	SymbolGB(0)=(WidgetTexture=Texture'HudContent.Generic.HUD',PosX=0.5,PosY=0.0,OffsetX=-32,OffsetY=7,DrawPivot=DP_UpperRight,RenderStyle=STY_Alpha,TextureCoords=(X1=119,Y1=258,X2=173,Y2=313),TextureScale=0.6,ScaleMode=SM_Right,Scale=1.000000,Tints[0]=(G=255,R=255,B=255,A=255),Tints[1]=(G=255,R=255,B=255,A=255))
	SymbolGB(1)=(WidgetTexture=Texture'HudContent.Generic.HUD',PosX=0.5,PosY=0.0,OffsetX=32,OffsetY=7,DrawPivot=DP_UpperLeft,RenderStyle=STY_Alpha,TextureCoords=(X1=119,Y1=258,X2=173,Y2=313),TextureScale=0.6,ScaleMode=SM_Right,Scale=1.000000,Tints[0]=(G=255,R=255,B=255,A=255),Tints[1]=(G=255,R=255,B=255,A=255))

	FlagHeldWidgets(0)=(WidgetTexture=Material'HudContent.Generic.HUDPulse',PosX=0.5,PosY=0.0,OffsetX=-105,OffsetY=40,DrawPivot=DP_UpperRight,RenderStyle=STY_Alpha,TextureCoords=(X1=341,Y1=211,X2=366,Y2=271),TextureScale=0.3,ScaleMode=SM_Right,Scale=1.000000,Tints[0]=(G=255,R=255,B=255,A=255),Tints[1]=(G=255,R=255,B=255,A=255))
    FlagHeldWidgets(1)=(WidgetTexture=Material'HudContent.Generic.HUDPulse',PosX=0.5,PosY=0.0,OffsetX=105,OffsetY=40,DrawPivot=DP_UpperLeft,RenderStyle=STY_Alpha,TextureCoords=(X1=341,Y1=211,X2=366,Y2=271),TextureScale=0.3,ScaleMode=SM_Right,Scale=1.000000,Tints[0]=(G=255,R=255,B=255,A=255),Tints[1]=(G=255,R=255,B=255,A=255))


	DomPoints(0)=(WidgetTexture=Material'HudContent.Reticles.rotDomRing',PosX=0.5,PosY=0.0,OffsetX=-55,OffsetY=5,DrawPivot=DP_UpperRight,RenderStyle=STY_Alpha,TextureCoords=(X1=0,Y1=0,X2=127,Y2=127),TextureScale=0.3,ScaleMode=SM_Right,Scale=1.000000,Tints[0]=(G=255,R=255,B=255,A=255),Tints[1]=(G=255,R=255,B=255,A=255))
	DomPoints(1)=(WidgetTexture=Material'HudContent.Reticles.rotDomRing',PosX=0.5,PosY=0.0,OffsetX=55,OffsetY=5,DrawPivot=DP_UpperLeft,RenderStyle=STY_Alpha,TextureCoords=(X1=0,Y1=0,X2=127,Y2=127),TextureScale=0.3,ScaleMode=SM_Right,Scale=1.000000,Tints[0]=(G=255,R=255,B=255,A=255),Tints[1]=(G=255,R=255,B=255,A=255))

	Points(0)=(PointState=PS_Neutral,Widgets[0]=(WidgetTexture=Material'HudContent.Generic.HUD',TextureCoords=(X1=0,Y1=451,X2=63,Y2=511),TextureScale=0.33,DrawPivot=DP_MiddleMiddle,OffsetX=-108,OffsetY=-5,PosX=0.5,PosY=0.045,ScaleMode=SM_None,Scale=1.0,RenderStyle=STY_Alpha,Tints[0]=(R=195,G=195,B=195,A=255),Tints[1]=(R=195,G=195,B=195,A=150)),Widgets[1]=(WidgetTexture=Material'HudContent.Generic.HUD',TextureCoords=(X1=0,Y1=451,X2=63,Y2=511),TextureScale=0.33,DrawPivot=DP_MiddleMiddle,OffsetX=-108,OffsetY=-5,PosX=0.5,PosY=0.045,ScaleMode=SM_None,Scale=1.0,RenderStyle=STY_Alpha,Tints[0]=(B=32,G=32,R=255,A=200),Tints[1]=(R=0,B=255,G=128,A=255)),Widgets[2]=(WidgetTexture=Material'HudContent.Generic.HUD',TextureCoords=(X1=0,Y1=451,X2=63,Y2=511),TextureScale=0.33,DrawPivot=DP_MiddleMiddle,OffsetX=-108,OffsetY=-5,PosX=0.5,PosY=0.045,ScaleMode=SM_None,Scale=1.0,RenderStyle=STY_Alpha,Tints[0]=(R=0,B=255,G=128,A=255),Tints[1]=(B=32,G=32,R=255,A=200)),Widgets[3]=(WidgetTexture=Material'HudContent.Generic.HUD',TextureCoords=(X1=0,Y1=451,X2=63,Y2=511),TextureScale=0.33,DrawPivot=DP_MiddleMiddle,OffsetX=-108,OffsetY=-5,PosX=0.5,PosY=0.045,ScaleMode=SM_None,Scale=1.0,RenderStyle=STY_Alpha,Tints[0]=(R=195,G=195,B=195,A=55),Tints[1]=(R=195,G=195,B=195,A=55)))
    Points(1)=(PointState=PS_Neutral,Widgets[0]=(WidgetTexture=Material'HudContent.Generic.HUD',TextureCoords=(X1=64,Y1=451,X2=125,Y2=511),TextureScale=0.33,DrawPivot=DP_MiddleMiddle,OffsetX=110,OffsetY=-4,PosX=0.5,PosY=0.045,ScaleMode=SM_None,Scale=1.0,RenderStyle=STY_Alpha,Tints[0]=(R=195,G=195,B=195,A=255),Tints[1]=(R=195,G=195,B=195,A=150)),Widgets[1]=(WidgetTexture=Material'HudContent.Generic.HUD',TextureCoords=(X1=64,Y1=451,X2=125,Y2=511),TextureScale=0.33,DrawPivot=DP_MiddleMiddle,OffsetX=110,OffsetY=-4,PosX=0.5,PosY=0.045,ScaleMode=SM_None,Scale=1.0,RenderStyle=STY_Alpha,Tints[0]=(B=32,G=32,R=255,A=200),Tints[1]=(R=0,B=255,G=128,A=255)),Widgets[2]=(WidgetTexture=Material'HudContent.Generic.HUD',TextureCoords=(X1=64,Y1=451,X2=125,Y2=511),TextureScale=0.33,DrawPivot=DP_MiddleMiddle,OffsetX=110,OffsetY=-4,PosX=0.5,PosY=0.045,ScaleMode=SM_None,Scale=1.0,RenderStyle=STY_Alpha,Tints[0]=(R=0,B=255,G=128,A=255),Tints[1]=(B=32,G=32,R=255,A=200)),Widgets[3]=(WidgetTexture=Material'HudContent.Generic.HUD',TextureCoords=(X1=64,Y1=451,X2=125,Y2=511),TextureScale=0.33,DrawPivot=DP_MiddleMiddle,OffsetX=110,OffsetY=-4,PosX=0.5,PosY=0.045,ScaleMode=SM_None,Scale=1.0,RenderStyle=STY_Alpha,Tints[0]=(R=195,G=195,B=195,A=55),Tints[1]=(R=195,G=195,B=195,A=55)))
}