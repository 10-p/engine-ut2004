//=============================================================================
// Object to facilitate properties editing
//=============================================================================
//  Preferences tab for the animation browser...
//  
 
class SkelPrefsEditProps extends Object
hidecategories(Object)
native;	

cpptext
{
	void PostEditChange();
}

var const pointer WBrowserAnimationPtr;

var(OnImport) bool  WeldDuplicateVertices;

defaultproperties
{	
	WeldDuplicateVertices=0;
}
