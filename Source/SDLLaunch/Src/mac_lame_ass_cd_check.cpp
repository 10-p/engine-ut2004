// Yeah, this might stop anyone...  --ryan.
#if MACOSX
#    include <sys/param.h>
#    include <sys/ucred.h>
#    include <sys/mount.h>
#    include <sys/types.h>
#    include <sys/stat.h>
#    include <CoreFoundation/CoreFoundation.h>
#    include <CoreServices/CoreServices.h>
#    include <IOKit/IOKitLib.h>
#    include <IOKit/storage/IOMedia.h>
#    include <IOKit/storage/IOCDMedia.h>
#    include <IOKit/storage/IODVDMedia.h>
#    include <unistd.h>
#    include <fcntl.h>
#    include <sys/types.h>
#    include <sys/stat.h>

static int darwinIsWholeMedia(io_service_t service)
{
    int retval = 0;
    CFTypeRef wholeMedia;

    if (!IOObjectConformsTo(service, kIOMediaClass))
        return(0);
        
    wholeMedia = IORegistryEntryCreateCFProperty(service,
                                                 CFSTR(kIOMediaWholeKey),
                                                 kCFAllocatorDefault, 0);
    if (wholeMedia == NULL)
        return(0);

    retval = CFBooleanGetValue(wholeMedia);
    CFRelease(wholeMedia);

    return retval;
} /* darwinIsWholeMedia */


static int darwinIsMountedDisc(char *bsdName, mach_port_t masterPort)
{
    int retval = 0;
    CFMutableDictionaryRef matchingDict;
    kern_return_t rc;
    io_iterator_t iter;
    io_service_t service;

    if ((matchingDict = IOBSDNameMatching(masterPort, 0, bsdName)) == NULL)
        return(0);

    rc = IOServiceGetMatchingServices(masterPort, matchingDict, &iter);
    if ((rc != KERN_SUCCESS) || (!iter))
        return(0);

    service = IOIteratorNext(iter);
    IOObjectRelease(iter);
    if (!service)
        return(0);

    rc = IORegistryEntryCreateIterator(service, kIOServicePlane,
             kIORegistryIterateRecursively | kIORegistryIterateParents, &iter);
    
    if (!iter)
        return(0);

    if (rc != KERN_SUCCESS)
    {
        IOObjectRelease(iter);
        return(0);
    } /* if */

    IOObjectRetain(service);  /* add an extra object reference... */

    do
    {
        if (darwinIsWholeMedia(service))
        {
            if ( (IOObjectConformsTo(service, kIOCDMediaClass)) ||
                 (IOObjectConformsTo(service, kIODVDMediaClass)) )
            {
                retval = 1;
            } /* if */
        } /* if */
        IOObjectRelease(service);
    } while ((service = IOIteratorNext(iter)) && (!retval));
                
    IOObjectRelease(iter);
    IOObjectRelease(service);

    return(retval);
} /* darwinIsMountedDisc */


int IsDiscInserted(void)
{
return 1;  // MacSoft gave clearance to remove disc check for next patch on 02/11/2005. --ryan.
    char f[128];
    const char *ansiFindThis = f;
    const char *devPrefix = "/dev/";
    int prefixLen = strlen(devPrefix);
    mach_port_t masterPort = 0;
    struct statfs *mntbufp;
    int mounts;
    int i = 0;

    // "UNREAL_COVER_final.png"
    f[i++] = 'U'; f[i++] = 'N'; f[i++] = 'R'; f[i++] = 'E'; f[i++] = 'A';
    f[i++] = 'L'; f[i++] = '_'; f[i++] = 'C'; f[i++] = 'O'; f[i++] = 'V';
    f[i++] = 'E'; f[i++] = 'R'; f[i++] = '_'; f[i++] = 'f'; f[i++] = 'i';
    f[i++] = 'n'; f[i++] = 'a'; f[i++] = 'l'; f[i++] = '.'; f[i++] = 'p';
    f[i++] = 'n'; f[i++] = 'g'; f[i++] =  0 ; f[i++] =  0 ; f[i++] =  0 ;

    if (IOMasterPort(MACH_PORT_NULL, &masterPort) == KERN_SUCCESS)
    {
        mounts = getmntinfo(&mntbufp, MNT_WAIT);  /* NOT THREAD SAFE! */
        for (i = 0; i < mounts; i++)
        {
            char *dev = mntbufp[i].f_mntfromname;
            char *mnt = mntbufp[i].f_mntonname;
            if (strncmp(dev, devPrefix, prefixLen) != 0)  /* virtual device? */
                continue;

            /* darwinIsMountedDisc needs to skip "/dev/" part of string... */
            if (darwinIsMountedDisc(dev + prefixLen, masterPort))
            {
                int fd;
                char buf[MAXPATHLEN];
                struct stat statbuf;
                snprintf(buf, sizeof (buf), "%s/%s", mnt, ansiFindThis);
                if ((fd = open(buf, O_RDONLY)) != -1)
                {
                    if (fstat(fd, &statbuf) != -1)
                    {
                        if (statbuf.st_size == 193551)  /* the "right" file. */
                        {
                            /*
                             * Leave file open for duration of program. This
                             *  prevents people from ejecting the disc and
                             *  handing it around a LAN party.
                             */
                            return(1);
                        }
                    }
                    close(fd);
                }
            }
        }
    }

    return(0);  /* no valid disc. */
}
#endif
