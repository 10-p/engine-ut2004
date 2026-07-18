class IRC_Page extends UT2K3TabPanel
    abstract;

var moEditBox           TextEntry;
var GUIScrollTextBox    TextDisplay;

var localized string HasLeftText;
var localized string HasJoinedText;
var localized string WasKickedByText;
var localized string NowKnownAsText;
var localized string QuitText;
var localized string SetsModeText;
var localized string NewTopicText;

var config int MaxChatScrollback;
var config int InputHistorySize;

var transient array<string> InputHistory;
var transient int           InputHistoryPos;
var transient bool          bDoneInputScroll;

var config color IRCTextColor;
var config color IRCNickColor;
var config color IRCActionColor;
var config color IRCInfoColor;
var config color IRCLinkColor;

// Pure Virtual
function ProcessInput(string Text)
{

}

// When you hit enter in the input box, call the class
function bool InternalOnKeyEvent(out byte Key, out byte State, float delta)
{
    local string Input;
    local int Index;

    // Only care about key-press events
    if(State != 1)
        return false;

    if( Key == 0x0D ) // ENTER
    {
        Input = TextEntry.GetText();

        if(Input != "")
        {
            // Add string to end of history
            Index = InputHistory.Length;
            InputHistory.Insert(Index, 1);
            InputHistory[Index] = Input;

            // If history is too long, remove chat from start of history
            if(InputHistory.Length > InputHistorySize)
                InputHistory.Remove(0, InputHistory.Length - InputHistorySize);

            // Once you enter something - reset history position to most recent entry
            InputHistoryPos = InputHistory.Length - 1;
            bDoneInputScroll = false;

            ProcessInput(Input); // Handle whatever you typed
            TextEntry.SetText(""); // And empty box again.
        }

        return true;
    }
    else if( Key == 0x26 ) // UP
    {
        if( InputHistory.Length > 0 ) // do nothing if no history
        {
            TextEntry.SetText( InputHistory[ InputHistoryPos ] );

            InputHistoryPos--;
            if(InputHistoryPos < 0)
                InputHistoryPos = InputHistory.Length - 1;

            bDoneInputScroll = true;
        }

        return true;
    }
    else if( Key == 0x28 ) // DOWN
    {
        if( InputHistory.Length > 0 )
        {
            if(!bDoneInputScroll)
                InputHistoryPos = 0; // Hack so pressing 'down' gives you the oldest input

            TextEntry.SetText( InputHistory[ InputHistoryPos ] );

            InputHistoryPos++;
            if(InputHistoryPos > InputHistory.Length - 1)
                InputHistoryPos = 0;

            bDoneInputScroll = true;
        }

        return true;
    }

    return false;
}


function string ColorizeLinks(string InString)
{
    local int i;
    local string OutString, Character, Word, ColourlessWord;
    local bool InWord, HaveWord;

    i=0;
    while(true)
    {
        // Get the next word in the string
        while( i<Len(InString) && !HaveWord )
        {
            Character = Mid(InString, i, 1);

            if(InWord) // We are in the middle of a word.
            {
                if( Character == " " ) // We hit a terminating space - word complete
                {
                    HaveWord = true;
                }
                else // We are just working through the word
                {
                    Word = Word$Character;
                    i++;
                }
            }
            else
            {
                if( Character == " " ) // Pass over spaces (add straight to output)
                {
                    OutString = OutString$Character;
                    i++;
                }
                else // Hit the first character of a word.
                {
                    InWord = true;
                    Word = Word$Character;
                    i++;
                }
            }
        }

        if(Word == "")
            return OutString;

        // Deal with that word
        ColourlessWord = StripColorCodes(Word);
        if( Left(ColourlessWord, 7) == "http://" || Left(ColourlessWord, 9) == "unreal://" || Left(ColourlessWord, Len(PlayerOwner().GetURLProtocol())+3)==(PlayerOwner().GetURLProtocol()$"://") )
            OutString = OutString$MakeColorCode(IRCLinkColor)$ColourlessWord$MakeColorCode(IRCTextColor);
        else
            OutString = OutString$Word;

        // Reset for next word;
        Word = "";
        HaveWord = false;
        InWord = false;
    }

    return OutString;
}

function bool IRCTextDblClick(GUIComponent Sender)
{
    local string ClickString;

    ClickString = StripColorCodes(TextDisplay.MyScrollText.ClickedString);
    //Log("DOUBLE CLICKED: ["$ClickString$"]");

    // Check for WWW URL
    if( Left(ClickString, 7) == "http://" )
    {
        PlayerOwner().ConsoleCommand("start"@ClickString);
    }
    else
    if( Left(ClickString, 9) == "unreal://" || Left(ClickString, Len(PlayerOwner().GetURLProtocol())+3)==(PlayerOwner().GetURLProtocol()$"://") )
    {
        Controller.CloseAll(false);
        PlayerOwner().ClientTravel( ClickString, TRAVEL_Absolute, false );
    }

    return true;
}

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
    Super.Initcomponent(MyController, MyOwner);

    TextEntry = moEditBox(Controls[0]);
    TextEntry.OnKeyEvent = InternalOnKeyEvent;
    TextEntry.MyEditBox.Style = Controller.GetStyle("IRCEntry", TextEntry.MyEditBox.FontScale); // Force the style of the sub-component to the IRC text style

    TextDisplay = GUIScrollTextBox( GUIPanel( GUISplitter(Controls[1]).Controls[0] ).Controls[0] );
    TextDisplay.MyScrollText.Separator = Chr(13); // New line character
    TextDisplay.MyScrollText.MaxHistory = MaxChatScrollback;
    TextDisplay.MyScrollText.Style = Controller.GetStyle("IRCText",TextDisplay.MyScrollText.FontScale);
    TextDisplay.MyScrollText.bClickText = true;
    TextDisplay.MyScrollText.OnDblClick = IRCTextDblClick;
}

defaultproperties
{
    Begin Object class=moEditBox Name=EntryBox
        WinWidth=1.0
        WinHeight=0.05
        WinLeft=0
        WinTop=0.95
        CaptionWidth=0
    End Object
    Controls(0)=moEditBox'EntryBox'


    // Scroll box
    Begin Object Class=GUIScrollTextBox Name=DisplayBox
        WinWidth=1.0
        WinHeight=1.0
        WinLeft=0
        WinTop=0
        CharDelay=0.0015
        EOLDelay=0.25
        bVisibleWhenEmpty=true
        bScaleToParent=true
        StyleName="ServerBrowserGrid"
        bNoTeletype=true
        bStripColors=true
    End Object

    // Display panel
    Begin Object Class=GUIPanel Name=DisplayPanel
        Controls(0)=DisplayBox;
    End Object

    // Util panel
    Begin Object Class=GUIPanel Name=UtilPanel
    End Object

    // Splitter to divide main window in two: server list and details
    Begin Object Class=GUISplitter Name=MainSplitter
        WinWidth=1.0
        WinHeight=0.95
        WinTop=0
        WinLeft=0
        Controls(0)=DisplayPanel
        Controls(1)=UtilPanel
        SplitOrientation=SPLIT_Vertical
        SplitPosition=0.8
        bFixedSplitter=true
    End Object
    Controls(1)=MainSplitter

    WinTop=0.15
    WinLeft=0
    WinWidth=1
    WinHeight=0.73
    bAcceptsInput=false

    MaxChatScrollback=250
    InputHistorySize=16

    HasLeftText="has left"
    HasJoinedText="has joined"
    WasKickedByText="was kicked by"
    NowKnownAsText="is now known as"
    QuitText="Quit"
    SetsModeText="sets mode"
    NewTopicText="Topic"

    IRCTextColor=(R=160,G=160,B=160,A=0)
    IRCNickColor=(R=150,G=150,B=255,A=0)
    IRCActionColor=(R=230,G=200,B=0,A=0)
    IRCInfoColor=(R=130,G=130,B=160,A=0)
    IRCLinkColor=(R=255,G=150,B=150,A=0)
}
