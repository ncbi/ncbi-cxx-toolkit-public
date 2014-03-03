#ifndef OBJECTS_OBJMGR___DATA_LOADER__HPP
#define OBJECTS_OBJMGR___DATA_LOADER__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman, Eugene Vasilchenko
*
* File Description:
*   Data loader base class for object manager
*
*/

#include <corelib/ncbiobj.hpp>
#include <util/range.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/annot_name.hpp>
#include <objmgr/annot_type_selector.hpp>
#include <objmgr/impl/tse_lock.hpp>
#include <objmgr/blob_id.hpp>

#include <objects/seq/seq_id_handle.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <corelib/plugin_manager.hpp>
#include <set>
#include <map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/** @addtogroup ObjectManagerCore
 *
 * @{
 */


// fwd decl
class CDataSource;
class CTSE_Info;
class CTSE_Chunk_Info;
class CBioseq_Info;
class IEditSaver;
struct SAnnotSelector;
class CScope_Impl;

/////////////////////////////////////////////////////////////////////////////
// structure to describe required data set
//

struct SRequestDetails
{
    typedef CRange<TSeqPos> TRange;
    typedef set<SAnnotTypeSelector> TAnnotTypesSet;
    typedef map<CAnnotName, TAnnotTypesSet> TAnnotSet;
    enum FAnnotBlobType {
        fAnnotBlobNone      = 0,
        fAnnotBlobInternal  = 1<<0,
        fAnnotBlobExternal  = 1<<1,
        fAnnotBlobOrphan    = 1<<2,
        fAnnotBlobAll       = (fAnnotBlobInternal |
                               fAnnotBlobExternal |
                               fAnnotBlobOrphan)
    };
    typedef int TAnnotBlobType;
    
    SRequestDetails(void)
        : m_NeedSeqMap(TRange::GetEmpty()),
          m_NeedSeqData(TRange::GetEmpty()),
          m_AnnotBlobType(fAnnotBlobNone)
        {
        }

    TRange          m_NeedSeqMap;
    TRange          m_NeedSeqData;
    TAnnotSet       m_NeedAnnots;
    TAnnotBlobType  m_AnnotBlobType;
};


/////////////////////////////////////////////////////////////////////////////
// Template for data loader construction.
class CLoaderMaker_Base
{
public:
    // Virtual method for creating an instance of the data loader
    virtual CDataLoader* CreateLoader(void) const = 0;

    virtual ~CLoaderMaker_Base(void) {}

protected:
    typedef SRegisterLoaderInfo<CDataLoader> TRegisterInfo_Base;
    string             m_Name;
    TRegisterInfo_Base m_RegisterInfo;

    friend class CObjectManager;
};


// Construction of data loaders without arguments
template <class TDataLoader>
class CSimpleLoaderMaker : public CLoaderMaker_Base
{
public:
    CSimpleLoaderMaker(void)
        {
            m_Name = TDataLoader::GetLoaderNameFromArgs();
        }

    virtual ~CSimpleLoaderMaker(void) {}

    virtual CDataLoader* CreateLoader(void) const
        {
            return new TDataLoader(m_Name);
        }
    typedef SRegisterLoaderInfo<TDataLoader> TRegisterInfo;
    TRegisterInfo GetRegisterInfo(void)
        {
            TRegisterInfo info;
            info.Set(m_RegisterInfo.GetLoader(), m_RegisterInfo.IsCreated());
            return info;
        }
};


// Construction of data loaders with an argument. A structure
// may be used to create loaders with multiple arguments.
template <class TDataLoader, class TParam>
class CParamLoaderMaker : public CLoaderMaker_Base
{
public:
    typedef TParam TParamType;
public:
    // TParam should have copy method.
    CParamLoaderMaker(TParam param)
        : m_Param(param)
        {
            m_Name = TDataLoader::GetLoaderNameFromArgs(param);
        }

    virtual ~CParamLoaderMaker(void) {}

    virtual CDataLoader* CreateLoader(void) const
        {
            return new TDataLoader(m_Name, m_Param);
        }
    typedef SRegisterLoaderInfo<TDataLoader> TRegisterInfo;
    TRegisterInfo GetRegisterInfo(void)
        {
            TRegisterInfo info;
            info.Set(m_RegisterInfo.GetLoader(), m_RegisterInfo.IsCreated());
            return info;
        }
protected:
    TParam m_Param;
};


////////////////////////////////////////////////////////////////////
//
//  CDataLoader --
//
//  Load data from different sources
//

// There are three types of blobs (top-level Seq-entries) related to
// any Seq-id:
//   1. main (eBioseq/eBioseqCore/eSequence):
//      Seq-entry containing Bioseq with Seq-id.
//   2. external (eExtAnnot):
//      Seq-entry doesn't contain Bioseq but contains annotations on Seq-id,
//      provided this data source contain some blob with Bioseq.
//   3. orphan (eOrphanAnnot):
//      Seq-entry contains only annotations and this data source doesn't
//      contain Bioseq with specified Seq-id at all.

class NCBI_XOBJMGR_EXPORT CDataLoader : public CObject
{
protected:
    CDataLoader(void);
    CDataLoader(const string& loader_name);

public:
    virtual ~CDataLoader(void);

public:
    /// main blob is blob with sequence
    /// all other blobs are external and contain external annotations
    enum EChoice {
        eBlob,        ///< whole main
        eBioseq,      ///< main blob with complete bioseq
        eCore,        ///< ?only seq-entry core?
        eBioseqCore,  ///< main blob with bioseq core (no seqdata and annots)
        eSequence,    ///< seq data 
        eFeatures,    ///< features from main blob
        eGraph,       ///< graph annotations from main blob
        eAlign,       ///< aligns from main blob
        eAnnot,       ///< all annotations from main blob
        eExtFeatures, ///< external features
        eExtGraph,    ///< external graph annotations
        eExtAlign,    ///< external aligns
        eExtAnnot,    ///< all external annotations
        eOrphanAnnot, ///< all external annotations if no Bioseq exists 
        eAll          ///< all blobs (main and external)
    };
    
    typedef CTSE_Lock               TTSE_Lock;
    typedef set<TTSE_Lock>          TTSE_LockSet;
    typedef CRef<CTSE_Chunk_Info>   TChunk;
    typedef vector<TChunk>          TChunkSet;

    /// Request from a datasource using handles and ranges instead of seq-loc
    /// The TSEs loaded in this call will be added to the tse_set.
    /// The GetRecords() may throw CBlobStateException if the sequence
    /// is not available (not known or disabled), and blob state
    /// is different from minimal fState_no_data.
    /// The actual blob state can be read from the exception in this case.
    virtual TTSE_LockSet GetRecords(const CSeq_id_Handle& idh,
                                    EChoice choice);
    /// The same as GetRecords() but always returns empty TSE lock set
    /// instead of throwing CBlobStateException.
    TTSE_LockSet GetRecordsNoBlobState(const CSeq_id_Handle& idh,
                                       EChoice choice);
    /// Request from a datasource using handles and ranges instead of seq-loc
    /// The TSEs loaded in this call will be added to the tse_set.
    /// Default implementation will call GetRecords().
    virtual TTSE_LockSet GetDetailedRecords(const CSeq_id_Handle& idh,
                                            const SRequestDetails& details);
    /// Request from a datasource set of blobs with external annotations.
    /// CDataLoader has reasonable default implementation.
    virtual TTSE_LockSet GetExternalRecords(const CBioseq_Info& bioseq);

    virtual TTSE_LockSet GetOrphanAnnotRecords(const CSeq_id_Handle& idh,
                                               const SAnnotSelector* sel);
    virtual TTSE_LockSet GetExternalAnnotRecords(const CSeq_id_Handle& idh,
                                                 const SAnnotSelector* sel);
    virtual TTSE_LockSet GetExternalAnnotRecords(const CBioseq_Info& bioseq,
                                                 const SAnnotSelector* sel);

    typedef vector<CSeq_id_Handle> TIds;
    /// Request for a list of all Seq-ids of a sequence.
    /// The result container should not change if sequence with requested id
    /// is not known.
    virtual void GetIds(const CSeq_id_Handle& idh, TIds& ids);

    /// Request for a accession.version Seq-id of a sequence.
    /// Returns null CSeq_id_Handle if sequence with requested id is not known.
    virtual CSeq_id_Handle GetAccVer(const CSeq_id_Handle& idh);
    /// Request for a gi of a sequence.
    /// Returns zero gi if sequence with requested id is not known.
    virtual TGi GetGi(const CSeq_id_Handle& idh);
    /// Request for a label string of a sequence.
    /// Returns empty string if sequence with requested id is not known.
    virtual string GetLabel(const CSeq_id_Handle& idh);
    /// Request for a taxonomy id of a sequence.
    /// Returns -1 if sequence with requested id is not known.
    virtual int GetTaxId(const CSeq_id_Handle& idh);
    /// Request for a length of a sequence.
    /// Returns kInvalidSeqPos if sequence with requested id is not known.
    virtual TSeqPos GetSequenceLength(const CSeq_id_Handle& idh);
    /// Request for a type of a sequence
    /// Returns CSeq_inst::eMol_not_set if sequence is not known.
    virtual CSeq_inst::TMol GetSequenceType(const CSeq_id_Handle& idh);
    /// Request for a state of a sequence.
    /// Returns fState_not_found|fState_no_data if sequence
    /// with requested id is not known.
    virtual int GetSequenceState(const CSeq_id_Handle& idh);

    /// Bulk loading interface for a small pieces of information per id.
    /// The 'loaded' bit set (in/out) marks ids that already processed.
    /// If an element in 'loaded' is set on input then bulk methods
    /// should skip corresponding id, as it's already processed.
    /// Othewise, if the id is known and processed, the 'loaded' element
    /// will be set to true.
    /// Othewise, the 'loaded' element will remain false.
    typedef vector<bool> TLoaded;
    typedef vector<TGi> TGis;
    typedef vector<string> TLabels;
    typedef vector<int> TTaxIds;
    typedef vector<TSeqPos> TSequenceLengths;
    typedef vector<CSeq_inst::TMol> TSequenceTypes;
    typedef vector<int> TSequenceStates;
    /// Bulk request for accession.version Seq-ids of a set of sequences.
    virtual void GetAccVers(const TIds& ids, TLoaded& loaded, TIds& ret);
    /// Bulk request for gis of a set of sequences.
    virtual void GetGis(const TIds& ids, TLoaded& loaded, TGis& ret);
    /// Bulk request for label strings of a set of sequences.
    virtual void GetLabels(const TIds& ids, TLoaded& loaded, TLabels& ret);
    /// Bulk request for taxonomy ids of a set of sequences.
    virtual void GetTaxIds(const TIds& ids, TLoaded& loaded, TTaxIds& ret);
    /// Bulk request for lengths of a set of sequences.
    virtual void GetSequenceLengths(const TIds& ids, TLoaded& loaded,
                                    TSequenceLengths& ret);
    /// Bulk request for types of a set of sequences.
    virtual void GetSequenceTypes(const TIds& ids, TLoaded& loaded,
                                  TSequenceTypes& ret);
    /// Bulk request for states of a set of sequences.
    virtual void GetSequenceStates(const TIds& ids, TLoaded& loaded,
                                   TSequenceStates& ret);

    // Load multiple seq-ids. Same as GetRecords() for multiple ids
    // with choise set to eBlob. The map should be initialized with
    // the id handles to be loaded.
    typedef map<CSeq_id_Handle, TTSE_LockSet> TTSE_LockSets;
    virtual void GetBlobs(TTSE_LockSets& tse_sets);

    // blob operations
    typedef CBlobIdKey TBlobId;
    typedef int TBlobVersion;
    virtual TBlobId GetBlobId(const CSeq_id_Handle& idh);
    virtual TBlobId GetBlobIdFromString(const string& str) const;
    virtual TBlobVersion GetBlobVersion(const TBlobId& id);

    virtual bool CanGetBlobById(void) const;
    virtual TTSE_Lock GetBlobById(const TBlobId& blob_id);

    virtual SRequestDetails ChoiceToDetails(EChoice choice) const;
    virtual EChoice DetailsToChoice(const SRequestDetails::TAnnotSet& annots) const;
    virtual EChoice DetailsToChoice(const SRequestDetails& details) const;

    virtual void GetChunk(TChunk chunk_info);
    virtual void GetChunks(const TChunkSet& chunks);
    
    // 
    virtual void DropTSE(CRef<CTSE_Info> tse_info);
    
    /// Specify datasource to send loaded data to.
    void SetTargetDataSource(CDataSource& data_source);
    
    string GetName(void) const;
    
    /// Resolve TSE conflict
    /// *select the best TSE from the set of dead TSEs.
    /// *select the live TSE from the list of live TSEs
    ///  and mark the others one as dead.
    virtual TTSE_Lock ResolveConflict(const CSeq_id_Handle& id,
                                      const TTSE_LockSet& tse_set);
    virtual void GC(void);

    typedef CRef<IEditSaver> TEditSaver;
    virtual TEditSaver GetEditSaver() const;

protected:
    /// Register the loader only if the name is not yet
    /// registered in the object manager
    static void RegisterInObjectManager(
        CObjectManager&            om,
        CLoaderMaker_Base&         loader_maker,
        CObjectManager::EIsDefault is_default,
        CObjectManager::TPriority  priority);

    void SetName(const string& loader_name);
    CDataSource* GetDataSource(void) const;

    friend class CGBReaderRequestResult;
    friend class CScope_Impl;
    
private:
    CDataLoader(const CDataLoader&);
    CDataLoader& operator=(const CDataLoader&);

    string       m_Name;
    CDataSource* m_DataSource;

    friend class CObjectManager;
};


/* @} */

END_SCOPE(objects)

NCBI_DECLARE_INTERFACE_VERSION(objects::CDataLoader, "xloader", 3, 4, 0);

template<>
class CDllResolver_Getter<objects::CDataLoader>
{
public:
    CPluginManager_DllResolver* operator()(void)
    {
        CPluginManager_DllResolver* resolver =
            new CPluginManager_DllResolver
            (CInterfaceVersion<objects::CDataLoader>::GetName(), 
             kEmptyStr,
             CVersionInfo::kAny,
             CDll::eAutoUnload);

        resolver->SetDllNamePrefix("ncbi");
        return resolver;
    }
};


END_NCBI_SCOPE

#endif  // OBJECTS_OBJMGR___DATA_LOADER__HPP
