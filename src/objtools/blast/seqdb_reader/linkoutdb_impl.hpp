#ifndef OBJTOOLS_BLAST_SEQDB_READER___LINKOUTDB_IMPL__HPP
#define OBJTOOLS_BLAST_SEQDB_READER___LINKOUTDB_IMPL__HPP

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

/// @file linkoutdb_impl.hpp
/// Defines implementation classes to access the Linkout DB that maps SeqIDs
/// (GI/accession) to linkout bits

#include "seqdbisam.hpp"

BEGIN_NCBI_SCOPE

/// Include definitions from the objects namespace.
USING_SCOPE(objects);

/// Implementation class to support the linkout DB
class NCBI_XOBJREAD_EXPORT CLinkoutDB_Impl : public CObject {
public:
    /// Parametrized constructor, uses its argument to initialized the linkout 
    /// indices
    /// @throw CSeqDBException if the indices are not found
    CLinkoutDB_Impl(const string& dbname);
    
    /// Obtain the linkout bits for a given gi
    /// @param gi GI of interest [in]
    /// @return integer encoding linkout bits or 0 if not found
    int GetLinkout(int gi);

    /// Obtain the linkout bits for a given CSeq_id
    /// @param id Sequence identifier of interest [in]
    /// @return integer encoding linkout bits or 0 if not found
    int GetLinkout(const CSeq_id& id);

private:
    void x_Init(const string& dbname);
    CSeqDBAtlasHolder m_AtlasHolder;
    CSeqDBAtlas& m_Atlas;
    CRef<CSeqDBIsam> m_Accession2Linkout;
    CRef<CSeqDBIsam> m_Gi2Linkout;
};

END_NCBI_SCOPE

#endif // OBJTOOLS_BLAST_SEQDB_READER___LINKOUTDB_IMPL__HPP

