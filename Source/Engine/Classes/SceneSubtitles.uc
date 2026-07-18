//=============================================================================
// SceneSubtitles
// Stores subtitles for matinee cinematics
//=============================================================================

class SceneSubtitles extends Info
		native
		placeable;

cpptext
{
	void ProcessEvent( ESST_Mode Mode );
}

enum ESST_Mode
{
	ESST_SkipToNextLine,
};


var() Localized Array<String> SubTitles;	// storing all subtitles	
var int		CurrentIndex;					// current subtitles index


native final function ProcessEvent( ESST_Mode Mode );


function string GetSubTitles()
{
	if ( CurrentIndex >= SubTitles.Length )
		CurrentIndex = -1;

	if ( CurrentIndex >= 0 )
		return SubTitles[CurrentIndex];

	return "";
}

event Reset()
{	
	super.Reset();
	CurrentIndex=-1;
}

defaultproperties
{
	bNoDelete=true
	bStatic=true
	CurrentIndex=-1
}