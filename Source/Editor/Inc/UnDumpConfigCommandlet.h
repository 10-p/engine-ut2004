/*=========================================================================================
	UnDumpConfigCommandlet.h: Dumps all configurable variables in the game to an ini file.
	Revision history:
		* Created by Ron Prestenback
===========================================================================================*/

class UDumpConfigCommandlet : public UCommandlet
{
	DECLARE_CLASS(UDumpConfigCommandlet,UCommandlet,CLASS_Transient,Editor)
	void StaticConstructor();
	INT Main( const TCHAR* Parms );

	void DumpPackage( UPackage* Package );
	void DumpClass( UClass* Cls );

	// If true, GConfig will be allowed to add sections to the dump .ini files
	// Should be used when generating the initial dump files, false thereafter
	UBOOL bInit;

	// Controls whether objects should load their values from default.ini prior to dumping the configuration
	UBOOL bNoLoad;
};
