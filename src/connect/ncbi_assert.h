#ifndef CONNECT___NCBI_ASSERT__H
#define CONNECT___NCBI_ASSERT__H

/*  $Id$
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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *    Run-time debugging
 */

#include "ncbi_config.h"

#if defined(verify)
#undef verify
#endif

#if !defined(NDEBUG)  &&  !defined(_DEBUG)
#  define NDEBUG
#endif
#include <assert.h>
#if defined(NDEBUG)
#  define verify(expr)  (void)(expr)
#else
#  ifdef NCBI_COMPILER_METROWERKS
     /* The following 2 headers are only required for Codewarrior
      * on Mac to prototype printf() and abort() respectively */
#    include <stdio.h>
#    include <stdlib.h>
#  endif
#  define verify(expr)  assert(expr)
#endif


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 1.1  2005/04/20 18:11:23  lavr
 * Initial revision
 *
 * ==========================================================================
 */

#endif /* CONNECT___NCBI_ASSERT__H */
