#ifndef OBJTOOLS_WRITERS_WRITEDB__MASK_INFO_REGISTRY_HPP
#define OBJTOOLS_WRITERS_WRITEDB__MASK_INFO_REGISTRY_HPP

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

/// @file mask_info_registry.hpp
/// Declares CMaskInfoRegistry class

#include <objects/blastdb/Blast_filter_program.hpp>
#include <set>

#ifndef SKIP_DOXYGEN_PROCESSING
BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
#endif /* SKIP_DOXYGEN_PROCESSING */

/// Registry class for the sequence masking/filtering algorithms used to create
/// masks to be added to a CWriteDB object. Encapsulates the logic of assigning
/// IDs to identify masking information for CWriteDB's internal use
class NCBI_XOBJWRITE_EXPORT CMaskInfoRegistry
{
public:
    /// Attempt to register the information about a masking algorithm 
    /// @param program Filtering program used [in]
    /// @param options options passed to this filtering program. Default value
    /// implies that default parameters were used with this filtering program
    /// [in]
    /// @throw CWriteDBException on error
    int Add(EBlast_filter_program program, const string& options = string());

    /// Verify whether the provided algorithm ID has been registered with this
    /// object
    /// @param algo_id algorithm ID to validate [in]
    bool IsRegistered(int algo_id) const {
        return m_UsedIds.find(algo_id) != m_UsedIds.end();
    }

private:
    std::set<int> m_UsedIds;
    int x_FindNextValidIdWithinRange(int start, int stop);
    int x_AssignId(int start, int end);
    int x_AssignId(int start, int end, bool use_start);
};

END_NCBI_SCOPE


#endif // OBJTOOLS_WRITERS_WRITEDB__MASK_INFO_REGISTRY_HPP


