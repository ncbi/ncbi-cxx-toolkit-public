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

/// CSeqDBVolSet
/// 
/// This class stores a set of CSeqDBVol objects and defines an
/// interface to control usage of them.  Several methods are provided
/// to create the set of volumes, or to get the required volumes by
/// different criteria.  Also, certain methods perform operations over
/// the set of volumes.  The CVolEntry class, defined internally to
/// this one, provides some of this abstraction.

class CSeqDBVolSet {
public:
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
    const CSeqDBVol * FindVol(Uint4 oid, Uint4 & vol_oid) const
    {
        Uint4 rec_indx = m_RecentVol;
        
        if (rec_indx < m_VolList.size()) {
            const CVolEntry & rvol = m_VolList[rec_indx];
            
            if ((rvol.OIDStart() <= oid) &&
                (rvol.OIDEnd()   >  oid)) {
                
                vol_oid = oid - rvol.OIDStart();
                
                return rvol.Vol();
            }
        }
        
        for(Uint4 index = 0; index < m_VolList.size(); index++) {
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
    const CSeqDBVol * GetVol(Uint4 i) const
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
        if (const CVolEntry * v = x_FindVolName(volname)) {
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
        if (CVolEntry * v = x_FindVolName(volname)) {
            return v->Vol();
        }
        return 0;
    }
    
    /// Get the number of volumes
    /// 
    /// This returns the number of volumes available from this set.
    /// It would be needed, for example, in order to iterate over all
    /// volumes with the GetVol(Uint4) method.
    /// @return
    ///   The number of volumes available from this set.
    Uint4 GetNumVols() const
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
        for(Uint4 i = 0; i < m_VolList.size(); i++) {
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
        return ((m_VolList.size()          == 1) &&
                (m_VolList[0].NumMasks()   == 1) &&
                (m_VolList[0].NumGiLists() == 0));
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
    Uint4 GetNumOIDs() const
    {
        return x_GetNumOIDs();
    }
    
    /// Get the set of inclusion filter files
    ///
    /// Volumes may be filtered by OID masks, or GI lists.  The OID
    /// mask is a file containing a bit array, with one OID for each
    /// OID in a volume of a database.  The GI list mechanism is a
    /// file with lists of GIs either as numerals expressed in decimal
    /// ASCII, or 32-bit binary numbers in network byte order.  This
    /// method returns the accumulated configuration of filtering as
    /// it pertains to one volume of the database.
    /// 
    /// @param index
    ///   This indicates which volume of the database to deal with.
    /// @param all_oids
    ///   This will be returned true if this volume is unfiltered.
    /// @param mask_files
    ///   This vector will contain the name of all OID masks for this volume.
    /// @param gilist_files
    ///   The GI lists to apply to this volume.
    /// @param oid_start
    ///   The first (not necessarily included) OID in this volume's range.
    /// @param oid_end
    ///   The first OID after the end of this volume's range.
    void GetMaskFiles(Uint4          index,
                      bool         & all_oids,
                      list<string> & mask_files,
                      list<string> & gilist_files,
                      Uint4        & oid_start,
                      Uint4        & oid_end) const
    {
        const CVolEntry & v = m_VolList[index];
        
        if (v.GetIncludeAll()) {
            all_oids = true;
        } else {
            all_oids = false;
            mask_files.clear();
            v.GetFilterFiles(mask_files, gilist_files);
        }
        
        oid_start = v.OIDStart();
        oid_end   = v.OIDEnd();
    }
    
    /// Add a volume with a GI list filter
    ///
    /// This method adds a new volume to the set, which will be
    /// filtered by the specified GI list.
    /// 
    /// @param volname
    ///   The name of the new volume.
    /// @param gilist
    ///   The filename of the GI list used to filter this volume.
    void AddGiListVolume(const string & volname,
                         const string & gilist)
    {
        CVolEntry * v = x_FindVolName(volname);
        if (! v) {
            NCBI_THROW(CSeqDBException, eFileErr,
                       "Error: Could not find volume (" + volname + ").");
        }
        v->AddGiList(gilist);
    }
    
    /// Add a volume with an OID mask filter
    ///
    /// This method adds a new volume to the set, which will be
    /// filtered by the specified OID mask.
    /// 
    /// @param volname
    ///   The name of the new volume.
    /// @param maskfile
    ///   The filename of the OID mask used to filter this volume.
    void AddMaskedVolume(const string & volname, const string & maskfile)
    {
        CVolEntry * v = x_FindVolName(volname);
        if (! v) {
            NCBI_THROW(CSeqDBException, eFileErr,
                       "Error: Could not find volume (" + volname + ").");
        }
        v->AddMask(maskfile);
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
        CVolEntry * v = x_FindVolName(volname);
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
        for(Uint4 index = 0; index < m_VolList.size(); index++) {
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
    const Uint4 GetVolOIDStart(Uint4 i) const
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
    
private:
    /// Private constructor to prevent copy operation.
    CSeqDBVolSet(const CSeqDBVolSet &);
    
    /// Private operator to prevent assignment.
    CSeqDBVolSet & operator=(const CSeqDBVolSet &);
    
    /// CVolEntry class
    /// 
    /// This class provides access to the CSeqDBVol class.  It also
    /// contains data that is not relevant to the internal operation
    /// of a volume, but is associated with that volume for operations
    /// over the volume set as a whole, such as the starting OID of
    /// the volume and masking information (GI and OID lists).
    
    class CVolEntry {
    public:
        /// Constructor for CVolEntry class
        ///
        /// This creates a object containing the specified volume
        /// object pointer.  Although this object owns the pointer, it
        /// uses a vector, so it does not keep an auto pointer or
        /// CRef<>.  Instead, the destructor of the CSeqDBVolSet class
        /// deletes the volumes by calling Free() in a destructor.
        /// Using indirect pointers (CRef<> for example) would require
        /// slightly more cycles in several performance critical
        /// paths.
        ///
        /// @param new_vol
        ///   A pointer to a volume.
        CVolEntry(CSeqDBVol * new_vol)
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
        /// The volume is queried for the number of OIDs it contains,
        /// and the starting and ending OIDs are set.
        ///
        /// @param start
        ///   The first OID in the range.
        void SetStartEnd(Uint4 start)
        {
            m_OIDStart = start;
            m_OIDEnd   = start + m_Vol->GetNumOIDs();
        }
        
        /// Filter the volume with an OID mask file
        /// 
        /// The specified filename is recorded in this object as an
        /// OID mask file unless the volume is already marked as
        /// unfiltered.
        /// 
        /// @param mask_file
        ///   The file name of an OID mask file.
        void AddMask(const string & mask_file)
        {
            if (! m_AllOIDs) {
                m_MaskFiles.insert(mask_file);
            }
        }
        
        /// Filter the volume with a GI list file
        /// 
        /// The specified filename is recorded as a GI list file,
        /// unless the volume is already marked as unfiltered.
        /// 
        /// @param gilist_file
        ///   The file name of GI list file.
        void AddGiList(const string & gilist_file)
        {
            if (! m_AllOIDs) {
                m_GiListFiles.insert(gilist_file);
            }
        }
        
        /// Set this volume to an unfiltered state
        /// 
        /// The volume is marked as unfiltered (all OIDs are
        /// included), and any existing filters are cleared.
        void SetIncludeAll()
        {
            m_AllOIDs = true;
            m_MaskFiles.clear();
            m_GiListFiles.clear();
        }
        
        /// Test whether the volume is filtered.
        /// 
        /// This method checks whether any filtering should be done on
        /// this volume and returns true if it is unfiltered or
        /// completely included.
        /// 
        /// @return
        ///   true if the volume is completely included
        bool GetIncludeAll() const
        {
            return (m_AllOIDs ||
                    (m_MaskFiles.empty() && m_GiListFiles.empty()));
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
            return (NumMasks() != 0) || (NumGiLists() != 0);
        }
        
        /// Get the number of masks applied to this volume
        /// 
        /// This returns the number of OID mask files associated with
        /// this volume.
        /// 
        /// @return
        ///   The number of OID mask files.
        Uint4 NumMasks() const
        {
            return m_MaskFiles.size();
        }
        
        /// Get the number of GI lists applied to this volume
        /// 
        /// This returns the number of GI lists associated with this
        /// volume.
        /// 
        /// @return
        ///   The number of GI list files.
        Uint4 NumGiLists() const
        {
            return m_GiListFiles.size();
        }
        
        /// Get the starting OID in this volume's range.
        /// 
        /// This returns the first OID in this volume's OID range.
        /// 
        /// @return
        ///   The starting OID of the range
        Uint4 OIDStart() const
        {
            return m_OIDStart;
        }
        
        /// Get the ending OID in this volume's range.
        /// 
        /// This returns the first OID past the end of this volume's
        /// OID range.
        /// 
        /// @return
        ///   The ending OID of the range
        Uint4 OIDEnd() const
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
        string GetSimpleMask() const
        {
            _ASSERT((1 == m_MaskFiles.size()) && (m_GiListFiles.empty()));
            return *(m_MaskFiles.begin());
        }
        
        /// Get the filenames of all files used to filter this volume
        ///
        /// There can be any number of GI lists and OID masks applied
        /// to volumes by the hierarchy of alias files.  This method
        /// returns all such files.  Note that some possible alias
        /// file configurations are not supported by SeqDB.
        ///
        /// @param mask_files
        ///   Returned list of OID mask files for this volume.
        /// @param gilist_files
        ///   Returned list of GI list files for this volume.
        void GetFilterFiles(list<string> & mask_files, list<string> & gilist_files) const
        {
            ITERATE(set<string>, i, m_MaskFiles) {
                mask_files.push_back(*i);
            }
            
            ITERATE(set<string>, i, m_GiListFiles) {
                gilist_files.push_back(*i);
            }
        }
        
    private:
        /// The underlying volume object
        CSeqDBVol     * m_Vol;
        
        /// The start of the OID range.
        Uint4           m_OIDStart;
        
        /// The end of the OID range.
        Uint4           m_OIDEnd;
        
        /// True if all OIDs are included.
        bool            m_AllOIDs;
        
        /// The OID masks applied to this volume.
        set<string>     m_MaskFiles;
        
        /// The GI lists applied to this volume.
        set<string>     m_GiListFiles;
    };
    
    /// Get the size of the entire OID range.
    Uint4 x_GetNumOIDs() const
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
    void x_AddVolume(CSeqDBAtlas  & atlas,
                     const string & nm,
                     char           pn);
    
    /// Find a volume by name
    /// 
    /// This returns the CVolEntry object for the volume matching the
    /// specified name.
    /// 
    /// @param volname
    ///   The name of the volume.
    /// @return
    ///   A const pointer to the CVolEntry object, or NULL.
    const CVolEntry * x_FindVolName(const string & volname) const
    {
        for(Uint4 i = 0; i<m_VolList.size(); i++) {
            if (volname == m_VolList[i].Vol()->GetVolName()) {
                return & m_VolList[i];
            }
        }
        
        return 0;
    }
    
    /// Find a volume by name
    /// 
    /// This returns the CVolEntry object for the volume matching the
    /// specified name (non const version).
    /// 
    /// @param volname
    ///   The name of the volume.
    /// @return
    ///   A non-const pointer to the CVolEntry object, or NULL.
    CVolEntry * x_FindVolName(const string & volname)
    {
        for(Uint4 i = 0; i<m_VolList.size(); i++) {
            if (volname == m_VolList[i].Vol()->GetVolName()) {
                return & m_VolList[i];
            }
        }
        
        return 0;
    }
    
    // Data
    
    /// The actual set of volumes.
    vector<CVolEntry>        m_VolList;
    
    /// The index of the most recently used volume
    ///
    /// This variable is mutable and volatile, but is not protected by
    /// locking.  Instead, the following precautions are always taken.
    ///
    /// 1. First, the value is copied into a local variable.
    /// 2. Secondly, the range is always checked.
    /// 3. It is always treated as a hint; there is always fallback
    ///    code to search for the correct volume.
    mutable volatile Uint4   m_RecentVol;
};

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBVOLSET_HPP


