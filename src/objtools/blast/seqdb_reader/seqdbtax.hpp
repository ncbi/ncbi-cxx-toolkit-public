#ifndef OBJTOOLS_READERS_SEQDB__SEQDBTAX_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDBTAX_HPP

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
 * Author:  Kevin Bealer
 *
 */

/// @file seqdbtax.hpp
/// Defines database volume access classes.
/// 
/// Defines classes:
///     CSeqDBTaxNames, CSeqDBTaxId, CSeqDBTaxInfo
/// 
/// Implemented for: UNIX, MS-Windows

#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include "seqdbfile.hpp"
#include "seqdbisam.hpp"

BEGIN_NCBI_SCOPE

/// Import definitions from the objects namespace.
USING_SCOPE(objects);

/// CSeqDBTaxId class
/// 
/// This is a memory overlay class.  Do not change the size or layout
/// of this class unless corresponding changes happen to the taxonomy
/// database file format.  This class's constructor and destructor are
/// not called; instead, a pointer to mapped memory is cast to a
/// pointer to this type, and the access methods are used to examine
/// the fields.

class CSeqDBTaxId {
public:
    /// Constructor
    ///
    /// This class is a read-only memory overlay and is not expected
    /// to ever be constructed.
    CSeqDBTaxId()
    {
        _ASSERT(0);
    }
    
    /// Return the taxonomic identifier field (in host order)
    Int4 GetTaxId()
    {
        return SeqDB_GetStdOrd(& m_Taxid);
    }
    
    /// Return the offset field (in host order)
    Int4 GetOffset()
    {
        return SeqDB_GetStdOrd(& m_Offset);
    }
    
private:
    /// This structure should not be copy constructed
    CSeqDBTaxId(const CSeqDBTaxId &);
    
    /// The taxonomic identifier
    Uint4 m_Taxid;
    
    /// The offset of the start of the taxonomy data.
    Uint4 m_Offset;
};


/// CSeqDBTaxInfo class
/// 
/// This manages access to the taxonomy database.

class CSeqDBTaxInfo : public CObject {
public:
    /// Constructor
    CSeqDBTaxInfo(CSeqDBAtlas & atlas);
    
    /// Destructor
    virtual ~CSeqDBTaxInfo();
    
    /// Get the taxonomy names for a given tax id
    ///
    /// The tax id is looked up in the taxonomy database and the
    /// corresponding strings indicating the taxonomy names are
    /// returned in the provided structure.
    ///
    /// @param tax_id
    ///   The taxonomic identiifer.
    /// @param info
    ///   A container structure in which to return the names.
    /// @param locked
    ///   The lock holder object for this thread.
    /// @return true if the taxonomic id was found
    bool GetTaxNames(Int4             tax_id,
                     SSeqDBTaxInfo  & info,
                     CSeqDBLockHold & locked);
    
private:
    /// The memory management layer
    CSeqDBAtlas & m_Atlas;
    
    /// A memory lease for the index file
    CSeqDBMemLease m_Lease;
    
    /// The filename of the taxonomic db index file
    string m_IndexFN;
    
    /// The filename of the taxnomoic db data file
    string m_DataFN;
    
    /// Total number of taxids in the database
    Int4 m_AllTaxidCount;
    
    /// Memory map of the index file
    CSeqDBTaxId * m_TaxData;
};

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBTAX_HPP


