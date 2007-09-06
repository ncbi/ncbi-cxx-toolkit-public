#ifndef ALGO_ALIGN_PROSPLIGN_SCORING__HPP
#define ALGO_ALIGN_PROSPLIGN_SCORING__HPP

/* $Id$
* ===========================================================================
*
*                            public DOMAIN NOTICE                          
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
* Author:  Vyacheslav Chetvernin
*
*
*/

#include <algo/align/prosplign/prosplign.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(prosplign)

class CProSplignScaledScoring : public CProSplignScoring {
public:
    CProSplignScaledScoring(const CProSplignScoring& user_scores);

    int GetScale() const;
    CProSplignScaledScoring& SetIntronExtensionCost(int val);
    int GetIntronExtensionCost() const;

private:
    int scale;
    int intron_extention;


// original scoring

public:
    //regular scores 'DScore'
    //    static int sm_g, sm_e, sm_f, sm_GT, sm_GC, sm_AT, sm_ANY;
    //scores t use in functions 'IScore'
    //scaled to be all integer
//     static
    int sm_Ig, sm_Ine, sm_If, sm_ICGT, sm_ICGC, sm_ICAT, sm_ICANY;
//     static
    int sm_koef;//'DScore' = 'IScore' / koef

    int lmin;//minimum intron length
    int ie;//intron extention cost (scaled)
    int ini_nuc_margin;//minimum j-index where to check score before.
                           //in other words we requere ini_nuc_margin nucleotides before start thinking 
                           //about splice. In old version it is 0 meaning splice can be everywhere
private:
//     static
    void Init();
};

END_SCOPE(prosplign)
END_NCBI_SCOPE


#endif
