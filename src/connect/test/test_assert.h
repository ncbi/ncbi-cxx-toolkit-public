#ifndef TEST_ASSERT__H
#define TEST_ASSERT__H

/* $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Setup #NDEBUG and #_DEBUG preprocessor macro in a way that ASSERTs
 *   will be active even in the "Release" mode (it's useful for test apps).
 *   Special wrapper for shared CONNECT library use in both C and C++ tkits.
 *
 */

#include "../ncbi_config.h"
#ifdef NCBI_OS_BSD
#  include <sys/param.h>
#  ifdef __FreeBSD_version
#    if __FreeBSD_version / 100000 == 8
     /* If a client orderly closes a data connection and does some reconnect
      * attempts (connect / close in rather rapid succession), while the
      * server side proceeds with closing the original connection, FreeBSD
      * 8.0 sometimes returns -1 from the server's close() with errno set to
      * "Connection reset by peer".
      * We consider this behavior as a kernel bug / race condition as it
      * disappears if either server or client (or both) get ktrace'd.
      */
#      define TEST_IGNORE_CLOSE 1
#    endif /*__FreeBSD_version/100000==8*/
#  endif /*__FreeBSD_version*/
#endif /*NCBI_OS_BSD*/
#include "test_assert_impl.h"

#endif  /* TEST_ASSERT__H */
