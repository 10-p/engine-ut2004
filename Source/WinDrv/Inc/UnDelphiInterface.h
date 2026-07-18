/*=============================================================================
	UnDelphiInterface.h: Delphi(new) Debugger Interface interface
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Lucas Alonso, Demiurge Studios
=============================================================================*/

#include "UnDebuggerInterface.h"

#ifndef __UDELPHIINTERFACE_H__
#define __UDELPHIINTERFACE_H__

typedef void(*DelphiVoidVoid)(void);
typedef void(*DelphiVoidChar)(const char*);
typedef void(*DelphiVoidVoidPtr)(void*);
typedef void(*DelphiVoidCharInt)(const char*, int);
typedef void(*DelphiVoidInt)(int);
typedef void(*DelphiVoidIntInt)(int,int);
typedef void(*DelphiVoidCharChar)(const char*, const char*);
typedef void(*DelphiVoidIntChar)(int, const char*);
typedef int(*DelphiIntIntIntCharChar)(int, int, const char*, const char*); // Whoa

void DelphiCallback( const char* C );

/*
procedure ShowForm;cdecl;external 'dlltest.dll' name 'ShowDllForm'; 
procedure EditorCommand(cmd : pChar);cdecl;external 'dlltest.dll' name 'EditorCommand'; 
procedure EditorLoadTextBuffer(c,p : pchar);cdecl;external 'dlltest.dll' name 'EditorLoadTextBuffer'; 
procedure AddClassToHierarchy(classid : pChar);cdecl;external 'dlltest.dll' name 'AddClassToHierarchy'; 
procedure ClearHierarchy;cdecl;external 'dlltest.dll' name 'ClearHierarchy'; 
procedure BuildHierarchy;cdecl;external 'dlltest.dll' name 'BuildHierarchy'; 
procedure ClearWatch(watch : integer);cdecl;external 'dlltest.dll' name 'ClearWatch'; 
procedure AddWatch(watch : integer; data : pChar)cdecl;external 'dlltest.dll' name 'AddWatch'; 
procedure SetCallback(funcptr : pointer);cdecl;external 'dlltest.dll' name 'SetCallback'; 
procedure AddBreakpoint(ClassName: pChar; LineNo : integer);cdecl;external 'dlltest.dll' name 'AddBreakpoint'; 
procedure RemoveBreakpoint(ClassName: pChar; LineNo : integer);cdecl;external 'dlltest.dll' name 'RemoveBreakpoint'; 
procedure EditorGotoLine(lineNo, Highlight : integer); cdecl;external 'dlltest.dll' name 'EditorGotoLine'; 
procedure AddLineToLog(NewLine :pChar);cdecl;external 'dlltest.dll' name 'AddLineToLog'; 
procedure EditorLoadClass(ClassName :pChar);cdecl;external 'dlltest.dll' name 'EditorLoadClass'; 
procedure CallStackClear;cdecl;external 'dlltest.dll' name 'CallStackClear'; 
procedure CallStackAdd(Item :pchar);cdecl;external 'dlltest.dll' name 'CallStackAdd';
debugwindowstate(int Visibility) where 0=hide, 1=show.
void ClearAWatch(int watch) will clear out a given watch tree (0=local, 1=global, 2=user).
int AddAWatch(int watch, int ParentIndex, char* ObjectName, char* Data);
void LockList(int WatchList) 
void UnlockList(int WatchList)
void SetCurrentObjectName(char* objname)
*/
 

class DelphiInterface : public UDebuggerInterface
{
public:
	DelphiInterface( const TCHAR* DLLName );
	~DelphiInterface();
	UBOOL Initialize( class UDebuggerCore* DebuggerOwner );
	virtual void Close();
	virtual void Show();
	virtual void Hide();
	virtual void AddToLog( const TCHAR* Line );
	virtual void Update( const TCHAR* ClassName, const TCHAR* PackageName, INT LineNumber, const TCHAR* OpcodeName, const TCHAR* objectName );
	virtual void UpdateCallStack( TArray<FString>& StackNames );
	virtual void SetBreakpoint( const TCHAR* ClassName, INT Line );
	virtual void RemoveBreakpoint( const TCHAR* ClassName, INT Line );
	virtual void UpdateClassTree();
	virtual int AddAWatch(int watch, int ParentIndex, const TCHAR* ObjectName, const TCHAR* Data);
	virtual void ClearAWatch(int watch);
	virtual void LockWatch(int watch);
	virtual void UnlockWatch(int watch);

	void Callback( const char* C );

protected:
	UBOOL IsLoaded() const { return LoadCount > 0; }

private:
	FString DllName;
	HMODULE hInterface;
	TMultiMap<UClass*, UClass*> ClassTree;

	void BindToDll();
	void UnbindDll();
	void RecurseClassTree( UClass* RClass );


	DelphiVoidVoid ShowDllForm;
	DelphiVoidChar EditorCommand;
	DelphiVoidCharChar EditorLoadTextBuffer;
	DelphiVoidChar AddClassToHierarchy;
	DelphiVoidVoid ClearHierarchy;
	DelphiVoidVoid BuildHierarchy;
	DelphiVoidInt ClearWatch;
	DelphiVoidIntChar AddWatch;
	DelphiVoidVoidPtr SetCallback;
	DelphiVoidCharInt AddBreakpoint;
	DelphiVoidCharInt DRemoveBreakpoint;
	DelphiVoidIntInt EditorGotoLine;
	DelphiVoidChar AddLineToLog;
	DelphiVoidChar EditorLoadClass;
	DelphiVoidVoid CallStackClear;
	DelphiVoidChar CallStackAdd;
	DelphiVoidInt DebugWindowState;
	DelphiVoidInt DClearAWatch;
	DelphiIntIntIntCharChar DAddAWatch;
	DelphiVoidInt DLockList;
	DelphiVoidInt DUnlockList;
	DelphiVoidChar SetCurrentObjectName;
};

#endif // __UDEPHIINTERFACE_H__