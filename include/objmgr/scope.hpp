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
* Revision 1.1  2002/01/11 19:04:03  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/


#include <corelib/ncbistd.hpp>
#include <set>

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbithr.hpp>

#include <objects/seqset/Seq_entry.hpp>

#include <objects/objmgr1/data_source.hpp>
#include <objects/objmgr1/seq_vector.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// fwd decl
class CObjectManager;
class CDataLoader;
class CDataSource;
//class CSeq_entry;
class CSeq_loc;
class CBioseq;
class CSeq_id;


////////////////////////////////////////////////////////////////////
//
//  CScope::
//

class CScope : public CObject
{
public:
    CScope(CObjectManager& objmgr);
    virtual ~CScope(void);

public:
    // Add default data loaders and seq_entries from object manager
    void AddDefaults(void);
    // Add data loader by name.
    // The loader (or its factory) must be known to Object Manager.
    void AddDataLoader(const string& loader_name);
    // add seq_entry
    void AddTopLevelSeqEntry(CSeq_entry& top_entry);

public:
    // Bioseq core -- using partially populated CBioseq
    typedef CDataSource::TBioseqCore TBioseqCore;

    // Get bioseq handle by seq-id
    // Declared "virtual" to avoid circular dependencies with seqloc
    virtual CBioseqHandle  GetBioseqHandle(const CSeq_id& id);
    // Get the complete bioseq (as loaded by now)
    // Declared "virtual" to avoid circular dependencies with seqloc
    virtual const CBioseq& GetBioseq(const CBioseqHandle& handle);
    // Get top level seq-entry for a bioseq
    virtual const CSeq_entry& GetTSE(const CBioseqHandle& handle);
    // Get bioseq core structure
    // Declared "virtual" to avoid circular dependencies with seqloc
    virtual TBioseqCore GetBioseqCore(const CBioseqHandle& handle);
    // Get sequence map. References to the whole bioseqs may have
    // length of 0 unless GetSequence() has been called for the handle.
    virtual const CSeqMap& GetSeqMap(const CBioseqHandle& handle);
    // Get sequence's title (used in various flat-file formats.)
    // This function is here rather than in CBioseq because it may need
    // to inspect other sequences.  The reconstruct flag indicates that it
    // should ignore any existing title Seqdesc.

    enum EGetTitleFlags {
        fGetTitle_Reconstruct = 0x1, // ignore existing title Seqdesc.
        fGetTitle_Accession   = 0x2, // prepend (accession)
        fGetTitle_Organism    = 0x4  // append [organism]
    };
    typedef int TGetTitleFlags;

    virtual string GetTitle(const CBioseqHandle& handle,
                            TGetTitleFlags flags = 0);

    // Get iterator to traverse through descriprtor sets 
    virtual CDesc_CI BeginDescr(const CBioseqHandle& handle);

    // Get sequence: Iupacna or Iupacaa
    virtual CSeqVector GetSequence(const CBioseqHandle& handle,
                                    bool plus_strand = true);

    // Other iterators
    virtual CFeat_CI  BeginFeat(const CSeq_loc& loc,
                                CSeqFeatData::E_Choice feat_choice);
    virtual CGraph_CI BeginGraph(const CSeq_loc& loc);
    virtual CAlign_CI BeginAlign(const CSeq_loc& loc);

    // Find mode flags: how to treat duplicate IDs within the same scope
    enum EFindMode {
        eFirst,       // silently return the first handle found (default)
        eDup_Warning,  // generate warning, act like "eFirst"
        eDup_Fatal,    // generate fatal error
        eDup_Throw     // throw runtime_error
    };
    virtual void SetFindMode(EFindMode mode);

    void DropTSE(CSeq_entry& tse);

    // Add annotations to a seq-entry (seq or set)
    bool AttachAnnot(const CSeq_entry& entry, CSeq_annot& annot);

private:

    void x_AddDataSource(CDataSource& source);
    // Add new sub-entry to the existing tree if it is in this scope
    bool x_AttachEntry(const CSeq_entry& parent, CSeq_entry& entry);
    // Add sequence map for a bioseq if it is in this scope
    bool x_AttachMap(const CSeq_entry& bioseq, CSeqMap& seqmap);
    // Add seq-data to a bioseq if it is in this scope
    bool x_AttachSeqData(const CSeq_entry& bioseq, CSeq_data& seq,
                      TSeqPosition start, TSeqLength length);

    bool x_GetSequence(const CBioseqHandle& handle,
                       TSeqPosition point,
                       CDataSource::SSeqData* seq_piece);

//    TDataSources m_DataSources;
    CRef<CObjectManager> m_pObjMgr;
    set<CDataSource*>    m_setDataSrc;

    EFindMode m_FindMode;

    static CMutex sm_Scope_Mutex;

    friend class CObjectManager;
    friend class CSeqVector;
    friend class CDataSource;
    friend class CAnnotTypes_CI;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SCOPE__HPP
