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

#include <objtools/readers/seqdb/seqdb.hpp>
#include "seqdbfile.hpp"
#include "seqdbvolset.hpp"

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
    
    /// Constructor
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
    /// @param locked
    ///   The lock holder object for this thread.
    CSeqDBOIDList(CSeqDBAtlas        & atlas,
                  const CSeqDBVolSet & volumes,
                  CSeqDBLockHold     & locked);
    
    /// Destructor
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
        if (x_IsSet(next_oid)) {
            return true;
        }
        return x_FindNext(next_oid);
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
    
    /// Include the specified oid.
    /// 
    /// Set the inclusion bit for the specified oid to true.
    ///
    /// @param oid
    ///   The oid to adjust.
    void x_SetBit(TOID oid);
    
    /// Find the next OID.
    /// 
    /// This method gets the next included oid.  The semantics are
    /// exactly the same as CheckOrFindOID(), but this method is
    /// always only called if the specified oid was not found by
    /// x_IsSet().  This contains optimizations and should be many
    /// times more efficient that calling CheckOrFindOID() in a loop.
    /// 
    /// @param oid
    ///   The oid to check and possibly adjust
    /// @return
    ///   true if an included oid was found.
    bool x_FindNext(TOID & oid) const;
    
    /// Map a pre-built oid mask from a file.
    /// 
    /// This method maps a file specified by filename, which contains
    /// a pre-built OID bit mask.  The atlas is used to get a lease on
    /// the file, which is held by m_Lease.  This method is only
    /// called when there is one OID mask which spans the entire OID
    /// range.
    /// 
    /// @param filename
    ///   The name of the mask file to use.
    /// @param locked
    ///   The lock holder object for this thread.
    void x_Setup(const string   & filename,
                 CSeqDBLockHold & locked);
    
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
    /// @param locked
    ///   The lock holder object for this thread.
    void x_Setup(const CSeqDBVolSet & volset,
                 CSeqDBLockHold     & locked);
    
    /// Copy data from an OID mask into the bit array.
    /// 
    /// This method maps an oid mask file which spans one volume, and
    /// combines the data from that file with the large bit array that
    /// spans the total oid range.  The combination is done as an "OR"
    /// operation.
    /// 
    /// @param mask_fname
    ///   The name of the mask file to use.
    /// @param vol_start
    ///   The volume's starting oid.
    /// @param oid_start
    ///   The starting oid of the oid range
    /// @param oid_end
    ///   The oid after the end of the oid range
    /// @param locked
    ///   The lock holder object for this thread.
    void x_OrMaskBits(const string   & mask_fname,
                      int              vol_start,
                      int              oid_start,
                      int              oid_end,
                      CSeqDBLockHold & locked);
    
    /// Add bits corresponding to a GI list.
    /// 
    /// This method reads a file containing a list of GIs.  The GIs
    /// which are found in this volume are converted to OIDs, and the
    /// corresponding bits are turned on.
    /// 
    /// @param gilist_fname
    ///   The name of the gi list file to use.
    /// @param volp
    ///   The volume this list is to be applied to.
    /// @param vol_start
    ///   The volume's starting oid.
    /// @param vol_end
    ///   The OID after the end of the volume's OID range.
    /// @param oid_start
    ///   The OID range's starting oid.
    /// @param oid_end
    ///   The OID after the end of the OID range.
    /// @param locked
    ///   The lock holder object for this thread.
    void x_OrGiFileBits(const string    & gilist_fname,
                        const CSeqDBVol * volp,
                        int               vol_start,
                        int               vol_end,
                        int               oid_start,
                        int               oid_end,
                        CSeqDBLockHold  & locked);
    
    /// Read a binary GI list.
    /// 
    /// This method reads a binary GI list file.  This is a binary
    /// array of GI numbers in network byte order.  It may need to
    /// swap the entries.
    /// 
    /// @param gilist
    ///   A file object for this gi list file.
    /// @param lease
    ///   A memory lease holder for the gi list file.
    /// @param num_gis
    ///   The number of entries in the file.
    /// @param gis
    ///   A vector to return the gis in.
    /// @param locked
    ///   The lock holder object for this thread.
    void x_ReadBinaryGiList(CSeqDBRawFile  & gilist,
                            CSeqDBMemLease & lease,
                            int              num_gis,
                            vector<int>    & gis,
                            CSeqDBLockHold & locked);
    
    /// Read a binary GI list.
    /// 
    /// This method reads a text GI list file.  This is a list of
    /// integer GI numbers in ASCII text, seperated by newlines.
    /// 
    /// @param gilist
    ///   A file object for this gi list file.
    /// @param lease
    ///   A memory lease holder for the gi list file.
    /// @param gis
    ///   A vector to return the gis in.
    /// @param locked
    ///   The lock holder object for this thread.
    void x_ReadTextGiList(CSeqDBRawFile  & gilist,
                          CSeqDBMemLease & lease,
                          vector<int>    & gis,
                          CSeqDBLockHold & locked);
    
    /// Set all bits in a range.
    /// 
    /// This method turns on all bits in the specified oid range.  It
    /// is used to turn on bit ranges for volumes where all OIDs are
    /// included.
    /// 
    /// @param oid_start
    ///   The volume's starting oid.
    /// @param oid_end
    ///   The volume's ending oid.
    void x_SetBitRange(int oid_start, int oid_end);
    
    /// Apply a filter to a volume
    ///
    /// This method applies the specified filter to a database volume.
    /// The filter in question may be an OID list, GI list, or OID
    /// range, or may be a combination of the above, except that OID
    /// lists and GI lists cannot be applied together.  In practice,
    /// the OID range is always used, but is specified to span the
    /// volume when the alias file does not contain a range.
    ///
    /// @param filter
    ///   The object specifying the filtering options.
    /// @param vol
    ///   The volume entry describing the volume to work with.
    /// @param locked
    ///   The lock holder object for this thread.
    void x_ApplyFilter(CRef<CSeqDBVolFilter>   filter,
                       const CSeqDBVolEntry  * vol,
                       CSeqDBLockHold        & locked);
    
    /// The memory management layer object.
    CSeqDBAtlas    & m_Atlas;
    
    /// A memory lease which holds the mask file (if only one is used).
    CSeqDBMemLease   m_Lease;
    
    /// The total number of OIDs represented in the bit array.
    int              m_NumOIDs;
    
    /// A pointer to the top of the bit array.
    TUC            * m_Bits;
    
    /// A pointer to the end of the bit array.
    TUC            * m_BitEnd;
    
    /// Set to true if the bit array was allocated.
    bool             m_BitOwner;
};

inline bool
CSeqDBOIDList::x_IsSet(TOID oid) const
{
    TCUC * bp = m_Bits + (oid >> 3);
    
    Int4 bitnum = (oid & 7);
    
    if (bp < m_BitEnd) {
        if (*bp & (0x80 >> bitnum)) {
            return true;
        }
    }
    
    return false;
}

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBOIDLIST_HPP

