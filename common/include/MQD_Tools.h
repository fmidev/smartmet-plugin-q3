/*
* MQD_TOOLS.H                       Copyright (c) 2008-2010, Ilmatieteen laitos
*/
#ifndef MMAPTOOLS_H
#define MMAPTOOLS_H

#ifndef MQD_ENABLED
# error "Should not have read this header."
#endif

#include "Tools.h"

#ifdef UNIX
# include <sys/mman.h>   // mmap(), munmap()
# include <unistd.h>
#endif

/*
* Note: Boost has memory mapping API, but it's a rather thin API around the Win32/Posix features.
*       We're not needing Boost otherwise (Newbase does, though) so let's not start here.
*       -- AKa 24-Aug-2009
*/
class MemoryMap {
  public:
    MemoryMap( const char *fn, size_t offset, size_t length ) throw (E_BAD_FILE);
    ~MemoryMap();

    const void *getPointer() const { return offset_ptr; }

    size_t getFileSize() const;

  private:    

#ifdef UNIX
    const int fd;     // file descriptor (>=3)
#else
    /*const*/ HANDLE handle;  // Win32 handle for file mapping
#endif

    const void *offset_ptr;    // pointer to byte at 'offset' (see constructor)
#ifdef UNIX
    void *mmap_ptr;      // pointer received from 'mmap()' (page aligned); needed by 'munmap()'
    size_t length;      // needed by 'munmap()'
#endif

#ifndef NDEBUG
    void _INVARIANT( const char *file, unsigned line ) const {
#ifdef UNIX
        assert_invariant( fd >= 3 );  // 0,1,2 are for stdio..stderr
        assert_invariant( length != (size_t)(-1) );     // constructor has placed real length there
        assert_invariant( mmap_ptr );
#else
        assert_invariant( handle != NULL );
#endif
        assert_invariant( offset_ptr );
    }
#endif
};

#endif
    // MQD_TOOLS_H
