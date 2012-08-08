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
 * Author:  Christiam Camacho
 *
 */

/// @file ilinkoutdb.hpp
/// Declares the ILinkoutDB interface

#ifndef OBJTOOLS_ALIGN_FORMAT___ILINKOUTDB_HPP
#define OBJTOOLS_ALIGN_FORMAT___ILINKOUTDB_HPP

#include <objects/seqloc/Seq_id.hpp>

/**setting up scope*/
BEGIN_NCBI_SCOPE
BEGIN_SCOPE(align_format)

/// Interface to LinkoutDB
class NCBI_ALIGN_FORMAT_EXPORT ILinkoutDB : public CObject {
public:
    /// Retrieve the Linkout for a given GI
    /// @param gi GI of interest [in]
    /// @param mv_build_name MapViewer build name for this GI [in]
    /// @return integer encoding linkout bits or 0 if not found
    virtual int GetLinkout(int gi, const string& mv_build_name) = 0;

    /// Retrieve the Linkout for a given Seq-id
    /// @param id Seq-id of interest [in]
    /// @param mv_build_name MapViewer build name for this Seq-id [in]
    /// @return integer encoding linkout bits or 0 if not found
    virtual int GetLinkout(const objects::CSeq_id& id, const string& mv_build_name) = 0;
};


END_SCOPE(align_format)
END_NCBI_SCOPE

#endif /* OBJTOOLS_ALIGN_FORMAT___ILINKOUTDB_HPP */
