#ifndef BLOB_ID__HPP_INCLUDED
#define BLOB_ID__HPP_INCLUDED
/* */

/*  $Id$
* ===========================================================================
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
* ===========================================================================
*
*  Author:  Eugene Vasilchenko
*
*  File Description: Loaded Blob ID
*
*/

#include <corelib/ncbiobj.hpp>
#include <utility>
#include <string>
#include <objmgr/blob_id.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class NCBI_XREADER_EXPORT CBlob_id : public CBlobId
{
public:
    CBlob_id(void)
        : m_Sat(-1), m_SubSat(0), m_SatKey(0)
        {
        }
    
    explicit CBlob_id(CTempString str_id);

    typedef Int4 TSat;
    typedef Int4 TSubSat;
    typedef Int4 TSatKey;
    
    TSat GetSat() const
        {
            return m_Sat;
        }
    TSubSat GetSubSat() const
        {
            return m_SubSat;
        }
    TSatKey GetSatKey() const
        {
            return m_SatKey;
        }

    CNcbiOstream& Dump(CNcbiOstream& ostr) const;
    string ToString(void) const;
    string ToPsgId(void) const;
    static CBlob_id* CreateFromString(const string& str);

    bool operator<(const CBlobId& blob_id) const;
    bool operator==(const CBlobId& blob_id) const;

    bool operator<(const CBlob_id& blob_id) const
        {
            if ( m_Sat != blob_id.m_Sat )
                return m_Sat < blob_id.m_Sat;
            if ( m_SubSat != blob_id.m_SubSat )
                return m_SubSat < blob_id.m_SubSat;
            return m_SatKey < blob_id.m_SatKey;
        }
    bool operator==(const CBlob_id& blob_id) const
        {
            return m_SatKey == blob_id.m_SatKey &&
                m_Sat == blob_id.m_Sat && m_SubSat == blob_id.m_SubSat;
        }
    bool operator!=(const CBlob_id& blob_id) const
        {
            return !(*this == blob_id);
        }

    bool IsEmpty(void) const
        {
            return m_Sat < 0;
        }

    bool IsMainBlob(void) const
        {
            return m_SubSat == 0;
        }

    void SetSat(TSat v)
        {
            m_Sat = v;
        }
    void SetSubSat(TSubSat v)
        {
            m_SubSat = v;
        }
    void SetSatKey(TSatKey v)
        {
            m_SatKey = v;
        }

protected:
    TSat m_Sat;
    TSubSat m_SubSat;
    TSatKey m_SatKey;
};


inline CNcbiOstream& operator<<(CNcbiOstream& ostr, const CBlob_id& id)
{
    return id.Dump(ostr);
}


typedef int TChunkId;
enum {
    kMain_ChunkId       = -1, // not a chunk, but main Seq-entry
    kMasterWGS_ChunkId  = kMax_Int-1, // chunk with master WGS descr
    kDelayedMain_ChunkId= kMax_Int // main Seq-entry with delayed ext annot
};


enum EBlobContentsMask
{
    fBlobHasCore        = 1 << 0,
    fBlobHasDescr       = 1 << 1,
    fBlobHasSeqMap      = 1 << 2,
    fBlobHasSeqData     = 1 << 3,
    fBlobHasIntFeat     = 1 << 4,
    fBlobHasExtFeat     = 1 << 5,
    fBlobHasOrphanFeat  = 1 << 6,
    fBlobHasIntAlign    = 1 << 7,
    fBlobHasExtAlign    = 1 << 8,
    fBlobHasOrphanAlign = 1 << 9,
    fBlobHasIntGraph    = 1 << 10,
    fBlobHasExtGraph    = 1 << 11,
    fBlobHasOrphanGraph = 1 << 12,
    fBlobHasNamedFeat   = 1 << 13,
    fBlobHasNamedAlign  = 1 << 14,
    fBlobHasNamedGraph  = 1 << 15,
    
    fBlobHasIntAnnot    = (fBlobHasIntFeat |
                           fBlobHasIntAlign |
                           fBlobHasIntGraph),
    fBlobHasExtAnnot    = (fBlobHasExtFeat |
                           fBlobHasExtAlign |
                           fBlobHasExtGraph),
    fBlobHasNamedAnnot  = (fBlobHasNamedFeat |
                           fBlobHasNamedAlign |
                           fBlobHasNamedGraph),
    fBlobHasOrphanAnnot = (fBlobHasOrphanFeat |
                           fBlobHasOrphanAlign |
                           fBlobHasOrphanGraph),
    fBlobHasAnyFeat     = (fBlobHasIntFeat |
                           fBlobHasExtFeat |
                           fBlobHasOrphanFeat |
                           fBlobHasNamedFeat),
    fBlobHasAnyAlign    = (fBlobHasIntAlign |
                           fBlobHasExtAlign |
                           fBlobHasOrphanAlign |
                           fBlobHasNamedAlign),
    fBlobHasAnyGraph    = (fBlobHasIntGraph |
                           fBlobHasExtGraph |
                           fBlobHasOrphanGraph |
                           fBlobHasNamedGraph),
    fBlobHasAllLocal    = (fBlobHasCore | fBlobHasDescr |
                           fBlobHasSeqMap | fBlobHasSeqData |
                           fBlobHasIntAnnot),

    fBlobHasAll          = (1 << 16)-1
};
typedef int TBlobContentsMask;


enum EGBErrorAction {
    eGBErrorAction_ignore,
    eGBErrorAction_report,
    eGBErrorAction_throw
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//BLOB_ID__HPP_INCLUDED
