#if defined(NCBI_USE_PCH)

#  ifndef NCBI_PCH__HPP
#  define NCBI_PCH__HPP
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
 */

/** @file ncbi_pch.hpp
 ** Header file to be pre-compiled and speed up build of NCBI C++ Toolkit
 **/

// All of the below headers appear in >40% of C++ Toolkit compilation
// units.  (So do about a dozen other corelib headers, but these
// indirectly include all the rest.)

#include <corelib/ncbimtx.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbi_limits.hpp>

// Third Party Libraries specific includes
#ifdef NCBI_WXWIN_USE_PCH
#  include <wx/wxprec.h>
#endif
/*
 * ===========================================================================
 * $Log$
 * Revision 1.9  2005/04/20 16:31:24  vakatov
 * '  #include'  -->  '#  include'
 *
 * Revision 1.8  2004/06/29 14:46:15  ucko
 * Reduce to a non-redundant handful of headers with >40% usage.
 *
 * Revision 1.7  2004/06/01 13:16:59  gorelenk
 * Changed PCH for wxWindows to wxprec.h
 *
 * Revision 1.6  2004/05/26 19:32:35  gorelenk
 * Commented accidentaly restored include of ncbi_safe_static.hpp
 *
 * Revision 1.5  2004/05/26 18:45:10  gorelenk
 * Added section for using wxWindows headers in PCH .
 *
 * Revision 1.3  2004/05/14 16:46:02  gorelenk
 * Comment some rarely used includes
 *
 * Revision 1.2  2004/05/14 14:14:14  gorelenk
 * Changed ifdef
 *
 * Revision 1.1  2004/05/14 13:58:11  gorelenk
 * Initial revision
 *
 * ===========================================================================
 */

#  endif /* NCBI_PCH__HPP */

#endif /* NCBI_USE_PCH */
