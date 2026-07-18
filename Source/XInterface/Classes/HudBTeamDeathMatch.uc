class HudBTeamDeathMatch extends HudBDeathMatch;

var() NumericWidget ScoreTeam[2];
var() NumericWidget totalLinks;

var() NumericWidget Score;

var() SpriteWidget LTeamHud[3];
var() SpriteWidget RTeamHud[3];

var() SpriteWidget TeamSymbols[2];
var() int		Links;

var() Color CarrierTextColor1;
var() Color CarrierTextColor2;
var() Color CarrierTextColor3;
var() String CarriersName, CarriersLocation;
var() float CNPosX, CNPosY;

var localized string LinkEstablishedMessage;

simulated function DrawSpectatingHud (Canvas C)
{
	Super.DrawSpectatingHud(C);

	if ( (PlayerOwner == None) || (PlayerOwner.PlayerReplicationInfo == None)
		|| !PlayerOwner.PlayerReplicationInfo.bOnlySpectator )
		return;

	UpdateRankAndSpread(C);
	ShowTeamScorePassA(C);
	ShowTeamScorePassC(C);
	UpdateTeamHUD();
}

simulated function Tick(float deltaTime)
{
	Super.Tick(deltaTime);

	if (Links >0)
	{
		TeamLinked = true;
		pulseWidget(deltaTime, 255, 0, RHud2[3], TeamIndex, 1, 2000);
	}
	else
	{
		TeamLinked = false;
		RHud2[3].Tints[TeamIndex].A = 255;
	}
}

simulated function showLinks()
{
	local Inventory Inv;
	Inv = PawnOwner.FindInventoryType(class'LinkGun');

    if (Inv != None)
		Links = LinkGun(Inv).Links;
	else
		Links = 0;
}

simulated function drawLinkText(Canvas C)
{
	text = LinkEstablishedMessage;

	C.Font = LoadLevelActionFont();
	C.DrawColor = LevelActionFontColor;

	C.DrawColor = LevelActionFontColor;
	C.Style = ERenderStyle.STY_Alpha;

	C.DrawScreenText (text, 1, 0.81, DP_LowerRight);
}

simulated function UpdateRankAndSpread(Canvas C)
{
	// making sure that the Rank and Spread dont get drawn in other gametypes
}

simulated function DrawHudPassA (Canvas C)
{
    Super.DrawHudPassA (C);
	UpdateRankAndSpread(C);
	ShowTeamScorePassA(C);
	if( bShowWeaponInfo )
	{
		if (Links >0)
	        DrawNumericWidget (C, totalLinks, DigitsBig);
		totalLinks.value = Links;
	}
}

simulated function ShowTeamScorePassA(Canvas C)
{
	if ( bShowPoints )
	{
    	DrawSpriteWidget (C, LTeamHud[2]);
		DrawSpriteWidget (C, LTeamHud[1]);
		DrawSpriteWidget (C, LTeamHud[0]);

		DrawSpriteWidget (C, RTeamHud[2]);
		DrawSpriteWidget (C, RTeamHud[1]);
		DrawSpriteWidget (C, RTeamHud[0]);
	}
}

simulated function ShowTeamScorePassC(Canvas C)
{
	if ( bShowPoints )
	{
		if (TeamSymbols[0].WidgetTexture != None)
			DrawSpriteWidget (C, TeamSymbols[0]);

		if (TeamSymbols[1].WidgetTexture != None)
			DrawSpriteWidget (C, TeamSymbols[1]);

	    TeamScoreOffset();

		DrawNumericWidget (C, ScoreTeam[0], DigitsBig);
		DrawNumericWidget (C, ScoreTeam[1], DigitsBig);
	}
}

simulated function TeamScoreOffset()
{
	ScoreTeam[1].OffsetX = 80;
	if ( ScoreTeam[1].Value < 0 )
		ScoreTeam[1].OffsetX += 90;
	if( abs(ScoreTeam[1].Value) > 9 )
		ScoreTeam[1].OffsetX += 90;
}

simulated function ShowPersonalScore(Canvas C)
{
    DrawNumericWidget (C, Score, DigitsBig);
}
// Alpha Pass ==================================================================================
simulated function DrawHudPassC (Canvas C)
{
    Super.DrawHudPassC (C);

	if (Links >0)
		drawLinkText(C);

    if( bShowPoints )
        ShowPersonalScore(C);
	ShowTeamScorePassC(C);
}

// Alternate Texture Pass ======================================================================

simulated function UpdateTeamHud()
{
    local GameReplicationInfo GRI;
    local int i;

    Score.Value = Min (CurScore, 999);		// drawing limitation

	GRI = PlayerOwner.GameReplicationInfo;

	if (GRI == None)
        return;

    for (i = 0; i < 2; i++)
    {
        if (GRI.Teams[i] == None)
            continue;

        ScoreTeam[i].Value = Min (GRI.Teams[i].Score, 999);  // max space in hud

        if (GRI.TeamSymbols[i] != None)
			TeamSymbols[i].WidgetTexture = GRI.TeamSymbols [i];
    }
}

simulated function UpdateHud()
{
	UpdateTeamHUD();
	showLinks();
    Super.UpdateHud();
}

defaultproperties
{
	RTeamHud[0]=(WidgetTexture=Material'InterfaceContent.Hud.SkinA',TextureCoords=(X1=611,Y1=900,X2=979,Y2=1023),TextureScale=0.3,DrawPivot=DP_UpperLeft,PosX=0.5,PosY=0.01,OffsetX=0,OffsetY=0,ScaleMode=SM_Right,Scale=1.0,RenderStyle=STY_Alpha,Tints[0]=(R=255,G=255,B=0,A=200),Tints[1]=(R=255,G=255,B=0,A=200))
	RTeamHud[1]=(WidgetTexture=Material'InterfaceContent.Hud.SkinA',TextureCoords=(X1=611,Y1=777,X2=979,Y2=899),TextureScale=0.3,DrawPivot=DP_UpperLeft,PosX=0.5,PosY=0.01,OffsetX=0,OffsetY=0,ScaleMode=SM_Right,Scale=1.0,RenderStyle=STY_Alpha,Tints[0]=(R=255,G=0,B=0,A=50),Tints[1]=(R=0,G=64,B=255,A=50))
	RTeamHud[2]=(WidgetTexture=Material'InterfaceContent.Hud.SkinA',TextureCoords=(X1=611,Y1=654,X2=979,Y2=776),TextureScale=0.3,DrawPivot=DP_UpperLeft,PosX=0.5,PosY=0.01,OffsetX=0,OffsetY=0,ScaleMode=SM_Right,Scale=1.0,RenderStyle=STY_Alpha,Tints[0]=(R=255,G=0,B=0,A=50),Tints[1]=(R=0,G=64,B=255,A=50))

	LTeamHud[0]=(WidgetTexture=Material'InterfaceContent.Hud.SkinA',TextureCoords=(X2=611,Y1=900,X1=979,Y2=1023),TextureScale=0.3,DrawPivot=DP_UpperRight,PosX=0.5,PosY=0.01,OffsetX=0,OffsetY=0,ScaleMode=SM_Right,Scale=1.0,RenderStyle=STY_Alpha,Tints[0]=(R=255,G=255,B=0,A=255),Tints[1]=(R=255,G=255,B=0,A=255))
	LTeamHud[1]=(WidgetTexture=Material'InterfaceContent.Hud.SkinA',TextureCoords=(X2=611,Y1=777,X1=979,Y2=899),TextureScale=0.3,DrawPivot=DP_UpperRight,PosX=0.5,PosY=0.01,OffsetX=0,OffsetY=0,ScaleMode=SM_Right,Scale=1.0,RenderStyle=STY_Alpha,Tints[0]=(R=255,G=0,B=0,A=50),Tints[1]=(R=0,G=64,B=255,A=50))
	LTeamHud[2]=(WidgetTexture=Material'InterfaceContent.Hud.SkinA',TextureCoords=(X2=611,Y1=654,X1=979,Y2=776),TextureScale=0.3,DrawPivot=DP_UpperRight,PosX=0.5,PosY=0.01,OffsetX=0,OffsetY=0,ScaleMode=SM_Left,Scale=1.0,RenderStyle=STY_Alpha,Tints[0]=(R=255,G=0,B=0,A=50),Tints[1]=(R=0,G=64,B=255,A=50))

    totalLinks=(TextureScale=0.2,MinDigitCount=2,DrawPivot=DP_MiddleRight,PosX=1.0,PosY=0.835,OffsetX=-30,OffsetY=90,RenderStyle=STY_Alpha,Tints[0]=(R=255,G=255,B=255,A=255),Tints[1]=(R=255,G=255,B=255,A=255))
    ScoreTeam(0)=(TextureScale=0.24,MinDigitCount=2,DrawPivot=DP_MiddleRight,PosX=0.5,PosY=0.01,OffsetX=-150,OffsetY=80,RenderStyle=STY_Alpha,Tints[0]=(R=255,G=255,B=255,A=255),Tints[1]=(R=255,G=255,B=255,A=255))
    ScoreTeam(1)=(TextureScale=0.24,MinDigitCount=2,DrawPivot=DP_MiddleLeft,PosX=0.5,PosY=0.01,OffsetX=120,OffsetY=80,RenderStyle=STY_Alpha,Tints[0]=(R=255,G=255,B=255,A=255),Tints[1]=(R=255,G=255,B=255,A=255))

    Score=(TextureScale=0.18,MinDigitCount=2,DrawPivot=DP_UpperRight,PosX=0,PosY=0,OffsetX=560,OffsetY=40,RenderStyle=STY_Alpha,Tints[0]=(R=255,G=255,B=255,A=255),Tints[1]=(R=255,G=255,B=255,A=255))

    TeamSymbols(0)=(TextureCoords=(X1=0,Y1=0,X2=256,Y2=256),TextureScale=0.1,DrawPivot=DP_UpperRight,PosX=0.5,PosY=0.01,OffsetX=-60,OffsetY=60,RenderStyle=STY_Alpha,Tints[0]=(R=255,G=100,B=100,A=200),Tints[1]=(R=255,G=32,B=32,A=200))
    TeamSymbols(1)=(TextureCoords=(X1=0,Y1=0,X2=256,Y2=256),TextureScale=0.1,DrawPivot=DP_UpperLeft,PosX=0.5,PosY=0.01,OffsetX=90,OffsetY=60,RenderStyle=STY_Alpha,Tints[0]=(R=0,G=128,B=255,A=200),Tints[1]=(R=32,G=210,B=255,A=200))

    CNPosX=0.01;
    CNPosY=0.01;

    CarrierTextColor1=(R=255,G=255,B=0,A=255)
    CarrierTextColor2=(R=0,G=255,B=0,A=255)
    CarrierTextColor3=(R=200,G=200,B=200,A=255)

	LinkEstablishedMessage="LINK ESTABLISHED"
}