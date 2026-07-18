// ====================================================================
//  Class:  XInterface.STY_ListBox
//  Parent: XInterface.STY_SquareButton
//
//  Background style for the actual "list" area of a listbox
// ====================================================================

class STY_ListBox extends STY_SquareButton;

defaultproperties
{
    KeyName="ListBox"
    AlternateKeyName(0)="SmallListBox"
    AlternateKeyName(1)="LargeListBox"
    Images(0)=Material'InterfaceContent.Menu.EditBox'
    //Images(1)=Material'InterfaceContent.Menu.EditBoxWatched'
    //Images(2)=Material'InterfaceContent.Menu.EditBoxFocused'
    //Images(3)=Material'InterfaceContent.Menu.fbEditBoxPressed'
    Images(1)=Material'InterfaceContent.Menu.EditBox'
    Images(2)=Material'InterfaceContent.Menu.EditBox'
    Images(3)=Material'InterfaceContent.Menu.EditBox'
    Images(4)=Material'InterfaceContent.Menu.EditBox'
    FontColors(3)=(R=230,G=200,B=0,A=255)
    BorderOffsets(0)=12
    BorderOffsets(1)=12
    BorderOffsets(2)=12
    BorderOffsets(3)=12
}
