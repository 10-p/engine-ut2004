class TestVignette extends Vignette;

#exec OBJ LOAD FILE=InterfaceContent.utx

var() config array<string> Backgrounds;
var() transient texture Background;

var() Texture Logo;
var() float LogoPosX, LogoPosY;
var() float LogoScaleX, LogoScaleY;

var() localized String LoadingFontName;
var() font LoadingFontFont;
var() localized String LoadingString;
var() float LoadingPosX, LoadingPosY, MapPosY;
var() EDrawPivot LoadingPivot;
var() Color LoadingColor;

simulated event Init()
{
    local int i;

    Super.PreBeginPlay();
    i = Rand( Backgrounds.Length );
    Background = Texture( DynamicLoadObject( Backgrounds[i], class'Texture') );
    if( Background == none )
        log( Backgrounds[i] $" not found for Vignette", 'Error' );
}
simulated function ScreenText(Canvas C, String Text, float posX, float posY, float ScaleX,float ScaleY)
{
    C.Style = ERenderStyle.STY_Alpha;
    C.Font = LoadLoadingFont();
    C.DrawColor = LoadingColor;
	C.FontScaleX = ScaleX;
	C.FontScaleY = ScaleY;
    C.DrawScreenText( Text, posX, posY, LoadingPivot );
}
simulated function font LoadLoadingFont()
{
	if( LoadingFontFont == None )
	{
		LoadingFontFont = Font(DynamicLoadObject(LoadingFontName, class'Font'));
		if( LoadingFontFont == None )
			Log("Warning: "$Self$" Couldn't dynamically load font "$LoadingFontName);
	}
	return LoadingFontFont;
}
simulated event DrawVignette( Canvas C, float Progress )
{
    local float ResScaleX, ResScaleY;
    local float PosX, PosY, DX, DY;

    C.Reset();

    ResScaleX = C.SizeX / 640.0;
    ResScaleY = C.SizeY / 480.0;

	C.Style = ERenderStyle.STY_Alpha;
    C.DrawColor = C.MakeColor( 255, 255, 255 );

    C.SetPos( 0, 0 );
	C.DrawTile( Background, C.SizeX, C.SizeY, 0, 0, Background.USize, Background.VSize );

    DX = Logo.USize * ResScaleX * LogoScaleX;
    DY = Logo.VSize * ResScaleY * LogoScaleY;
    PosX = (LogoPosX * C.SizeX) - (DX * 0.5);
    PosY = (LogoPosY * C.SizeY) - (DY * 0.5);

    C.SetPos( PosX, PosY );
	C.Style = ERenderStyle.STY_Alpha;
    C.DrawTile( Logo, DX, DY, 0, 0, Logo.USize, Logo.VSize );
    ScreenText( C, LoadingString, LoadingPosX, LoadingPosY, ResScaleX*0.5, ResScaleY*0.5 );

	if (MapName~="endgame.ut2" || MapName~="ut2-intro.ut2" || caps(Left(MapName,3))=="TUT")
    	return;



    ScreenText( C, MapName, LoadingPosX, MapPosY, ResScaleX, ResScaleY );

}

defaultproperties
{
    Logo=Texture'InterfaceContent.Logos.Logo'
    LogoScaleX=0.5
    LogoScaleY=0.5
    LogoPosX=0.49
    LogoPosY=0.25

	LoadingFontName="UT2003Fonts.FontLarge"
    LoadingString=". . . L O A D I N G . . ."
    LoadingPosX=0.5
    LoadingPosY=0.65
    MapPosY=0.725
    LoadingColor=(R=255,G=255,B=255,A=255)
    LoadingPivot=DP_MiddleMiddle
}
