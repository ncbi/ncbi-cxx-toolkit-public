#ifndef ALGO___MM_ALIGNER_THREADS__HPP
#define ALGO___MM_ALIGNER_THREADS__HPP

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
* File Description:  CMMAligner thread classes
*
*/

#include <algo/align/nw/mm_aligner.hpp>
#include <corelib/ncbithr.hpp>

BEGIN_NCBI_SCOPE


class CThreadRunOnTop: public CThread
{
public:
    
    CThreadRunOnTop(const CMMAligner* aligner, const SCoordRect* rect,
                    vector<CNWAligner::TScore>* e,
                    vector<CNWAligner::TScore>* f,
                    vector<CNWAligner::TScore>* g,
                    vector<unsigned char>* trace, bool free_corner_fgap );
    
    virtual void* Main();
    virtual void  OnExit();

protected:
        
    virtual ~CThreadRunOnTop() {}
    
    const CMMAligner           *m_aligner;
    const SCoordRect           *m_rect;
    vector<CNWAligner::TScore> *m_E, *m_F, *m_G;
    vector<unsigned char>      *m_trace;
    bool                       m_free_corner_fgap;
};



class CThreadDoSM: public CThread
{
public:
    
    CThreadDoSM(CMMAligner* aligner, const SCoordRect* rect,
                list<CNWAligner::ETranscriptSymbol>::iterator translist_pos,
                bool free_lt_fgap, bool free_rb_fgap );
    
    virtual void* Main();
    virtual void  OnExit();

protected:
        
    virtual ~CThreadDoSM() {}
    
    CMMAligner                        *m_aligner;
    const SCoordRect                  *m_rect;
    list<CNWAligner::ETranscriptSymbol>::iterator m_translist_pos;
    bool                              m_free_lt_fgap;
    bool                              m_free_rb_fgap;
};


bool MM_RequestNewThread(const unsigned int max_threads);


END_NCBI_SCOPE

#endif  /* ALGO___MM_ALIGNER_THREADS_HPP */
