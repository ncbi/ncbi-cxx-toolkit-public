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

/// CSeqDBVolSet class
/// 
/// This object defines access to a set of database volume.

#include "seqdbvol.hpp"

BEGIN_NCBI_SCOPE

using namespace ncbi::objects;

class CSeqDBVolSet : public CObject {
public:
    CSeqDBVolSet(CSeqDBMemPool        & mempool,
                 const vector<string> & vol_names,
                 char                   prot_nucl,
                 bool                   use_mmap);
    
    CSeqDBVol * FindVol(Uint4 oid, Uint4 & vol_oid)
    {
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
    
    CSeqDBVol * GetVol(Uint4 i)
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
    
    CSeqDBVol * GetVol(const string & volname)
    {
        if (CVolEntry * v = x_FindVolName(volname)) {
            return v->Vol();
        }
        return 0;
    }
    
    CSeqDBVol * GetRecentVol(void)
    {
        CSeqDBVol * v = GetVol(m_RecentVol);
        
        if (! v) {
            v = GetVol(0);
        }
        
        return v;
    }
    
    CSeqDBVol * GetLastVol(void)
    {
        if (! m_VolList.empty()) {
            return m_VolList.back().Vol();
        }
        return 0;
    }
    
    void SetRecentVol(Uint4 i)
    {
        m_RecentVol = i;
    }
    
    Uint4 GetNumVols(void)
    {
        return m_VolList.size();
    }
    
    bool HasMask(void)
    {
        for(Uint4 i = 0; i < m_VolList.size(); i++) {
            if (0 != m_VolList[i].NumMasks()) {
                return true;
            }
        }
        return false;
    }
    
    bool HasSimpleMask(void)
    {
        return ((m_VolList.size()        == 1) &&
                (m_VolList[0].NumMasks() == 1));
    }
    
    string GetSimpleMask(void)
    {
        assert(HasSimpleMask());
        
        return m_VolList[0].GetSimpleMask();
    }
    
    Uint4 GetNumSeqs(void)
    {
        if (! m_VolList.empty()) {
            return m_VolList.back().OIDEnd();
        }
        return 0;
    }
    
    void GetMaskFiles(Uint4          index,
                      bool         & all_oids,
                      list<string> & mask_files,
                      Uint4        & oid_start,
                      Uint4        & oid_end)
    {
        CVolEntry & v = m_VolList[index];
        
        if (v.GetIncludeAll()) {
            all_oids = true;
        } else {
            all_oids = false;
            mask_files.clear();
            v.GetMaskFiles(mask_files);
        }
        
        oid_start = v.OIDStart();
        oid_end   = v.OIDEnd();
    }
    
    void AddMaskedVolume(const string & volname, const string & maskfile)
    {
        CVolEntry * v = x_FindVolName(volname);
        assert(v != 0);
        v->AddMask(maskfile);
    }
    
    void AddFullVolume(const string & volname)
    {
        CVolEntry * v = x_FindVolName(volname);
        assert(v != 0);
        v->SetIncludeAll();
    }
    
private:
    
    class CVolEntry {
    public:
        CVolEntry(CSeqDBVol * new_vol)
            : m_Vol     (new_vol),
              m_OIDStart(0),
              m_OIDEnd  (0),
              m_AllOIDs (false)
        {
        }
        
        void SetStartEnd(Uint4 start)
        {
            m_OIDStart = start;
            m_OIDEnd   = start + m_Vol->GetNumSeqs();
        }
        
        void AddMask(const string & mask_file)
        {
            if (! m_AllOIDs) {
                m_MaskFiles.insert(mask_file);
            }
        }
        
        void SetIncludeAll(void)
        {
            m_AllOIDs = true;
            m_MaskFiles.clear();
        }
        
        bool GetIncludeAll(void)
        {
            return (m_AllOIDs || m_MaskFiles.empty());
        }
        
        Uint4 NumMasks(void)
        {
            return m_MaskFiles.size();
        }
        
        Uint4 OIDStart(void)
        {
            return m_OIDStart;
        }
        
        Uint4 OIDEnd(void)
        {
            return m_OIDEnd;
        }
        
        CSeqDBVol * Vol(void)
        {
            return m_Vol.GetNonNullPointer();
        }
        
        string GetSimpleMask(void)
        {
            assert(1 == m_MaskFiles.size());
            
            return *(m_MaskFiles.begin());
        }
        
        void GetMaskFiles(list<string> & mask_files)
        {
            set<string>::iterator i = m_MaskFiles.begin();
            
            while(i != m_MaskFiles.end()) {
                mask_files.push_back(*i);
                i++;
            }
        }
        
    private:
        CRef<CSeqDBVol> m_Vol;
        Uint4           m_OIDStart;
        Uint4           m_OIDEnd;
        bool            m_AllOIDs;
        set<string>     m_MaskFiles;
    };
    
    Uint4 x_GetNumSeqs(void)
    {
        if (m_VolList.empty())
            return 0;
        
        return m_VolList.back().OIDEnd();
    }
    
    void x_AddVolume(CSeqDBMemPool & mempool,
                     const string  & nm,
                     char            pn,
                     bool            use_mm)
    {
        CVolEntry new_vol( new CSeqDBVol(mempool, nm, pn, use_mm) );
        new_vol.SetStartEnd( x_GetNumSeqs() );
        m_VolList.push_back( new_vol );
    }
    
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

    vector<CVolEntry> m_VolList;
    volatile Uint4    m_RecentVol;
};

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBVOLSET_HPP


