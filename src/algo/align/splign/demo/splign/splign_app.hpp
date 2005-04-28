#ifndef ALGO_ALIGN_DEMO_SPLIGN_APP_HPP
#define ALGO_ALIGN_DEMO_SPLIGN_APP_HPP

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
* File Description:  Splign application class declarations
*                   
* ===========================================================================
*/

#include "seq_loader.hpp"

#include <algo/align/splign/splign.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

BEGIN_NCBI_SCOPE


class CSplignApp: public CNcbiApplication
{
public:

    virtual void Init();
    virtual int  Run();

protected:

    string    x_RunOnPair(vector<CHit>* hits, int model_id,
                          size_t range_left, size_t range_right);
    bool      x_GetNextPair(istream* ifs, vector<CHit>* hits);
    istream*  x_GetPairwiseHitStream(CSeqLoaderPairwise& seq_loader_pw,
                                     bool cross_species_mode) const;

    // status log
    ofstream m_logstream;
    void   x_LogStatus(size_t model_id, bool query_strand, const string& query,
                       const string& subj, bool error, const string& msg);
    
private:

    string       m_firstline;
    vector<CHit> m_pending; 

#ifdef GENOME_PIPELINE

    CNWAligner::TScore m_Wm;
    CNWAligner::TScore m_Wms;
    CNWAligner::TScore m_Wg;
    CNWAligner::TScore m_Ws;
    CNWAligner::TScore m_Wi [4];
    size_t m_IntronMinSize;

#endif

};

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2005/04/28 19:17:13  kapustin
 * Add cross-species mode flag
 *
 * Revision 1.10  2004/06/21 18:16:45  kapustin
 * Support computation on both query strands
 *
 * Revision 1.9  2004/05/10 16:40:12  kapustin
 * Support a pairwise mode
 *
 * Revision 1.8  2004/05/04 15:23:45  ucko
 * Split splign code out of xalgoalign into new xalgosplign.
 *
 * Revision 1.7  2004/04/23 14:33:32  kapustin
 * *** empty log message ***
 *
 * Revision 1.5  2003/12/23 16:50:25  kapustin
 * Reorder includes to activate msvc pragmas
 *
 * Revision 1.4  2003/12/15 20:16:58  kapustin
 * GetNextQuery() ->GetNextPair()
 *
 * Revision 1.3  2003/11/20 14:38:10  kapustin
 * Add -nopolya flag to suppress Poly(A) detection.
 *
 * Revision 1.2  2003/11/05 20:32:11  kapustin
 * Include source information into the index
 *
 * Revision 1.1  2003/10/30 19:37:20  kapustin
 * Initial toolkit revision
 *
 * ===========================================================================
 */


#endif
