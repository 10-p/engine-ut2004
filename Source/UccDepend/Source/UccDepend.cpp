// Note: it's hard-coded to be run in the directory containing the system directory.
// if pass "-full" as an argument, it purges the .u's and rebuilds them regardless of their date.
// if pass "-debug" as an argument, it builds .u's in debug mode

#define WIN32_LEAN_AND_MEAN // makes intel compiler happy

#pragma pack(push, 8)
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <process.h>
#include <direct.h>
#pragma pack(pop)

#define MAX_LINE_LENGTH      (unsigned)(1024)
#define EDIT_PACKAGES_STRING "EditPackages"
#define MAX_LOCKED_RETRIES   10

enum PackageState { upToDate, outOfDate, absent };

bool deletePackage (const char *packageName, int depth = 0)
{
    int rc;

    char packageFile [MAX_PATH + 1];

    if ( depth > MAX_LOCKED_RETRIES )
    {
        printf ("ERROR: %s is locked!\n", packageName);
        return (false);
    }

    strcpy (packageFile, "System\\");
    strcat (packageFile, packageName);
    strcat (packageFile, ".u");

    rc = DeleteFile (packageFile);

    if( !rc )
    {
        DWORD err = GetLastError();

        if( err == ERROR_FILE_NOT_FOUND )
            return( true );
        else if(( err == ERROR_ACCESS_DENIED ) || ( err == ERROR_LOCK_VIOLATION ) || ( err == ERROR_SHARING_VIOLATION ))
        {
            fprintf (stdout, "WARNING: %s is locked (waiting).\n", packageFile);
            fflush (stdout);
            Sleep (1000);

            return( deletePackage( packageName, depth + 1 ));
        }
    }

    return (true);
}

bool CheckIsExcluded( WIN32_FIND_DATA *findFileData )
{
	static const char *excludeFiles[] =
	{
		".",
        ".."
	};

	static const char *excludeExts[] =
	{
		"scc",
        "exe",
        "lib",
        "exp",
        "cpp",
        "h",
        "idb",
        "pdb",
        "sbr",
        "pch",
        "cod",
        "dll",
        "dsp",
        "opt",
        "dds"
	};

    int u;

	for (u = 0; u < sizeof (excludeFiles) / sizeof (excludeFiles[0]); u++)
    {
		if ( stricmp( findFileData->cFileName, excludeFiles[u] ) == 0 )
			return true;
	}

	for (u = 0; u < sizeof (excludeExts) / sizeof (excludeExts[0]); u++)
    {
		size_t l = strlen( findFileData->cFileName );
		if ( stricmp( &findFileData->cFileName[l-3], excludeExts[u] ) == 0 )
			return true;
	}
	return false;
}

PackageState checkPackageFiles (FILETIME oPackageDate, const char *path);

PackageState checkFile (FILETIME oPackageDate, const char *path, WIN32_FIND_DATA *oFindFileData)
{
    if (CheckIsExcluded( oFindFileData ))
        return (upToDate);

    if (oFindFileData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        char sourceFiles [MAX_PATH + 1];

        strcpy (sourceFiles, path);
        strcat (sourceFiles, oFindFileData->cFileName);
        strcat (sourceFiles, "\\");

        return (checkPackageFiles (oPackageDate, sourceFiles));
    }

    if (CompareFileTime (&oPackageDate, &oFindFileData->ftLastWriteTime) < 0)
        return (outOfDate);

    return (upToDate);
}

PackageState checkPackageFiles (FILETIME oPackageDate, const char *path)
{
    HANDLE hFind;
    WIN32_FIND_DATA oFindFileData;

    char sourceFiles [MAX_PATH + 1];

    strcpy (sourceFiles, path);
    strcat (sourceFiles, "*.*");

    PackageState p;

    hFind = FindFirstFile (sourceFiles, &oFindFileData);

    if (hFind == INVALID_HANDLE_VALUE)
        return (upToDate);

    p = checkFile (oPackageDate, path, &oFindFileData);

    if (p != upToDate)
    {
        FindClose (hFind);
        return (p);
    }

    while (FindNextFile (hFind, &oFindFileData))
    {
        p = checkFile (oPackageDate, path, &oFindFileData);

        if (p != upToDate)
        {
            FindClose (hFind);
            return (p);
        }
    }

    FindClose (hFind);
    return (upToDate);
}

PackageState checkPackage (const char *packageName)
{
    HANDLE hFind;

    char packageFile [MAX_PATH + 1];
    FILETIME oPackageDate;
    WIN32_FIND_DATA oFindFileData;

    char sourceFiles [MAX_PATH + 1];

    static const char *subDirs[] =
    {
        "Classes",
        "Models",
        "Textures",
        "Sounds",
    };

    unsigned u;

    strcpy (packageFile, "System\\");
    strcat (packageFile, packageName);
    strcat (packageFile, ".u");

    hFind = FindFirstFile (packageFile, &oFindFileData);

    if (hFind == INVALID_HANDLE_VALUE)
        return (absent);

    oPackageDate = oFindFileData.ftLastWriteTime;

	for (u = 0; u < sizeof (subDirs) / sizeof (subDirs[0]); u++)
    {
        strcpy (sourceFiles, packageName);
        strcat (sourceFiles, "\\");
        strcat (sourceFiles, subDirs[u]);
        strcat (sourceFiles, "\\");

    	if (checkPackageFiles (oPackageDate, sourceFiles) == outOfDate)
            return(outOfDate);
    }

    return(upToDate);
}

int main (char argc, char *argv[])
{
    FILE *iniFile=0;
    char line [MAX_LINE_LENGTH];
	char iniText [MAX_PATH];
    char packageName [MAX_PATH];

	char uccArgs[128] = "-silentbuild";	// automatically answer yes to all prompts
	// Skip the bind for debug builds as it's a pain in the ass but leave it in for release as it
    // can occasionally find problems.
//    #ifdef _DEBUG
//        strcat(uccArgs, " -nobind");	// do not bind to dll's
//    #endif
    bool cleaning, linking, mustRunUcc, bLink;  
	cleaning = linking = mustRunUcc = bLink = false;

	iniText[0] = 0;
    printf ("Starting %s...\n", argv[0]);

	getcwd(line, MAX_PATH);
    if (argc > 1)
    {
		INT i=0;

		while ( ++i<argc )
		{
			if ( iniFile == NULL ) 
			{
				if ( *argv[i] != '-' )
				{
					iniFile = fopen (argv[i], "rt");
					if (iniFile == NULL)
						strcpy(iniText, argv[i]);
				}

			}

			if (*argv[i] == '-')
			{
				if (!stricmp(argv[i],"-link"))
					bLink = true;

				else if (!stricmp(argv[i],"-full"))
				{
					cleaning = true;
					mustRunUcc = true;
				}
				else if ( iniFile == NULL && strstr( _strlwr(argv[i]), "-userini=") )
				{
					char* c = strchr(argv[i], '=');
					iniFile = fopen( ++c, "rt" );
					if ( iniFile == NULL )
						strcpy(iniText, argv[i]);
				}
				else if (stricmp(argv[i],"-silentbuild"))
				{
					strcat(uccArgs, " ");
					strcat(uccArgs, argv[i]);
				}
//				else
//				{
//					printf ("ERROR: Syntax 'UccDepend [-full] [-link] [ucc args]'\n");
//					return (1);
//				}
			}
		}
    }

 	while ( strstr(line, "System") )
	{
		chdir("..");
		getcwd(line, MAX_PATH);
	}

	if (iniFile == NULL)
		iniFile = fopen ("System\\Default.ini", "rt");

    if (iniFile == NULL)
    {
        printf ("ERROR: Could not open ini!\n");
        fflush (stdout);
        return (1);
    }

	if (*iniText)
	{
		printf("Could not find file %s so using System\\Default.ini\n", iniText);
	}

	line[0] = 0;
    while (fgets (line, MAX_LINE_LENGTH, iniFile))
    {
        const char *p;
        char *q;

        for (p = line; isspace (*p); p++);

        if (*p == ';')
            continue;

        if (strnicmp (p, EDIT_PACKAGES_STRING, strlen (EDIT_PACKAGES_STRING)))
            continue;

        p += strlen (EDIT_PACKAGES_STRING);

        while (isspace (*p))
            p++;

        if (*p != '=')
            continue;
        else
            p++;

        while (isspace (*p))
            p++;

        q = packageName;

        while (!isspace (*p) && (*p != '\0'))
        {
            *q = *p;
            q++;
            p++;
        }

        *q = '\0';

        if (!cleaning)
        {
            switch (checkPackage (packageName))
            {
                case upToDate:
                    break;

                case outOfDate:
                {
                    printf ("%s is out of date.\n", packageName);

                    mustRunUcc = true;
					if (bLink)
					{
						linking = true;
						cleaning = true;
					}

                    if (!deletePackage (packageName))
                        return (1);

                    break;
                }

                case absent:
                {
                    printf ("%s not found.\n", packageName);
                    mustRunUcc = true;
					if (bLink)
					{
						linking = true;
						cleaning = true;
					}

                    break;
                }
            }
        }
        else
        {
            if (!deletePackage (packageName))
                return (1);

			else if (linking)
				printf("Deleting linked package '%s'.\n", packageName);
        }
    }

    fclose (iniFile);

    fflush (stdout);

    if (!mustRunUcc)
    {
        printf ("Done - packages already built.\n");
        return (0);
    }

    _chdir ("System");
 
	printf ("Running ucc make %s%s%s\n", uccArgs, bLink ? " -link" : "", cleaning ? " -full" : "");
    fflush (stdout);
    return (_spawnl (_P_WAIT, "ucc", "ucc", "make", uccArgs, NULL));
}

