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
*/

#include <objects/objmgr/annot_types_ci.hpp>
#include <objects/objmgr/seq_map.hpp>
#include <objects/objmgr/seqmatch_info.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbithr.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// fwd decl
class CObjectManager;
class CDataLoader;
class CDataSource;
class CTSE_Info;
struct SSeqData;

class NCBI_XOBJMGR_EXPORT CScope : public CObject
{
public:
    CScope(CObjectManager& objmgr);
    virtual ~CScope(void);

    // Add default data loaders from object manager
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
    bool RemoveAnnot(const CSeq_entry& entry, const CSeq_annot& annot);
    bool ReplaceAnnot(const CSeq_entry& entry,
                      const CSeq_annot& old_annot,
                      CSeq_annot& new_annot);

    // Add new sub-entry to the existing tree if it is in this scope
    bool AttachEntry(const CSeq_entry& parent, CSeq_entry& entry);
    // Add sequence map for a bioseq if it is in this scope
    bool AttachMap(const CSeq_entry& bioseq, CSeqMap& seqmap);
    // Add seq-data to a bioseq if it is in this scope
    bool AttachSeqData(const CSeq_entry& bioseq, CSeq_data& seq,
                       size_t index,
                       TSeqPos start, TSeqPos length);

    // Get bioseq handle by seq-id
    // Declared "virtual" to avoid circular dependencies with seqloc
    virtual CBioseq_Handle  GetBioseqHandle(const CSeq_id& id);

    // Get bioseq handle by seqloc
    CBioseq_Handle  GetBioseqHandle(const CSeq_loc& loc);

    // Get bioseq handle by bioseq
    CBioseq_Handle  GetBioseqHandle(const CBioseq& seq);


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

    void ResetHistory(void);

    virtual void DebugDump(CDebugDumpContext ddc, unsigned int depth) const;

    CSeq_id_Handle GetIdHandle(const CSeq_id& id) const;

private:
    void x_DetachFromOM(void);
    // Get requests history (used by data sources to process requests)
    const TRequestHistory& x_GetHistory(void);
    // Add an entry to the requests history, lock the TSE by default
    void x_AddToHistory(const CTSE_Info& tse, bool lock=true);

    // Find the best possible resolution for the Seq-id
    CSeqMatch_Info x_BestResolve(const CSeq_id& id);

    // Get TSEs containing annotations for the given location
    void x_PopulateTSESet(CHandleRangeMap& loc,
                          CSeq_annot::C_Data::E_Choice sel,
                          CAnnotTypes_CI::TTSESet& tse_set);

    // Get bioseq handles for sequences from the given TSE using the filter
    void x_PopulateBioseq_HandleSet(const CSeq_entry& tse,
                                    set<CBioseq_Handle>& handles,
                                    CSeq_inst::EMol filter);

    bool x_GetSequence(const CBioseq_Handle& handle,
                       TSeqPos point,
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

    CObjectManager      *m_pObjMgr;
    set<CDataSource*>    m_setDataSrc;

    EFindMode m_FindMode;

    TRequestHistory m_History;

    mutable CMutex m_Scope_Mtx;

    friend class CObjectManager;
    friend class CSeqVector;
    friend class CDataSource;
    friend class CAnnotTypes_CI;
    friend class CBioseq_CI;
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.27  2002/12/26 20:51:36  dicuccio
* Added Win32 export specifier
*
* Revision 1.26  2002/12/26 16:39:21  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.25  2002/11/08 22:15:50  grichenk
* Added methods for removing/replacing annotations
*
* Revision 1.24  2002/11/01 05:34:06  kans
* added GetBioseqHandle taking CBioseq parameter
*
* Revision 1.23  2002/10/31 22:25:09  kans
* added GetBioseqHandle taking CSeq_loc parameter
*
* Revision 1.22  2002/10/18 19:12:39  grichenk
* Removed mutex pools, converted most static mutexes to non-static.
* Protected CSeqMap::x_Resolve() with mutex. Modified code to prevent
* dead-locks.
*
* Revision 1.21  2002/10/02 17:58:21  grichenk
* Added sequence type filter to CBioseq_CI
*
* Revision 1.20  2002/09/30 20:01:17  grichenk
* Added methods to support CBioseq_CI
*
* Revision 1.19  2002/07/08 20:50:56  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.18  2002/06/04 17:18:32  kimelman
* memory cleanup :  new/delete/Cref rearrangements
*
* Revision 1.17  2002/05/28 18:01:11  gouriano
* DebugDump added
*
* Revision 1.16  2002/05/14 20:06:23  grichenk
* Improved CTSE_Info locking by CDataSource and CDataLoader
*
* Revision 1.15  2002/05/06 03:30:36  vakatov
* OM/OM1 renaming
*
* Revision 1.14  2002/05/03 21:28:02  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.13  2002/04/17 21:09:38  grichenk
* Fixed annotations loading
*
* Revision 1.12  2002/04/08 19:14:15  gouriano
* corrected comment to AddDefaults()
*
* Revision 1.11  2002/03/27 18:46:26  gouriano
* three functions made public
*
* Revision 1.10  2002/03/20 21:20:38  grichenk
* +CScope::ResetHistory()
*
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

#endif  // SCOPE__HPP
