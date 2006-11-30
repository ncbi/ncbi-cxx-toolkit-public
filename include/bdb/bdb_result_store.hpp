#ifndef RESULTSET_STORE_HPP_
#define RESULTSET_STORE_HPP_

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
 * File Description: Multi-volume query result set store
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbimisc.hpp>
#include <util/bitset/ncbi_bitset.hpp>

#include <bdb/bdb_blob.hpp>
#include <bdb/bdb_cursor.hpp>

#include <vector>


BEGIN_NCBI_SCOPE

/// Query result store
/// 
/// Each query result consists of multiple subsets
/// Subset can be presented as a bitvector or sorted vector of row ids
///
/// Each subset is defined by subset id (lower ids means higher rank)
/// Volume id - volume address
/// format    - result set format
///
class NCBI_BDB_EXPORT CBDB_ResultStoreBase : public CBDB_BLobFile
{
public:
    enum ESubsetFormat {
        eUndefined= 0,
        eBitSet   = 1,
        eIdVector = 2,
        eSingleId = 3
    };

    typedef  vector<unsigned char>   TBuffer;
    typedef  TBuffer::value_type     TBufferValue;
    typedef  vector<unsigned>        TIdVector;

    /// Structure encoding current position in the result set
    ///
    struct TResultSetPosition
    {
        unsigned          subset;
        unsigned          volume;
        ESubsetFormat     format;
        unsigned          set_position;
        
        TResultSetPosition() 
        {
            subset = volume = set_position = 0; 
            format = eUndefined;
        }
    };

public:
    CBDB_FieldUint4        subset_id;  ///< Subset id number (defines priority)
    CBDB_FieldUint4        volume_id;  ///< volume number
    CBDB_FieldUint2        format;     ///< Result format
public:
    CBDB_ResultStoreBase();

    /// Get access to fetch buffer
    TBuffer& GetBuffer() { return m_Buffer; }

protected:
    /// temporary de-serialization buffer
    TBuffer                   m_Buffer;
};

/// Main result set storage parameterised by bit-vector implementation
///
///
template<class TBV>
class CBDB_ResultStore : public CBDB_ResultStoreBase
{
public:
    typedef  TBV                   TBitVector;
    typedef  CBDB_ResultStoreBase  TParent;

    /// subset loaded in memory, union fields correspond to
    /// format types
    ///
    union TSubset 
    {
        TBitVector*  bv;
        TIdVector*   idv;
        unsigned     id;
    };

public:
    CBDB_ResultStore() : CBDB_ResultStoreBase() 
    {
        m_STmpBlock = m_TmpBVec.allocate_tempblock();
    }

    ~CBDB_ResultStore()
    {
        if (m_STmpBlock) {
            m_TmpBVec.free_tempblock(m_STmpBlock);
        }        
    }

    EBDB_ErrCode Insert(unsigned          subset,
                        unsigned          volume, 
                        const TBitVector& bv);

    EBDB_ErrCode Insert(unsigned                subset,
                        unsigned                volume, 
                        const vector<unsigned>& id_vect);

    EBDB_ErrCode Insert(unsigned       subset,
                        unsigned       volume, 
                        unsigned       id);

    
    typedef CBDB_ResultStore<TBV>  TResultStore;

    /// Result set traverse iterator 
    ///
    class CResultStoreEnumerator
    {
    public:
        ~CResultStoreEnumerator()
        {
            delete m_Cursor;
            FreeCurentSubset();
        }

        /// Get current result set position
        const typename TResultStore::TResultSetPosition& GetPosition() const
        {
            return m_RS_Pos;
        }

        /// Get current document id
        unsigned GetId() const { return m_CurrentId; }

        /// Returns TRUE if last iteration changed the subset 
        /// (maybe changed  current volume)
        bool IsNewSunset() const { return m_SubsetChanged; }

        /// No more results
        bool IsEof() const { return m_LastFetchRes != eBDB_Ok;}

        /// Go to the next document (method fetches a new subset if needs to)
        void Next()
        {
            switch (m_RS_Pos.format) 
            {
            case TResultStore::eUndefined:
                break
            case TResultStore::eSingleId:
                m_LastFetchRes = m_Cursor->Fetch(&buf);
                if (m_LastFetchRes == eBDB_Ok) {
                    ProcessNewFetch();
                }
                break;
            case TResultStore::eBitSet:
                m_CurrentId = m_RS_Pos.set_position = 
                    m_CurrentSubset.bv->get_next(m_RS_Pos.set_position);
                if (m_CurrentId == 0) {
                    m_LastFetchRes = m_Cursor->Fetch(&buf);
                    if (m_LastFetchRes == eBDB_Ok) {
                        ProcessNewFetch();
                    }
                }
                break;
            case TResultStore::eIdVector:
                if (++m_RS_Pos.set_position >= m_CurrentSubset.idv->size()) {
                    m_RS_Pos.set_position = 0;
                    m_LastFetchRes = m_Cursor->Fetch(&buf);
                    if (m_LastFetchRes == eBDB_Ok) {
                        ProcessNewFetch();
                    }
                }                
                break;
            default:
                _ASSERT(0);
            } // switch
        }

    protected:
        CResultStoreEnumerator(TResultStore& store)
        : m_ResultStore(store),
          m_Cursor(0),
          m_SubsetChanged(false)
        {
            m_CurrentSubset.bv = 0;
        }

        void FreeCurrentSubset()
        {
            switch (m_RS_Pos.format) 
            {
            case TResultStore::eUndefined:
            case TResultStore::eSingleId:
                break;
            case TResultStore::eBitSet:
                delete m_CurrentSubset.bv;
                break;
            case TResultStore::eIdVector:
                delete m_CurrentSubset.idv;
                break;
            default:
                _ASSERT(0);
            } // switch
            m_CurrentSubset.bv = 0;
        }

        /// Set new enumerator position 
        void SetPosition(const typename TResultStore::TResultSetPosition& pos)
        {
            m_RS_Pos = pos;
            CreateCursor();
        }

        void CreateCursor()
        {
            delete m_Cursor;
            m_Cursor = new CBDB_FileCursor(m_ResultStore);
            m_Cursor->SetCondition(CBDB_FileCursor::eGE);
            m_Cursor->From << m_RS_Pos.subset 
                           << m_RS_Pos.volume
                           << (unsigned)m_RS_Pos.format;

            TResultStore::TBuffer& buf = m_ResultStore.GetBuffer();
            m_LastFetchRes = m_Cursor->FetchFirst(&buf);
            if (m_LastFetchRes == eBDB_Ok) {
                ProcessNewFetch();
            }
        }

    private:
        CResultStoreEnumerator(const CResultStoreEnumerator&);
        CResultStoreEnumerator& operator=(const CResultStoreEnumerator&);

        void ProcessNewFetch()
        {
            FreeCurrentSubset();

            m_RS_Pos.subset = m_ResultStore.subset_id;
            m_RS_Pos.volume = m_ResultStore.volume_id;
            m_RS_Pos.format = m_ResultStore.format;

            m_SubsetChanged = true;

            switch (pos->format) 
            {
            case CBDB_ResultStoreBase::eBitSet:
                {
                TResultStore::TBuffer& buf = m_ResultStore.GetBuffer();
                m_CurrentSubset.bv = new TResultStore::TBitVector();
                bm::deserialize(*(m_CurrentSubset.bv), 
                                &(buf[0]), m_ResultStore.m_STmpBlock);
                _ASSERT(!m_CurrentSubset.bv->empty());
                if (m_RS_Pos.set_position == 0) {
                    m_RS_Pos.set_position = m_CurrentSubset.bv->get_first();
                } else {
                    m_CurrentId = m_RS_Pos.set_position = 
                        m_CurrentSubset.bv->get_next(--m_RS_Pos.set_position);
                }
                }
                break;
            case CBDB_ResultStoreBase::eIdVector:
                {
                TResultStore::TBuffer& buf = m_ResultStore.GetBuffer();
                m_CurrentSubset.idv = new TResultStore::TIdVector();
                size_t idv_size = buf.size() / sizeof(unsigned);
                m_CurrentSubset.idv->resize(idv_size);
                ::memcpy(&(*m_CurrentSubset.idv)[0],
                         &(buf[0]),
                         buf.size());
                m_CurrentId = (*m_CurrentSubset.idv)[m_RS_Pos.set_position];
                }
                break;
            case CBDB_ResultStoreBase::eSingleId:
                _ASSERT(buf.size() == sizeof(m_CurrentSubset.id));
                ::memcpy(&m_CurrentSubset.id, &(buf[0]), sizeof(unsigned));
                m_CurrentId = m_CurrentSubset.id;
                break;
            default:
                _ASSERT(0);
            }
        }

    friend class TResultStore;
    private:
        TResultStore&                              m_ResultStore;
        typename TResultStore::TResultSetPosition  m_RS_Pos;
        CBDB_FileCursor*                           m_Cursor;
        bool                                       m_SubsetChanged;
        EBDB_ErrCode                               m_LastFetchRes;
        typename TResultStore::TSubset             m_CurrentSubset;
        unsigned                                   m_CurrentId;
    };

private:
    CBDB_ResultStore(const CBDB_ResultStore&);
    CBDB_ResultStore& operator=(const CBDB_ResultStore);
protected:
    friend class CResultStoreEnumerator;
protected:
    /// temporary bitset
    TBitVector                m_TmpBVec;
    /// temp block for bitvector serialization
    bm::word_t*               m_STmpBlock;
};


/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


template<class TBV>
EBDB_ErrCode 
CBDB_ResultStore<TBV>::Insert(unsigned          subset,
                              unsigned          volume, 
                              const TBitVector& bv)
{
    this->subset_id = subset;
    this->volume_id = volume;
    this->format = (unsigned) eBitSet;

    if (m_STmpBlock == 0) {
        m_STmpBlock = m_TmpBVec.allocate_tempblock();
    }

    typename TBitVector::statistics st1;
    bv.calc_stat(&st1);

    if (st1.max_serialize_mem > m_Buffer.size()) {
        m_Buffer.resize(st1.max_serialize_mem);
    }
    size_t size = bm::serialize(bv, &m_Buffer[0], m_STmpBlock);
    return CBDB_BLobFile::Insert(&m_Buffer[0], size);
}

template<class TBV>
EBDB_ErrCode CBDB_ResultStore<TBV>::Insert(unsigned      subset,
                                           unsigned      volume, 
                              const vector<unsigned>&    id_vect)
{
    this->subset_id = subset;
    this->volume_id = volume;
    this->format = (unsigned) eIdVector;
    return CBDB_BLobFile::Insert(&id_vect[0], sizeof(id_vect[0]) * id_vect.size());
}


template<class TBV>
EBDB_ErrCode CBDB_ResultStore<TBV>::Insert(unsigned      subset,
                                           unsigned      volume, 
                                           unsigned      id)
{
    this->subset_id = subset;
    this->volume_id = volume;
    this->format = (unsigned) eSingleId;
    return CBDB_BLobFile::Insert((const void*)&id, sizeof(id));
}




END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2006/11/30 14:23:09  kuznets
 * Compilation fixes
 *
 * Revision 1.2  2006/11/29 12:41:39  kuznets
 * Compilation fixes (MSVC)
 *
 * Revision 1.1  2006/11/29 11:42:32  kuznets
 * initial revision
 *
 *
 * ===========================================================================
 */

#endif /* RESULTSET_STORE_HPP_ */

