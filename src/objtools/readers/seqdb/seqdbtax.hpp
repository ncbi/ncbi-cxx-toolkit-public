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

#include <objtools/readers/seqdb/seqdb.hpp>
#include "seqdbfile.hpp"
#include "seqdbisam.hpp"

BEGIN_NCBI_SCOPE

using namespace ncbi::objects;

/// CSeqDBTaxNames class
/// 
/// This class is used as a simple container to return taxonomic data.

class CSeqDBTaxNames {
public:
    CSeqDBTaxNames()
        : m_TaxId(0)
    {
    }
    
    Int4   GetTaxId()      { return m_TaxId; }
    string GetSciName()    { return m_SciName; }
    string GetCommonName() { return m_CommonName; }
    string GetBlastName()  { return m_BlastName; }
    string GetSKing()      { return m_SKing; }
    
    void SetTaxId     (Int4 v)   { m_TaxId      = v; }
    void SetSciName   (string v) { m_SciName    = v; }
    void SetCommonName(string v) { m_CommonName = v; }
    void SetBlastName (string v) { m_BlastName  = v; }
    void SetSKing     (string v) { m_SKing      = v; }
    
private:
    Int4   m_TaxId;
    string m_SciName;
    string m_CommonName;
    string m_BlastName;
    string m_SKing;
};

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
    // Swaps and returns the tax id field.
    Int4 GetTaxId()
    {
        return SeqDB_GetStdOrd(& m_Taxid);
    }
    
    // Swaps and returns the offset field.
    Int4 GetOffset()
    {
        return SeqDB_GetStdOrd(& m_Offset);
    }
    
private:
    CSeqDBTaxId(const CSeqDBTaxId &);
    
    Uint4 m_Taxid;
    Uint4 m_Offset;
};

/// CSeqDBTaxInfo class
/// 
/// This manages access to the taxonomy database.

class CSeqDBTaxInfo : public CObject {
public:
    CSeqDBTaxInfo(CSeqDBAtlas & atlas);
    
    virtual ~CSeqDBTaxInfo();
    
    bool GetTaxNames(Int4             tax_id,
                     CSeqDBTaxNames & tnames,
                     CSeqDBLockHold & locked);
    
private:
    CSeqDBAtlas    & m_Atlas;
    CSeqDBMemLease   m_Lease;
    string           m_IndexFN;
    string           m_DataFN;
    
    Int4             m_AllTaxidCount; /* Total number of taxids in the database */
    CSeqDBTaxId    * m_TaxData;         // Mapped array of TaxId objects.
};

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBTAX_HPP


