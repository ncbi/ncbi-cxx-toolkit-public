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

#include "seqdbimpl.hpp"

BEGIN_NCBI_SCOPE

CSeqDBImpl::CSeqDBImpl(const string & dbpath, const string & db_name_list, char prot_nucl, bool use_mmap)
    : m_Aliases  (dbpath, db_name_list, prot_nucl, use_mmap),
      m_RecentVol(0)
{
    const vector<string> & vols = m_Aliases.GetVolumeNames();
    
    for(Uint4 i = 0; i < vols.size(); i++) {
        string volpath = SeqDB_CombinePath(dbpath, vols[i], '/');
        
        x_AddVolume(volpath, prot_nucl, use_mmap);
        
        ifdebug_alias << "Added Volume [" << volpath
                      << "] start,end =(" << m_VolStart.back() << "," << m_VolEnd.back()
                      << ")" << endl;
        
        if (prot_nucl == kSeqTypeUnkn) {
            // Once one volume picks a prot/nucl type, enforce that
            // for the rest of the volumes.  This should happen at
            // most once.
            
            prot_nucl = m_Volumes.back()->GetSeqType();
        }
    }
}

CSeqDBImpl::~CSeqDBImpl(void)
{
}

Int4 CSeqDBImpl::GetSeqLength(Uint4 oid)
{
    Uint4 vol_oid = 0;
    
    if (CSeqDBVol * vol = x_FindVol(oid, vol_oid)) {
        return vol->GetSeqLength(vol_oid, false);
    }
    
    return -1;
}

Int4 CSeqDBImpl::GetSeqLengthApprox(Uint4 oid)
{
    Uint4 vol_oid = 0;
    
    if (CSeqDBVol * vol = x_FindVol(oid, vol_oid)) {
        return vol->GetSeqLength(vol_oid, true);
    }
    
    return -1;
}

CRef<CBioseq>
CSeqDBImpl::GetBioseq(Int4 oid,
                      bool use_objmgr,
                      bool insert_ctrlA)
{
    Uint4 vol_oid = 0;
    
    if (CSeqDBVol * vol = x_FindVol(oid, vol_oid)) {
        return vol->GetBioseq(vol_oid, use_objmgr, insert_ctrlA);
    }
    
    return CRef<CBioseq>();
}

void CSeqDBImpl::RetSequence(const char ** buffer)
{
    Uint4 recvol = m_RecentVol;
    
    if (m_Volumes.size() > recvol) {
        if (m_Volumes[recvol]->RetSequence(buffer)) {
            return;
        }
    }
    
    for(Uint4 i = 0; i < m_Volumes.size(); i++) {
        if (m_Volumes[i]->RetSequence(buffer)) {
            m_RecentVol = i;
            return;
        }
    }
}

Int4 CSeqDBImpl::GetSequence(Int4 oid, const char ** buffer)
{
    Uint4 vol_oid = 0;
    if (CSeqDBVol * vol = x_FindVol(oid, vol_oid)) {
        return vol->GetSequence(vol_oid, buffer);
    }
    
    return -1;
}

Uint4 CSeqDBImpl::GetNumSeqs(void)
{
    //return (m_VolEnd.empty() ? 0 : m_VolEnd[m_VolEnd.size()-1]);
    return m_Aliases.GetNumSeqs(m_Volumes);
}

Uint8 CSeqDBImpl::GetTotalLength(void)
{
    //return (m_VolEnd.empty() ? 0 : m_VolEnd[m_VolEnd.size()-1]);
    return m_Aliases.GetTotalLength(m_Volumes);
}

// Uint8 CSeqDBImpl::GetTotalLength(void)
// {
//     Uint8 total_length = 0;
    
//     for(Uint4 i = 0; i < m_Volumes.size(); i++) {
//         total_length += m_Volumes[i]->GetTotalLength();
        
//         ifdebug_mvol << "name[" << i << "]='" << m_Volumes[i]->GetVolName()
//                      << "', length[~]="       << m_Volumes[i]->GetTotalLength()
//                      << ", running total = "  << total_length << endl;
//     }
    
//     return total_length;
// }

string CSeqDBImpl::GetTitle(void)
{
//     if (m_VolList.empty()) {
//         return string();
//     }
//     return m_VolList[0]->m_Vol.GetTitle();
    return m_Aliases.GetTitle(m_Volumes);
}

char CSeqDBImpl::GetSeqType(void)
{
    if (m_Volumes.empty()) {
        return kSeqTypeUnkn;
    }
    return m_Volumes[0]->GetSeqType();
}

string CSeqDBImpl::GetDate(void)
{
    if (m_Volumes.empty()) {
        return string();
    }
    return m_Volumes[0]->GetDate();
}

CRef<CBlast_def_line_set> CSeqDBImpl::GetHdr(Uint4 oid)
{
    Uint4 vol_oid = 0;
    
    if (CSeqDBVol * vol = x_FindVol(oid, vol_oid)) {
        return vol->GetHdr(vol_oid);
    }
    
    return CRef<CBlast_def_line_set>();
}

Uint4 CSeqDBImpl::GetMaxLength(void)
{
    Uint4 max_len = 0;
    
    for(Uint4 i = 0; i < m_Volumes.size(); i++) {
        Uint4 max_this = m_Volumes[i]->GetMaxLength();
        
        if (max_this > max_len) {
            max_len = max_this;
        }
    }
    
    return max_len;
}

END_NCBI_SCOPE

