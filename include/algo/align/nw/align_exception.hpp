#ifndef ALGO_ALIGN_EXCEPTION__HPP
#define ALGO_ALIGN_EXCEPTION__HPP

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
* Author:  Yuri Kapustin, Andrei Gourianov
*
* File Description:
*   Algo library exceptions
*
*/

#include <corelib/ncbiexpt.hpp>


/** @addtogroup AlgoAlignExcep
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class CAlgoAlignException : public CException
{
public:
    enum EErrCode {
        eInternal = 100,
        eBadParameter,
        eInvalidMatrix,
        eMemoryLimit,
        eInvalidCharacter,
        eIncorrectSequenceOrder,
        eInvalidSpliceTypeIndex,
        eIntronTooLong,
	eNoSeqData,
	ePattern,
	eNoHits,
	eNoAlignment,
	eNotInitialized,
        eFormat
    };
    virtual const char* GetErrCodeString(void) const {
        switch ( GetErrCode() ) {
        case eInternal:
            return "Internal error";
        case eBadParameter:
            return "One or more parameters passed are invalid";
        case eInvalidMatrix:
            return "Invalid score matrix";
        case eMemoryLimit:
            return "Memory limit exceeded";
        case eInvalidCharacter:
            return "Sequence contains one or more invalid characters";
        case eIncorrectSequenceOrder:
            return "mRna should go first";
        case eInvalidSpliceTypeIndex:
            return "Splice type index out of range";
        case eIntronTooLong:
            return "Max supported intron length exceeded";
	case eNoSeqData:
	    return "No sequence data available";
	case ePattern:
	    return "Problem with the hit pattern";
	case eNoHits:
	    return "Zero hit count";
	case eNoAlignment:
	    return "No alignment found";
        case eNotInitialized:
            return "Object not properly initialized";
        case eFormat:
            return "Unexpected format";
        default:
            return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CAlgoAlignException, CException);
};


END_NCBI_SCOPE


/* @} */

#endif  /* ALGO_ALIGN_EXCEPTION__HPP */
