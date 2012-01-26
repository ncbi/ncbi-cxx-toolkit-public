#ifndef OBJTOOLS_READERS_SEQDB__SEQDBVOLSET_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDBVOLSET_HPP

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

/// @file seqdbvolset.hpp
/// Manages a set of database volumes.
/// 
/// Defines classes:
///     CSeqDBVolSet
///     CVolEntry
/// 
/// Implemented for: UNIX, MS-Windows

#include "seqdbvol.hpp"
#include "seqdbfilter.hpp"
#include <algo/blast/core/ncbi_std.h>

BEGIN_NCBI_SCOPE

/// Import definitions from the ncbi::objects namespace.
USING_SCOPE(objects);

/// CSeqDBVolEntry
/// 
/// This class controls access to the CSeqDBVol class.  It contains
/// data that is not relevant to the internal operation of a volume,
/// but is associated with that volume for operations over the volume
/// set as a whole, such as the starting OID of the volume and masking
/// information (GI and OID lists).

class CSeqDBVolEntry {
public:
    /// Constructor
    ///
    /// This creates a object containing the specified volume object
    /// pointer.  Although this object owns the pointer, it uses a
    /// vector, so it does not keep an auto pointer or CRef<>.
    /// Instead, the destructor of the CSeqDBVolSet class deletes the
    /// volumes by calling Free() in a destructor.  Using indirect
    /// pointers (CRef<> for example) would require slightly more
    /// cycles in several performance critical paths.
    ///
    /// @param new_vol
    ///   A pointer to a volume.
    CSeqDBVolEntry(CSeqDBVol * new_vol)
        : m_Vol        (new_vol),
          m_OIDStart   (0),
          m_OIDEnd     (0),
          m_AllOIDs    (false)
    {
    }
    
    /// Free the volume object
    ///
    /// The associated volume object is deleted.
    void Free()
    {
        if (m_Vol) {
            delete m_Vol;
            m_Vol = 0;
        }
    }
    
    /// Set the OID range
    /// 
    /// The volume is queried for the number of OIDs it contains, and
    /// the starting and ending OIDs are set.
    /// 
    /// @param start The first OID in the range.
    void SetStartAndEnd(int start)
    {
        m_OIDStart = start;
        m_OIDEnd   = start + m_Vol->GetNumOIDs();
    }
    
    /// Get the starting OID in this volume's range.
    /// 
    /// This returns the first OID in this volume's OID range.
    /// 
    /// @return The starting OID of the range
    int OIDStart() const
    {
        return m_OIDStart;
    }
    
    /// Get the ending OID in this volume's range.
    /// 
    /// This returns the first OID past the end of this volume's OID
    /// range.
    /// 
    /// @return
    ///   The ending OID of the range
    int OIDEnd() const
    {
        return m_OIDEnd;
    }
    
    /// Get a pointer to the underlying volume object.
    CSeqDBVol * Vol()
    {
        return m_Vol;
    }
    
    /// Get a const pointer to the underlying volume object.
    const CSeqDBVol * Vol() const
    {
        return m_Vol;
    }
    
private:
    /// The underlying volume object
    CSeqDBVol     * m_Vol;
    
    /// The start of the OID range.
    int             m_OIDStart;
    
    /// The end of the OID range.
    int             m_OIDEnd;
    
    /// True if all OIDs are included.
    bool            m_AllOIDs;
};


/// CSeqDBVolSet
/// 
/// This class stores a set of CSeqDBVol objects and defines an
/// interface to control usage of them.  Several methods are provided
/// to create the set of volumes, or to get the required volumes by
/// different criteria.  Also, certain methods perform operations over
/// the set of volumes.  The CSeqDBVolEntry class, defined internally
/// to this one, provides some of this abstraction.
class CSeqDBVolSet {
public:
    /// Standard Constructor
    /// 
    /// An object of this class will be constructed after the alias
    /// files have been read, and the volume names will come from that
    /// processing step.  All of the specified volumes will be opened
    /// and the metadata will be verified during construction.
    /// 
    /// @param atlas
    ///   The memory management object to use.
    /// @param vol_names
    ///   The names of the volumes this object will manage.
    /// @param prot_nucl
    ///   Whether these are protein or nucleotide sequences.
    /// @param user_list
    ///   If specified, will be used to include deflines by GI or TI.
    /// @param neg_list
    ///   If specified, will be used to exclude deflines by GI or TI.
    CSeqDBVolSet(CSeqDBAtlas          & atlas,
                 const vector<string> & vol_names,
                 char                   prot_nucl,
                 CSeqDBGiList         * user_list,
                 CSeqDBNegativeList   * neg_list);
    
    /// Default Constructor
    ///
    /// An empty volume set will be created; this is in support of the
    /// CSeqDBExpert class's default constructor.
    CSeqDBVolSet();
    
    /// Destructor
    ///
    /// The destructor will release all resources still held, but some
    /// of the resources will probably already be cleaned up via a
    /// call to the UnLease method.
    ~CSeqDBVolSet();
    
    /// Find a volume by OID.
    /// 
    /// Many of the CSeqDB methods identify which sequence to use by
    /// OID.  That OID applies to all sequences in all volumes of the
    /// opened database(s).  This method is used to find the volume
    /// (if any) that contains this OID, and to return both a pointer
    /// to that volume and the OID within that volume that corresponds
    /// to the global input OID.
    ///
    /// @param oid
    ///   The global OID to search for.
    /// @param vol_oid
    ///   The returned OID within the relevant volume.
    /// @return
    ///   A pointer to the volume containing the oid, or NULL.
    CSeqDBVol * FindVol(int oid, int & vol_oid) const
    {
        // The 'const' usage here should be cleaned up, i.e. const
        // should be removed from most of SeqDB's methods.  Since the
        // atlas often remaps the actual file data due to seemingly
        // read-only user requests, there are very few parts of this
        // code that can really be considered const.  "Conceptual"
        // const is not worth the trouble, particularly for internal
        // methods.
        
        // A good technique would be to remove all or nearly all of
        // the 'mutable' keywords, then remove the word 'const' from
        // almost everything the compiler complains about.
        
        int vol_idx(0);
        return const_cast<CSeqDBVol*>(FindVol(oid, vol_oid, vol_idx));
    }
    
    /// Find a volume by OID.
    /// 
    /// Many of the CSeqDB methods identify which sequence to use by
    /// OID.  That OID applies to all sequences in all volumes of the
    /// opened database(s).  This method is used to find the volume
    /// (if any) that contains this OID, and to return a pointer to
    /// that volume, the OID within that volume that corresponds to
    /// the global input OID, and the volume index.
    ///
    /// @param oid
    ///   The global OID to search for.
    /// @param vol_oid
    ///   The returned OID within the relevant volume.
    /// @param vol_idx
    ///   The returned index of the relevant volume.
    /// @return
    ///   A pointer to the volume containing the oid, or NULL.
    const CSeqDBVol * FindVol(int oid, int & vol_oid, int & vol_idx) const
    {
        int rec_indx = m_RecentVol;
        
        if (rec_indx < (int) m_VolList.size()) {
            const CSeqDBVolEntry & rvol = m_VolList[rec_indx];
            
            if ((rvol.OIDStart() <= oid) &&
                (rvol.OIDEnd()   >  oid)) {
                
                vol_oid = oid - rvol.OIDStart();
                vol_idx = rec_indx;
                
                return rvol.Vol();
            }
        }
        
        for(int index = 0; index < (int) m_VolList.size(); index++) {
            if ((m_VolList[index].OIDStart() <= oid) &&
                (m_VolList[index].OIDEnd()   >  oid)) {
                
                m_RecentVol = index;
                
                vol_oid = oid - m_VolList[index].OIDStart();
                vol_idx = index;
                
                return m_VolList[index].Vol();
            }
        }
        
        return NULL;
    }
    
    /// Find a volume by OID.
    /// 
    /// Many of the CSeqDB methods identify which sequence to use by
    /// OID.  That OID applies to all sequences in all volumes of the
    /// opened database(s).  This method is used to find the volume
    /// (if any) that contains this OID, and to return both a pointer
    /// to that volume and the OID within that volume that corresponds
    /// to the global input OID.
    ///
    /// @param oid
    ///   The global OID to search for.
    /// @param vol_oid
    ///   The returned OID within the relevant volume.
    /// @return
    ///   A pointer to the volume containing the oid, or NULL.
    CSeqDBVol * FindVol(int oid, int & vol_oid)
    {
        int rec_indx = m_RecentVol;
        
        if (rec_indx < (int) m_VolList.size()) {
            CSeqDBVolEntry & rvol = m_VolList[rec_indx];
            
            if ((rvol.OIDStart() <= oid) &&
                (rvol.OIDEnd()   >  oid)) {
                
                vol_oid = oid - rvol.OIDStart();
                
                return rvol.Vol();
            }
        }
        
        for(int index = 0; index < (int) m_VolList.size(); index++) {
            if ((m_VolList[index].OIDStart() <= oid) &&
                (m_VolList[index].OIDEnd()   >  oid)) {
                
                m_RecentVol = index;
                
                vol_oid = oid - m_VolList[index].OIDStart();
                
                return m_VolList[index].Vol();
            }
        }
        
        return 0;
    }
    
    /// Find a volume by index.
    /// 
    /// This method returns a volume by index, so that 0 is the first
    /// volume, and N-1 is the last volume of a set of N.
    ///
    /// @param i
    ///   The index of the volume to return.
    /// @return
    ///   A pointer to the indicated volume, or NULL.
    const CSeqDBVol * GetVol(int i) const
    {
        if (m_VolList.empty()) {
            return 0;
        }
        
        if (i >= (int) m_VolList.size()) {
            return 0;
        }
        
        m_RecentVol = i;
        
        return m_VolList[i].Vol();
    }
    
    /// Find a volume by index.
    /// 
    /// This method returns a volume by index, so that 0 is the first
    /// volume, and N-1 is the last volume of a set of N.
    ///
    /// @param i
    ///   The index of the volume to return.
    /// @return
    ///   A pointer to the indicated volume, or NULL.
    CSeqDBVol * GetVolNonConst(int i)
    {
        if (m_VolList.empty()) {
            return 0;
        }
        
        if (i >= (int) m_VolList.size()) {
            return 0;
        }
        
        m_RecentVol = i;
        
        return m_VolList[i].Vol();
    }
    
    /// Find a volume entry by index.
    /// 
    /// This method returns a CSeqDBVolEntry by index, so that 0 is
    /// the first volume, and N-1 is the last volume of a set of N.
    ///
    /// @param i
    ///   The index of the volume entry to return.
    /// @return
    ///   A pointer to the indicated volume entry, or NULL.
    const CSeqDBVolEntry * GetVolEntry(int i) const
    {
        if (m_VolList.empty()) {
            return 0;
        }
        
        if (i >= (int) m_VolList.size()) {
            return 0;
        }
        
        m_RecentVol = i;
        
        return & m_VolList[i];
    }
    
    /// Find a volume by name.
    /// 
    /// Each volume has a name, which is the name of the component
    /// files (.pin, .psq, etc), without the file extension.  This
    /// method returns a const pointer to the volume matching the
    /// specified name.
    /// 
    /// @param volname
    ///   The name of the volume to search for.
    /// @return
    ///   A pointer to the volume matching the specified name, or NULL.
    const CSeqDBVol * GetVol(const string & volname) const
    {
        if (const CSeqDBVolEntry * v = x_FindVolName(volname)) {
            return v->Vol();
        }
        return 0;
    }
    
    /// Find a volume by name (non-const version).
    /// 
    /// Each volume has a name, which is the name of the component
    /// files (.pin, .psq, etc), without the file extension.  This
    /// method returns a non-const pointer to the volume matching the
    /// specified name.
    /// 
    /// @param volname
    ///   The name of the volume to search for.
    /// @return
    ///   A pointer to the volume matching the specified name, or NULL.
    CSeqDBVol * GetVol(const string & volname)
    {
        if (CSeqDBVolEntry * v = x_FindVolName(volname)) {
            return v->Vol();
        }
        return 0;
    }
    
    /// Get the number of volumes
    /// 
    /// This returns the number of volumes available from this set.
    /// It would be needed, for example, in order to iterate over all
    /// volumes with the GetVol(int) method.
    /// @return
    ///   The number of volumes available from this set.
    int GetNumVols() const
    {
        return (int)m_VolList.size();
    }
    
    /// Get the size of the OID range.
    ///
    /// This method returns the total size of the combined (global)
    /// OID range of this database.
    ///
    /// @return
    ///   The number of OIDs.
    int GetNumOIDs() const
    {
        return x_GetNumOIDs();
    }
    
    /// Return storage held by the volumes
    /// 
    /// This method returns any storage held by CSeqDBMemLease objects
    /// which are part of this set of volumes.  The memory leases will
    /// be reacquired by the volumes if the data is requested again.
    void UnLease()
    {
        for(int index = 0; index < (int) m_VolList.size(); index++) {
            m_VolList[index].Vol()->UnLease();
        }
    }
    
    /// Get the first OID in a volume.
    /// 
    /// Each volume is considered to span a range of OIDs.  This
    /// method returns the first OID in the OID range of the indicated
    /// volume.  The returned OID may not be included (ie. it may be
    /// turned off via a filtering mechanism).
    /// 
    /// @param i
    ///   The index of the volume.
    int GetVolOIDStart(int i) const
    {
        if (m_VolList.empty()) {
            return 0;
        }
        
        if (i >= (int) m_VolList.size()) {
            return 0;
        }
        
        m_RecentVol = i;
        
        return m_VolList[i].OIDStart();
    }
    
    /// Find total volume length for all volumes
    /// 
    /// Each volume in the set has an internally stored length, which
    /// indicates the length (in nucleotides/residues/bases) of all of
    /// the sequences in the volume.  This returns the total of these
    /// lengths.
    /// 
    /// @return
    ///   The sum of the lengths of all volumes.
    Uint8 GetVolumeSetLength() const
    {
        Uint8 vol_total = 0;
        
        for(int index = 0; index < (int) m_VolList.size(); index++) {
            vol_total += m_VolList[index].Vol()->GetVolumeLength();
        }
        
        return vol_total;
    }

    int GetMaxLength() const
    {
        int max_len = 0;

        for(int index = 0; index < (int) m_VolList.size(); index++) {
            max_len = max( max_len, m_VolList[index].Vol()->GetMaxLength());
        }

        return max_len;
    }       
    
    int GetMinLength() const
    {
        int min_len = INT4_MAX;

        for(int index = 0; index < (int) m_VolList.size(); index++) {
            min_len = min( min_len, m_VolList[index].Vol()->GetMinLength());
        }

        return min_len;
    }       
    
    /// Optimize the GI list configuration.
    /// 
    /// This tells the volumes to examine and optimize their GI list
    /// configuration.  It should not be called until all GI lists
    /// have been added to the volumes (by alias file processing).
    void OptimizeGiLists()
    {
        for(int i = 0; i< (int) m_VolList.size(); i++) {
            m_VolList[i].Vol()->OptimizeGiLists();
        }
    }
    
private:
    /// Private constructor to prevent copy operation.
    CSeqDBVolSet(const CSeqDBVolSet &);
    
    /// Private operator to prevent assignment.
    CSeqDBVolSet & operator=(const CSeqDBVolSet &);
    
    /// Get the size of the entire OID range.
    int x_GetNumOIDs() const
    {
        if (m_VolList.empty())
            return 0;
        
        return m_VolList.back().OIDEnd();
    }
    
    /// Add a volume
    /// 
    /// This method adds a volume to the set.
    /// 
    /// @param atlas
    ///   The memory management layer object.
    /// @param nm
    ///   The name of the volume.
    /// @param pn
    ///   The sequence type.
    /// @param user_list
    ///   If specified, will be used to include deflines by ID.
    /// @param neg_list
    ///   If specified, will be used to exclude deflines by ID.
    /// @param locked
    ///   The lock holder object for this thread.
    void x_AddVolume(CSeqDBAtlas        & atlas,
                     const string       & nm,
                     char                 pn,
                     CSeqDBGiList       * user_list,
                     CSeqDBNegativeList * neg_list,
                     CSeqDBLockHold     & locked);
    
    /// Find a volume by name
    /// 
    /// This returns the CSeqDBVolEntry object for the volume matching
    /// the specified name.
    /// 
    /// @param volname
    ///   The name of the volume.
    /// @return
    ///   A const pointer to the CSeqDBVolEntry object, or NULL.
    const CSeqDBVolEntry * x_FindVolName(const string & volname) const
    {
        for(int i = 0; i< (int) m_VolList.size(); i++) {
            if (volname == m_VolList[i].Vol()->GetVolName()) {
                return & m_VolList[i];
            }
        }
        
        return 0;
    }
    
    /// Find a volume by name
    /// 
    /// This returns the CSeqDBVolEntry object for the volume matching
    /// the specified name (non const version).
    /// 
    /// @param volname
    ///   The name of the volume.
    /// @return
    ///   A non-const pointer to the CSeqDBVolEntry object, or NULL.
    CSeqDBVolEntry * x_FindVolName(const string & volname)
    {
        for(int i = 0; i < (int) m_VolList.size(); i++) {
            if (volname == m_VolList[i].Vol()->GetVolName()) {
                return & m_VolList[i];
            }
        }
        
        return 0;
    }
    
    /// The actual set of volumes.
    vector<CSeqDBVolEntry> m_VolList;
    
    /// The index of the most recently used volume
    ///
    /// This variable is mutable and volatile, but is not protected by
    /// locking.  Instead, the following precautions are always taken.
    ///
    /// 1. First, the value is copied into a local variable.
    /// 2. Secondly, the range is always checked.
    /// 3. It is always treated as a hint; there is always fallback
    ///    code to search for the correct volume.
    mutable volatile int m_RecentVol;
};

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBVOLSET_HPP


