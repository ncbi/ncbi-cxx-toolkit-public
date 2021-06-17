#ifndef ALGO_COBALT___PATTERNS__HPP
#define ALGO_COBALT___PATTERNS__HPP

/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: patterns.hpp

Author: Greg Boratyn

Contents: Interface for default CDD patterns

******************************************************************************/


/// @file patterns.hpp
/// Interface for default CDD patterns


#include <corelib/ncbistl.hpp>
#include <algo/cobalt/options.hpp>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

/// Asignes default patterns to a given vector
/// @param patterns Vector of patterns [out]
///
void AssignDefaultPatterns(vector<CMultiAlignerOptions::CPattern>& patterns);

END_SCOPE(cobalt)
END_NCBI_SCOPE

#endif /* ALGO_COBALT___COBALT_PATTERNS__HPP */
