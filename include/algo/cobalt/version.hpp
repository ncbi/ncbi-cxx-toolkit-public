#ifndef ALGO_COBALT___VERSION__HPP
#define ALGO_COBALT___VERSION__HPP

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

File name: version.hpp

Author: Greg Boratyn

Contents: MultiAligner version

******************************************************************************/

#include <corelib/version_api.hpp>
#include <algo/cobalt/cobalt.hpp>


/// @file version.hpp
/// CMultiAligner version

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)


/// Keeps track of CMultiAligner version
class CMultiAlignerVersion : public CVersionInfo 
{
public:
    CMultiAlignerVersion() : CVersionInfo(CMultiAligner::kMajorVersion,
                                          CMultiAligner::kMinorVersion,
                                          CMultiAligner::kPatchVersion)
    {}

    virtual string Print(void) const {
        return CVersionInfo::Print();
    }
};


END_SCOPE(cobalt)
END_NCBI_SCOPE

#endif /* ALGO_COBALT___VERSION__HPP */
