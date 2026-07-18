//==============================================================================
//  Created on: 12/02/2003
//  Description
//
//  Written by Ron Prestenback
//  © 2003, Epic Games, Inc. All Rights Reserved
//==============================================================================

class PropertyManagerBase extends Object
	native
	abstract;

cpptext
{
	virtual void SetParent( UGUIController* InParent ) {}
	virtual void SetCurrent( UObject** InCurrent )     {}
	virtual void SetWindow( void* InWindow )           {}

	virtual void Show(UBOOL bVisible) {         }
	virtual UBOOL IsVisible()         {return 0;}

	virtual void* GetSnoop() {return NULL;}
	virtual void* GetHook()  {return NULL;}
	virtual void* GetWindow(){return NULL;}
}

var GUIController Parent;

