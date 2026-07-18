class ONSHeadlightProjector extends Projector
	native;

defaultproperties
{
	DrawScale=0.65
	bHidden=true
	FrameBufferBlendingOp=PB_Add
	MaterialBlendingOp=PB_Modulate
    FOV=40
    MaxTraceDistance=2048
    bProjectOnUnlit=True
    bGradient=True
    bProjectOnAlpha=True
    bLightChanged=True
    bHardAttach=True
    bProjectActor=True
    bProjectOnParallelBSP=True
	bClipBSP=True
	RemoteRole=ROLE_None
	bNoProjectOnOwner=True
	bStatic=False
	bDynamicAttach=True
	CullDistance=2000.0
    bDetailAttachment=true
}
