#ifndef OBJTOOLS_READERS_SEQDB__SEQDBGILISTSET_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDBGILISTSET_HPP

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

/// @file seqdbgilistset.hpp
/// Defines set of GI lists.
/// 
/// Defines classes:
///     CSeqDBGiListSet
/// 
/// Implemented for: UNIX, MS-Windows

#include <objtools/readers/seqdb/seqdbcommon.hpp>
#include "seqdbvolset.hpp"

BEGIN_NCBI_SCOPE

/// CSeqDBGiListSet class
/// 
/// This class provides a single set interface to the collection of GI
/// lists used by the OID list building process.

class CSeqDBGiListSet {
public:
    /// Type used for a reference to a GI list.
    typedef CRef<CSeqDBGiList> TListRef;
    
    /// Constructor
    ///
    /// This class encapsulates some of the behavior of the GI lists
    /// owned by the SeqDB object.  First, GI lists read from files
    /// will be shared by any number of volumes.  Secondly, the
    /// constructor takes a user GI list, which has a special effect
    /// when translating GIs.  If it is not specified, all alias node
    /// GI lists will be completely translated via the ISAM indices.
    /// If it is specified, then alias node GI lists will only be
    /// translated from the user GI list, so that only those GIs
    /// mentioned in the user GI list will be available (but the
    /// translation should be faster).
    ///
    /// @param vol_set
    ///   The set of database volumes.
    /// @param user_gi_list
    ///   GI list provided by the end user.
    /// @param locked
    ///   The lock holder object for this thread.
    CSeqDBGiListSet(CSeqDBAtlas        & atlas,
                    const CSeqDBVolSet & vol_set,
                    TListRef             user_gi_list,
                    CSeqDBLockHold     & locked);
    
    /// Get a reference to a named GI list.
    ///
    /// This returns a reference to an object containing the GI list
    /// read from the specified filename.  A cache is kept so that
    /// each GI list file is only read once.  OIDs for the file are
    /// translated after reading.  If the user gi list was specified,
    /// it will be used to translate GIs on this list the first time
    /// (and only the first time) this method is called for each gi
    /// list.  If the user GI list is not available, the ISAM database
    /// will be consulted to translate this GI list on every call to
    /// this method.  This is necessary because each volume can supply
    /// only part of the translation for this GI list.  Therefore, for
    /// performance reasons, this method should not be called more
    /// than once for the same volume/GI list combination.
    ///
    /// @param filename
    ///   The filename of the GI list file.
    /// @return
    ///   A reference to the specified GI list.
    TListRef GetNodeGiList(const string    & filename,
                           const CSeqDBVol * volp,
                           int               vol_start,
                           int               vol_end,
                           CSeqDBLockHold  & locked);
    
private:
    void x_TranslateFromUserList(CSeqDBGiList & gilist);
    
    /// Memory management layer object.
    CSeqDBAtlas & m_Atlas;
    
    /// User-specified GI list.
    TListRef m_UserList;
    
    /// Map of all alias node specified GI lists.
    map<string, TListRef> m_NodeListMap;
};

END_NCBI_SCOPE


#endif // OBJTOOLS_READERS_SEQDB__SEQDBGILISTSET_HPP


