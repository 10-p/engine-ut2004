//=============================================================================
// Engine: The base class of the global application object classes.
// This is a built-in Unreal class and it shouldn't be modified.
//=============================================================================
class Engine extends Subsystem
	native
	noexport
	transient;

// Variables.
var primitive Cylinder;
var const client Client;
var const audiosubsystem Audio;
var const renderdevice GRenDev;

// Stats.
var int bShowFrameRate;
var int bShowRenderStats;
var int bShowHardwareStats;
var int bShowGameStats;
var int bShowNetStats;
var int bShowAnimStats;		 // Show animation statistics.

// ifdef WITH_LIPSINC
var int bShowLIPSincStats;   // Show LIPSinc statistics.
// endif

var int bShowHistograph;
var int bShowXboxMemStats;
var int bShowMatineeStats;	// Show Matinee specific information
var int bShowAudioStats;
var int bShowLightStats;    // Show dynamic lighting statistics.

var int TickCycles, GameCycles, ClientCycles;
var(Settings) config int CacheSizeMegs;
var(Settings) config bool UseSound;
var(Settings) config bool UseStaticMeshBatching;
var(Settings) config bool ServerReadsStdin;
var(Settings) config bool bSlowRefChecking; // toggle object ref checking on cleanupdestroyed

var(Settings) float CurrentTickRate;
var config int DetectedVideoMemory;

// Color preferences.
var(Colors) config color
	C_WorldBox,
	C_GroundPlane,
	C_GroundHighlight,
	C_BrushWire,
	C_Pivot,
	C_Select,
	C_Current,
	C_AddWire,
	C_SubtractWire,
	C_GreyWire,
	C_BrushVertex,
	C_BrushSnap,
	C_Invalid,
	C_ActorWire,
	C_ActorHiWire,
	C_Black,
	C_White,
	C_Mask,
	C_SemiSolidWire,
	C_NonSolidWire,
	C_WireBackground,
	C_WireGridAxis,
	C_ActorArrow,
	C_ScaleBox,
	C_ScaleBoxHi,
	C_ZoneWire,
	C_Mover,
	C_OrthoBackground,
	C_StaticMesh,
	C_VolumeBrush,
	C_ConstraintLine,
	C_AnimMesh,
	C_TerrainWire;

defaultproperties
{
	CacheSizeMegs=2
	UseSound=True
	C_WorldBox=(R=0,G=0,B=107,A=255)
	C_GroundPlane=(R=0,G=0,B=63,A=255)
	C_GroundHighlight=(R=0,G=0,B=127,A=255)
	C_BrushWire=(R=255,G=63,B=63,A=255)
	C_Pivot=(R=0,G=255,B=0,A=255)
	C_Select=(R=0,G=0,B=127,A=255)
	C_AddWire=(R=127,G=127,B=255,A=255)
	C_SubtractWire=(R=255,G=192,B=63,A=255)
	C_GreyWire=(R=163,G=163,B=163,A=255)
	C_Invalid=(R=163,G=163,B=163,A=255)
	C_ActorWire=(R=127,G=63,B=0,A=255)
	C_ActorHiWire=(R=255,G=127,B=0,A=255)
	C_White=(R=255,G=255,B=255,A=255)
	C_SemiSolidWire=(R=127,G=255,B=0,A=255)
	C_NonSolidWire=(R=63,G=192,B=32,A=255)
	C_WireGridAxis=(R=119,G=119,B=119,A=255)
	C_ActorArrow=(R=163,G=0,B=0,A=255)
	C_ScaleBox=(R=151,G=67,B=11,A=255)
	C_ScaleBoxHi=(R=223,G=149,B=157,A=255)
	C_Mover=(R=255,G=0,B=255,A=255)
	C_OrthoBackground=(R=163,G=163,B=163,A=255)
	C_Current=(R=0,G=0,B=0,A=255)
	C_BrushVertex=(R=0,G=0,B=0,A=255)
	C_BrushSnap=(R=0,G=0,B=0,A=255)
	C_Black=(R=0,G=0,B=0,A=255)
	C_Mask=(R=0,G=0,B=0,A=255)
	C_WireBackground=(R=0,G=0,B=0,A=255)
	C_ZoneWire=(R=0,G=0,B=0,A=255)
	C_StaticMesh=(R=0,G=255,B=255,A=255)
	C_VolumeBrush=(R=255,G=196,B=225,A=255)
	C_AnimMesh=(R=221,G=221,B=28,A=255)
	C_ConstraintLine=(R=0,G=255,B=0,A=255)
	C_TerrainWire=(R=255,G=255,B=255,A=255)
	UseStaticMeshBatching=True
}
