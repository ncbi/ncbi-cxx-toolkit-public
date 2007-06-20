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

/// @file seqdbvolset.cpp
/// Implementation for the CSeqDBVolSet class, which manages a list of volumes.

#include <ncbi_pch.hpp>
#include "seqdbvolset.hpp"

BEGIN_NCBI_SCOPE

CSeqDBVolSet::CSeqDBVolSet(CSeqDBAtlas          & atlas,
                           const vector<string> & vol_names,
                           char                   prot_nucl,
                           CSeqDBGiList         * user_gilist)
    : m_RecentVol(0)
{
    CSeqDBLockHold locked(atlas);
    atlas.Verify(locked);
    
    try {
        for(int i = 0; i < (int) vol_names.size(); i++) {
            x_AddVolume(atlas, vol_names[i], prot_nucl, user_gilist, locked);
            
            if (prot_nucl == '-') {
                // Once one volume picks a prot/nucl type, enforce that
                // for the rest of the volumes.  This should happen at
                // most once.
                
                prot_nucl = m_VolList.back().Vol()->GetSeqType();
            }
        }
    }
    catch(CSeqDBException&) {
        // The volume destructor will assume the lock is not held.
        atlas.Unlock(locked);
        
        // For SeqDB's own exceptions, we'll keep the error message.
        
        for(int i = 0; i < (int) m_VolList.size(); i++) {
            m_VolList[i].Free();
        }
        throw;
    }
    catch(...) {
        // The volume destructor will assume the lock is not held.
        atlas.Unlock(locked);
        
        // For other exceptions, we'll provide a message.
        
        for(int i = 0; i < (int) m_VolList.size(); i++) {
            m_VolList[i].Free();
        }
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "Error: Could not construct all volumes.");
    }
}

CSeqDBVolSet::CSeqDBVolSet()
    : m_RecentVol(0)
{
}

CSeqDBVolSet::~CSeqDBVolSet()
{
    for(int i = 0; i < (int) m_VolList.size(); i++) {
        m_VolList[i].Free();
    }
}

void CSeqDBVolSet::x_AddVolume(CSeqDBAtlas    & atlas,
                               const string   & nm,
                               char             pn,
                               CSeqDBGiList   * user_gilist,
                               CSeqDBLockHold & locked)
{
    int num_oids = x_GetNumOIDs();
    
    CSeqDBVol * new_volp =
        new CSeqDBVol(atlas, nm, pn, user_gilist, num_oids, locked);
    
    CSeqDBVolEntry new_vol( new_volp );
    new_vol.SetStartEnd( num_oids );
    m_VolList.push_back( new_vol );
}

CSeqDBVolFilter::CSeqDBVolFilter(const string & oid_fn,
                                 const string & id_fn,
                                 bool           use_tis,
                                 int            start,
                                 int            end)
    : m_OIDMask  (oid_fn),
      m_IdList   (id_fn),
      m_BeginOID (start),
      m_EndOID   (end),
      m_UseTis   (use_tis)
{
    _ASSERT(oid_fn.empty() || id_fn.empty());
    
    // It is legal for start == end, it means the list is empty.
}

bool CSeqDBVolFilter::operator == (const CSeqDBVolFilter & rhs) const
{
    return ((m_OIDMask  == rhs.m_OIDMask ) &&
            (m_IdList   == rhs.m_IdList  ) &&
            (m_BeginOID == rhs.m_BeginOID) &&
            (m_EndOID   == rhs.m_EndOID  ));
}

bool CSeqDBVolFilter::IsSimple() const
{
    return (m_OIDMask.empty() == false     &&
            m_IdList.empty()  == true      &&
            m_BeginOID        == 0         &&
            m_EndOID          == INT_MAX);
}

void CSeqDBVolEntry::AddMask(const string & mask_file, int begin, int end)
{
    if (! m_AllOIDs) {
        //m_MaskFiles.insert(mask_file);
        CRef<CSeqDBVolFilter>
            new_filter(new CSeqDBVolFilter(mask_file, "", false, begin, end));
        
        x_InsertFilter(new_filter);
    }
}

void CSeqDBVolEntry::AddIdList(const string & idlist_file,
                               bool           use_tis,
                               int            begin,
                               int            end)
{
    if (! m_AllOIDs) {
        //m_GiListFiles.insert(gilist_file);
        CRef<CSeqDBVolFilter>
            new_filter(new CSeqDBVolFilter("",
                                           idlist_file,
                                           use_tis,
                                           begin,
                                           end));
        
        x_InsertFilter(new_filter);
    }
}

void CSeqDBVolEntry::AddRange(int begin, int end)
{
    if (! m_AllOIDs) {
        //m_GiListFiles.insert(gilist_file);
        CRef<CSeqDBVolFilter>
            new_filter(new CSeqDBVolFilter("", "", false, begin, end));
        
        x_InsertFilter(new_filter);
    }
}

void CSeqDBVolEntry::x_InsertFilter(CRef<CSeqDBVolFilter> newfilt)
{
    CSeqDBVolFilter & new_vf = *newfilt;
        
    ITERATE(TFilters, iter, m_Filters) {
        if ((**iter) == new_vf) {
            return;
        }
    }
    m_Filters.push_back(newfilt);
}

END_NCBI_SCOPE

