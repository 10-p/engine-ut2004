/*=============================================================================
	BugReport.cpp: Release-mode debugging monitor.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Andrew Scheidecker
=============================================================================*/

#include <windows.h>
#include <stdio.h>
#include <psapi.h>

//
//	throwFormattedError
//

void throwFormattedError(const char* Format,...)
{
	va_list	ArgList;
	char	Error[1024];

	va_start(ArgList,Format);
	vsprintf(Error,Format,ArgList);
	va_end(ArgList);

	throw Error;
}

//
//	module
//

class module
{
public:

	struct section
	{
		DWORD	RVA,
				Size;
	};

	HANDLE		Process;
	HMODULE		Handle;
	char		Name[MAX_PATH];
	LPVOID		Base;
	DWORD		Size;
	section*	Sections;
	DWORD		NumSections;

	module*		NextModule;

	// Constructor.

	module(HANDLE InProcess,HMODULE InHandle,module* InNext)
	{
		Process = InProcess;
		Handle = InHandle;
		NextModule = InNext;

		// Determine the module filename.

		if(!GetModuleFileNameEx(Process,Handle,Name,MAX_PATH))
			throwFormattedError("GetModuleFileNameEx failed.  GetLastError=%08x",GetLastError());

		// Determine the module base and size.

		MODULEINFO	ModuleInfo;

		if(!GetModuleInformation(Process,Handle,&ModuleInfo,sizeof(MODULEINFO)))
			throwFormattedError("GetModuleInformation(%s) failed.  GetLastError=%08x",Name,GetLastError());

		Base = ModuleInfo.lpBaseOfDll;
		Size = ModuleInfo.SizeOfImage;

		// Read the module's image headers.

		BYTE				ImageHeader[4096];
		IMAGE_DOS_HEADER*	DosHeader;
		IMAGE_NT_HEADERS*	NtHeaders;
		SIZE_T				BytesRead;

		if(!ReadProcessMemory(Process,(LPCVOID)Handle,ImageHeader,4096,&BytesRead) || BytesRead != 4096)
			throwFormattedError("ReadProcessMemory(%08x,%u) failed.  GetLastError=%08x",Handle,4096,GetLastError());

		DosHeader = (IMAGE_DOS_HEADER*)ImageHeader;
		NtHeaders = (IMAGE_NT_HEADERS*)&ImageHeader[DosHeader->e_lfanew];

        // Build an array of sections in the module.

		NumSections = NtHeaders->FileHeader.NumberOfSections;
		Sections = new section[NumSections];

		IMAGE_SECTION_HEADER*	SectionHeaders = IMAGE_FIRST_SECTION(NtHeaders);

		for(DWORD SectionIndex = 0;SectionIndex < NumSections;SectionIndex++)
		{
			Sections[SectionIndex].RVA = SectionHeaders[SectionIndex].VirtualAddress;
			Sections[SectionIndex].Size = SectionHeaders[SectionIndex].Misc.VirtualSize;
		}
	}
};

//
//	logicalAddress
//

class logicalAddress
{
public:

	module*	Module;
	DWORD	Section,
			Offset;

	// Constructor.

	logicalAddress(module* Modules,PVOID VirtualAddress)
	{
		// Find the module containing this virtual address.

		Module = NULL;

		for(module* ModuleIterator = Modules;ModuleIterator;ModuleIterator = ModuleIterator->NextModule)
		{
			DWORD	RVA = *(DWORD*)&VirtualAddress - *(DWORD*)&ModuleIterator->Base;

			if(VirtualAddress >= ModuleIterator->Base && RVA < ModuleIterator->Size)
			{
				for(DWORD SectionIndex = 0;SectionIndex < ModuleIterator->NumSections;SectionIndex++)
				{
					if(RVA >= ModuleIterator->Sections[SectionIndex].RVA && (RVA - ModuleIterator->Sections[SectionIndex].RVA) < ModuleIterator->Sections[SectionIndex].Size)
					{
						Module = ModuleIterator;
						Section = SectionIndex;
						Offset = RVA - ModuleIterator->Sections[SectionIndex].RVA;
					}
				}
			}
		}

		if(!Module)
		{
			Section = 0;
			Offset = *(DWORD*)&VirtualAddress;
		}
	}

	// describe

	const char* describe() const
	{
		static char	Description[1024];

		if(Module)
		{
			sprintf(
				Description,
				"%08x %04x:%08x %s",
				*(DWORD*)&Module->Base + Module->Sections[Section].RVA + Offset,
				Section + 1,
				Offset,
				Module->Name
				);
		}
		else
		{
			sprintf(
				Description,
				"%08x",
				Offset
				);
		}

		return Description;
	}
};

//
//	WinMain
//

int WINAPI WinMain(HINSTANCE Instance,HINSTANCE PreviousInstance,LPSTR CommandLine,int ShowCommand)
{
	try
	{
		DWORD	ProcessId = atoi(CommandLine);
		
		if(ProcessId && DebugActiveProcess(ProcessId))
		{
			while(1)
			{
				DEBUG_EVENT	DebugEvent;

				if(WaitForDebugEvent(&DebugEvent,INFINITE))
				{
					if(DebugEvent.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT)
					{
						ContinueDebugEvent(DebugEvent.dwProcessId,DebugEvent.dwThreadId,DBG_CONTINUE);
						break;
					}
					else if(DebugEvent.dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
					{
						DWORD	ContinueStatus = DBG_EXCEPTION_NOT_HANDLED;

						if(DebugEvent.u.Exception.dwFirstChance)
						{
							DWORD	ExceptionCode = DebugEvent.u.Exception.ExceptionRecord.ExceptionCode;

							switch(ExceptionCode)
							{
								case EXCEPTION_BREAKPOINT:
								{
									ContinueStatus = DBG_CONTINUE;
									break;
								}
								case EXCEPTION_ACCESS_VIOLATION:
								case EXCEPTION_STACK_OVERFLOW:
								case EXCEPTION_PRIV_INSTRUCTION:
								{
									// Open the main process.

									HANDLE	Process = OpenProcess(
															PROCESS_ALL_ACCESS,
															FALSE,
															DebugEvent.dwProcessId
															);

									if(!Process)
										throwFormattedError("OpenProcess failed.  GetLastError=%08x",GetLastError());

									// Enumerate modules used by the main process.

									HMODULE	Modules[1024];
									DWORD	NumModules;

									if(!EnumProcessModules(Process,Modules,1024 * sizeof(HMODULE),&NumModules))
										throwFormattedError("EnumProcessModules failed.  GetLastError=%08x",GetLastError());

									NumModules /= sizeof(HMODULE);

									// Retrieve module descriptions and put them in a linked list.

									module*	ModuleList = NULL;

									for(DWORD ModuleIndex = 0;ModuleIndex < NumModules;ModuleIndex++)
										ModuleList = new module(Process,Modules[ModuleIndex],ModuleList);

									// Log the exception code and logical exception address.

									FILE*	ReportFile = fopen("BugReport.txt","wt");

									fprintf(
										ReportFile,
										"Exception code: %08x\n",
										ExceptionCode
										);

									fprintf(
										ReportFile,
										"Exception address: %s\n",
										logicalAddress(ModuleList,DebugEvent.u.Exception.ExceptionRecord.ExceptionAddress).describe()
										);

									// Open the thread the exception occured in and get it's context.

									HANDLE	Thread = OpenThread(
															THREAD_GET_CONTEXT,
															FALSE,
															DebugEvent.dwThreadId
															);
									CONTEXT	ThreadContext;

									ZeroMemory(&ThreadContext,sizeof(CONTEXT));
									ThreadContext.ContextFlags = CONTEXT_FULL;

									if(Thread && GetThreadContext(Thread,&ThreadContext))
									{
										// Log the call stack.

										fprintf(
											ReportFile,
											"Call stack:\n"
											);

										PVOID	EIP = *(PVOID*)&ThreadContext.Eip;
										PVOID	EBP = *(PVOID*)&ThreadContext.Ebp;

										while(1)
										{
											fprintf(
												ReportFile,
												"\t%s\n",
												logicalAddress(ModuleList,EIP).describe()
												);

											PVOID	Stack[2];
											DWORD	NumBytesRead = 0;

											if(!ReadProcessMemory(Process,EBP,Stack,sizeof(PVOID) * 2,&NumBytesRead))
												break;

											MEMORY_BASIC_INFORMATION	MemoryInfo;

											if(!VirtualQueryEx(Process,Stack[0],&MemoryInfo,sizeof(MemoryInfo)))
												break;

											if((MemoryInfo.Protect & ~(PAGE_NOCACHE | PAGE_GUARD)) != PAGE_READWRITE)
												break;

											if(Stack[0] <= EBP)
												break;

											EIP = Stack[1];
											EBP = Stack[0];
										};

										CloseHandle(Thread);
									}

									CloseHandle(Process);
									fclose(ReportFile);
									break;
								}
							}
						}

						if(DebugEvent.u.Exception.ExceptionRecord.ExceptionFlags & EXCEPTION_NONCONTINUABLE)
							ContinueDebugEvent(DebugEvent.dwProcessId,DebugEvent.dwThreadId,DBG_EXCEPTION_NOT_HANDLED);
						else
							ContinueDebugEvent(DebugEvent.dwProcessId,DebugEvent.dwThreadId,ContinueStatus);
					}
					else
						ContinueDebugEvent(DebugEvent.dwProcessId,DebugEvent.dwThreadId,DBG_CONTINUE);
				}
			};
		}
	}
	catch(char* ErrorMessage)
	{
		MessageBox(NULL,ErrorMessage,"BugReport error",MB_OK);
		return -1;
	}

	return 0;
}
