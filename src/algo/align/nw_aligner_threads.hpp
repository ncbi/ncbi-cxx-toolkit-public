#ifndef ALGO___NW_ALIGNER_THREADS__HPP
#define ALGO___NW_ALIGNER_THREADS__HPP

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
* File Description:  CNWAligner thread classes
*
*/

#include <algo/align/nw_aligner.hpp>
#include <corelib/ncbithr.hpp>

BEGIN_NCBI_SCOPE


class CNWAlignerThread_Align: public CThread
{
public:
    
    CNWAlignerThread_Align(CNWAligner* aligner, CNWAligner::SAlignInOut* data):
        m_aligner(aligner),
        m_data(data)
    {}
    
    virtual void* Main();
    virtual void  OnExit();

protected:
        
    virtual ~CNWAlignerThread_Align() {}
    
    CNWAligner*                 m_aligner;
    CNWAligner::SAlignInOut*    m_data;

    auto_ptr<CException>        m_exception;
};

bool NW_RequestNewThread(const unsigned int max_threads);


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/06/29 20:28:48  kapustin
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* ALGO___NW_ALIGNER_THREADS_HPP */
