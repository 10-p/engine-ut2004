// ====================================================================
// (C) 2002, Epic Games
// ====================================================================

class STY_ListSelection extends GUIStyles;

event Initialize()
{
    local int i;

    super.Initialize();

    for (i=0;i<5;i++)
        Images[i]=Controller.DefaultPens[0];
}

defaultproperties
{
    KeyName="ListSelection"
    Images(0)=None
    Images(1)=None
    Images(2)=None
    Images(3)=None
    Images(4)=None

    FontNames(10)="UT2HeaderFont"
    FontNames(11)="UT2HeaderFont"
    FontNames(12)="UT2HeaderFont"
    FontNames(13)="UT2HeaderFont"
    FontNames(14)="UT2HeaderFont"

    FontColors(0)=(r=64,g=64,b=128,a=255)
    FontColors(1)=(r=64,g=64,b=128,a=255)
    FontColors(2)=(r=64,g=64,b=128,a=255)
    FontColors(3)=(r=64,g=64,b=128,a=255)
    FontColors(4)=(r=64,g=64,b=128,a=255)
}
