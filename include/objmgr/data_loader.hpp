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
#include <objects/seq/seq_id_handle.hpp>
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
private:
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
    virtual TTSE_LockSet GetRecords(const CSeq_id_Handle& idh,
                                    EChoice choice);
    /// Request from a datasource using handles and ranges instead of seq-loc
    /// The TSEs loaded in this call will be added to the tse_set.
    /// Default implementation will call GetRecords().
    virtual TTSE_LockSet GetDetailedRecords(const CSeq_id_Handle& idh,
                                            const SRequestDetails& details);

    /// Request from a datasource set of blobs with external annotations.
    /// CDataLoader has reasonable default implementation.
    virtual TTSE_LockSet GetExternalRecords(const CBioseq_Info& bioseq);

    typedef vector<CSeq_id_Handle> TIds;
    virtual void GetIds(const CSeq_id_Handle& idh, TIds& ids);

    // blob operations
    typedef CConstRef<CObject> TBlobId;
    typedef int TBlobVersion;
    virtual TBlobId GetBlobId(const CSeq_id_Handle& idh);
    virtual TBlobVersion GetBlobVersion(const TBlobId& id);

    virtual bool CanGetBlobById(void) const;
    virtual TTSE_Lock GetBlobById(const TBlobId& blob_id);

    virtual bool LessBlobId(const TBlobId& id1, const TBlobId& id2) const;
    virtual string BlobIdToString(const TBlobId& id) const;

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
    
private:
    CDataLoader(const CDataLoader&);
    CDataLoader& operator=(const CDataLoader&);

    string       m_Name;
    CDataSource* m_DataSource;

    friend class CObjectManager;
};


/* @} */


class NCBI_XOBJMGR_EXPORT CBlobIdKey
{
public:
    CBlobIdKey(void)
        {
        }
    CBlobIdKey(const CDataLoader* loader, const CDataLoader::TBlobId& blob_id)
        : m_Loader(loader), m_BlobId(blob_id)
        {
        }

    string ToString(void) const;

    bool operator<(const CBlobIdKey& id) const
        {
            if ( m_Loader != id.m_Loader ) return m_Loader < id.m_Loader;
            if ( !m_Loader ) return m_BlobId < id.m_BlobId;
            return m_Loader->LessBlobId(m_BlobId, id.m_BlobId);
        }
    bool operator==(const CBlobIdKey& id) const
        {
            if ( m_Loader != id.m_Loader ) return false;
            if ( !m_Loader ) return m_BlobId == id.m_BlobId;
            return !m_Loader->LessBlobId(m_BlobId, id.m_BlobId) &&
                !m_Loader->LessBlobId(id.m_BlobId, m_BlobId);
        }
    bool operator!=(const CBlobIdKey& id) const
        {
            if ( m_Loader != id.m_Loader ) return true;
            if ( !m_Loader ) return m_BlobId != id.m_BlobId;
            return m_Loader->LessBlobId(m_BlobId, id.m_BlobId) ||
                m_Loader->LessBlobId(id.m_BlobId, m_BlobId);
        }

private:
    CConstRef<CDataLoader>  m_Loader;
    CDataLoader::TBlobId    m_BlobId;
};


END_SCOPE(objects)

NCBI_DECLARE_INTERFACE_VERSION(objects::CDataLoader, "xloader", 1, 0, 0);

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



/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.42  2005/03/07 14:43:15  ssikorsk
* Do not unload PluginManager drivers by default
*
* Revision 1.41  2004/12/22 19:26:31  grichenk
* +CDllResolver_Getter<objects::CDataLoader>
*
* Revision 1.40  2004/12/22 15:56:06  vasilche
* Added possibility to reload TSEs by their BlobId.
* Added convertion of BlobId to string.
* Added helper class CBlobIdKey to use it as key, and to print BlobId.
* Removed obsolete DebugDump.
* Fixed default implementation of CDataSource::GetIds() for matching Seq-ids.
*
* Revision 1.39  2004/12/13 15:19:20  grichenk
* Doxygenized comments
*
* Revision 1.38  2004/11/22 16:04:06  grichenk
* Fixed/added doxygen comments
*
* Revision 1.37  2004/10/26 15:47:43  vasilche
* Added short description of various types of annotation blobs.
*
* Revision 1.36  2004/10/25 16:53:25  vasilche
* Removed obsolete comments and methods.
* Added support for orphan annotations.
* One of GetRecords() methods renamed to avoid name conflict.
*
* Revision 1.35  2004/09/27 14:13:50  kononenk
* Added doxygen formating
*
* Revision 1.34  2004/08/31 21:03:48  grichenk
* Added GetIds()
*
* Revision 1.33  2004/08/19 16:54:04  vasilche
* CDataLoader::GetDataSource() made const.
*
* Revision 1.32  2004/08/05 18:24:36  vasilche
* CAnnotName and CAnnotTypeSelector are moved in separate headers.
*
* Revision 1.31  2004/08/04 14:53:25  vasilche
* Revamped object manager:
* 1. Changed TSE locking scheme
* 2. TSE cache is maintained by CDataSource.
* 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
* 4. Fixed processing of split data.
*
* Revision 1.30  2004/08/02 17:34:43  grichenk
* Added data_loader_factory.cpp.
* Renamed xloader_cdd to ncbi_xloader_cdd.
* Implemented data loader factories for all loaders.
*
* Revision 1.29  2004/07/28 14:02:56  grichenk
* Improved MT-safety of RegisterInObjectManager(), simplified the code.
*
* Revision 1.28  2004/07/26 14:13:31  grichenk
* RegisterInObjectManager() return structure instead of pointer.
* Added CObjectManager methods to manipuilate loaders.
*
* Revision 1.27  2004/07/21 15:51:23  grichenk
* CObjectManager made singleton, GetInstance() added.
* CXXXXDataLoader constructors made private, added
* static RegisterInObjectManager() and GetLoaderNameFromArgs()
* methods.
*
* Revision 1.26  2004/07/12 15:05:31  grichenk
* Moved seq-id mapper from xobjmgr to seq library
*
* Revision 1.25  2003/11/26 17:55:52  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.24  2003/10/07 13:43:22  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.23  2003/09/30 16:21:59  vasilche
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
* Revision 1.22  2003/07/17 20:07:55  vasilche
* Reduced memory usage by feature indexes.
* SNP data is loaded separately through PUBSEQ_OS.
* String compression for SNP data.
*
* Revision 1.21  2003/06/19 18:23:44  vasilche
* Added several CXxx_ScopeInfo classes for CScope related information.
* CBioseq_Handle now uses reference to CBioseq_ScopeInfo.
* Some fine tuning of locking in CScope.
*
* Revision 1.20  2003/06/02 16:01:36  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.19  2003/05/20 15:44:37  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.18  2003/05/06 18:54:06  grichenk
* Moved TSE filtering from CDataSource to CScope, changed
* some filtering rules (e.g. priority is now more important
* than scope history). Added more caches to CScope.
*
* Revision 1.17  2003/04/29 19:51:12  vasilche
* Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
* Made some typedefs more consistent.
*
* Revision 1.16  2003/04/25 14:23:46  vasilche
* Added explicit constructors, destructor and assignment operator to make it compilable on MSVC DLL.
*
* Revision 1.15  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.14  2003/03/21 19:22:48  grichenk
* Redesigned TSE locking, replaced CTSE_Lock with CRef<CTSE_Info>.
*
* Revision 1.13  2002/12/26 20:51:35  dicuccio
* Added Win32 export specifier
*
* Revision 1.12  2002/08/28 17:05:13  vasilche
* Remove virtual inheritance
*
* Revision 1.11  2002/06/30 03:27:38  vakatov
* Get rid of warnings, ident the code, move CVS logs to the end of file
*
* Revision 1.10  2002/06/04 17:18:32  kimelman
* memory cleanup :  new/delete/Cref rearrangements
*
* Revision 1.9  2002/05/14 20:06:23  grichenk
* Improved CTSE_Info locking by CDataSource and CDataLoader
*
* Revision 1.8  2002/05/06 03:30:35  vakatov
* OM/OM1 renaming
*
* Revision 1.7  2002/03/30 19:37:05  kimelman
* gbloader MT test
*
* Revision 1.6  2002/03/20 04:50:34  kimelman
* GB loader added
*
* Revision 1.5  2002/03/18 23:05:18  kimelman
* comments
*
* Revision 1.4  2002/03/18 17:26:32  grichenk
* +CDataLoader::x_GetSeq_id(), x_GetSeq_id_Key(), x_GetSeq_id_Handle()
*
* Revision 1.3  2002/03/11 21:10:11  grichenk
* +CDataLoader::ResolveConflict()
*
* Revision 1.2  2002/02/21 19:27:00  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.1  2002/01/11 19:04:00  gouriano
* restructured objmgr
*
* ===========================================================================
*/

#endif  // OBJECTS_OBJMGR___DATA_LOADER__HPP
