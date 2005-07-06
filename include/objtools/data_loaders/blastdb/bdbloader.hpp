#ifndef BDBLOADER_HPP
#define BDBLOADER_HPP

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
*  ===========================================================================
*
*  Author: Christiam Camacho
*
*  File Description:
*   Data loader implementation that uses the blast databases
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <objmgr/data_loader.hpp>
#include <objtools/readers/seqdb/seqdb.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqset/Seq_entry.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////////
//
// CBlastDbDataLoader
//   Data loader implementation that uses the blast databases.
//

// Parameter names used by loader factory

const string kCFParam_BlastDb_DbName = "DbName"; // = string
const string kCFParam_BlastDb_DbType = "DbType"; // = EDbType (e.g. "Protein")


class NCBI_XLOADER_BLASTDB_EXPORT CBlastDbDataLoader : public CDataLoader
{
    /// The sequence data will sliced into pieces of this size.
    enum { kSequenceSliceSize = 65536 };
    
public:

    /// Describes the type of blast database to use
    enum EDbType {
        eNucleotide = 0,    ///< nucleotide database
        eProtein = 1,       ///< protein database
        eUnknown = 2        ///< protein is attempted first, then nucleotide
    };

    struct SBlastDbParam
    {
        SBlastDbParam(const string& db_name = "nr",
                      EDbType       dbtype = eUnknown)
            : m_DbName(db_name), m_DbType(dbtype) {}
        string  m_DbName;
        EDbType m_DbType;
    };

    typedef SRegisterLoaderInfo<CBlastDbDataLoader> TRegisterLoaderInfo;
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const string& dbname = "nr",
        const EDbType dbtype = eUnknown,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(const SBlastDbParam& param);
    static string GetLoaderNameFromArgs(const string& dbname = "nr",
                                        const EDbType dbtype = eUnknown)
    {
        return GetLoaderNameFromArgs(SBlastDbParam(dbname, dbtype));
    }
    
    virtual ~CBlastDbDataLoader(void);
    
    virtual TTSE_LockSet GetRecords(const CSeq_id_Handle& idh, EChoice choice);
    
    virtual void DebugDump(CDebugDumpContext ddc, unsigned int depth) const;
    
    
    /// Unload a TSE, clear chunk mappings
    virtual void DropTSE(CRef<CTSE_Info> tse_info);
    
    /// Load a description or data chunk.
    virtual void GetChunk(TChunk chunk);
    
    /// Gets the blob id for a given sequence.
    ///
    /// Given a Seq_id_Handle, this method finds the corresponding top
    /// level Seq-entry (TSE) and returns a blob corresponding to it.
    /// The BlobId is initialized with a pointer to that CSeq_entry if
    /// the sequence is known to this data loader, which will be true
    /// if GetRecords() was called for this sequence.
    ///
    /// @param idh
    ///   Indicates the sequence for which to get a blob id.
    /// @return
    ///   A TBlobId corresponding to the provided Seq_id_Handle.
    virtual TBlobId GetBlobId(const CSeq_id_Handle& idh);
    
    /// Returns true to indicate that GetBlobId is implemented.
    virtual bool CanGetBlobById(void) const;
    
    /// For a given TBlobId, get the TTSE_Lock.
    ///
    /// If the provided TBlobId is known to this code, the
    /// corresponding TTSE_Lock data will be fetched and returned.
    /// Otherwise, an empty valued TTSE_Lock is returned.
    ///
    /// @param blob_id
    ///   Indicates which data to get.
    /// @return
    ///   The returned data.
    virtual TTSE_Lock GetBlobById(const TBlobId& blob_id);
    
private:
    typedef CTSE_Chunk_Info::TPlace         TPlace;
    typedef vector< CRef<CTSE_Chunk_Info> > TChunks;

    typedef CParamLoaderMaker<CBlastDbDataLoader, SBlastDbParam> TMaker;
    friend class CParamLoaderMaker<CBlastDbDataLoader, SBlastDbParam>;
    
    CBlastDbDataLoader(const string&        loader_name,
                       const SBlastDbParam& param);
    
    CBlastDbDataLoader(const CBlastDbDataLoader &);
    CBlastDbDataLoader & operator=(const CBlastDbDataLoader &);

public:    
    // Per-Sequence Chunk Data
    class CSeqChunkData {
    public:
        CSeqChunkData(CRef<CSeqDB>     seqdb,
                      CSeq_id_Handle & sih,
                      int              oid,
                      int              begin,
                      int              end)
            : m_SeqDB(seqdb), m_OID(oid), m_SIH(sih), m_Begin(begin), m_End(end)
        {
        }
        
        CSeqChunkData()
            : m_Begin(-1)
        {
        }
        
        bool HasLiteral()
        {
            return m_Literal.NotEmpty();
        }
        
        void BuildLiteral();
        
        CRef<CSeq_literal> GetLiteral()
        {
            _ASSERT(m_Literal.NotEmpty());
            return m_Literal;
        }
        
        CSeq_id_Handle GetSeqIdHandle()
        {
            return m_SIH;
        }
        
        int GetPosition()
        {
            return m_Begin;
        }
        
    private:
        CRef<CSeqDB>       m_SeqDB;
        int                m_OID;
        CSeq_id_Handle     m_SIH;
        int                m_Begin;
        int                m_End;
        CRef<CSeq_literal> m_Literal;
    };
    
    // Description data
    class CDescrData {
    public:
        CDescrData()
        {
        }
        
        CDescrData(CSeq_id_Handle & sih, CRef<CSeq_descr> descr)
            : m_SIH(sih), m_Descr(descr)
        {
        }
        
        CSeq_id_Handle & GetSeqIdHandle()
        {
            return m_SIH;
        }
        
        CRef<CSeq_descr> GetDescr()
        {
            return m_Descr;
        }
        
    private:
        CSeq_id_Handle   m_SIH;
        CRef<CSeq_descr> m_Descr;
    };
    
    typedef map< int, CSeqChunkData >  TSeqChunkCache;
    typedef map< int, CDescrData >     TDescrChunks;
    typedef map< CSeq_id_Handle, int > TIds;
    
    // Per-OID Data
    
    class CCachedSeqData : public CObject {
    public:
        CCachedSeqData(const CSeq_id_Handle & sih, CSeqDB & seqdb, int oid);
        
        CRef<CSeq_entry> GetTSE()
        {
            return m_TSE;
        }
        
        CSeq_id_Handle & GetSeqIdHandle()
        {
            return m_SIH;
        }
        
        CRef<CSeq_descr> GetDescr()
        {
            return m_Descr;
        }
        
        int GetLength()
        {
            return m_Length;
        }
        
        CSeqChunkData BuildDataChunk(int id, int begin, int end);
        
        CDescrData AddDescr(int chunknum)
        {
            m_DescrIds.push_back(chunknum);
            return CDescrData(GetSeqIdHandle(), GetDescr());
        }
        
        void FreeChunks(TDescrChunks & descrs, TSeqChunkCache & seqdata)
        {
            m_TSE.Reset();
            
            ITERATE(vector<int>, diter, m_DescrIds) {
                descrs.erase(*diter);
            }
            m_DescrIds.clear();
            
            ITERATE(vector<int>, sditer, m_SeqDataIds) {
                seqdata.erase(*sditer);
            }
            m_SeqDataIds.clear();
        }
        
        void RegisterIds(TIds & idmap, int oid);
        
    private:
        void x_AddDelta(int begin, int end);
        
        /// SeqID handle
        CSeq_id_Handle m_SIH;
        
        /// Seq entry
        CRef<CSeq_entry> m_TSE;
        
        /// Sequence length in bases
        int m_Length;
        
        /// Descriptors
        CRef<CSeq_descr> m_Descr;
        
        /// Database reference
        CRef<CSeqDB> m_SeqDB;
        
        /// Database reference
        int m_OID;
        
        /// Associated chunk numbers for descriptions
        vector<int> m_DescrIds;
        
        /// Associated chunk numbers for sequence data
        vector<int> m_SeqDataIds;
    };
private:
    
    bool x_LoadData(const CSeq_id_Handle& idh, CTSE_LoadLock & lock);
    
    void x_SplitDescr(TChunks & chunks, CCachedSeqData & seqdata);
    
    void x_SplitSeq(TChunks & chunks, CCachedSeqData & seqdata);
    
    void x_SplitSeqData(TChunks & chunks, CCachedSeqData & seqdata);

    void x_AddSplitSeqChunk(TChunks        & chunks,
                            CCachedSeqData & seqdata,
                            int              begin,
                            int              end);
    
    typedef map< int, CRef<CCachedSeqData> > TOidCache;
    
    const string    m_DBName;      ///< blast database name
    EDbType         m_DBType;      ///< is this database protein or nucleotide?
    CRef<CSeqDB>    m_SeqDB;
    TOidCache       m_OidCache;
    TSeqChunkCache  m_SeqChunks;
    TDescrChunks    m_DescrChunks;
    TIds            m_Ids;         /// ID to OID translation
    
    int m_NextChunkId;
};

END_SCOPE(objects)


extern NCBI_XLOADER_BLASTDB_EXPORT const string kDataLoader_BlastDb_DriverName;

extern "C"
{

NCBI_XLOADER_BLASTDB_EXPORT
void NCBI_EntryPoint_DataLoader_BlastDb(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

NCBI_XLOADER_BLASTDB_EXPORT
void NCBI_EntryPoint_xloader_blastdb(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

} // extern C


END_NCBI_SCOPE

/* ========================================================================== 
 *
 * $Log$
 * Revision 1.18  2005/07/06 20:16:17  vasilche
 * Declare internal classes and typedefs in public section to make WorkShop happy.
 *
 * Revision 1.17  2005/07/06 20:13:37  ucko
 * Include Seq_entry.hpp to keep references to CRef<CSeq_entry> from
 * breaking (which happens under at least GCC 2.95).
 *
 * Revision 1.16  2005/07/06 19:03:26  bealer
 * - Some doxygen.
 *
 * Revision 1.15  2005/07/06 17:21:44  bealer
 * - Sequence splitting capability for BlastDbDataLoader.
 *
 * Revision 1.14  2005/06/23 16:21:15  camacho
 * Doxygen fix
 *
 * Revision 1.13  2004/11/29 20:57:09  grichenk
 * Added GetLoaderNameFromArgs with full set of arguments.
 * Fixed BlastDbDataLoader name.
 *
 * Revision 1.12  2004/10/25 16:53:37  vasilche
 * No need to reimplement GetRecords as method name conflict is resolved.
 *
 * Revision 1.11  2004/10/21 22:46:54  bealer
 * - Unhide inherited method.
 *
 * Revision 1.10  2004/10/05 16:44:33  jianye
 * Use CSeqDB
 *
 * Revision 1.9  2004/08/10 16:56:10  grichenk
 * Fixed dll export declarations, moved entry points to cpp.
 *
 * Revision 1.8  2004/08/04 19:35:08  grichenk
 * Renamed entry points to be found by dll resolver.
 * GB loader uses CPluginManagerStore to get/put
 * plugin manager for readers.
 *
 * Revision 1.7  2004/08/04 14:56:34  vasilche
 * Updated to changes in TSE locking scheme.
 *
 * Revision 1.6  2004/08/02 17:34:43  grichenk
 * Added data_loader_factory.cpp.
 * Renamed xloader_cdd to ncbi_xloader_cdd.
 * Implemented data loader factories for all loaders.
 *
 * Revision 1.5  2004/07/28 14:02:56  grichenk
 * Improved MT-safety of RegisterInObjectManager(), simplified the code.
 *
 * Revision 1.4  2004/07/26 14:13:31  grichenk
 * RegisterInObjectManager() return structure instead of pointer.
 * Added CObjectManager methods to manipuilate loaders.
 *
 * Revision 1.3  2004/07/21 15:51:23  grichenk
 * CObjectManager made singleton, GetInstance() added.
 * CXXXXDataLoader constructors made private, added
 * static RegisterInObjectManager() and GetLoaderNameFromArgs()
 * methods.
 *
 * Revision 1.2  2003/09/30 16:36:33  vasilche
 * Updated CDataLoader interface.
 *
 * Revision 1.1  2003/08/06 16:15:17  jianye
 * Add BLAST DB loader.
 *
 * Revision 1.4  2003/05/19 21:11:46  camacho
 * Added caching
 *
 * Revision 1.3  2003/05/16 14:27:48  camacho
 * Proper use of namespaces
 *
 * Revision 1.2  2003/05/08 15:11:43  camacho
 * Changed prototype for GetRecords in base class
 *
 * Revision 1.1  2003/03/14 22:37:26  camacho
 * Initial revision
 *
 *
 * ========================================================================== */

#endif
