#ifndef BDB___SPLIT_BLOB_HPP__
#define BDB___SPLIT_BLOB_HPP__

/* $Id$
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
 * Author:  Anatoliy Kuznetsov
 *   
 * File Description: BDB library split BLOB store.
 *
 */


/// @file bdb_split_blob.hpp
/// BDB library split BLOB store.

#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbifile.hpp>

#include <util/math/matrix.hpp>

#include <bdb/bdb_blob.hpp>
#include <bdb/bdb_bv_store.hpp>
#include <bdb/bdb_cursor.hpp>
#include <bdb/bdb_trans.hpp>

#include <util/id_mux.hpp>


BEGIN_NCBI_SCOPE

/** @addtogroup BDB_BLOB
 *
 * @{
 */


/// Persistent storage for demux information
///
template<class TBV>
class CBDB_BlobStoreDict : public CBDB_BvStore<TBV>
{
public:
    CBDB_FieldUint4        dim;     ///< dimention
    CBDB_FieldUint4        dim_idx; ///< projection index

    typedef CBDB_BvStore<TBV>         TParent;

public:
    CBDB_BlobStoreDict()
    {
        this->BindKey("dim",       &dim);
        this->BindKey("dim_idx",   &dim_idx);
    }
};

/// Volume split BLOB demultiplexer
///
/// This class is doing some simple accounting, counting size and number
/// of incoming LOBs, splitting them into [volume, page size]
///
class CBDB_BlobDeMux : public IObjDeMux<unsigned>
{
public:
    typedef CNcbiMatrix<double>    TVolumeSize;
    typedef CNcbiMatrix<unsigned>  TVolumeRecs;

public:
    CBDB_BlobDeMux(double    vol_max = 1.5 * (1024.00*1024.00*1024.00), 
                   unsigned  rec_max = 3 * 1000000) 
        : m_VolMax(vol_max), m_RecMax(rec_max)
    {
    }

    /// coordinates:
    ///
    ///  0 - active volume number
    ///  1 - page split number
    ///
    void GetCoordinates(unsigned blob_size, unsigned* coord)
    {
        _ASSERT(coord);
        _ASSERT(m_RecS.GetRows() == m_VolS.GetRows());
        _ASSERT(m_RecS.GetCols() == m_VolS.GetCols());

        coord[1] = CBDB_BlobDeMux::SelectSplit(blob_size);
        size_t max_col = max((size_t)m_RecS.GetCols(),
                             (size_t)coord[1] + 1);
        m_RecS.Resize(m_RecS.GetRows(), max_col);
        m_VolS.Resize(m_VolS.GetRows(), max_col);

        for (unsigned i = 0;  i < m_RecS.GetRows();  ++i) {
            if (m_RecS(i, coord[1]) < m_RecMax  &&
                m_VolS(i, coord[1]) < m_VolMax) {
                coord[0] = i;
                ++m_RecS(i, coord[1]);
                m_VolS  (i, coord[1]) += blob_size;
                return;
            }
        }

        /// not found
        NewPlane();
        coord[0] = m_RecS.GetRows() - 1;
    }

    /// LOBs are getting split into slices based on LOB size,
    /// similar BLOBs go to the compartment with more optimal storage 
    /// paramaters
    ///
    static
    unsigned SelectSplit(unsigned blob_size)
    {
        static unsigned size_split[] = {
            256, 512, 2048, 4096, 8192, 16384, 32768
        };
        for(unsigned i = 0; i < sizeof(size_split)/sizeof(*size_split); ++i) {
            if (blob_size < size_split[i])  
                return i;
        }
        return 7;
    }

protected:
    void NewPlane()
    {
        size_t max_col = max((size_t)m_RecS.GetCols(), (size_t)1);
        m_RecS.Resize(m_RecS.GetRows() + 1, max_col);
        m_VolS.Resize(m_VolS.GetRows() + 1, max_col);
    }


    TVolumeSize  m_VolS;  ///< Volumes BLOB sizes
    TVolumeRecs  m_RecS;  ///< Volumes record counts
    
    double       m_VolMax; ///< Volume max size
    unsigned     m_RecMax; ///< Maximum number of records
};

/// Split demux which can save and load state into a file 
/// Stateful (persistent) class.
///
class CBDB_BlobDeMuxPersistent : public CBDB_BlobDeMux
{
public:
    CBDB_BlobDeMuxPersistent(const string& path,
        double    vol_max = 1.5 * (1024.00*1024.00*1024.00),
        unsigned  rec_max = 3 * 1000000) 
        : CBDB_BlobDeMux(vol_max, rec_max)
        , m_Path(path)
    {
        if ( !m_Path.empty()  &&  CFile(m_Path).Exists()) {
            CNcbiIfstream istr(m_Path.c_str());
            Load(istr);
        }
    }

    ~CBDB_BlobDeMuxPersistent()
    {
        if ( !m_Path.empty() ) {
            try {
                CNcbiOfstream ostr(m_Path.c_str());
                Save(ostr);
            }
            catch (CException& e) {
                LOG_POST(Error << "CBDB_BlobDeMux::~CBDB_BlobDeMux(): "
                         "error saving demultiplex data: " << e.what());
            }
        }
    }


    void Save(CNcbiOstream& ostr)
    {
        ostr << m_RecS.GetRows() << " " << m_RecS.GetCols() << endl;
        for (size_t i = 0;  i < m_RecS.GetRows();  ++i) {
            for (size_t j = 0;  j < m_RecS.GetCols();  ++j) {
                ostr << m_RecS(i, j) << " ";
            }
            ostr << endl;
        }
        ostr << m_VolS.GetRows() << " " << m_VolS.GetCols() << endl;
        for (size_t i = 0;  i < m_VolS.GetRows();  ++i) {
            for (size_t j = 0;  j < m_VolS.GetCols();  ++j) {
                ostr << m_VolS(i, j) << " ";
            }
            ostr << endl;
        }
    }

    void Load(CNcbiIstream& istr)
    {
        size_t i, j;
        istr >> i >> j;
        m_RecS.Resize(i, j);
        for (size_t i = 0;  i < m_RecS.GetRows();  ++i) {
            for (size_t j = 0;  j < m_RecS.GetCols();  ++j) {
                istr >> m_RecS(i, j);
            }
        }
        istr >> i >> j;
        m_VolS.Resize(i, j);
        for (size_t i = 0;  i < m_VolS.GetRows();  ++i) {
            for (size_t j = 0;  j < m_VolS.GetCols();  ++j) {
                istr >> m_VolS(i, j);
            }
        }
    }

private:
    string m_Path;
};


/// BLOB storage based on single unsigned integer key
/// Supports BLOB volumes and different base page size files in the volume
/// to guarantee the best fit.
///
/// Object maintains a matrix of BDB databases.
/// Every row maintains certain database volume or(and) number of records.
/// Every column groups BLOBs of certain size together, so class can choose
/// the best page size to store BLOBs without long chains of overflow pages.
/// <pre>
///                      Page size split:
///  Volume 
///  split:        4K     8K     16K    32K
///              +------+------+------+------+
///  row = 0     | DB   | ...................|  = SUM = N Gbytes
///  row = 1     | DB   | .....              |  = SUM = N GBytes
///
///                .........................
///
///              +------+------+------+------+
///
/// </pre>
///
template<class TBV, class TObjDeMux=CBDB_BlobDeMux, class TL=CFastMutex>
class CBDB_BlobSplitStore
{
public:
    typedef CIdDeMux<TBV>                TIdDeMux;
    typedef TBV                          TBitVector;
    typedef CBDB_BlobStoreDict<TBV>      TDeMuxStore;
    typedef TL                           TLock;
    typedef typename TL::TWriteLockGuard TLockGuard;
    
    /// BDB Database together with the locker
    struct SLockedDb 
    {
        AutoPtr<CBDB_IdBlobFile> db;    ///< database file
        AutoPtr<TLock>           lock;  ///< db lock
    };

    /// Volume split on optimal page size
    struct SVolume 
    {
        vector<SLockedDb>  db_vect;        
    };

    typedef vector<SVolume*>  TVolumeVect;

public:
    /// Construction
    /// The main parameter here is object demultiplexer for splitting
    /// incoming LOBs into volumes and slices
    ///
    CBDB_BlobSplitStore(TObjDeMux* de_mux);
    ~CBDB_BlobSplitStore();

    /// Open storage (reads storage dictionary into memory)
    void Open(const string&             storage_name, 
              CBDB_RawFile::EOpenMode   open_mode,
              CBDB_RawFile::EDBType     db_type=CBDB_RawFile::eBtree);

    /// Save storage dictionary (demux disposition).
    /// If you modified storage (like added new BLOBs to the storage)
    /// you MUST call save; otherwise some disposition information is lost.
    ///
    void Save(CBDB_Transaction* trans=0);


    void SetVolumeCacheSize(unsigned int cache_size) 
        { m_VolumeCacheSize = cache_size; }
    /// Associate with the environment. Should be called before opening.
    void SetEnv(CBDB_Env& env) { m_Env = &env; }

    CBDB_Env* GetEnv(void) const { return m_Env; }


    // ---------------------------------------------------------------
    // Data manipulation interface
    // ---------------------------------------------------------------

    /// Insert BLOB into the storage.
    ///
    /// This method does NOT check if this object is already storead
    /// somewhere. Method can create duplicates.
    ///
    /// @param id      insertion key
    /// @param data    buffer pointer
    /// @param size    LOB data size in bytes
    /// @param trans   BDB transaction
    ///
    EBDB_ErrCode Insert(unsigned  id, 
                        const void* data, size_t size,
                        CBDB_Transaction* trans = 0);

    /// Update or insert BLOB
    EBDB_ErrCode UpdateInsert(unsigned id, 
                              const void* data, size_t size,
                              CBDB_Transaction* trans = 0);

    /// Read BLOB into vector. 
    /// If BLOB does not fit, method resizes the vector to accomodate.
    /// 
    EBDB_ErrCode ReadRealloc(unsigned id, 
	                         CBDB_RawFile::TBuffer& buffer,
                             CBDB_Transaction* trans = 0);


    /// Fetch LOB record directly into the provided '*buf'.
    /// If size of the LOB is greater than 'buf_size', then
    /// if reallocation is allowed -- '*buf' will be reallocated
    /// to fit the LOB size; otherwise, a exception will be thrown.
    ///
    EBDB_ErrCode Fetch(unsigned     id, 
                       void**       buf, 
                       size_t       buf_size, 
                       CBDB_RawFile::EReallocMode allow_realloc,
                       CBDB_Transaction* trans = 0);

    /// Create stream oriented reader
    /// @returns NULL if BLOB not found
    ///
    /// This method does NOT block the specified ID from concurrent access
    /// for the life of IReader. The nature of BDB IReader is that each Read
    /// maps into BDB get, so somebody can delete the BLOB between IReader calls.
    /// This potential race should be taken into account in MT concurrent 
    /// application.
    ///
    /// Caller is responsible for deletion.
    ///
    IReader* CreateReader(unsigned          id,
                          CBDB_Transaction* trans = 0);


    /// Get size of the BLOB
    ///
    /// @note Price of this operation is almost the same as getting
    /// the actual BLOB. It is often better just to fetch BLOB speculatively, 
    /// hoping it fits in the buffer and resizing the buffer on exception.
    ///
    EBDB_ErrCode BlobSize(unsigned   id, 
                          size_t*    blob_size,
                          CBDB_Transaction* trans = 0);

    /// Delete BLOB
    EBDB_ErrCode Delete(unsigned id, 
	                    CBDB_RawFile::EIgnoreError on_error = 
						                        CBDB_RawFile::eThrowOnError,
                        CBDB_Transaction* trans = 0);

    /// Reclaim unused memory 
    void FreeUnusedMem();
protected:
    /// Close volumes without saving or doing anything with id demux
    void CloseVolumes();

    void LoadIdDeMux(TIdDeMux& de_mux, TDeMuxStore& dict_file);

    /// Store id demux (projection vectors) into the database file
    void SaveIdDeMux(const TIdDeMux&    de_mux, 
                     TDeMuxStore&       dict_file,
                     CBDB_Transaction*  trans);

    /// Select preferred page size for the specified slice
    unsigned GetPageSize(unsigned splice) const;

    /// Open split storage dictionary
    void OpenDict();

    /// Make BDB file name based on volume and page size split
    string MakeDbFileName(unsigned vol, 
                          unsigned slice);

    /// Get database pair (method opens and mounts database if necessary)
    SLockedDb& GetDb(unsigned vol, unsigned slice);

    /// Init database mutex lock (mathod is protected against double init)
    void InitDbMutex(SLockedDb* ldb); 

protected:
    vector<unsigned>        m_PageSizes;
    unsigned                m_VolumeCacheSize;
    CBDB_Env*               m_Env;
    auto_ptr<TDeMuxStore>   m_DictFile;  ///< Split dictionary(id demux file)

    auto_ptr<TIdDeMux>      m_IdDeMux;   ///< Id to coordinates mapper
    mutable CRWLock         m_IdDeMuxLock;

    auto_ptr<TObjDeMux>     m_ObjDeMux;  ///< Obj to coordinates mapper
    TLock                   m_ObjDeMuxLock;

    TVolumeVect             m_Volumes;     ///< Volumes
    mutable TLock           m_VolumesLock; ///< Volumes locker

    string                  m_StorageName;
    CBDB_RawFile::EOpenMode m_OpenMode;
    CBDB_RawFile::EDBType   m_DB_Type;
};

/* @} */


/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


template<class TBV, class TObjDeMux, class TL>
CBDB_BlobSplitStore<TBV, TObjDeMux, TL>::CBDB_BlobSplitStore(TObjDeMux* de_mux)
 : m_PageSizes(7),
   m_VolumeCacheSize(0),
   m_Env(0),
   m_IdDeMux(new TIdDeMux(2)),
   m_ObjDeMux(de_mux),
   m_DB_Type(CBDB_RawFile::eBtree)
{
    m_PageSizes[0] = 0;         // max blob size =   256
    m_PageSizes[1] = 0;         // max blob size =   512
    m_PageSizes[2] = 8 * 1024;  // max blob size =  2048
    m_PageSizes[3] = 16* 1024;  // max blob size =  4096
    m_PageSizes[4] = 32* 1024;  // max blob size =  8192
    m_PageSizes[5] = 64* 1024;  // max blob size = 16384
    m_PageSizes[6] = 64* 1024;  // max blob size = 32768
}

template<class TBV, class TObjDeMux, class TL>
CBDB_BlobSplitStore<TBV, TObjDeMux, TL>::~CBDB_BlobSplitStore()
{
    try {
        CloseVolumes();
    }
    catch (std::exception& e) {
        LOG_POST(Error
                 << "CBDB_BlobSplitStore<>::~CBDB_BlobSplitStore(): "
                 "error in CloseVolumes(): " << e.what());
    }

    try {
        if (m_OpenMode == CBDB_RawFile::eReadWriteCreate) {
            Save();
        }
    }
    catch (std::exception& e) {
        LOG_POST(Error
                 << "CBDB_BlobSplitStore<>::~CBDB_BlobSplitStore(): "
                 "error in Save(): " << e.what());
    }
}

template<class TBV, class TObjDeMux, class TL>
void CBDB_BlobSplitStore<TBV, TObjDeMux, TL>::CloseVolumes()
{

	for (size_t i = 0; i < m_Volumes.size(); ++i) {
		SVolume* v = m_Volumes[i];
		delete v;
	}
}

template<class TBV, class TObjDeMux, class TL>
EBDB_ErrCode 
CBDB_BlobSplitStore<TBV, TObjDeMux, TL>::Insert(unsigned int     id, 
                                               const void*       data, 
                                               size_t            size,
                                               CBDB_Transaction* trans)
{
    unsigned coord[2];
    {{
        TLockGuard lg(m_ObjDeMuxLock);
        m_ObjDeMux->GetCoordinates(size, coord);
    }}

    {{
        CWriteLockGuard lg(m_IdDeMuxLock);
        m_IdDeMux->SetCoordinatesFast(id, coord);
    }}

    SLockedDb& dbp = this->GetDb(coord[0], coord[1]);
    {{
        TLockGuard lg(*(dbp.lock));
        if (trans) {
            dbp.db->SetTransaction(trans);
        }
        dbp.db->id = id;
        return dbp.db->Insert(data, size);
    }}
}

template<class TBV, class TObjDeMux, class TL>
EBDB_ErrCode 
CBDB_BlobSplitStore<TBV, TObjDeMux, TL>::UpdateInsert(unsigned int     id, 
                                                     const void*       data, 
                                                     size_t            size,
                                                     CBDB_Transaction* trans)
{
    unsigned coord[2];
    bool found;
    {{
        CReadLockGuard lg(m_IdDeMuxLock);
        found = m_IdDeMux->GetCoordinatesFast(id, coord);
    }}
    if (!found) {
        return Insert(id, data, size, trans);
    }
    SLockedDb& dbp = this->GetDb(coord[0], coord[1]);
    {{
        TLockGuard lg(*(dbp.lock));
        if (trans) {
            dbp.db->SetTransaction(trans);
        }
        dbp.db->id = id;
        return dbp.db->UpdateInsert(data, size);
    }}
}

template<class TBV, class TObjDeMux, class TL>
EBDB_ErrCode 
CBDB_BlobSplitStore<TBV, TObjDeMux, TL>::Delete(unsigned          id, 
                                      CBDB_RawFile::EIgnoreError  on_error,
                                               CBDB_Transaction*  trans)
{
    unsigned coord[2];
    bool found;
    {{
        CReadLockGuard lg(m_IdDeMuxLock);
        found = m_IdDeMux->GetCoordinatesFast(id, coord);
    }}
    if (!found) {
        return eBDB_NotFound;
    }
    // clear coordinate mapping
    {{
        CWriteLockGuard lg(m_IdDeMuxLock);
        m_IdDeMux->SetCoordinatesFast(id, coord, false);
    }}

    SLockedDb& dbp = this->GetDb(coord[0], coord[1]);
    {{
        TLockGuard lg(*(dbp.lock));
        if (trans) {
            dbp.db->SetTransaction(trans);
        }
        dbp.db->id = id;
        return dbp.db->Delete(on_error);
    }}
}

template<class TBV, class TObjDeMux, class TL>
IReader* 
CBDB_BlobSplitStore<TBV, TObjDeMux, TL>::CreateReader(unsigned          id,
                                                      CBDB_Transaction* trans)
{
    unsigned coord[2];
    bool found;
    {{
        CReadLockGuard lg(m_IdDeMuxLock);
        found = m_IdDeMux->GetCoordinatesFast(id, coord);
    }}
    if (!found) {
        return 0;
    }
    SLockedDb& dbp = this->GetDb(coord[0], coord[1]);
    {{
        TLockGuard lg(*(dbp.lock));
        if (trans) {
            dbp.db->SetTransaction(trans);
        }
        dbp.db->id = id;
        if (dbp.db->Fetch() != eBDB_Ok) {
            return 0;
        }
        return dbp.db->CreateReader();
    }}
}

template<class TBV, class TObjDeMux, class TL>
void CBDB_BlobSplitStore<TBV, TObjDeMux, TL>::FreeUnusedMem()
{
   CWriteLockGuard lg(m_IdDeMuxLock);
   m_IdDeMux->FreeUnusedMem();
}


template<class TBV, class TObjDeMux, class TL>
EBDB_ErrCode 
CBDB_BlobSplitStore<TBV, TObjDeMux, TL>::Fetch(unsigned     id, 
                                              void**       buf, 
                                              size_t       buf_size, 
                               CBDB_RawFile::EReallocMode  allow_realloc,
                               CBDB_Transaction*           trans)
{
    unsigned coord[2];
    bool found;
    {{
        CReadLockGuard lg(m_IdDeMuxLock);
        found = m_IdDeMux->GetCoordinatesFast(id, coord);
    }}
    if (!found) {
        return eBDB_NotFound;
    }
    SLockedDb& dbp = this->GetDb(coord[0], coord[1]);
    {{
        TLockGuard lg(*(dbp.lock));
        if (trans) {
            dbp.db->SetTransaction(trans);
        }
        dbp.db->id = id;
        return dbp.db->Fetch(buf, buf_size, allow_realloc);
    }}
}

template<class TBV, class TObjDeMux, class TL>
EBDB_ErrCode 
CBDB_BlobSplitStore<TBV, TObjDeMux, TL>::ReadRealloc(
                                            unsigned               id,
                                            CBDB_RawFile::TBuffer& buffer,
                                            CBDB_Transaction*      trans)
{
    unsigned coord[2];
    bool found;
    {{
        CReadLockGuard lg(m_IdDeMuxLock);
        found = m_IdDeMux->GetCoordinatesFast(id, coord);
    }}
    if (!found) {
        return eBDB_NotFound;
    }
    SLockedDb& dbp = this->GetDb(coord[0], coord[1]);
    {{
        TLockGuard lg(*(dbp.lock));

        if (trans) {
            dbp.db->SetTransaction(trans);
        }
        dbp.db->id = id;
        EBDB_ErrCode e = dbp.db->ReadRealloc(buffer);
        return e;
    }}

}

template<class TBV, class TObjDeMux, class TL>
EBDB_ErrCode 
CBDB_BlobSplitStore<TBV, TObjDeMux, TL>::BlobSize(unsigned   id, 
                                                 size_t*    blob_size,
                                                 CBDB_Transaction*  trans)
{
    unsigned coord[2];
    bool found;
    {{
        CReadLockGuard lg(m_IdDeMuxLock);
        found = m_IdDeMux->GetCoordinatesFast(id, coord);
    }}
    if (!found) {
        return eBDB_NotFound;
    }
    SLockedDb& dbp = this->GetDb(coord[0], coord[1]);
    {{
        TLockGuard lg(*(dbp.lock));
        if (trans) {
            dbp.db.SetTransaction(trans);
        }
        dbp.db->id = id;
        EBDB_ErrCode e = dbp.db->Fetch();
        if (e != eBDB_Ok) {
            return e;
        }
        *blob_size = dbp.db->LobSize();
        return e;
    }}
}


template<class TBV, class TObjDeMux, class TL>
void 
CBDB_BlobSplitStore<TBV, TObjDeMux, TL>::Open(const string&     storage_name,
                                          CBDB_RawFile::EOpenMode  open_mode,
                                          CBDB_RawFile::EDBType      db_type)
{
    CloseVolumes();
    m_StorageName = storage_name;
    m_OpenMode    = open_mode;
    m_DB_Type     = db_type;

    OpenDict();
    LoadIdDeMux(*m_IdDeMux, *m_DictFile);
}

template<class TBV, class TObjDeMux, class TL>
void 
CBDB_BlobSplitStore<TBV, TObjDeMux, TL>::OpenDict()
{
    m_DictFile.reset(new TDeMuxStore);
    if (m_Env) {
        m_DictFile->SetEnv(*m_Env);
    }
    string dict_fname(m_StorageName);
    dict_fname.append(".splitd");

    m_DictFile->Open(dict_fname.c_str(), m_OpenMode);

    m_IdDeMux.reset(new TIdDeMux(2));
}

template<class TBV, class TObjDeMux, class TL>
void 
CBDB_BlobSplitStore<TBV, TObjDeMux, TL>::LoadIdDeMux(TIdDeMux&      de_mux, 
                                                   TDeMuxStore&   dict_file)
{
    CBDB_FileCursor cur(dict_file);
    cur.SetCondition(CBDB_FileCursor::eGE);
    cur.From << 0;

    typename TDeMuxStore::TBuffer& buf = dict_file.GetBuffer();
    EBDB_ErrCode err;
    while (true) {
        err = dict_file.FetchToBuffer(cur);
        if (err != eBDB_Ok) {
            break;
        }
        unsigned dim = dict_file.dim;
        unsigned dim_idx = dict_file.dim_idx;

        auto_ptr<TBitVector>  bv(new TBitVector(bm::BM_GAP));
        dict_file.Deserialize(bv.get(), &buf[0]);

        de_mux.SetProjection(dim, dim_idx, bv.release());
        
    } // while
}

template<class TBV, class TObjDeMux, class TL>
void 
CBDB_BlobSplitStore<TBV, TObjDeMux, TL>::Save(CBDB_Transaction* trans)
{
    this->SaveIdDeMux(*m_IdDeMux, *m_DictFile, trans);
}


template<class TBV, class TObjDeMux, class TL>
void 
CBDB_BlobSplitStore<TBV, TObjDeMux, TL>::SaveIdDeMux(const TIdDeMux& de_mux, 
                                                 TDeMuxStore&      dict_file,
                                                 CBDB_Transaction* trans)
{
    if (trans) {
        dict_file.SetTransaction(trans);
    }
    size_t N = de_mux.GetN();
    for (size_t i = 0; i < N; ++i) {
        const typename TIdDeMux::TDimVector& dv = de_mux.GetDimVector(i);

        for (size_t j = 0; j < dv.size(); ++j) {
            dict_file.dim = i; 
            dict_file.dim_idx = j;

            const TBitVector* bv = dv[j].get();
            if (!bv) {
                dict_file.Delete(CBDB_RawFile::eIgnoreError);
            } else {
                dict_file.WriteVector(*bv, TDeMuxStore::eCompact);
            }

        } // for j
    } // for i
}

template<class TBV, class TObjDeMux, class TL>
unsigned 
CBDB_BlobSplitStore<TBV, TObjDeMux, TL>::GetPageSize(unsigned splice) const
{
    if (splice < m_PageSizes.size()) 
        return m_PageSizes[splice];
    return 64* 1024;
}

template<class TBV, class TObjDeMux, class TL>
string 
CBDB_BlobSplitStore<TBV, TObjDeMux, TL>::MakeDbFileName(unsigned vol, 
                                                      unsigned slice)
{
    string ext;
    switch (m_DB_Type) 
    {
    case CBDB_RawFile::eBtree:
        ext = ".db";
        break;
    case CBDB_RawFile::eHash:
        ext = ".hdb";
        break;
    default:
        _ASSERT(0);
    } // switch
    return m_StorageName + "_" + 
           NStr::UIntToString(vol) + "_" + NStr::UIntToString(slice) + ext;
}

template<class TBV, class TObjDeMux, class TL>
void CBDB_BlobSplitStore<TBV, TObjDeMux, TL>::InitDbMutex(SLockedDb* ldb)
{
    if (ldb->lock.get() == 0) {
        TLockGuard lg(m_VolumesLock);
        if (ldb->lock.get() == 0) {
            ldb->lock.reset(new TLock);
        }        
    }
}


template<class TBV, class TObjDeMux, class TL>
typename CBDB_BlobSplitStore<TBV, TObjDeMux, TL>::SLockedDb& 
CBDB_BlobSplitStore<TBV, TObjDeMux, TL>::GetDb(unsigned vol, unsigned slice)
{
    // speculative un-locked check if everything is open already
    // (we don't close or shrink the store in parallel, so it is safe)
    SLockedDb* lp = 0;

    if ((m_Volumes.size() > vol) && 
        ((m_Volumes[vol])->db_vect.size() > slice)
        ) {
        SVolume& volume = *(m_Volumes[vol]);
        lp = &(volume.db_vect[slice]);
        if (lp->db.get() != 0) {
            InitDbMutex(lp);
            return *lp;
        }
    } else {  // lock protected open
        TLockGuard lg(m_VolumesLock);
        while (m_Volumes.size() < (vol+1)) {
            m_Volumes.push_back(new SVolume);
        }

        SVolume& volume = *(m_Volumes[vol]);
        if (volume.db_vect.size() <= slice) {
            volume.db_vect.resize(slice+1);
        }
        lp = &(volume.db_vect[slice]);

        if (lp->lock.get() == 0) {
            lp->lock.reset(new TLock);
        }
    }

    {{
    InitDbMutex(lp);
    TLockGuard lg(*(lp->lock));
    if (lp->db.get() == 0) {
        string fname = this->MakeDbFileName(vol, slice);
        lp->db.reset(new CBDB_IdBlobFile(CBDB_File::eDuplicatesDisable,
                                         m_DB_Type));
        if (m_Env) {
            lp->db->SetEnv(*m_Env);
        } else {
            if (m_VolumeCacheSize) {
                lp->db->SetCacheSize(m_VolumeCacheSize);
            }
        }
        unsigned page_size = GetPageSize(slice);
        if (page_size) {
            lp->db->SetPageSize(page_size);
        }

        /// also twiddle min keys per page
        switch (slice) {
        case 0:
            /// page size = default
            /// blobs <= 256 bytes
            lp->db->SetBtreeMinKeysPerPage(6);
            break;

        case 1:
            /// page size = default
            /// blobs > 256, <= 512 bytes
            lp->db->SetBtreeMinKeysPerPage(3);
            break;

        default:
            /// use default = 2
            break;
        }

        lp->db->Open(fname.c_str(), m_OpenMode);
    }
    }}

    return *lp;
}


END_NCBI_SCOPE


#endif 

