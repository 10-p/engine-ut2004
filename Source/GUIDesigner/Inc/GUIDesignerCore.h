
#ifndef GUIDESIGNER_API
  #define GUIDESIGNER_API DLL_IMPORT
#endif

#include "XInterface.h"

#define addflag(flag) { if ( *c ) appStrncat(c,TEXT(","),STATICSTRINGLENGTH); appStrncat(c,TEXT(#flag),STATICSTRINGLENGTH); }
GUIDESIGNER_API TCHAR* GUIGetPropFlags( const UProperty* Prop );

#ifndef NAMES_ONLY
#ifndef __GUIDESIGNERCORE_H__
#define __GUIDESIGNERCORE_H__

class GUIDESIGNER_API UPropertyManager : public UPropertyManagerBase
{
    DECLARE_CLASS(UPropertyManager,UPropertyManagerBase,CLASS_Transient,GUIDesigner)
    NO_DEFAULT_CONSTRUCTOR(UPropertyManager)

	class WObjectProperties*      CurWindow;
	class FPropertyManagerHook*   Hook;
	class FPropertyManagerSnoop*  Snoop;

protected:
	void SetWindow( WObjectProperties* InWindow );

public:
	void SetParent( UGUIController* InParent );
	void SetCurrent( UObject** InCurrent );
	void SetWindow( void* InWindow );

	void Show(UBOOL bVisible);
	UBOOL IsVisible();

	class FPropertyItem* GetSelectedItem();

	void* GetSnoop()  { return GetControlSnoop(); }
	void* GetHook()   { return GetNotifyHook(); }
	void* GetWindow() { return GetCurrentWindow(); }

	class FPropertyManagerSnoop* GetControlSnoop();
	class FPropertyManagerHook* GetNotifyHook();
	class WObjectProperties* GetCurrentWindow();

	// Snoop/Notify Relays
	void SnoopChar( WWindow* Src, INT Char );
	void SnoopKeyDown( WWindow* Src, INT Char );
	void SnoopLeftMouseDown( WWindow* Src, FPoint P );
	void SnoopRightMouseDown( WWindow* Src, FPoint P );

	void NotifyDestroy( void* Src );
	void NotifyPreChange( void* Src );
	void NotifyPostChange( void* Src );
	void NotifyExec( void* Src, const TCHAR* Cmd );

};

class GUIDESIGNER_API FPropertyManagerBase
{
protected:
	UPropertyManager* Owner GCC_PACK(4);

public:
	FPropertyManagerBase( UPropertyManager* InOwner = NULL ) : Owner(InOwner) { }
};

class GUIDESIGNER_API FPropertyManagerSnoop : public FPropertyManagerBase, public FControlSnoop
{
public:
	// FControlSnoop interface.
	void SnoopChar( WWindow* Src, INT Char )           {if(Owner)Owner->SnoopChar(Src,Char);}
	void SnoopKeyDown( WWindow* Src, INT Char )        {if(Owner)Owner->SnoopKeyDown(Src,Char);}
	void SnoopLeftMouseDown( WWindow* Src, FPoint P )  {if(Owner)Owner->SnoopLeftMouseDown(Src,P);}
	void SnoopRightMouseDown( WWindow* Src, FPoint P ) {if(Owner)Owner->SnoopRightMouseDown(Src,P);}

	FPropertyManagerSnoop( UPropertyManager* InOwner = NULL ) : FPropertyManagerBase(InOwner) { }
};

class GUIDESIGNER_API FPropertyManagerHook : public FPropertyManagerBase, public FNotifyHook
{
public:
	// FNotify interface.
	void NotifyDestroy( void* Src )                {if(Owner)Owner->NotifyDestroy(Src);}
	void NotifyPreChange( void* Src )              {if(Owner)Owner->NotifyPreChange(Src);}
	void NotifyPostChange( void* Src )             {if(Owner)Owner->NotifyPostChange(Src);}
	void NotifyExec( void* Src, const TCHAR* Cmd ) {if(Owner)Owner->NotifyExec(Src,Cmd);}

	FPropertyManagerHook( UPropertyManager* InOwner = NULL ) : FPropertyManagerBase(InOwner) { }
};

#endif // __GUIDESIGNERCORE_H__
#endif // NAMES_ONLY