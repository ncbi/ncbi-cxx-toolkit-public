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
    
    virtual ~CBlastDbDataLoader();
    
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
    
    /// Test method for GetBlobById feature.
    ///
    /// The caller will use this method to determine whether this data
    /// loader allows blobs to be managed by ID.
    ///
    /// @return
    ///   Returns true to indicate that GetBlobById() is available.
    virtual bool CanGetBlobById() const;
    
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
    /// TPlace is a Seq-id or an integer id, this data loader uses the former.
    typedef CTSE_Chunk_Info::TPlace         TPlace;
    
    /// A list of 'chunk' objects, generic sequence related data elements.
    typedef vector< CRef<CTSE_Chunk_Info> > TChunks;
    
    typedef CParamLoaderMaker<CBlastDbDataLoader, SBlastDbParam> TMaker;
    friend class CParamLoaderMaker<CBlastDbDataLoader, SBlastDbParam>;
    
    CBlastDbDataLoader(const string&        loader_name,
                       const SBlastDbParam& param);
    
    /// Prevent automatic copy constructor generation
    CBlastDbDataLoader(const CBlastDbDataLoader &);
    
    /// Prevent automatic assignment operator generation
    CBlastDbDataLoader & operator=(const CBlastDbDataLoader &);

public:    
    /// Manages a chunk of sequence data.
    class CSeqChunkData {
    public:
        /// Construct a chunk of sequence data.
        ///
        /// This constructor stores the components needed to build a
        /// range of sequence data (the Seq-literal).  The literal is
        /// not built here, and will not be built until/unless the
        /// user requests the indicated portion of the sequence.
        /// 
        /// @param seqdb
        ///   A reference to the CSeqDB database.
        /// @param sih
        ///   Identifies the original sequence to build a piece of.
        /// @param oid
        ///   Locates the sequence within the CSeqDB database.
        /// @param begin
        ///   The coordinate of the start of the region to build.
        /// @param end
        ///   The coordinate of the end of the region to build.
        CSeqChunkData(CRef<CSeqDB>     seqdb,
                      CSeq_id_Handle & sih,
                      int              oid,
                      int              begin,
                      int              end)
            : m_SeqDB(seqdb), m_OID(oid), m_SIH(sih), m_Begin(begin), m_End(end)
        {
        }
        
        /// Default constructor; required by std::map.
        CSeqChunkData()
            : m_Begin(-1)
        {
        }
        
        /// Test whether the Seq-literal has already been built.
        /// @return
        ///   true if the literal is present here.
        bool HasLiteral()
        {
            return m_Literal.NotEmpty();
        }
        
        /// Code to actually build the literal.
        void BuildLiteral();
        
        /// Fetch the Seq-literal, which must already exist.
        /// @return
        ///   The sequence literal.
        CRef<CSeq_literal> GetLiteral()
        {
            _ASSERT(m_Literal.NotEmpty());
            return m_Literal;
        }
        
        /// Get the Seq-id-handle identifying this sequence.
        CSeq_id_Handle GetSeqIdHandle()
        {
            return m_SIH;
        }
        
        /// Get the starting offset of this sequence.
        int GetPosition()
        {
            return m_Begin;
        }
        
    private:
        /// A reference to the CSeqDB database.
        CRef<CSeqDB>       m_SeqDB;
        
        /// Locates the sequence within the CSeqDB database.
        int                m_OID;
        
        /// Identifies the original sequence to build a piece of.
        CSeq_id_Handle     m_SIH;
        
        /// The coordinate of the start of the region to build.
        int                m_Begin;
        
        /// The coordinate of the end of the region to build.
        int                m_End;
        
        /// Holds the sequence literal once it is built.
        CRef<CSeq_literal> m_Literal;
    };
    
    /// Description data
    class CDescrData {
    public:
        /// Empty constructor needed for std::map.
        CDescrData()
        {
        }
        
        /// Description data.
        ///
        /// Build an object containing the description data.
        /// Construction of these description objects is relatively
        /// inexpensive, and is done immediately rather than deferred.
        ///
        /// @param sih
        ///   Identifies the original sequence this data describes.
        /// @param descr
        ///   A description of the sequence.
        CDescrData(CSeq_id_Handle & sih, CRef<CSeq_descr> descr)
            : m_SIH(sih), m_Descr(descr)
        {
        }
        
        /// Get the identifier this description corresponds to.
        /// @return
        ///   A handle for the sequence identifier.
        CSeq_id_Handle & GetSeqIdHandle()
        {
            return m_SIH;
        }
        
        /// Get the sequence description.
        /// @return
        ///   An object describing the sequence.
        CRef<CSeq_descr> GetDescr()
        {
            return m_Descr;
        }
        
    private:
        /// A handle to the sequence identifier.
        CSeq_id_Handle   m_SIH;
        
        /// The sequence description object.
        CRef<CSeq_descr> m_Descr;
    };
    
    /// A mapping from chunk id to sequence data region.
    typedef map< int, CSeqChunkData >  TSeqChunkCache;
    
    /// A mapping from chunk id to sequence description.
    typedef map< int, CDescrData >     TDescrChunks;
    
    /// A mapping from sequence identifier to blob ids.
    typedef map< CSeq_id_Handle, int > TIds;
    
    
    /// Manages a sequence and its subordinate chunks.
    class CCachedSeqData : public CObject {
    public:
        /// Construct and process the sequence.
        ///
        /// This constructor starts the processing of the specified
        /// sequence.  It loads the bioseq (minus sequence data) and
        /// caches the sequence length.
        ///
        /// @param sih
        ///   A handle to the sequence identifier.
        /// @param seqdb
        ///   The sequence database containing the original sequence.
        /// @param oid
        ///   Locates the sequence within the CSeqDB database.
        CCachedSeqData(const CSeq_id_Handle & sih, CSeqDB & seqdb, int oid);
        
        /// Get the top-level seq-entry.
        CRef<CSeq_entry> GetTSE()
        {
            return m_TSE;
        }
        
        /// Get the sequence identification.
        CSeq_id_Handle & GetSeqIdHandle()
        {
            return m_SIH;
        }
        
        /// Get the sequence description
        CRef<CSeq_descr> GetDescr()
        {
            return m_Descr;
        }
        
        /// Get the length of the sequence.
        int GetLength()
        {
            return m_Length;
        }
        
        /// Build an object representing a range of sequence data.
        ///
        /// An object will be constructed to represent a range of this
        /// sequence's data.  This ids of the objects returned from
        /// this method are stored in this (CCachedSeqData) object.
        ///
        /// @param id
        ///   The chunk id for the new chunk.
        /// @param begin
        ///   The offset into the sequence of the start of this chunk.
        /// @param end
        ///   The offset into the sequence of the end of this chunk.
        /// @return
        ///   An object representing the specified range of sequence data.
        CSeqChunkData BuildDataChunk(int id, int begin, int end);
        
        /// Build an object representing a sequence description.
        ///
        /// An object will be constructed to represent a sequence
        /// description.  The ids of the objects returned from this
        /// method are stored in this (CCachedSeqData) object.
        ///
        /// @param chunknum
        ///   The chunk id for the new chunk.
        /// @return
        ///   An object representing the sequence description.
        CDescrData AddDescr(int chunknum)
        {
            m_DescrIds.push_back(chunknum);
            return CDescrData(GetSeqIdHandle(), GetDescr());
        }
        
        /// Remove this sequence's sub-objects from the data loader.
        ///
        /// The data loader stores a number of objects associated with
        /// this sequence.  This method removes those objects from the
        /// std::map containers passed to it, and clears internal data
        /// stored for this object.
        ///
        /// @param descrs
        ///   A map from chunk id to sequence description objects.
        /// @param seqdata
        ///   A map from chunk id to sequence data ranges objects.
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
        
        /// Add this sequence's identifiers to a lookup table.
        ///
        /// This method adds identifiers from this sequence to a
        /// lookup table so that the OID of the sequence can be found
        /// quickly during future processing of the same sequence.
        ///
        /// @param idmap
        ///   A map from CSeq_id_Handle to OID.
        void RegisterIds(TIds & idmap);
        
    private:
        /// Add an empty CDelta_seq to the Seq-entry in m_TSE.
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
        
        /// Locates this sequence within m_SeqDB.
        int m_OID;
        
        /// Associated chunk numbers for descriptions
        vector<int> m_DescrIds;
        
        /// Associated chunk numbers for sequence data
        vector<int> m_SeqDataIds;
    };
private:
    /// Load sequence data from cache or from the database.
    ///
    /// This checks the OID cache and loads the sequence data from
    /// there or if not found, from the CSeqDB database.  When new
    /// data is built, the sequence is also split into chunks.  A
    /// description of what data is available will be returned in the
    /// "lock" parameter.
    ///
    /// @param idh
    ///   Identifies the sequence for which to load data.
    /// @param lock
    ///   Information about the sequence data is returned here.
    /// @return
    ///   true for success, false if the identifier is not found.
    bool x_LoadData(const CSeq_id_Handle& idh, CTSE_LoadLock & lock);
    
    /// Split the sequence description.
    ///
    /// The sequence description is stored and a list of available
    /// description chunks is returned via the 'chunks' parameter.
    /// These do not contain the sequence descriptions, but merely
    /// indicate what is available.
    ///
    /// @param chunks
    ///   The sequence description chunks will be returned here.
    /// @param seqdata
    ///   Object managing all data pieces for this sequence.
    void x_SplitDescr(TChunks & chunks, CCachedSeqData & seqdata);
    
    /// Split the sequence.
    ///
    /// Chunks listing the descriptions and data ranges for this
    /// sequence is returned via the 'chunks' parameter.
    ///
    /// @param chunks
    ///   The sequence data chunks will be returned here.
    /// @param seqdata
    ///   Object managing all data pieces for this sequence.
    void x_SplitSeq(TChunks & chunks, CCachedSeqData & seqdata);
    
    /// Split the sequence data.
    ///
    /// The sequence data is stored and a list of the available ranges
    /// of data is returned via the 'chunks' parameter.  These do not
    /// contain sequence data, but merely indicate what is available.
    ///
    /// @param chunks
    ///   The sequence data chunks will be returned here.
    /// @param seqdata
    ///   Object managing all data pieces for this sequence.
    void x_SplitSeqData(TChunks & chunks, CCachedSeqData & seqdata);
    
    /// Add a chunk of sequence data
    ///
    /// This method builds a description of a specific range of
    /// sequence data, returning it via the 'chunks' parameter.  The
    /// actual data is not built, just a description that identifies
    /// the sequence and the range of that sequence's data represented
    /// by this chunk.
    void x_AddSplitSeqChunk(TChunks        & chunks,
                            CCachedSeqData & seqdata,
                            int              begin,
                            int              end);
    
    /// A mapping from OID to cached sequence information.
    typedef map< int, CRef<CCachedSeqData> > TOidCache;
    
    const string    m_DBName;      ///< Blast database name
    EDbType         m_DBType;      ///< Is this database protein or nucleotide?
    CRef<CSeqDB>    m_SeqDB;       ///< The sequence database
    TOidCache       m_OidCache;    ///< The primary cache of sequence information
    TSeqChunkCache  m_SeqChunks;   ///< A cache of chunks of sequence data
    TDescrChunks    m_DescrChunks; ///< A cache of sequence descriptions
    TIds            m_Ids;         ///< ID to OID translation
    int             m_NextChunkId; ///< The next chunk id to assign
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
 * Revision 1.20  2005/07/12 12:53:33  camacho
 * Doxygen fix
 *
 * Revision 1.19  2005/07/11 15:23:07  bealer
 * - Doxygen
 * - Remove unnecessary parameter.
 *
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
