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
 * Author:  Christiam Camacho
 *
 */

/// @file blast_exception.hpp
/// Declares the BLAST exception class.

#ifndef ALGO_BLAST_API___BLAST_EXCEPTION__HPP
#define ALGO_BLAST_API___BLAST_EXCEPTION__HPP

#include <corelib/ncbiexpt.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Defines Blast specific exceptional error codes
class CBlastException : public CException
{
public:
    enum EErrCode {
        eInternal,
        eBadParameter,
        eOutOfMemory,
        eMemoryLimit,
        eNotSupported,
        eInvalidCharacter,
        eMaxErrCode
    };
    virtual const char* GetErrCodeString(void) const {
        switch ( GetErrCode() ) {
        case eInternal:
            return "Internal error";
        case eBadParameter:
            return "One or more parameters passed are invalid";
        case eOutOfMemory:
            return "Out of memory";
        case eMemoryLimit:
            return "Memory limit exceeded";
        case eInvalidCharacter:
            return "Sequence contains one or more invalid characters";
        case eNotSupported:
            return "This operation is not supported";
        default:
            return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CBlastException,CException);
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.10  2004/08/02 13:26:15  camacho
 * Minor
 *
 * Revision 1.9  2004/03/19 18:56:04  camacho
 * Move to doxygen AlgoBlast group
 *
 * Revision 1.8  2004/03/15 22:59:24  dondosha
 * Removed unnecessary function ThrowBlastException
 *
 * Revision 1.7  2004/03/13 00:27:04  dondosha
 * Added function to throw a CBlastException given code and message
 *
 * Revision 1.6  2004/03/11 17:11:25  dicuccio
 * Dropped export specifier (not needed on inline class)
 *
 * Revision 1.5  2003/12/09 12:40:22  camacho
 * Added windows export specifiers
 *
 * Revision 1.4  2003/11/26 18:22:14  camacho
 * +Blast Option Handle classes
 *
 * Revision 1.3  2003/08/19 13:45:21  dicuccio
 * Removed 'USING_SCOPE(objects)'.  Changed #include guards to be standards
 * compliant.  Added 'objects::' where necessary.
 *
 * Revision 1.2  2003/08/18 20:58:56  camacho
 * Added blast namespace, removed *__.hpp includes
 *
 * Revision 1.1  2003/07/10 18:34:19  camacho
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif  /* ALGO_BLAST_API___BLAST_EXCEPTION__HPP */
