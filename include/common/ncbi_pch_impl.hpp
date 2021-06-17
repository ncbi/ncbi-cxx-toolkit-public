#if !defined(NCBI_USE_PCH)  ||  !defined(NCBI_PCH__HPP)
#  error "Must not use this header alone, but from a proper wrapper."
#endif

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

/** @file common/ncbi_pch_impl.hpp
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
