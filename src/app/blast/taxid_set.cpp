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
* File Description:
*     Class which defines sequence id to taxid mapping.
*/

#include <ncbi_pch.hpp>
#include "taxid_set.hpp"
#include "multisource_util.hpp"
#include <serial/typeinfo.hpp>

USING_NCBI_SCOPE;

/// Maps GI to taxid;
typedef map< int, int > TGiToTaxid;

CTaxIdSet::CTaxIdSet()
    : m_KeepTaxId   (true),
      m_GlobalTaxId (0)
{
}

void CTaxIdSet::SetGlobalTaxId(int taxid)
{
    _ASSERT(m_TaxIdMap.empty());
    
    m_GlobalTaxId = taxid;
}

void CTaxIdSet::SetMappingFromFile(CNcbiIstream & f)
{
    _ASSERT(m_GlobalTaxId == 0);
    
    while(f && (! f.eof())) {
        string s;
        NcbiGetlineEOL(f, s);
        
        if (! s.size())
            continue;
        
        // Remove leading/trailing spaces.
        s = NStr::TruncateSpaces(s);
        
        size_t p = s.find(" ");
        
        if (p == s.npos) {
            // GI with no taxid; is this an error?  Only if the
            // file is strictly for setting the taxid field.  If
            // the file could also be used for the ids file, then
            // the lack of a taxid on this line simply means that
            // no taxid is specified for this GI or other ID.
                
            continue;
        }
        
        string gi_str(s, 0, p);
        string tx_str(s, p+1, s.size()-(p+1));
        
        if (gi_str.size() && tx_str.size()) {
            int taxid = NStr::StringToInt(tx_str, NStr::fAllowLeadingSpaces);
            string key = AccessionToKey(gi_str);
            
            m_TaxIdMap[key] = taxid;
        }
    }
}

int CTaxIdSet::SelectTaxid(const CBlast_def_line & defline, bool keep_taxids)
{
    int taxid = m_GlobalTaxId;
    
    if (taxid <= 0) {
        vector<string> keys;
        GetDeflineKeys(defline, keys);
        
        ITERATE(vector<string>, key, keys) {
            if (! key->size())
                continue;
            
            map<string, int>::iterator item = m_TaxIdMap.find(*key);
            
            if (item != m_TaxIdMap.end()) {
                taxid = item->second;
                break;
            }
        }
        
        if (taxid <= 0 && defline.IsSetTaxid()) {
            taxid = defline.GetTaxid();
        }
    }
    
    return taxid;
}

// Note: this may involve excess duplication, i.e. when combined with
// the other causes of duplication, etc.

CRef<CBlast_def_line_set>
CTaxIdSet::FixTaxId(CRef<CBlast_def_line_set> deflines, bool keep_taxids)
{
    typedef CBlast_def_line TDefline;
    
    // Check that each defline has the specified taxid; if not,
    // replace the defline and set the taxid.
    
    // We will not be changing any of the original data; instead, we
    // duplicate the data and return the changed duplicate.  It would
    // be more efficient to use shallow duplication of the top level
    // object and then use shallow duplication of each defline that
    // has the wrong taxid.  This can be done later if it is found
    // that this path is taking a lot of time -- my assumption is that
    // most of the time the deflines found here will contain all the
    // right taxids.
    
    CRef<CBlast_def_line_set> bdls(new CBlast_def_line_set);
    SerialAssign(*bdls, *deflines);
    
    NON_CONST_ITERATE(list< CRef<CBlast_def_line> >, iter, bdls->Set()) {
        TDefline & dl = **iter;
        
        int taxid = SelectTaxid(dl, keep_taxids);
        
        dl.SetTaxid(taxid);
    }
    
    return bdls;
}

