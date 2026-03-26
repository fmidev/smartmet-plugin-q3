/*
* MQD_TOOLS.CPP                     Copyright (c) 2008-2010, Ilmatieteen laitos
*/
#ifdef MQD_ENABLED

#include "MQD_Tools.h"
#include "Invariant.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <stdio.h>

using namespace std;


/*---=== One time initializations ===---*/

size_t getPageSize() {
    size_t ret;
#ifdef UNIX
    ret= getpagesize();
#else
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    ret= si.dwAllocationGranularity;
#endif
    assert( ret > 0 );
        //
        // Linux x86_64:    4096
        // Windows XP:      65536

    return ret;
}


/*---=== MemoryMap ===---*/

/*
* Start a (read-only) memory mapping. The caller wants a pointer to the file contents at
* 'start' (but the memory mapping will be made to start earlier, due to page size issues).
* The mapping will be valid for at least 'length' bytes (and maybe some more, again due
* to page sizes).
*
* 'length' is '(size_t)(-1)' for mapping till the end of the file
*/
MemoryMap::MemoryMap( const char *fn, size_t start, size_t length_ )
#ifdef UNIX
    : fd( open(fn,O_RDONLY) ), length(length_)
#else
    : handle()
#endif
{
#ifdef UNIX
    if (fd<0) {
        int _errno= errno;  // grab as early as you can
        throw E_LOG_BAD_FILE( fn, string_fmt("errno=%d", _errno) );
    }
#else
    #error "Code below is old; needs editing to compile for Windows."
/**
    // Open a memory mapping handle for Win32 (actual mappings are called 'views')
    //
    handle= ::CreateFileMapping(
            h_file,
            nullptr,   // default security (handle cannot be inherited)
            PAGE_READONLY,
            0,0,    // size = actual file size
            nullptr    // name
        );
    
    // Note: MSDN says 'CreateFileMapping()' fails with nullptr return value,
    //       not with INVALID_HANDLE. Win32 HANDLEs are split on this aspect.
    //
    if (h_fd == nullptr) {
        fclose(f);
        throw E_LOG_FATAL( "'CreateFileMapping() failed: %d", GetLastError() );
    }
**/
#endif

    /*
    * Windows and Linux memory mapping APIs require the mapping to begin at an even VM 
    * page size (OS X allows any offset).
    */
    const size_t page_size= getPageSize();

    size_t o_skip1= (start / page_size) * page_size;    // relying on integer division here!
    size_t o_skip2= start % page_size;

    assert( o_skip1 % page_size == 0 );     // starting of 'mmap()'
    assert( o_skip2 < page_size );          // bytes to skip after start of mmap

#ifdef UNIX
    //---
    // "You must specify exactly one of MAP_SHARED and MAP_PRIVATE."
    // 
    // "MAP_PRIVATE: Create a private copy-on-write mapping. Stores to the region
    //  do not affect the original file. It is unspecified whether changes made
    //  to the file after the mmap() call are visible in the mapped region."
    //
    // "MAP_SHARED: Share this mapping with all other processes that map this
    //  object. Storing to the region is equivalent to writing to the file."
    
    //---
    // Block length (Linux 'man mmap'):
    //
    // "A file is mapped in multiples of the page size. For a file that is not
    //  a multiple of the page size, the remaining memory is zeroed when mapped,
    //  and writes to that region are not written out to the file."

    // Remember the received pointer for destructor
    //
    mmap_ptr= mmap( 0,   // address hint (don't use)
                    o_skip2 + length,
                    PROT_READ,
                    MAP_SHARED, // "share with all other processes that map this object"
                    fd, o_skip1 );

    // NOTE: using '-Wold-style-cast' bans even using the MAP_FAILED coming from
    //       system headers, where it is defined as ((void *)-1)

    if (mmap_ptr == MAP_FAILED) {   // -1
        int _errno= errno;
        
        // [EACCES 13]  file descriptor not open for reading
        //          or: PROT_WRITE and MAP_SHARED are specified .. and file descriptor is not open for writing
        // [EBADF 9]    not a valid file descriptor
        // [EINVAL 22]  fd does not reference a regular or character special file
        //          or: flags does not include either MAP_PRIVATE or MAP_SHARED
        //          or: len is not greater than zero
        //          or: offset is not a multiple of the page size, as returned by sysconf(3)
        // [EMFILE 24]  exceeded limit on mapped regions (per process or system)
        // [ENODEV 19]  file type for fd is not supported for mapping
        // [ENOMEM 12]  MAP_ANON is specified and insufficient memory is available
        // [EOVERFLOW 75] Addresses in the specified range exceed the maximum offset set for fd

        throw E_LOG_FATAL( "Memory mapping failed: %s (errno=%d)", fn, _errno );
    }

#else
    // Windows
    //    
    void *mmap_ptr= MapViewOfFile(
        handle,
        FILE_MAP_READ,
        (DWORD)(o_aligned >> 32), (DWORD)(o_aligned),
        gap + bytes() );

    if (!mmap_ptr) {
        throw E_LOG_FATAL( "Memory map failed! %d", GetLastError() );
    }
#endif

    offset_ptr= (const void*) ( (char*)(mmap_ptr) + o_skip2 );
    
    INVARIANT();
}


/*
* Help function to tell the memory mapped file's total size.
*/
size_t MemoryMap::getFileSize() const {
#ifdef UNIX
    off_t pos= lseek( fd, 0, SEEK_END );
    assert( pos != (off_t)(-1) );
    return pos;
#else
# error "Not implemented"
#endif
}


/*
*/
MemoryMap::~MemoryMap() {
    INVARIANT();

#ifdef UNIX
    int tmp= munmap( mmap_ptr, length );
    if (tmp==-1) {
        throw E_LOG_FATAL( "'munmap()' failed: errno=%d", errno );
    }
#else
    BOOL rc= UnmapViewOfFile( mmap_ptr );
    if (!rc) {
        throw E_LOG_FATAL0( "UnmapViewOfFile()' failed" );
    }
#endif
}


#endif
    // MQD_ENABLED
