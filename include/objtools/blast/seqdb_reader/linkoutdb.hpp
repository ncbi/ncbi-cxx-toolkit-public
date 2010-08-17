#ifndef OBJTOOLS_BLAST_SEQDB_READER___LINKOUTDB__HPP
#define OBJTOOLS_BLAST_SEQDB_READER___LINKOUTDB__HPP

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

/// @file linkoutdb.hpp
/// Defines classes to access the Linkout DB that maps SeqIDs (GI/accession) to
/// linkout bits

#include <corelib/ncbistl.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/seqloc/Seq_id.hpp>

BEGIN_NCBI_SCOPE

// forward declaration
class CLinkoutDB_Impl;

/// Interface to the linkout DB, which encode the mapping between sequence IDs
/// (GIs/accessions) to linkout bits (as defined in defline_extra.hpp).
/// Starting the fall 2010, the linkouts will be migrated from the ASN.1
/// deflines in the BLAST DBs header files to the linkout DB.
class NCBI_XOBJREAD_EXPORT CLinkoutDB : public CObject {
public:
    /// Default construtor, uses the 'linkouts' as the base name of the indices
    /// to perform its linkout lookups
    /// @throw CSeqDBException if the indices are not found
    CLinkoutDB();

    /// Parametrized constructor, uses its argument to initialized the linkout 
    /// indices
    /// @throw CSeqDBException if the indices are not found
    CLinkoutDB(const string& dbname);

    /// Destructor
    ~CLinkoutDB();
    
    /// Obtain the linkout bits for a given gi
    /// @param gi GI of interest [in]
    /// @return integer encoding linkout bits or 0 if not found
    int GetLinkout(int gi);

    /// Obtain the linkout bits for a given CSeq_id
    /// @param id Sequence identifier of interest [in]
    /// @return integer encoding linkout bits or 0 if not found
    int GetLinkout(const objects::CSeq_id& id);

private:
    /// The actual implementation of this class
    CLinkoutDB_Impl* m_Impl;
    /// Prohibit copy constructor
    CLinkoutDB(const CLinkoutDB& rhs);
    /// Prohibit assignment operator
    CLinkoutDB operator=(const CLinkoutDB& rhs);
};

END_NCBI_SCOPE

#endif // OBJTOOLS_BLAST_SEQDB_READER___LINKOUTDB__HPP

