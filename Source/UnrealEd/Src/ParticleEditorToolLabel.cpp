//=============================================================================
//
// ParticleEditorToolLabel.cpp	
// Copyright(c) 2002 Demiurge Studios. All Rights Reserved.
// 
//=============================================================================

#include "UnrealEd.h"




//=============================================================================
// WParticleToolLabel 

WParticleToolLabel::WParticleToolLabel( WWindow* InOwner, FString InLabelText, WParticleEditorComponent *inParentComponent, INT inToolIndex ) : 
	WPropertyPage(InOwner),
	LabelText(InLabelText),
	ParentComponent(inParentComponent),
	ToolIndex(inToolIndex),
	X_OFFSET(20)
{
}



void WParticleToolLabel::OnDestroy()
{
}



void WParticleToolLabel::SetExpand(UBOOL Expanded)
{
}




//=============================================================================
// WParticleStaticLabel 

WParticleStaticLabel::WParticleStaticLabel( WWindow* InOwner, FString InLabelText, WParticleEditorComponent *inParentComponent, INT inToolIndex) : 
	WParticleToolLabel(InOwner, InLabelText, inParentComponent, inToolIndex)
{
}



void WParticleStaticLabel::OnDestroy()
{
	if(Label) { delete Label; Label = NULL;}
}



FPoint WParticleStaticLabel::GetSize() const
{
	return FPoint(180,15);
}



void WParticleStaticLabel::OpenWindow(HMODULE InHMOD)
{
	WPropertyPage::OpenWindow( 0, InHMOD );

	Label = new WLabel(this, 0);
	Label->OpenWindow(1, 0);

	//hWnd, left, top, width, height, repaint
	::MoveWindow(Label->hWnd, X_OFFSET, 0, GetSize().X, GetSize().Y, TRUE );

	//Label->SetFont( (HFONT)GetStockObject(ANSI_FIXED_FONT) );
	//Label->SetFont( (HFONT)GetStockObject(PS_BOLD_FONT) );
	Label->SetText(*LabelText);
	//Label->UDNHelpTopic = UDNHelpTopic;
}



//=============================================================================
// WParticleExpandingLabel 

WParticleExpandingLabel::WParticleExpandingLabel( WWindow* InOwner, FString InLabelText, WParticleEditorComponent *inParentComponent, INT inToolIndex, UBOOL inExpanded ) : 
	WParticleToolLabel(InOwner, InLabelText, inParentComponent, inToolIndex),
	Expanded(inExpanded)
{
}



void WParticleExpandingLabel::OnDestroy()
{
	if(Label) { delete Label; Label = NULL;}
	if(ExpandButton) { delete ExpandButton; ExpandButton = NULL;}
}



FPoint WParticleExpandingLabel::GetSize() const
{
	return FPoint(300, 16);
}



void WParticleExpandingLabel::OpenWindow(HMODULE InHMOD)
{
	WPropertyPage::OpenWindow( 0, InHMOD );

	ExpandButton = new WButton(this, 0);
	ExpandButton->OpenWindow( 1, 0, 0, 15, 15, Expanded ? TEXT("-") : TEXT("+"));
	ExpandButton->SetFont( (HFONT)GetStockObject(ANSI_FIXED_FONT) );
	ExpandButton->ClickDelegate = FDelegate(this, (TDelegate)OnExpandClick);


	Label = new WLabel(this, 0);
	Label->OpenWindow(1, 0);
	::MoveWindow(Label->hWnd, X_OFFSET, 0, GetSize().X, GetSize().Y, TRUE );

	//Different Font Choices	
	//Label->SetFont( (HFONT)GetStockObject(ANSI_FIXED_FONT) );
	//Label->SetFont( (HFONT)GetStockObject(PS_BOLD_FONT) );

	Label->SetText(*LabelText);
}



void WParticleExpandingLabel::OnExpandClick()
{
	Expanded = !Expanded;
	ExpandButton->SetText(Expanded ? TEXT("-") : TEXT("+"));
	ParentComponent->ShowTool(ToolIndex, Expanded);
}



void WParticleExpandingLabel::SetExpand(UBOOL InExpanded)
{
	Expanded = InExpanded;
	ExpandButton->SetText(Expanded ? TEXT("-") : TEXT("+"));
}





