#ifndef SCOPE__HPP
#define SCOPE__HPP

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
* Authors:
*           Andrei Gourianov
*           Aleksey Grichenko
*           Michael Kimelman
*           Denis Vakatov
*
* File Description:
*           Scope is top-level object available to a client.
*           Its purpose is to define a scope of visibility and reference
*           resolution and provide access to the bio sequence data
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.9  2002/02/21 19:27:00  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.8  2002/02/07 21:27:33  grichenk
* Redesigned CDataSource indexing: seq-id handle -> TSE -> seq/annot
*
* Revision 1.7  2002/02/06 21:46:43  gouriano
* *** empty log message ***
*
* Revision 1.6  2002/02/05 21:47:21  gouriano
* added FindSeqid function, minor tuneup in CSeq_id_mapper
*
* Revision 1.5  2002/01/28 19:45:33  gouriano
* changed the interface of BioseqHandle: two functions moved from Scope
*
* Revision 1.4  2002/01/23 21:59:29  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.3  2002/01/18 17:07:12  gouriano
* renamed GetSequence to GetSeqVector
*
* Revision 1.2  2002/01/16 16:26:35  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:04:03  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#include <objects/objmgr1/annot_types_ci.hpp>
#include <objects/objmgr1/seq_map.hpp>
#include <objects/objmgr1/seqmatch_info.hpp>
#include <objects/seq/Seq_data.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbithr.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// fwd decl
class CObjectManager;
class CDataLoader;
class CDataSource;
class CTSE_Info;


class CScope : public CObject
{
public:
    CScope(CObjectManager& objmgr);
    virtual ~CScope(void);

    // Add default data loaders and seq_entries from object manager
    void AddDefaults(void);
    // Add data loader by name.
    // The loader (or its factory) must be known to Object Manager.
    void AddDataLoader(const string& loader_name);
    // Add seq_entry
    void AddTopLevelSeqEntry(CSeq_entry& top_entry);

    //### TSEs should be dropped from CScope dectructor.
    //### This function is obsolete.
    //### void DropTopLevelSeqEntry(CSeq_entry& top_entry);

    // Add annotations to a seq-entry (seq or set)
    bool AttachAnnot(const CSeq_entry& entry, CSeq_annot& annot);

    // Get bioseq handle by seq-id
    // Declared "virtual" to avoid circular dependencies with seqloc
    virtual CBioseq_Handle  GetBioseqHandle(const CSeq_id& id);

    // Find set of CSeq_id by a string identifier
    // The latter could be name, accession, something else
    // which could be found in CSeq_id
    void FindSeqid(set< CRef<const CSeq_id> >& setId,
                   const string& searchBy) const;

    // Find mode flags: how to treat duplicate IDs within the same scope
    enum EFindMode {
        eFirst,       // silently return the first handle found (default)
        eDup_Warning,  // generate warning, act like "eFirst"
        eDup_Fatal,    // generate fatal error
        eDup_Throw     // throw runtime_error
    };
    virtual void SetFindMode(EFindMode mode);

    // History of requests
    typedef set< CConstRef<CTSE_Info> > TRequestHistory;

private:
    // Get requests history (used by data sources to process requests)
    const TRequestHistory& x_GetHistory(void);
    // Add an entry to the requests history
    void x_AddToHistory(const CTSE_Info& tse);

    // Find the best possible resolution for the Seq-id
    CSeqMatch_Info x_BestResolve(const CSeq_id& id);

    // Get TSEs containing annotations for the given location
    void x_PopulateTSESet(CHandleRangeMap& loc,
                          CAnnotTypes_CI::TTSESet& tse_set);
    // Add new sub-entry to the existing tree if it is in this scope
    bool x_AttachEntry(const CSeq_entry& parent, CSeq_entry& entry);
    // Add sequence map for a bioseq if it is in this scope
    bool x_AttachMap(const CSeq_entry& bioseq, CSeqMap& seqmap);
    // Add seq-data to a bioseq if it is in this scope
    bool x_AttachSeqData(const CSeq_entry& bioseq, CSeq_data& seq,
                      TSeqPosition start, TSeqLength length);

    bool x_GetSequence(const CBioseq_Handle& handle,
                       TSeqPosition point,
                       SSeqData* seq_piece);

    CSeq_id_Mapper& x_GetIdMapper(void) const;

    // Conflict reporting function
    enum EConflict {
        eConflict_History,
        eConflict_Live
    };
    void x_ThrowConflict(EConflict conflict_type,
                         const CSeqMatch_Info& info1,
                         const CSeqMatch_Info& info2) const;

    CRef<CObjectManager> m_pObjMgr;
    set<CDataSource*>    m_setDataSrc;

    EFindMode m_FindMode;

    TRequestHistory m_History;

    static CMutex sm_Scope_Mutex;

    friend class CObjectManager;
    friend class CSeqVector;
    friend class CDataSource;
    friend class CAnnotTypes_CI;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SCOPE__HPP
