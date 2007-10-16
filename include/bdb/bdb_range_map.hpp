#ifndef BDB__RANGE_MAP_HPP
#define BDB__RANGE_MAP_HPP

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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description: BDB range map storage
 *
 */

/// @file bdb_range_map.hpp
/// BDB range map storage

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiexpt.hpp>

#include <bdb/bdb_blob.hpp>
#include <bdb/bdb_cursor.hpp>
#include <bdb/error_codes.hpp>

#include <util/bitset/ncbi_bitset.hpp>
#include <util/bitset/ncbi_bitset_util.hpp>


BEGIN_NCBI_SCOPE

/// Range map class, stores unsigned integer range mappings
/// like  10 to 15 maps into 100 to 115
///
/// This class also maitains free list of mappings 
/// (inside the mapping regions)
/// The free list can be used externally to do id assignments, so ids fall into some
/// predefined regions of the mapped list
///
/// Range 0-0 is RESERVED for the free list storage.
///
/// <pre>
/// Example:
///    10-15 --> 100-115
///    20-30 --> 1020-1030
///    Free list: { 11,12,23 }
/// </pre>
///
/// Typical use case: add a range of ids and take them out excluding 
/// out of free list. When the free list is empty - add a new range.
///
template<class TBV>
class CBDB_RangeMap : public CBDB_BLobFile
{
public: 
    typedef  TBV  TBitVector;

public:
    CBDB_FieldUint4  FromId;  ///< From id
    CBDB_FieldUint4  ToId;    ///< To id
public:
    CBDB_RangeMap();
    ~CBDB_RangeMap();

    /// Load free list (Storage should be open first)
    void LoadFreeList();

    /// Save free list
    EBDB_ErrCode Save();

    /// Add remapping range. The range should NOT intersect with any existing range
    /// Optionally new range can be added to the free id list.
    EBDB_ErrCode AddRange(unsigned from, unsigned to, unsigned dest, 
                          bool add_to_free_list);

    /// Get list of free ids.
    const TBitVector& GetFreeList() const { return *m_FreeList; }
    TBitVector& GetFreeList() { return *m_FreeList; }

    /// Remap one single id
    ///
    /// @return Remapped id or 0 if remapping is impossible
    ///
    unsigned Remap(unsigned id, bool ignore_free_list=false);
    
    /// Remap a bitvector into another bitvector
    ///
    /// @param bv_src
    ///     Source bit vector
    /// @param bv_dst
    ///     Destination set
    /// @param bv_remapped
    ///     Set of successfully remapped ids
    ///
    void Remap(const TBitVector& bv_src, 
               TBitVector*       bv_dst, 
               TBitVector*       bv_remapped = 0,
               bool              ignore_free_list=false);
protected:
    TBitVector*   m_FreeList;
};



/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////

template<class TBV>
CBDB_RangeMap<TBV>::CBDB_RangeMap()
: m_FreeList(new TBitVector(bm::BM_GAP))
{
    this->BindKey("FromId", &FromId);
    this->BindKey("ToId",   &ToId);
}

template<class TBV>
CBDB_RangeMap<TBV>::~CBDB_RangeMap()
{
    try {
        Save();
    } 
    catch (CException& ex)
    {
        ERR_POST_XX(Bdb_RangeMap, 1, "Exception in ~CBDB_RangeMap()" << ex.GetMsg());
    }
    catch (exception& ex)
    {
        ERR_POST_XX(Bdb_RangeMap, 2, "Exception in ~CBDB_RangeMap()" << ex.what());
    }

    delete m_FreeList;
}

template<class TBV>
void CBDB_RangeMap<TBV>::LoadFreeList()
{
    _ASSERT(m_FreeList);

    TBuffer buf;
    this->FromId = 0;
    this->ToId = 0;
    EBDB_ErrCode err = CBDB_BLobFile::ReadRealloc(buf);
    if (err == eBDB_Ok) {
        bm::deserialize(*m_FreeList, &(buf[0]));
    } else {
        m_FreeList->clear(true);
    }
}

template<class TBV>
EBDB_ErrCode CBDB_RangeMap<TBV>::Save()
{
    _ASSERT(m_FreeList);

    TBuffer buf;
    BV_Serialize(*m_FreeList, buf, 0, true /*optimize*/);

    return UpdateInsert(buf);
}

template<class TBV>
EBDB_ErrCode CBDB_RangeMap<TBV>::AddRange(unsigned  from, 
                                          unsigned  to, 
                                          unsigned  dest, 
                                          bool      add_to_free_list)
{
    if (from == 0 && to == 0) {
        BDB_THROW(eInvalidValue, "0-0 range is reserved.");
    }
    if (from > to) {
        BDB_THROW(eInvalidValue, "Incorrect range values.");
    }

    {{
    CBDB_FileCursor cur(*this);

    // check range overlaps
    //
    cur.SetCondition(CBDB_FileCursor::eLE);
    cur.From << from;
    EBDB_ErrCode ret = cur.FetchFirst();
    if (ret == eBDB_Ok) {
        unsigned from_db = this->FromId;
        unsigned to_db = this->ToId;

        //  from <= from1 <= to   ?
        //
        if (from >= from_db && from <= to_db) {
            BDB_THROW(eInvalidValue, 
                      "Range overlaps with the previous range.");
        }
    }

    cur.SetCondition(CBDB_FileCursor::eGE);
    cur.From << to;
    ret = cur.FetchFirst();
    if (ret == eBDB_Ok) {
        unsigned from_db = this->FromId;
        unsigned to_db = this->ToId;

        //  from <= to1 <= to   ?
        //
        if (to >= from_db && to <= to_db) {
            BDB_THROW(eInvalidValue, 
                      "Range overlaps with the next range.");
        }
    }
    }}

    this->FromId = from;
    this->ToId = to;

    EBDB_ErrCode ret = Insert(&dest, sizeof(dest));

    if (ret == eBDB_Ok) {
        if (add_to_free_list) {
            m_FreeList->set_range(from, to, true);
        }
    }
    return ret;
}

template<class TBV>
unsigned CBDB_RangeMap<TBV>::Remap(unsigned id, bool ignore_free_list)
{
    if (!ignore_free_list) {
        if (m_FreeList->test(id)) {
            return 0;
        }
    }
    CBDB_FileCursor cur(*this);

    // check range overlaps
    //
    cur.SetCondition(CBDB_FileCursor::eLE);
    cur.From << id;
    unsigned dst;
    void* ptr = &dst;
    EBDB_ErrCode ret = cur.FetchFirst(&ptr, sizeof(dst), eReallocForbidden);
    if (ret == eBDB_Ok) {
        unsigned from_id = this->FromId;
        unsigned to_id = this->ToId;
        if (from_id <= id && id <= to_id) {
            return dst + (id - from_id);
        }
    }
    return 0;
}

template<class TBV>
void CBDB_RangeMap<TBV>::Remap(const TBitVector& bv_src, 
                               TBitVector*       bv_dst, 
                               TBitVector*       bv_remapped,
                               bool              ignore_free_list)
{
    CBDB_FileCursor cur(*this);
    unsigned from_id, to_id, dst;
    from_id = to_id = dst = 0;

    typename TBV::enumerator en(bv_src.first());
    for ( ;en.valid(); ++en) {
        unsigned id = *en;
        if (!ignore_free_list) {
            if (m_FreeList->test(id)) {
                continue;
            }
        }
        
        if (from_id <= id && id <= to_id) {
            bv_dst->set(dst + (id - from_id));
            if (bv_remapped) {
                bv_remapped->set(id);
            }
        } else {
            cur.SetCondition(CBDB_FileCursor::eLE);
            cur.From << id;
            void* ptr = &dst;
            EBDB_ErrCode ret = 
                cur.FetchFirst(&ptr, sizeof(dst), eReallocForbidden);
            if (ret == eBDB_Ok) {
                from_id = this->FromId;
                to_id = this->ToId;
                if (from_id <= id && id <= to_id) {
                    bv_dst->set(dst + (id - from_id));
                    if (bv_remapped) {
                        bv_remapped->set(id);
                    }
                }
            }
        }
    } // for
}

END_NCBI_SCOPE

#endif  // BDB__RANGE_MAP_HPP
