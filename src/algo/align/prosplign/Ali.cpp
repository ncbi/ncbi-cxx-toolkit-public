/*
*  $Id$
*
* =========================================================================
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
*  Government do not and cannt warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* =========================================================================
*
*  Author: Boris Kiryutin
*
* =========================================================================
*/

#include <ncbi_pch.hpp>

#include <objects/seqloc/seqloc__.hpp>
#include <objects/general/general__.hpp>

#include <algo/align/prosplign/prosplign.hpp>
#include <algo/align/prosplign/prosplign_exception.hpp>

#include "Ali.hpp"
#include "NSeq.hpp"
#include "Info.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(prosplign)
using namespace objects;

CAli::CAli()
{
}

CAliCreator::CAliCreator(CAli& ali) : m_ali(ali), m_CurType(eMP)
{
    m_CurLen = 0; 
}

CAliCreator::~CAliCreator(void)
{
}

CAli::CAli(const vector<pair<int, int> >& igi, bool lgap, bool rgap, const CAli& frali)
{
    vector<CAliPiece>::const_iterator pit = frali.m_ps.begin();
    vector<pair<int, int> >::const_iterator igit = igi.begin();
    int curpos = 0;
    if(lgap) {//beginning gap
        if(igit == igi.end() || igit->first != 0) throw runtime_error("beginning gap not found");
        CAliPiece intron;
        intron.Init(eHP);
        if(pit != frali.m_ps.end() && pit->m_type == eHP) {//join beginning gaps
            curpos = igit->second + pit->m_len;
            ++pit;
        } else {//add beginning gap
            curpos = igit->second;
        }
        intron.m_len = curpos;
        m_ps.push_back(intron);
        ++igit;
    }
    while(pit != frali.m_ps.end()) {
        if(pit->m_type == eVP) {
            if(pit+1 == frali.m_ps.end()) {//the last V-gap
                if(pit == frali.m_ps.begin()) throw runtime_error("failed to insert introns, second stage alignment is artificial (V-gap)");
                if(rgap) {
                    if(igit == igi.end()) throw runtime_error("the last intron not found");
                } else if(igit != igi.end()) {//intron goes before the last V-gap
                    if(curpos != igit->first) throw runtime_error("the last intron is misplaced");
                    CAliPiece intron;
                    intron.Init(eSP);
                    intron.m_len = igit->second;
                    m_ps.push_back(intron);
                    curpos = igit->first + igit->second;
                    ++igit;
                }
            }
            m_ps.push_back(*pit);
      }
      else {
        if(igit != igi.end() && curpos > igit->first) throw runtime_error("failed to insert introns into second stage alignment");
        CAliPiece ps;
        ps.Init(pit->m_type);
        int rest = pit->m_len;
        while(igit != igi.end() && curpos + rest > igit->first) {
            ps.m_len = igit->first - curpos;
            if(ps.m_len > 0) {
                m_ps.push_back(ps);
                rest -= ps.m_len;
            }
            CAliPiece intron;
            intron.Init(eSP);
            intron.m_len = igit->second;
            m_ps.push_back(intron);
            curpos = igit->first + igit->second;
            ++igit;
        }
        if(rest > 0) {
            ps.m_len = rest;
            m_ps.push_back(ps);
            curpos += rest;
        }
      }
      ++pit;      
    }
    //the end gap if exists
    if(igit != igi.end()) {
        if(curpos != igit->first) throw runtime_error("misplaced end gap");
        //if(!rgap) throw runtime_error("wrong resulting alignment, intron at the end");
        if(!rgap) {//intron at the end. Artefact?
            CAliPiece intron;
            intron.Init(eSP);
            intron.m_len = igit->second;
            m_ps.push_back(intron);
        } else {//end gap
            if(!m_ps.empty() && m_ps.back().m_type == eHP) {//join end gaps
                m_ps.back().m_len += igit->second;
            } else {// add end gap
                CAliPiece intron;
                intron.Init(eHP);
                intron.m_len = igit->second;
                m_ps.push_back(intron);
            }
        }
        curpos = igit->first + igit->second;
    }
}



END_SCOPE(prosplign)
END_NCBI_SCOPE
