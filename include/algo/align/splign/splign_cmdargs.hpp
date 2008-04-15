#ifndef ALGO_ALIGN_SPLIGN_CMDARGS__HPP
#define ALGO_ALIGN_SPLIGN_CMDARGS__HPP

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
* Author:  Yuri Kapustin
*          Anatoliy Kuznetsov
*
* File Description:
*   Splign command line argument utilities
*/

#include <corelib/ncbiargs.hpp>
#include <algo/align/splign/splign.hpp>


BEGIN_NCBI_SCOPE



class NCBI_XALGOALIGN_EXPORT CSplignArgUtil
{
public:
    /// Setup core splign argument descriptions for the application
    static
    void SetupArgDescriptions(CArgDescriptions* arg_desc);

    /// Translate command line arguments into splign algorithm core settings
    static
    void ArgsToSplign(CSplign* splign, const CArgs& args);
};

#ifdef ALGOALIGN_NW_SPLIGN_MAKE_PUBLIC_BINARY
  const string kQueryType_mRNA ("mrna");
  const string kQueryType_EST  ("est");
#endif


END_NCBI_SCOPE

#endif
