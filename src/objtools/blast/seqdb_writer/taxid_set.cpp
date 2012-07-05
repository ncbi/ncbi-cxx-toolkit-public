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

/** @file taxid_set.cpp
*     Class which defines sequence id to taxid mapping.
*/

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <objtools/blast/seqdb_writer/taxid_set.hpp>
#include <objtools/blast/seqdb_writer/multisource_util.hpp>
#include <serial/typeinfo.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING
BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
#endif

void CTaxIdSet::SetMappingFromFile(CNcbiIstream & f)
{
    while(f && (! f.eof())) {
        string s;
        NcbiGetlineEOL(f, s);
        
        if (s.empty())
            continue;
        
        // Remove leading/trailing spaces.
        s = NStr::TruncateSpaces(s);
        
        vector<string> tokens;
        NStr::Tokenize(s, " \t", tokens);

        string gi_str = tokens.front();
        string tx_str;
        if (tokens.size() == 2) {
            tx_str = tokens.back();
        }
        
        if (gi_str.size() && tx_str.size()) {
            int taxid = NStr::StringToInt(tx_str, NStr::fAllowLeadingSpaces);
            string key = AccessionToKey(gi_str);
            
            m_TaxIdMap[key] = taxid;
        }
    }
    m_Matched = (m_GlobalTaxId != kTaxIdNotSet) || m_TaxIdMap.empty();
}

int CTaxIdSet::x_SelectBestTaxid(const objects::CBlast_def_line & defline) 
{
    int retval = m_GlobalTaxId;

    if (retval != kTaxIdNotSet) {
        return retval;
    }
    
    if ( !m_TaxIdMap.empty() ) {
        vector<string> keys;
        GetDeflineKeys(defline, keys);
        
        ITERATE(vector<string>, key, keys) {
            if (key->empty())
                continue;
            
            map<string, int>::const_iterator item = m_TaxIdMap.find(*key);
            
            if (item != m_TaxIdMap.end()) {
                retval = item->second;
                m_Matched = true;
                break;
            }
        }
    } else if (defline.IsSetTaxid()) {
        retval = defline.GetTaxid();
    }

    return retval;
}

void
CTaxIdSet::FixTaxId(CRef<objects::CBlast_def_line_set> deflines) 
{
    NON_CONST_ITERATE(CBlast_def_line_set::Tdata, itr, deflines->Set()) {
        (*itr)->SetTaxid(x_SelectBestTaxid(**itr));
    }
}

END_NCBI_SCOPE
