#ifndef COMMON___BOOST_SKEW_GUARD__HPP
#define COMMON___BOOST_SKEW_GUARD__HPP

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
 * Author:  Aaron Ucko
 *
 */

/// @file boost_skew_guard.hpp
/// Help catch Boost accidental version skew.

#include <ncbiconf.h>

#ifdef NCBI_EXPECTED_BOOST_VERSION
#  include <corelib/ncbistl.hpp>
#  include <boost/version.hpp>
static_assert(BOOST_VERSION == NCBI_EXPECTED_BOOST_VERSION,
    "Boost version skew detected; please remember to use $(BOOST_INCLUDE)!"
    " (Expected " NCBI_AS_STRING(NCBI_EXPECTED_BOOST_VERSION)
    ", but got " NCBI_AS_STRING(BOOST_VERSION) ".)");
#endif

#endif  /* COMMON___BOOST_SKEW_GUARD__HPP */
