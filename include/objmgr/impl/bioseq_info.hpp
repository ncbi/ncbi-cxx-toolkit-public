#ifndef OBJECTS_OBJMGR_IMPL___BIOSEQ_INFO__HPP
#define OBJECTS_OBJMGR_IMPL___BIOSEQ_INFO__HPP

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
 * Author: Aleksey Grichenko, Eugene Vasilchenko
 *
 * File Description:
 *   Bioseq info for data source
 *
 */

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>

#include <objmgr/seq_id_handle.hpp>

#include <objects/seq/Seq_inst.hpp>

#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CSeq_entry_Info;
class CBioseq;
class CSeq_id_Handle;
class CSeqMap;
class CTSE_Info;
class CDataSource;
class CSeq_inst;
class CSeq_id;
class CPacked_seqint;
class CSeq_loc;
class CSeq_loc_mix;
class CSeq_loc_equiv;
class CSeg_ext;
class CDelta_ext;
class CDelta_seq;
class CScope_Impl;

////////////////////////////////////////////////////////////////////
//
//  CBioseq_Info::
//
//    Structure to keep bioseq's parent seq-entry along with the list
//    of seq-id synonyms for the bioseq.
//


class NCBI_XOBJMGR_EXPORT CBioseq_Info : public CObject
{
public:
    typedef vector<CSeq_id_Handle> TSynonyms;

    // 'ctors
    CBioseq_Info(CBioseq& seq, CSeq_entry_Info& entry_info);
    virtual ~CBioseq_Info(void);

    CDataSource& GetDataSource(void) const;

    const CSeq_entry& GetTSE(void) const;
    const CTSE_Info& GetTSE_Info(void) const;
    CTSE_Info& GetTSE_Info(void);

    const CSeq_entry& GetSeq_entry(void) const;
    CSeq_entry& GetSeq_entry(void);

    const CSeq_entry_Info& GetSeq_entry_Info(void) const;
    CSeq_entry_Info& GetSeq_entry_Info(void);

    const CBioseq& GetBioseq(void) const;
    CBioseq& GetBioseq(void);

    // Get some values from core:
    TSeqPos GetBioseqLength(void) const;
    CSeq_inst::TMol GetBioseqMolType(void) const;
    const CSeqMap& GetSeqMap(void) const;

    const TSynonyms& GetSynonyms(void) const;
    string IdsString(void) const;

    virtual void DebugDump(CDebugDumpContext ddc, unsigned int depth) const;

    void x_AttachMap(CSeqMap& seq_map);

    void x_DSAttach(void);
    void x_DSDetach(void);

protected:
    friend class CDataSource;
    friend class CScope_Impl;
    friend class CSeq_entry_Info;

    void x_DSAttachThis(void);
    void x_DSDetachThis(void);

    void x_TSEAttach(void);
    void x_TSEDetach(void);

private:
    CBioseq_Info(const CBioseq_Info&);
    CBioseq_Info& operator=(const CBioseq_Info&);

    void x_InitBioseqInfo(void);

    TSeqPos x_CalcBioseqLength(void) const;
    TSeqPos x_CalcBioseqLength(const CSeq_inst& inst) const;
    TSeqPos x_CalcBioseqLength(const CSeq_id& whole) const;
    TSeqPos x_CalcBioseqLength(const CPacked_seqint& ints) const;
    TSeqPos x_CalcBioseqLength(const CSeq_loc& seq_loc) const;
    TSeqPos x_CalcBioseqLength(const CSeq_loc_mix& seq_mix) const;
    TSeqPos x_CalcBioseqLength(const CSeq_loc_equiv& seq_equiv) const;
    TSeqPos x_CalcBioseqLength(const CSeg_ext& seg_ext) const;
    TSeqPos x_CalcBioseqLength(const CDelta_ext& delta) const;
    TSeqPos x_CalcBioseqLength(const CDelta_seq& delta_seq) const;

    // Bioseq object
    CRef<CBioseq>            m_Bioseq;
    // bioseq parameters
    mutable TSeqPos          m_BioseqLength; // cached sequence length
    CSeq_inst::TMol          m_BioseqMolType;

    // Parent seq-entry for the bioseq
    CSeq_entry_Info*         m_Seq_entry_Info;
    CTSE_Info*               m_TSE_Info;

    // SeqMap object
    mutable CConstRef<CSeqMap>  m_SeqMap;
    mutable CFastMutex          m_SeqMap_Mtx;

    // Set of bioseq synonyms
    TSynonyms                m_Synonyms;
};



/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
const CSeq_entry_Info& CBioseq_Info::GetSeq_entry_Info(void) const
{
    return *m_Seq_entry_Info;
}


inline
CSeq_entry_Info& CBioseq_Info::GetSeq_entry_Info(void)
{
    return *m_Seq_entry_Info;
}


inline
const CTSE_Info& CBioseq_Info::GetTSE_Info(void) const
{
    return *m_TSE_Info;
}


inline
CTSE_Info& CBioseq_Info::GetTSE_Info(void)
{
    return *m_TSE_Info;
}


inline
const CBioseq& CBioseq_Info::GetBioseq(void) const
{
    return *m_Bioseq;
}


inline
CBioseq& CBioseq_Info::GetBioseq(void)
{
    return *m_Bioseq;
}


inline
TSeqPos CBioseq_Info::GetBioseqLength(void) const
{
    TSeqPos length = m_BioseqLength;
    if ( length == kInvalidSeqPos ) {
        length = m_BioseqLength = x_CalcBioseqLength();
    }
    return length;
}


inline
CSeq_inst::TMol CBioseq_Info::GetBioseqMolType(void) const
{
    return m_BioseqMolType;
}


inline
const CBioseq_Info::TSynonyms& CBioseq_Info::GetSynonyms(void) const
{
    return m_Synonyms;
}


inline
void CBioseq_Info::x_DSAttach(void)
{
    x_DSAttachThis();
}


inline
void CBioseq_Info::x_DSDetach(void)
{
    x_DSDetachThis();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.15  2003/11/28 15:13:25  grichenk
 * Added CSeq_entry_Handle
 *
 * Revision 1.14  2003/09/30 16:22:00  vasilche
 * Updated internal object manager classes to be able to load ID2 data.
 * SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
 * Scope caches results of requests for data to data loaders.
 * Optimized CSeq_id_Handle for gis.
 * Optimized bioseq lookup in scope.
 * Reduced object allocations in annotation iterators.
 * CScope is allowed to be destroyed before other objects using this scope are
 * deleted (feature iterators, bioseq handles etc).
 * Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
 * Added 'adaptive' option to objmgr_demo application.
 *
 * Revision 1.13  2003/06/02 16:01:37  dicuccio
 * Rearranged include/objects/ subtree.  This includes the following shifts:
 *     - include/objects/alnmgr --> include/objtools/alnmgr
 *     - include/objects/cddalignview --> include/objtools/cddalignview
 *     - include/objects/flat --> include/objtools/flat
 *     - include/objects/objmgr/ --> include/objmgr/
 *     - include/objects/util/ --> include/objmgr/util/
 *     - include/objects/validator --> include/objtools/validator
 *
 * Revision 1.12  2003/04/29 19:51:12  vasilche
 * Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
 * Made some typedefs more consistent.
 *
 * Revision 1.11  2003/04/25 14:23:46  vasilche
 * Added explicit constructors, destructor and assignment operator to make it compilable on MSVC DLL.
 *
 * Revision 1.10  2003/04/24 16:12:37  vasilche
 * Object manager internal structures are splitted more straightforward.
 * Removed excessive header dependencies.
 *
 * Revision 1.9  2003/04/14 21:31:05  grichenk
 * Removed operators ==(), !=() and <()
 *
 * Revision 1.8  2003/03/12 20:09:31  grichenk
 * Redistributed members between CBioseq_Handle, CBioseq_Info and CTSE_Info
 *
 * Revision 1.7  2003/02/05 17:57:41  dicuccio
 * Moved into include/objects/objmgr/impl to permit data loaders to be defined
 * outside of xobjmgr.
 *
 * Revision 1.6  2002/12/26 21:03:33  dicuccio
 * Added Win32 export specifier.  Moved file from src/objects/objmgr to
 * include/objects/objmgr.
 *
 * Revision 1.5  2002/07/08 20:51:01  grichenk
 * Moved log to the end of file
 * Replaced static mutex (in CScope, CDataSource) with the mutex
 * pool. Redesigned CDataSource data locking.
 *
 * Revision 1.4  2002/05/29 21:21:13  gouriano
 * added debug dump
 *
 * Revision 1.3  2002/05/06 03:28:46  vakatov
 * OM/OM1 renaming
 *
 * Revision 1.2  2002/02/21 19:27:05  grichenk
 * Rearranged includes. Added scope history. Added searching for the
 * best seq-id match in data sources and scopes. Updated tests.
 *
 * Revision 1.1  2002/02/07 21:25:05  grichenk
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif  /* OBJECTS_OBJMGR_IMPL___BIOSEQ_INFO__HPP */
