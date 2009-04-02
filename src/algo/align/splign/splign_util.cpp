/* $Id$
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
* Author:  Yuri Kapustin
*
* File Description:  Helper functions
*                   
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include "splign_util.hpp"
#include "messages.hpp"

#include <algo/align/nw/align_exception.hpp>
#include <algo/align/util/blast_tabular.hpp>

#include <algorithm>
#include <math.h>

BEGIN_NCBI_SCOPE

void CleaveOffByTail(CSplign::THitRefs* phitrefs, size_t polya_start)
{
    const size_t hit_dim = phitrefs->size();

    for(size_t i = 0; i < hit_dim; ++i) {

        CSplign::THitRef& hit = (*phitrefs)[i];
        if(hit->GetQueryStart() >= polya_start) {
            (*phitrefs)[i].Reset(NULL);
        }
        else if(hit->GetQueryStop() >= polya_start) {
            hit->Modify(1, polya_start - 1);
        }
    }

    typedef CSplign::THitRefs::iterator TI;
    TI ii0 = phitrefs->begin();
    for(TI ii = ii0, ie = phitrefs->end(); ii != ie; ++ii) {
        if(ii->NotEmpty()) {
            if(ii0 < ii) {
                *ii0 = *ii;
            }
            ++ii0;
        }
    }

    phitrefs->erase(ii0, phitrefs->end());
}

END_NCBI_SCOPE
