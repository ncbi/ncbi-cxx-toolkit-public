#ifndef ALGO_ALIGN_SPLIGN_UTIL__HPP
#define ALGO_ALIGN_SPLIGN_UTIL__HPP

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


#include <algo/align/splign/splign.hpp>


BEGIN_NCBI_SCOPE


void   CleaveOffByTail(CSplign::THitRefs* hitrefs, size_t polya_start);

void   GetHitsMinMax(const CSplign::THitRefs& hitrefs,
		     size_t* qmin, size_t* qmax,
		     size_t* smin, size_t* smax);

void   XFilter(CSplign::THitRefs* hitrefs);

struct SCompliment
{
    char operator() (char c) {
        switch(c) {
        case 'A': return 'T';
        case 'G': return 'C';
        case 'T': return 'A';
        case 'C': return 'G';
        }
        return c;
    }
};

END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.10  2005/11/21 16:06:38  kapustin
 * Move gpipe-sensitive items to the app level
 *
 * Revision 1.9  2005/09/12 16:24:00  kapustin
 * Move compartmentization to xalgoalignutil.
 *
 * Revision 1.8  2005/05/24 19:36:08  kapustin
 * -RLE()
 *
 * Revision 1.7  2005/01/26 21:33:12  kapustin
 * ::IsConsensusSplce ==> CSplign::SSegment::s_IsConsensusSplice
 *
 * Revision 1.6  2004/12/16 23:12:26  kapustin
 * algo/align rearrangement
 *
 * Revision 1.5  2004/11/29 15:55:55  kapustin
 * -ScoreByTranscript
 *
 * Revision 1.4  2004/05/04 15:23:45  ucko
 * Split splign code out of xalgoalign into new xalgosplign.
 *
 * Revision 1.3  2004/04/23 14:37:44  kapustin
 * *** empty log message ***
 *
 *
 * ===========================================================================
 */


#endif
