#ifndef ALGO_ALIGN_UTIL_MESSAGES_HPP
#define ALGO_ALIGN_UTIL_MESSAGES_HPP

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
*
* File Description:
*   Common text messages
*
*/

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

class CSimpleMessager 
{
public:

    enum EMsgCode {
        eFailedToInitFromString,
        eIncorrectSeqAlignDim,
        eIncorrectSeqAlignType,
        eUnexpectedStrand,
        eImproperTemplateArgument,
        eArgOutOfRange
    };

    static const char* s_GetMsg (EMsgCode code) {

        switch(code) {

            case eFailedToInitFromString:
                return "Failed to init CAlignShadow from the string: ";

            case eIncorrectSeqAlignDim:
                return "Incorrect seq-align dimension";

            case eIncorrectSeqAlignType:
                return "Incorrect seq-align type (dense-seg expected)";

            case eUnexpectedStrand:
                return "Unexpected strand reported";

            case eImproperTemplateArgument:
                return "Invoked with improper template argument";

            case eArgOutOfRange:
                return "ArgumentOutOfRange";

            default:
                return "";
        };
    }
};



#define STATIC_MSG(Choice) \
        CSimpleMessager::s_GetMsg(CSimpleMessager::Choice)


END_NCBI_SCOPE

/*
 * $Log$
 * Revision 1.2  2004/12/22 22:14:18  kapustin
 * Move static messages to CSimpleMessager to satisfy Solaris/forte6u2
 *
 * Revision 1.1  2004/12/21 20:07:47  kapustin
 * Initial revision
 *
 */

#endif
