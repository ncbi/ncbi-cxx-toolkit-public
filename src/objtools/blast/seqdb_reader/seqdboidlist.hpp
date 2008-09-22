#ifndef OBJTOOLS_READERS_SEQDB__SEQDBOIDLIST_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDBOIDLIST_HPP

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

/// @file seqdboidlist.hpp
/// The SeqDB oid filtering layer.
/// 
/// Defines classes:
///     CSeqDBOIDList
/// 
/// Implemented for: UNIX, MS-Windows

#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include "seqdbfile.hpp"
#include "seqdbvolset.hpp"
#include "seqdbfilter.hpp"
#include "seqdbgilistset.hpp"
#include "seqdbbitset.hpp"

BEGIN_NCBI_SCOPE

using namespace ncbi::objects;

/// CSeqDBOIDList
/// 
/// This class defines a set of included oids over the entire oid
/// range.  The underlying implementation is a large bit map.  If the
/// database has one volume, which uses an OID mask file, this object
/// will memory map that file and use it directly.  Otherwise, an area
/// of memory will be allocated (one bit per OID), and the relevant
/// bits will be turned on in that space.  This information may come
/// from memory mapped oid lists, or it may come from GI lists, which
/// are converted to OIDs using ISAM indices.  Because of these two
/// modes of operation, care must be taken to insure that the
/// placement of the bits exactly corresponds to the layout of the
/// memory mappable oid mask files.

class CSeqDBOIDList : public CObject {
public:
    /// A large enough type to span all OIDs.
    typedef int TOID;
    
    /// A type which spans possible file offsets.
    typedef CSeqDBAtlas::TIndx TIndx;
    
    /// Constructor.
    /// 
    /// All processing to build the oid mask array is done in the
    /// constructor.  The volumes will be queried for information on
    /// how many and what filter files to apply to each volume, and
    /// these files will be used to build the oid bit array.
    ///
    /// @param atlas
    ///   The CSeqDBAtlas object.
    /// @param volumes
    ///   The set of database volumes.
    /// @param filters
    ///   The filtering to apply to the database volumes.
    /// @param gi_list
    ///   The User GI List (if there is one).
    /// @param neg_list
    ///   The Negative User GI List (if there is one).
    /// @param locked
    ///   The lock holder object for this thread.
    CSeqDBOIDList(CSeqDBAtlas              & atlas,
                  const CSeqDBVolSet       & volumes,
                  CSeqDB_FilterTree        & filters,
                  CRef<CSeqDBGiList>       & gi_list,
                  CRef<CSeqDBNegativeList> & neg_list,
                  CSeqDBLockHold           & locked);
    
    /// Destructor.
    /// 
    /// All resources will be freed (returned to the atlas).  This
    /// class uses the atlas to get the memory it needs, so the space
    /// for the oid bit array is counted toward the memory bound.
    ~CSeqDBOIDList();
    
    /// Find an included oid from the specified point.
    /// 
    /// This call tests whether the specified oid is included in the
    /// map.  If it is, true is returned and the argument is not
    /// modified.  If it is not included, but a subsequent oid is, the
    /// argument is adjusted to the next included oid, and true is
    /// returned.  If no oids exist from here to the end of the array,
    /// false is returned.
    /// 
    /// @param next_oid
    ///   The oid to check, and also the returned oid.
    /// @return
    ///   True if an oid was found.
    bool CheckOrFindOID(TOID & next_oid) const
    {
        size_t bit = next_oid;
        bool found = m_AllBits->CheckOrFindBit(bit);
        
        next_oid = bit;
        _ASSERT(size_t(next_oid) == bit);
        
        return found;
    }
    
    /// Deallocate the memory ranges owned by this object.
    /// 
    /// This object may hold a lease on a file owned by the atlas.  If
    /// so, this method will release that memory.  It should only be
    /// called during destruction, since this class has no facilities
    /// for reacquiring the memory lease.
    void UnLease()
    {
        m_Lease.Clear();
    }
    
private:
    /// Shorthand type to clarify code that iterates over memory.
    typedef const unsigned char TCUC;
    
    /// Shorthand type to clarify code that iterates over memory.
    typedef unsigned char TUC;
    
    /// Check if a bit is set.
    /// 
    /// Returns true if the specified oid is included.
    ///
    /// @param oid
    ///   The oid to check.
    /// @return
    ///   true if the oid is included.
    inline bool x_IsSet(TOID oid) const;
    
    /// Build an oid mask in memory.
    /// 
    /// This method allocates an oid bit array which spans the entire
    /// oid range in use.  It then maps all OID mask files and GI list
    /// files.  It copies the bit data from the oid mask files into
    /// this array, translates all GI lists into OIDs and enables the
    /// associated bits, and sets all bits to 1 for any "fully
    /// included" volumes.  This up-front work is intended to make
    /// access to the data as fast as possible later on.  In some
    /// cases, this is not the most efficient way to do this.  Faster
    /// and more efficient storage methods are possible in cases where
    /// very sparse GI lists are used.  More efficient storage is
    /// possible in cases where small masked databases are mixed with
    /// large, "fully-in" volumes.
    /// 
    /// @param volset
    ///   The set of volumes to build an oid mask for.
    /// @param filters
    ///   The filtering to apply to the database volumes.
    /// @param gi_list
    ///   Gi list object.
    /// @param neg_list
    ///   Negative ID list object.
    /// @param locked
    ///   The lock holder object for this thread.
    void x_Setup(const CSeqDBVolSet       & volset,
                 CSeqDB_FilterTree        & filters,
                 CRef<CSeqDBGiList>       & gi_list,
                 CRef<CSeqDBNegativeList> & neg_list,
                 CSeqDBLockHold           & locked);

    /// Clear all bits in a range.
    /// 
    /// This method turns off all bits in the specified oid range.  It
    /// is used after alias file processing to turn off bit ranges
    /// that are masked by a user specified GI list.
    /// 
    /// @param oid_start
    ///   The volume's starting oid.
    /// @param oid_end
    ///   The volume's ending oid.
    void x_ClearBitRange(int oid_start, int oid_end);
    
    /// Compute the oid mask bitset for a database volume.
    ///
    /// The filter tree will be specialized to this database volume and
    /// the OID mask bitset for this volume will be computed.
    ///
    /// @param ft The filter tree for all volumes.
    /// @param vol The volume entry object for this volume.
    /// @param gis An object that manages the GI lists used here.
    /// @param locked The lock holder object for this thread.
    /// @return An OID bitset object.
    CRef<CSeqDB_BitSet>
    x_ComputeFilters(const CSeqDB_FilterTree & ft,
                     const CSeqDBVolEntry    & vol,
                     CSeqDBGiListSet         & gis,
                     CSeqDBLockHold          & locked);
    
    /// Load the named OID mask file into a bitset object.
    ///
    /// @param fn The filename from which to load the OID mask.
    /// @param vol_start The first OID included in this volume.
    /// @param vol_end The first OID after this volume.
    /// @param locked The lock holder object for this thread.
    /// @return An OID bitset object.
    CRef<CSeqDB_BitSet>
    x_GetOidMask(const CSeqDB_Path & fn,
                 int                 vol_start,
                 int                 vol_end,
                 CSeqDBLockHold    & locked);
    
    /// Load an ID (GI or TI) list file into a bitset object.
    ///
    /// @param ids A set of included GIs or TIs.
    /// @param vol_start The first OID included in this volume.
    /// @param vol_end The first OID after this volume.
    /// @return An OID bitset object.
    CRef<CSeqDB_BitSet>
    x_IdsToBitSet(const CSeqDBGiList & ids, int vol_start, int vol_end);
    
    /// Apply a user GI list to a volume.
    ///
    /// This method applies a user-specified filter to the OID list.
    /// Unlike x_ApplyFilter, which turns on the bits of the filter,
    /// this method turns OFF the disincluded bits.  It is therefore
    /// an AND operation between the user filter and the (already
    /// applied) alias file filters.
    ///
    /// @param gis
    ///   The user gi list to apply to the volumes.
    /// @param locked
    ///   The lock holder object for this thread.
    void x_ApplyUserGiList(CSeqDBGiList   & gis,
                           CSeqDBLockHold & locked);
    
    /// Apply a negative user GI list to a volume.
    ///
    /// This method applies a user-specified filter to the OID list.
    /// It serves the same purpose for negative GI lists that
    /// x_ApplyUserGiList serves for positive GI lists.  The operation
    /// performed here is an AND operation between the the (already
    /// applied) alias file filters and the negation of the user
    /// filter.
    ///
    /// @param neg
    ///   The negative user gi list to apply to the volumes.
    /// @param locked
    ///   The lock holder object for this thread.
    void x_ApplyNegativeList(CSeqDBNegativeList & neg,
                             CSeqDBLockHold     & locked);
    
    /// The memory management layer object.
    CSeqDBAtlas & m_Atlas;

    /// A memory lease which holds the mask file (if only one is used).
    CSeqDBMemLease m_Lease;
    
    /// The total number of OIDs represented in the bit set.
    int m_NumOIDs;
    
    /// An OID bit set covering all volumes.
    CRef<CSeqDB_BitSet> m_AllBits;
};

inline bool
CSeqDBOIDList::x_IsSet(TOID oid) const
{
    _ASSERT(m_AllBits.NotEmpty());
    return (oid < m_NumOIDs) && m_AllBits->GetBit(oid);
}

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBOIDLIST_HPP

