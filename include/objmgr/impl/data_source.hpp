#ifndef DATA_SOURCE__HPP
#define DATA_SOURCE__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman
*
* File Description:
*   Data source for object manager
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2002/01/16 16:25:55  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:04:01  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/


#include <corelib/ncbiobj.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <vector>
#include <set>

#include <objects/objmgr1/bioseq_handle.hpp>
#include <objects/objmgr1/data_loader.hpp>
#include <objects/objmgr1/seq_map.hpp>
#include <objects/objmgr1/annot_ci.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CDataSource : public CObject
{
public:
    /// 'ctors
    CDataSource(CDataLoader& loader);
    CDataSource(CSeq_entry& entry);
    virtual ~CDataSource(void);

    /// Register new TSE (Top Level Seq-entry)
    void AddTSE(CSeq_entry& se);

    /// Add new sub-entry to "parent".
    /// Return FALSE and do nothing if "parent" is not a node in an
    /// existing TSE tree of this data-source.
    bool AttachEntry(const CSeq_entry& parent, CSeq_entry& entry);

    /// Add sequence map for the given Bioseq.
    /// Return FALSE and do nothing if "bioseq" is not a node in an
    /// existing TSE tree of this data-source, or if "bioseq" is not a Bioseq.
    bool AttachMap(const CSeq_entry& bioseq, CSeqMap& seqmap);

    /// Add seq-data to a Bioseq.
    /// Return FALSE and do nothing if "bioseq" is not a node in an
    /// existing TSE tree of this data-source, or if "bioseq" is not a Bioseq.
    bool AttachSeqData(const CSeq_entry& bioseq, CSeq_data& seq,
                    TSeqPosition start, TSeqLength length);

    /// Add annotations to a Seq-entry.
    /// Return FALSE and do nothing if "parent" is not a node in an
    /// existing TSE tree of this data-source.
    bool AttachAnnot(const CSeq_entry& entry, CSeq_annot& annot);

    /// Get Bioseq handle by Seq-Id.
    /// Return "NULL" handle if the Bioseq cannot be resolved.
    CBioseqHandle GetBioseqHandle(const CSeq_id& id);

    // Remove TSE from the datasource, update indexes
    bool DropTSE(const CSeq_entry& tse);

    // Contains (or can load) any entries?
    bool IsEmpty(void) const;

private:

    /// Get the complete Bioseq (as loaded by now)
    const CBioseq& GetBioseq(const CBioseqHandle& handle);

    /// Get top level Seq-Entry for a Bioseq
    const CSeq_entry& GetTSE(const CBioseqHandle& handle);

    /// Get Bioseq core structure
    CBioseqHandle::TBioseqCore GetBioseqCore(const CBioseqHandle& handle);

    /// Get sequence map
    const CSeqMap& GetSeqMap(const CBioseqHandle& handle);

    /// Get a piece of seq. data ("seq_piece"), which the "point" belongs to:
    ///   "length"     -- length   of the data piece the "point" is found in;
    ///   "dest_start" -- position of the data piece in the dest. Bioseq;
    ///   "src_start"  -- position of the data piece in the source Bioseq;
    ///   "src_data"   -- Seq-data of the last Bioseq found in the chain;
    /// Return FALSE if the final source Bioseq (i.e. the Bioseq that actually
    /// describe the piece of sequence data containing the "point") cannot be
    /// found.
    bool GetSequence(const CBioseqHandle& handle,
                     TSeqPosition point,
                     SSeqData* seq_piece,
                     CScope& scope);


    // Internal typedefs
    typedef CAnnot_CI::TRange    TRange;
    typedef CAnnot_CI::TRangeMap TRangeMap;
    typedef CAnnot_CI::TAnnotMap TAnnotMap;
    typedef set< CRef<CSeq_entry> >       TEntries;
    typedef map<CBioseq*, CRef<CSeqMap> > TSeqMaps;

    // Process seq-entry recursively
    void x_IndexEntry     (CSeq_entry& entry, CSeq_entry& tse);
    void x_AddToBioseqMap (CSeq_entry& entry);
    void x_AddToAnnotMap  (CSeq_entry& entry);

    // Create CSeqMap for a bioseq
    void x_CreateSeqMap(CBioseq& seq);
    void x_LocToSeqMap(const CSeq_loc& loc, int& pos, CSeqMap& seqmap);
    void x_DataToSeqMap(const CSeq_data& data,
                        int& pos, int len,
                        CSeqMap& seqmap);
    // Non-const version of GetSeqMap()
    CSeqMap& x_GetSeqMap(const CBioseqHandle& handle);

    // Process a single data element
    void x_MapFeature(const CSeq_feat& feat);
    void x_MapAlign(const CSeq_align& align);
    void x_MapGraph(const CSeq_graph& graph);

    // Get range map for a given handle or null
    TRangeMap* x_GetRangeMap(const CBioseqHandle& handle,
                             bool create = false);

    // Check if the Seq-entry is handled by this data-source
    CSeq_entry* x_FindEntry(const CSeq_entry& entry);

    // Remove entry from the datasource, update indexes
    void x_DropEntry(CSeq_entry& entry);

private:
    CDataLoader* GetDataLoader(void);
    CSeq_entry* GetTopEntry(void);

private:
    CRef< CDataLoader> m_Loader;
    CRef< CSeq_entry> m_pTopEntry;

    // Structure to keep bioseq's parent seq-entry along with the list
    // of seq-id synonyms for the bioseq.
    struct SBioseqInfo : public CObject
    {
        typedef set<CBioseqHandle::THandle> TSynonyms;

        SBioseqInfo(void);
        SBioseqInfo(CSeq_entry& entry);
        SBioseqInfo(const SBioseqInfo& info);
        virtual ~SBioseqInfo(void);

        SBioseqInfo& operator= (const SBioseqInfo& info);

        bool operator== (const SBioseqInfo& info);
        bool operator!= (const SBioseqInfo& info);
        bool operator<  (const SBioseqInfo& info);

        CRef<CSeq_entry> m_Entry;
        TSynonyms        m_Synonyms;
    };

    // Replace initial handle range map with the new one, containing duplicate
    // range sets for each synonym of each handle
    void x_ResloveLocationHandles(CHandleRangeMap& loc);

    typedef map<CBioseqHandle::THandle, CRef<SBioseqInfo> > TBioseqMap;

    TEntries                 m_Entries;   // All known seq-entries
    TBioseqMap               m_BioseqMap; // Bioseq by seq-id key
    TSeqMaps                 m_SeqMaps;   // Sequence maps for bioseqs
    TAnnotMap                m_AnnotMap;  // Index of annotations

    friend class CDesc_CI;
    friend class CAnnot_CI;
    friend class CAnnotTypes_CI;
    friend class CScope;
    friend class CSeqVector;
    friend class CBioseqHandle;
    friend class CObjectManager;
};


inline
bool CDataSource::IsEmpty(void) const
{
    return m_Loader == 0  &&  m_Entries.empty();
}

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // DATA_SOURCE__HPP
