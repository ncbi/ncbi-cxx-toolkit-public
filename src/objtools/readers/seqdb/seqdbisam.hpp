#ifndef OBJTOOLS_READERS_SEQDB__SEQDBISAM_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDBISAM_HPP

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

/// Isam access objects for CSeqDB
///
/// This object defines access to the various ISAM index files.

#include "seqdbfile.hpp"

BEGIN_NCBI_SCOPE

/// ISAM file.
///
/// Manages one ISAM file, which will translate either PIGs, GIs, or
/// Accessions to OIDs.  Translation in the other direction is done in
/// the CSeqDBVol code.

class CSeqDBIsam : public CObject {
public:
    enum EIdentType { eGi, ePig, eStringID };
    
    enum EIsamDbType {
        eNumeric         = 0,
        eNumericNoData   = 1,
        eString          = 2,
        eStringDatabase  = 3,
        eStringBin       = 4
    };
    
    typedef CSeqDBAtlas::TIndx TIndx;
    
    typedef Uint4 TOid;
    typedef Uint4 TPig;
    typedef Uint4 TGi;
    
    CSeqDBIsam(CSeqDBAtlas  & atlas,
               const string & dbname,
               char           prot_nucl,
               char           file_ext_char,
               EIdentType     ident_type);
    
    ~CSeqDBIsam();
    
    bool PigToOid(TPig pig, TOid & oid, CSeqDBLockHold & locked)
    {
        _ASSERT(m_IdentType == ePig);
        return x_IdentToOid(pig, oid, locked);
    }
    
    bool GiToOid(TGi gi, TOid & oid, CSeqDBLockHold & locked)
    {
        _ASSERT(m_IdentType == eGi);
        return x_IdentToOid(gi, oid, locked);
    }
    
    void UnLease();
    
private:
    enum EErrorCode {
        eNotFound        =  1,
        eNoError         =  0,
        eNoSpace         =  -2,
        eBadFileName     =  -3,
        eNotImplemented  =  -4,
        eMemMap          =  -5,
        eNoOrder         =  -6,
        eNoCorrelation   =  -7,
        eBadParameter    =  -8,
        eNoMemory        =  -9,
        eBadVersion      =  -10,
        eBadType         =  -11,
        eWrongFile       =  -12,
        eFseekFailed     =  -13,
        eInvalidFormat   =  -14,
        eMiscError       =  -99
    };
    
    struct SNumericKeyData {
        Uint4 key;
        Uint4 data;
    };
    
    bool x_IdentToOid(Uint4            id,
                      TOid           & oid,
                      CSeqDBLockHold & locked);
    
    bool x_AccToOids(const string   & acc,
                     vector<TOid>   & oid,
                     CSeqDBLockHold & locked);
    
    EErrorCode
    x_SearchIndexNumeric(Uint4            Number, 
                         Uint4          * Data,
                         Uint4          * Index,
                         Int4           & SampleNum,
                         bool           & done,
                         CSeqDBLockHold & locked);
    
    EErrorCode
    x_SearchDataNumeric(Uint4            Number,
                        Uint4          * Data,
                        Uint4          * Index,
                        Int4             SampleNum,
                        CSeqDBLockHold & locked);
    
    EErrorCode
    x_NumericSearch(Uint4            Number,
                    Uint4          * Data,
                    Uint4          * Index,
                    CSeqDBLockHold & locked);
    
    EErrorCode
    x_InitSearch(CSeqDBLockHold & locked);
    
    Int4 x_GetPageNumElements(Int4       SampleNum,
                              Int4     * Start);
    
    // Data
    
    CSeqDBAtlas  & m_Atlas;
    EIdentType     m_IdentType;
    EIsamDbType    m_IsamType;
    
    CSeqDBMemLease m_IndexLease;
    CSeqDBMemLease m_DataLease;
    
    Int4           m_Type;
    
    string         m_DataFname;       // Filename of database file
    string         m_IndexFname;      // Filename of ISAM index file
    
    Int4           m_NumTerms;        // Number of terms in database
    Int4           m_NumSamples;      // Number of terms in ISAM index
    Int4           m_PageSize;        // Page size of ISAM index
    Int4           m_MaxLineSize;     // Maximum string length in the database
    Int4           m_IdxOption;       // Options set by upper layer
    bool           m_Initialized;     // Was this structure initialized
                                      // for ISAM Search ?
    TIndx          m_KeySampleOffset; // Offset of samples in index file.
    bool           m_TestNonUnique;   // Check if data for String ISAM sorted
    char         * m_FileStart;       // Pointer to index file if no memmap
    Int4           m_FirstOffset;     // First and last offset's of last page.
    Int4           m_LastOffset;      // First and last offset's of last page.
};

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBFILE_HPP


