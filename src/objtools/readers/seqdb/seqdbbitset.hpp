#ifndef OBJTOOLS_READERS_SEQDB__SEQDBBITSET_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDBBITSET_HPP

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
 * Author:  Kevin Bealer
 *
 */

/// @file seqdbbitset.hpp
/// Implementation for the CSeqDB_BitSet class, a bit vector.
/// 
/// Defines classes:
///     CSeqDB_BitSet
/// 
/// Implemented for: UNIX, MS-Windows

#include "seqdbvol.hpp"

BEGIN_NCBI_SCOPE

/// Import definitions from the ncbi::objects namespace.
USING_SCOPE(objects);

/// Bit set class.
class CSeqDB_BitSet : public CObject {
    // Some of this code might be more efficient if a larger word size
    // were used.  However, I think the difference would probably be
    // tiny, and reading of OID mask file data on such platforms would
    // then require special byte-order-aware algorithms.  Working at
    // the byte level completely sidesteps all such issues and results
    // in simpler, clearer code.
    
    /// Word size for vector elements.
    typedef unsigned char TByte;
    
    /// Some useful constants related to word size.
    enum {
        /// Number of bits per stored word.
        eWordBits = 8,
        
        /// Shift to convert from bit index to vector index.
        eWordShift = 3,
        
        /// Mask to compute bit index within word.
        eWordMask = eWordBits-1
    };
    
public:
    /// Special edge cases (all-set and all-clear).
    enum ESpecialCase {
        eNone,    ///< Normal OID list.
        eAllSet,  ///< All OIDs are set.
        eAllClear ///< All OIDs are clear.
    };
    
    /// Default constructor for zero-length empty bit array.
    CSeqDB_BitSet()
        : m_Start  (0),
          m_End    (0),
          m_Special(eNone)
    {
        _ASSERT(eWordShift); // must be 32 or 64
        _ASSERT(TByte(0) < TByte(-1)); // must be unsigned
    }
    
    /// Constructor for bit array with start/end range.
    ///
    /// This constructs a bit array with the given start and end
    /// points.  If the `sp' parameter is set to `eNone', a normal
    /// bit array is constructed, with all bits set to `false'.  If sp
    /// is `eAllSet' or `eAllClear', the array is not constructed, but
    /// rather an object that acts as though the given range of OIDs
    /// is all `true' or all `false' respectively.  These are termed
    /// `special case' bit sets.  Special cases tend to be much more
    /// efficient in terms of memory usage and in the efficiency of
    /// boolean operations.
    ///
    /// @param start Starting OID for this bit set.
    /// @param end Ending OID for this bit set.
    /// @param sp Special case (homogeneous value) arrays.
    CSeqDB_BitSet(size_t start, size_t end, ESpecialCase sp = eNone)
        : m_Start  (start),
          m_End    (end),
          m_Special(sp)
    {
        _ASSERT(eWordShift); // must be 32 or 64
        _ASSERT(TByte(0) < TByte(-1)); // must be unsigned
        
        if (sp == eNone) {
            x_Alloc(end-start);
        }
    }

    void foo();
    
    /// Constructor.
    ///
    /// This constructs a normal (eNone) bit array with the given
    /// start and end points, and populates it from the byte data
    /// in the memory found between addresses p1 and p2.  This method
    /// is meant for reading data from OID mask files and follows the
    /// format of those files, but the start pointer should be to the
    /// start of the bit map data within the file, not to the start of
    /// the file (the file contains a header that should be skipped).
    ///
    /// @param start The first OID represented by this data.
    /// @param end The OID after the last OID represented by this data.
    /// @param p1 A pointer to the beginning of the byte data.
    /// @param p2 A pointer to past the end of the byte data.
    CSeqDB_BitSet(size_t        start,
                  size_t        end,
                  const TByte * p1,
                  const TByte * p2);
    
    /// Set the specified bit (to true).
    /// @param index The index of the bit to set.
    void SetBit(size_t index);
    
    /// Clear the specified bit (to false).
    /// @param index The index of the bit to clear.
    void ClearBit(size_t index);
    
    /// Store a value at the given index.
    /// @param index The index of the bit to clear.
    /// @param value The value to store in this bit.
    void AssignBit(size_t i, bool value);
    
    /// Store the provided value in a range of bits.
    /// @param start The index of the first bit to assign.
    /// @param end The index after the last bit to assign.
    /// @param value The value to store in this range.
    void AssignBitRange(size_t start, size_t end, bool value);
    
    /// Get the value of the specified bit.
    /// @param index The index of the bit to get.
    bool GetBit(size_t index) const;
    
    /// Check if a bit is true or find the next bit that is.
    ///
    /// If the index points to a `false' value, the index is increased
    /// until a `true' value is found (true is returned) or until the
    /// index exceeds the OID range (false is returned).  If the index
    /// initially points to a `true' bit, the index will not change.
    ///
    /// @param index The start index, and the returned index [in|out].
    /// @return true if the index points to a true value.
    bool CheckOrFindBit(size_t & index) const;
    
    /// Swap two bitsets.
    ///
    /// All fields of this bitset are swapped with those of `other'.
    ///
    /// @param other A bitset to swap values with.
    void Swap(CSeqDB_BitSet & other);
    
    /// This bitset is assigned to the union of it and another.
    ///
    /// Each bit in this bitset will be `true' if it was true in
    /// either this bitset or `other'.  The `consume' flag can be
    /// specified as true if the value of the `other' bitset will not
    /// be used after this operation.  Specifying `true' for consume
    /// may change the data in the other bitset but can sometimes use
    /// a more efficient algorithm.
    ///
    /// @param other The bitset to union with this bitset.
    /// @param consume Specify true if the other bitset is expendable.
    void UnionWith(CSeqDB_BitSet & other, bool consume);
    
    /// This bitset is assigned to the intersection of it and another.
    ///
    /// Each bit in this bitset will be `true' if it was true in both
    /// this bitset and `other'.  The `consume' flag can be specified
    /// as true if the value of the `other' bitset will not be used
    /// after this operation.  Specifying `true' for consume may
    /// change the data in the other bitset but can sometimes use a
    /// more efficient algorithm.
    ///
    /// @param other The bitset to intersect with this bitset.
    /// @param consume Specify true if the other bitset is expendable.
    void IntersectWith(CSeqDB_BitSet & other, bool consume);
    
    /// If this is a special case bitset, convert it to a normal one.
    ///
    /// Operations on normal (`eNone') bitsets can be more expensive
    /// in terms of memory and CPU time than on special case (eAllSet
    /// and eAllClear) bitsets, but normal bitsets support operations
    /// such as SetBit() and ClearBit() that special bitsets don't.
    ///
    /// This method checks if this bitset is a special case, and
    /// converts it to a normal (`eNone') bitset if so.
    void Normalize();
    
private:
    /// Set all bits that are true in `src'.
    /// @param src The bitset to read from.
    void x_CopyBits(const CSeqDB_BitSet & src);
    
    /// Set all bits in the given range that are true in `src'.
    /// @param src The bitset to read from.
    /// @param begin The start of the index range to read.
    /// @param end The index past the end of the index range.
    void x_CopyBits(const CSeqDB_BitSet & src, size_t start, size_t end);
    
    /// Set this bitset to the value of the provided one.
    ///
    /// This is like a normal "copy assignment" operation, except that
    /// if `consume' is specified as true, a more efficient algorithm
    /// may be used.  (Implementation: if `consume' is true, and the
    /// source is a normal (eNone) bitset, this is a `swap'.)
    ///
    /// @param other The bitset to copy.
    /// @param consume Specify true if the other bitset is expendable.
    void x_Copy(CSeqDB_BitSet & other, bool consume);
    
    /// Replace `special' with normal bitsets, adjust the index range.
    /// 
    /// If this bitset is special, it becomes a normal bitset.  If the
    /// start or end point is outside of the current one, the bitset
    /// expands (but does not contract).  All bits that are `true' in
    /// the initial bitset will be true in the resulting bitset.
    ///
    /// @param start Move start point down (but not up) to here.
    /// @param end Move end point up (but not down) to here.
    void x_Normalize(size_t start, size_t end);
    
    /// Allocate memory for the bit data.
    /// @param bits Allocate enough storage to hold this many bits.
    void x_Alloc(size_t bits)
    {
        m_Bits.resize((bits + eWordBits - 1) >> eWordShift);
    }
    
    /// Prevent copy construction.
    CSeqDB_BitSet(const CSeqDB_BitSet &);
    
    /// Prevent copy assignment.
    CSeqDB_BitSet & operator=(const CSeqDB_BitSet &);
    
    /// Number of bits stored here.
    size_t m_Start;
    
    /// Number of bits stored here.
    size_t m_End;
    
    /// Special edge cases.
    ESpecialCase m_Special;
    
    /// Representation of bit data.
    vector<TByte> m_Bits;
};

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBBITSET_HPP

