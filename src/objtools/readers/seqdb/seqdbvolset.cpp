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

#include <ncbi_pch.hpp>
#include "seqdbvolset.hpp"

BEGIN_NCBI_SCOPE

CSeqDBVolSet::CSeqDBVolSet(CSeqDBAtlas          & atlas,
                           const vector<string> & vol_names,
                           char                   prot_nucl)
    : m_RecentVol(0)
{
    try {
        for(Uint4 i = 0; i < vol_names.size(); i++) {
            x_AddVolume(atlas, vol_names[i], prot_nucl);
        
            if (prot_nucl == kSeqTypeUnkn) {
                // Once one volume picks a prot/nucl type, enforce that
                // for the rest of the volumes.  This should happen at
                // most once.
            
                prot_nucl = m_VolList.back().Vol()->GetSeqType();
            }
        }
    }
    catch(...) {
        for(Uint4 i = 0; i < m_VolList.size(); i++) {
            m_VolList[i].Free();
        }
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "Error: Could not construct all volumes.");
    }
}

CSeqDBVolSet::~CSeqDBVolSet()
{
    for(Uint4 i = 0; i < m_VolList.size(); i++) {
        m_VolList[i].Free();
    }
}

void CSeqDBVolSet::x_AddVolume(CSeqDBAtlas  & atlas,
                               const string & nm,
                               char           pn)
{
    CSeqDBVol * new_volp = new CSeqDBVol(atlas, nm, pn);
    
    CVolEntry new_vol( new_volp );
    new_vol.SetStartEnd( x_GetNumSeqs() );
    m_VolList.push_back( new_vol );
}

END_NCBI_SCOPE

