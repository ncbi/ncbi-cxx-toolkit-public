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
        eInternal,
        eBadParameter,
        eMemoryLimit,
        eInvalidCharacter,
        eIncorrectSequenceOrder,
        eInvalidSpliceTypeIndex,
	eNoData,
	eNotInitialized,
        eFormat
    };
    virtual const char* GetErrCodeString(void) const {
        switch ( GetErrCode() ) {
        case eInternal:
            return "Internal error";
        case eBadParameter:
            return "One or more parameters passed are invalid";
        case eMemoryLimit:
            return "Memory limit exceeded";
        case eInvalidCharacter:
            return "Sequence contains one or more invalid characters";
        case eIncorrectSequenceOrder:
            return "mRna should go first";
        case eInvalidSpliceTypeIndex:
            return "Splice type index out of range";
	case eNoData:
	    return "No data available";
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


/*
 * ===========================================================================
 * $Log$
 * Revision 1.15  2004/08/19 12:42:26  dicuccio
 * Dropped unnecessary export specifier
 *
 * Revision 1.14  2004/04/23 14:39:22  kapustin
 * Add Splign librry and other changes
 *
 * Revision 1.10  2003/10/27 20:45:47  kapustin
 * Minor code cleanup
 *
 * Revision 1.9  2003/09/30 19:49:32  kapustin
 * Make use of standard score matrix interface
 *
 * Revision 1.8  2003/09/12 19:38:27  kapustin
 * Add eNoData subtype
 *
 * Revision 1.7  2003/09/02 22:28:44  kapustin
 * Get rid of CAlgoException
 *
 * Revision 1.5  2003/06/17 17:20:28  kapustin
 * CNWAlignerException -> CAlgoAlignException
 *
 * Revision 1.4  2003/04/10 19:04:27  siyan
 * Added doxygen support
 *
 * Revision 1.3  2003/03/25 22:07:09  kapustin
 * Add eInvalidSpliceTypeIndex exception type
 *
 * Revision 1.2  2003/03/14 19:15:28  kapustin
 * Add eMemoryLimit exception type
 *
 * Revision 1.1  2003/02/26 21:30:32  gouriano
 * modify C++ exceptions thrown by this library
 *
 *
 * ===========================================================================
 */

#endif  /* ALGO_ALIGN_EXCEPTION__HPP */
