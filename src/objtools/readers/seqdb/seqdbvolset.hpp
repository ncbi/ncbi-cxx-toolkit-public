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

BEGIN_NCBI_SCOPE

/// Import definitions from the ncbi::objects namespace.
USING_SCOPE(objects);


/// CSeqDBVolFilter
/// 
/// This class encapsulates information about one application of
/// filtering to a volume.  The filtering listed here is assumed to be
/// done in an "AND" fashion, the included OIDs being those included
/// in all the specified sets.

class CSeqDBVolFilter : public CObject {
public:
    /// Constructor
    ///
    /// Build a filter object, encapsulating one set of filtering to
    /// do on a volume.  Multiple types of filtering can be done here,
    /// the results are effectively ANDed together.  Multiple objects
    /// of this type may exist for a volume, the effects of which are
    /// ORred together.
    ///
    /// @param oid_fn
    ///     Filename of an OID mask file for this volume
    /// @param gi_fn
    ///     Filename of a GI list for this volume
    /// @param start
    ///     First OID to consider
    /// @param end
    ///     OID past the last OID to consider, or UINT_MAX
    CSeqDBVolFilter(const string & oid_fn,
                    const string & gi_fn,
                    int            start,
                    int            end);
    
    /// Destructor
    virtual ~CSeqDBVolFilter()
    {
    }
    
    /// Simple equality test on all fields.
    bool operator == (const CSeqDBVolFilter & rhs) const;
    
    /// Returns true if simple masking is possible.
    ///
    /// There are three masking modes: full inclusion, simple masking,
    /// and virtual masking.  Full inclusion includes every OID in
    /// every volume.  Simple masking memory maps an OID bit mask
    /// directly from one file.  This method returns true if this
    /// filter fits the restrictions of simple masking.
    ///
    /// @return
    ///   True if this filter is simple enough for simple masking.
    bool IsSimple() const;
    
    /// Returns the name of the OID mask file.
    ///
    /// There are three masking operations: OID bit masking, GI list
    /// masking, and range masking.  This method returns the filename
    /// of the OID mask file (an empty string if not an OID mask).
    ///
    /// @return
    ///   The OID mask filter file.
    const string & GetOIDMask() const
    {
        return m_OIDMask;
    }
    
    /// Returns the name of the GI list file.
    ///
    /// There are three masking operations: OID bit masking, GI list
    /// masking, and range masking.  This method returns the filename
    /// of the GI list file (an empty string if not a GI list).
    ///
    /// @return
    ///   The OID mask filter file.
    const string & GetGIList() const
    {
        return m_GIList;
    }
    
    /// Returns the first included OID.
    ///
    /// This method returns the starting offset of the OID range.  (If
    /// no OID range restriction was provided, this will be zero.)
    ///
    /// @return
    ///   The starting OID of the range.
    int BeginOID()
    {
        return m_BeginOID;
    }
    
    /// Returns the first OID past the end of the included OIDs.
    ///
    /// This method returns the ending offset of the range (which is
    /// not part of the range).  (If no range restriction was
    /// provided, this will be UINT_MAX.)
    ///
    /// @return
    ///   The ending OID of the range.
    int EndOID()
    {
        return m_EndOID;
    }
    
private:
    /// Filename of an OID mask file for this volume
    string m_OIDMask;
    
    /// Filename of a GI list for this volume
    string m_GIList;
    
    /// First OID to consider
    int m_BeginOID;
    
    /// OID past the last OID to consider, or UINT_MAX
    int m_EndOID;
};


/// CSeqDBVolEntry
/// 
/// This class controls access to the CSeqDBVol class.  It contains
/// data that is not relevant to the internal operation of a volume,
/// but is associated with that volume for operations over the volume
/// set as a whole, such as the starting OID of the volume and masking
/// information (GI and OID lists).

class CSeqDBVolEntry {
public:
    /// Type used to return the set of volume filters.
    typedef vector< CRef<CSeqDBVolFilter> > TFilters;
    
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
        : m_Vol     (new_vol),
          m_OIDStart(0),
          m_OIDEnd  (0),
          m_AllOIDs (false)
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
    /// @param start
    ///   The first OID in the range.
    void SetStartEnd(int start)
    {
        m_OIDStart = start;
        m_OIDEnd   = start + m_Vol->GetNumOIDs();
    }
    
    /// Filter the volume with an OID mask file
    /// 
    /// The specified filename is recorded in this object as an OID
    /// mask file unless the volume is already marked as unfiltered.
    /// 
    /// @param mask_file
    ///   The file name of an OID mask file.
    /// @param begin
    ///   The first OID to include in the range.
    /// @param end
    ///   The first OID after the included range.
    void AddMask(const string & mask_file, int begin, int end);
    
    /// Filter the volume with a GI list file
    /// 
    /// The specified filename is recorded as a GI list file, unless
    /// the volume is already marked as unfiltered.
    /// 
    /// @param gilist_file
    ///   The file name of GI list file.
    /// @param begin
    ///   The first OID to include in the range.
    /// @param end
    ///   The first OID after the included range.
    void AddGiList(const string & gilist_file, int begin, int end);
    
    /// Filter the volume with a GI list file
    /// 
    /// The specified filename is recorded as a GI list file, unless
    /// the volume is already marked as unfiltered.
    /// 
    /// @param begin
    ///   The first OID to include in the range.
    /// @param end
    ///   The first OID after the included range.
    void AddRange(int begin, int end);
    
    /// Set this volume to an unfiltered state
    /// 
    /// The volume is marked as unfiltered (all OIDs are included),
    /// and any existing filters are cleared.
    void SetIncludeAll()
    {
        m_AllOIDs = true;
        m_Filters.clear();
    }
    
    /// Test whether the volume is filtered.
    /// 
    /// This method checks whether any filtering should be done on
    /// this volume and returns true if it is unfiltered or completely
    /// included.
    /// 
    /// @return
    ///   true if the volume is completely included
    bool GetIncludeAll() const
    {
        return m_AllOIDs || m_Filters.empty();
    }
    
    /// Test whether the volume has a filtering mechanism.
    /// 
    /// This method checks whether any filtering should be done on
    /// this volume and returns true if it is unfiltered.
    /// 
    /// @return
    ///   true if the volume is not restricted by a filter.
    bool HasFilter() const
    {
        return ! m_Filters.empty();
    }
    
    /// Returns true if simple masking is possible.
    ///
    /// This method returns true if this volume's filters fit the
    /// restrictions of simple masking.  The restrictions are that
    /// only one filter may be used, that filter must be an OID mask,
    /// and no OID range can be applied.
    ///
    /// @return
    ///   True if this filter is simple enough for simple masking.
    bool HasSimpleMaskOnly() const
    {
        return (m_Filters.size() == 1) && m_Filters[0]->IsSimple();
    }
    
    /// Get the starting OID in this volume's range.
    /// 
    /// This returns the first OID in this volume's OID range.
    /// 
    /// @return
    ///   The starting OID of the range
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
    
    /// Get the filename of the mask file for this volume.
    const string & GetSimpleMask() const
    {
        _ASSERT(HasSimpleMaskOnly());
        return m_Filters[0]->GetOIDMask();
    }
    
    /// Get the set of filters applied to this volume.
    ///
    /// There can be any number of CSeqDBVolFilter objects applied to
    /// this volume by the hierarchy of alias files.  This method
    /// returns all such filters.  Note that some possible alias file
    /// configurations are not supported by SeqDB.
    /// 
    /// @return
    ///   Returned filter objects applied to this volume.
    const TFilters & GetFilterSet() const
    {
        return m_Filters;
    }
    
private:
    /// Insert a filter
    ///
    /// A filter object is added to the volume.  The results of all
    /// filters are effectively ORred together.
    ///
    /// @param newfilt
    ///     Filter object to add to this volume.
    void x_InsertFilter(CRef<CSeqDBVolFilter> newfilt);
    
    /// The underlying volume object
    CSeqDBVol     * m_Vol;
    
    /// The start of the OID range.
    int             m_OIDStart;
    
    /// The end of the OID range.
    int             m_OIDEnd;
    
    /// True if all OIDs are included.
    bool            m_AllOIDs;
    
    /// The filters applied to this volume.
    TFilters        m_Filters;
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
    /// Type used to return the set of volume filters.
    typedef CSeqDBVolEntry::TFilters TFilters;
    
    /// Constructor
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
    CSeqDBVolSet(CSeqDBAtlas          & atlas,
                 const vector<string> & vol_names,
                 char                   prot_nucl);
    
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
    const CSeqDBVol * FindVol(int oid, int & vol_oid) const
    {
        int rec_indx = m_RecentVol;
        
        if (rec_indx < m_VolList.size()) {
            const CSeqDBVolEntry & rvol = m_VolList[rec_indx];
            
            if ((rvol.OIDStart() <= oid) &&
                (rvol.OIDEnd()   >  oid)) {
                
                vol_oid = oid - rvol.OIDStart();
                
                return rvol.Vol();
            }
        }
        
        for(int index = 0; index < m_VolList.size(); index++) {
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
        
        if (i >= m_VolList.size()) {
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
        
        if (i >= m_VolList.size()) {
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
    size_t GetNumVols() const
    {
        return m_VolList.size();
    }
    
    /// Test for existence of an inclusion filter
    ///
    /// This method returns true if any of the volumes is filtered by
    /// either an OID mask or a GI list.
    ///
    /// @return
    ///   true if any filtering is present.
    bool HasFilter() const
    {
        for(int i = 0; i < m_VolList.size(); i++) {
            if (0 != m_VolList[i].HasFilter()) {
                return true;
            }
        }
        return false;
    }
    
    /// Test for simple one-file-filter case.
    ///
    /// This method returns true if there is exactly one volume, and
    /// that volume is filtered by exactly one OID list, and no GI
    /// lists are present.
    ///
    /// @return
    ///   true if there is one volume, one oid list, and no gi list.
    bool HasSimpleMask() const
    {
        return (m_VolList.size() == 1) && m_VolList[0].HasSimpleMaskOnly();
    }
    
    /// Get the mask file name in the simple case
    ///
    /// This method makes sure the HasSimpleMask predicate is true and
    /// returns the name of the file corresponding to the OID mask.
    ///
    /// @return
    ///   The filename of the OID mask file.
    string GetSimpleMask() const
    {
        _ASSERT(HasSimpleMask());
        return m_VolList[0].GetSimpleMask();
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
    
    /// Add a volume with a GI list filter
    ///
    /// This method adds a new volume to the set, which will be
    /// filtered by the specified GI list.  The begin and end
    /// parameters specify a range of OIDs.  If the GI translates to
    /// an OID that is outside of this range, it will be skipped.
    /// 
    /// @param volname
    ///   The name of the new volume.
    /// @param gilist
    ///   The filename of the GI list used to filter this volume.
    /// @param begin
    ///   Start of OID range to use.
    /// @param end
    ///   End of OID range to use (non-inclusive).
    void AddGiListVolume(const string & volname,
                         const string & gilist,
                         int            begin,
                         int            end)
    {
        CSeqDBVolEntry * v = x_FindVolName(volname);
        if (! v) {
            NCBI_THROW(CSeqDBException, eFileErr,
                       "Error: Could not find volume (" + volname + ").");
        }
        v->AddGiList(gilist, begin, end);
    }
    
    /// Add a volume with an OID mask filter
    ///
    /// This method adds a new volume to the set, which will be
    /// filtered by the specified OID mask.  The begin and end
    /// parameters specify a range of OIDs.  Only the part of the file
    /// corresponding to this range of OIDs will be copied into the
    /// virtual oid list.
    /// 
    /// @param volname
    ///   The name of the new volume.
    /// @param maskfile
    ///   The filename of the OID mask used to filter this volume.
    /// @param begin
    ///   Start of OID range to use.
    /// @param end
    ///   End of OID range to use (non-inclusive).
    void AddMaskedVolume(const string & volname,
                         const string & maskfile,
                         int            begin,
                         int            end)
    {
        CSeqDBVolEntry * v = x_FindVolName(volname);
        if (! v) {
            NCBI_THROW(CSeqDBException, eFileErr,
                       "Error: Could not find volume (" + volname + ").");
        }
        v->AddMask(maskfile, begin, end);
    }
    
    /// Add a volume with an OID range filter
    ///
    /// This method adds a new volume to the set, which will be
    /// filtered by the specified OID range.
    /// 
    /// @param volname
    ///   The name of the new volume.
    /// @param begin
    ///   The first OID to include.
    /// @param end
    ///   The oid after the last oid to include.
    void AddRangedVolume(const string & volname,
                         int            begin,
                         int            end)
    {
        CSeqDBVolEntry * v = x_FindVolName(volname);
        if (! v) {
            NCBI_THROW(CSeqDBException, eFileErr,
                       "Error: Could not find volume (" + volname + ").");
        }
        v->AddRange(begin, end);
    }
    
    /// Add a volume with no filtering
    /// 
    /// This method adds a new volume to the set, for which all of the
    /// OIDs are considered to be included in the database.
    /// 
    /// @param volname
    ///   The name of the new volume.
    void AddFullVolume(const string & volname)
    {
        CSeqDBVolEntry * v = x_FindVolName(volname);
        if (! v) {
            NCBI_THROW(CSeqDBException, eFileErr,
                       "Error: Could not find volume (" + volname + ").");
        }
        v->SetIncludeAll();
    }
    
    /// Return storage held by the volumes
    /// 
    /// This method returns any storage held by CSeqDBMemLease objects
    /// which are part of this set of volumes.  The memory leases will
    /// be reacquired by the volumes if the data is requested again.
    void UnLease()
    {
        for(int index = 0; index < m_VolList.size(); index++) {
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
    const int GetVolOIDStart(int i) const
    {
        if (m_VolList.empty()) {
            return 0;
        }
        
        if (i >= m_VolList.size()) {
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
        
        for(int index = 0; index < m_VolList.size(); index++) {
            vol_total += m_VolList[index].Vol()->GetVolumeLength();
        }
        
        return vol_total;
    }
    
    /// Get the all-oids-included bit for a volume
    /// 
    /// Each volume in the set may be filtered by one or more filters,
    /// of the GI, OID, or range types.  But if a node in the alias
    /// file tree includes the volume without filtering, we can
    /// disable all the filtering mechanisms and remove existing
    /// filters for the volume.  This method returns true if the
    /// volume is fully included.
    /// 
    /// @param i
    ///   The index of the volume.
    /// @return
    ///   True if this volume is fully included.
    bool GetIncludeAllOIDs(int i) const
    {
        return m_VolList[i].GetIncludeAll();
    }
    
    /// Get the set of inclusion filter files
    ///
    /// Volumes may be filtered by OID masks, GI lists, and OID
    /// ranges.  The OID mask is a file containing a bit array, with
    /// one bit for each OID in a volume of a database.  The GI list
    /// mechanism is a file with lists of GIs either as numerals
    /// expressed in decimal ASCII, or 32-bit binary numbers in
    /// network byte order.  OID ranges restrict to a range of OIDs,
    /// possibly in combination with one of the above options.  This
    /// method returns a set of objects describing all of the
    /// filtering done to this volume.  Each filter object contains
    /// one or more restrictions to be applied as an ANDed boolean
    /// query; the set of objects are to be applied as an ORred
    /// boolean query.  The overall operation is an OR-of-ANDs, also
    /// known as a union of intersections.  Any boolean polynomial can
    /// (in theory) be expressed in this form.
    /// 
    /// @param index
    ///   This indicates which volume of the database to deal with.
    /// @return
    ///   The set of all filter objects for the specified volume.
    const TFilters & GetFilterSet(int index) const
    {
        return m_VolList[index].GetFilterSet();
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
    /// @param locked
    ///   The lock holder object for this thread.
    void x_AddVolume(CSeqDBAtlas    & atlas,
                     const string   & nm,
                     char             pn,
                     CSeqDBLockHold & locked);
    
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
        for(int i = 0; i<m_VolList.size(); i++) {
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
        for(int i = 0; i<m_VolList.size(); i++) {
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


