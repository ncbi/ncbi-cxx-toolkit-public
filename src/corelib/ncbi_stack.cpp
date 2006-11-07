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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_stack.hpp>

#ifdef NCBI_OS_SOLARIS
#  include <sys/ucontext.h> // for additional test below
#endif

#ifdef NCBI_OS_MSWIN
#  include "ncbi_stack_win32.cpp"
#elif defined NCBI_OS_SOLARIS  &&  defined(GETUSTACK)
#  include "ncbi_stack_solaris.cpp"
#elif defined NCBI_OS_LINUX
#  include "ncbi_stack_linux.cpp"
#else
#  include "ncbi_stack_default.cpp"
#endif


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2006/11/07 15:56:04  ucko
 * Don't use ncbi_stack_solaris.cpp on versions too old to support walkcontext().
 *
 * Revision 1.1  2006/11/06 17:37:39  grichenk
 * Initial revision
 *
 * ===========================================================================
 */
