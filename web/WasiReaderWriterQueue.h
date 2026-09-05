#pragma once

// readerwriterqueue's POSIX semaphore implementation works with WASI threads,
// but its platform detection predates __wasi__.
#if defined(__wasi__) && !defined(__unix__)
 #define __unix__ 1
 #define CLAP_SAW_UNDEFINE_UNIX 1
#endif

#include <readerwriterqueue.h>

#if defined(CLAP_SAW_UNDEFINE_UNIX)
 #undef CLAP_SAW_UNDEFINE_UNIX
 #undef __unix__
#endif
