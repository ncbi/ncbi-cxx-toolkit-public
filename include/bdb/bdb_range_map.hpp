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

#include <bdb/bdb_blob.hpp>
#include <bdb/bdb_cursor.hpp>

#include <util/bitset/ncbi_bitset.hpp>
#include <util/bitset/bmserial.h>


BEGIN_NCBI_SCOPE

/// Range map class, stores unsigned integer range mappings
/// like  10 to 15 maps into 100 to 115
///
/// This class also maitains free list of mappings 
/// (inside the mapping regions)
/// The free list can be used externally to do id assignments, so ids fall into some
/// predefined regions of the mapped list
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
template<class BV>
class CBDB_RangeMap : public CBDB_BLobFile
{
public: 
    typedef  BV  TBitVector;

public:
    CBDB_FieldUint4  FromId;  ///< From id
    CBDB_FieldUint4  ToId;    ///< To id
public:
    CBDB_RangeMap();
    ~CBDB_RangeMap();

    /// Save free list
    Save();

    /// Add remapping range. The range should NOT intersect with any existing range
    /// Optionally new range can be added to the free id list.
    void AddRange(unsigned from, unsigned to, unsigned dest, 
                  bool add_to_free_list);

    /// Get list of free ids.
    const TBitVector& GetFreeList() const;
    TBitVector& GetFreeList();

    /// Remap one single id
    unsigned Remap(unsigned id, bool ignore_free_list=false);
    
    /// Remap a bitvector into another bitvector
    /// Method scans for available ranges, do a remapping (where it can), clears bits in
    /// the src. bit-vector (for successfully remapped ids)
    ///
    void Remap(TBitVector& bv_src, TBitVector& bv_dst, bool ignore_free_list=false);

protected:
    TBitVector*   m_FreeList;
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2006/12/12 21:56:14  kuznets
 * initial revision
 *
 *
 * ===========================================================================
 */

#endif  // BDB__RANGE_MAP_HPP


