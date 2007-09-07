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
*  Author: Vyacheslav Chetvernin
*
* =========================================================================
*/

#include <ncbi_pch.hpp>

#include "scoring.hpp"
#include "intron.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(prosplign)

CProSplignScaledScoring::CProSplignScaledScoring(const CProSplignScoring& user_scores) :
    CProSplignScoring(user_scores)
{
    scale = GetInvertedIntronExtensionCost() * 3;
    SetIntronExtensionCost(1);
    
    SetGapOpeningCost(scale * GetGapOpeningCost());
    SetGapExtensionCost((scale/3) * GetGapExtensionCost() );
    
    SetFrameshiftOpeningCost(scale * GetFrameshiftOpeningCost());

    SetGTIntronCost(scale * GetGTIntronCost());
    SetGCIntronCost(scale * GetGCIntronCost());
    SetATIntronCost(scale * GetATIntronCost());
    
    SetNonConsensusIntronCost(scale * GetNonConsensusIntronCost());

    Init();
}

int CProSplignScaledScoring::GetScale() const
{
    return scale;
}

CProSplignScaledScoring& CProSplignScaledScoring::SetIntronExtensionCost(int val)
{
    intron_extention = val;
    return *this;
}

int CProSplignScaledScoring::GetIntronExtensionCost() const
{
    return intron_extention;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

// int CFIntron::lmin = 30;
// int CAnyIntron::lmin = CFIntron::lmin;

//regular scores
// int CScoring::sm_e = 1;//one aminoacid (three nuc bases) gap extention
// int CScoring::sm_g = 10;//gap opening 
// int CScoring::sm_f = 30;//framshift opening
// int CScoring::sm_GT = 15;//GT/AG intron opening cost
// int CScoring::sm_GC = 20;//GC/AG intron opening cost
// int CScoring::sm_AT = 25;//AT/AC intron opening cost
// int CScoring::sm_ANY = 34;//should not exceed a sum of two other (different) intron opening costs!
                 // otherwise exons of length zero are possible

//int CScoring::sm_koef = 3000;// = gap extention/intron extention ratio

//Iscores (scaled scores)
// int CAnyIntron::ie;//intron extention cost
// int CFIntron::ie;
// int CScoring::sm_Ig;
// int CScoring::sm_If;
// int CScoring::sm_Ine;
// int CScoring::sm_ICGT;
// int CScoring::sm_ICGC;
// int CScoring::sm_ICAT;
// int CScoring::sm_ICANY;

void CProSplignScaledScoring::Init()
{

    lmin = GetMinIntronLen();
    ie = GetIntronExtensionCost();
    ie_x_lmin = ie*lmin;
    ini_nuc_margin = 1;

    sm_koef = GetScale();
    sm_Ig = GetGapOpeningCost();
    sm_Ine = GetGapExtensionCost();
    sm_If = GetFrameshiftOpeningCost();
    sm_ICGT = GetGTIntronCost();
    sm_ICGC = GetGCIntronCost();
    sm_ICAT = GetATIntronCost();
    sm_ICANY = GetNonConsensusIntronCost();
}

END_SCOPE(prosplign)
END_NCBI_SCOPE
