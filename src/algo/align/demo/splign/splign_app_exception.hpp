#ifndef ALGO_ALIGN_SPLIGN_SPLIGNAPP_EXCEPTION__HPP
#define ALGO_ALIGN_SPLIGN_SPLIGNAPP_EXCEPTION__HPP

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
*   CSplignApp exceptions
*
*/

#include <corelib/ncbiexpt.hpp>


BEGIN_NCBI_SCOPE


class CSplignAppException : public CException
{
public:
    enum EErrCode {
        eInternal,
        eBadParameter,
        eCannotOpenFile,
	eCannotReadFile,
	eErrorReadingIndexFile,
	eBadData,
	eNoHits,
	eGeneral
    };
    virtual const char* GetErrCodeString(void) const {
        switch ( GetErrCode() ) {
        case eInternal:
            return "Internal error";
        case eBadParameter:
            return "One or more parameters are invalid";
        case eCannotOpenFile:
            return "Unable to open file";
        case eCannotReadFile:
            return "Unable to read file";
        case eErrorReadingIndexFile:
            return "Error encountered while reading index file";
        case eBadData:
            return "Error in input data";
        case eNoHits:
            return "Default Blast returned no hits";
        case eGeneral:
            return "General exception";
        default:
            return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CSplignAppException, CException);
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2004/11/05 15:54:01  kapustin
 * Remove unused exception classes
 *
 * Revision 1.4  2004/06/07 13:47:37  kapustin
 * Throw when no hits returned from Blast
 *
 * Revision 1.3  2004/03/01 20:07:26  kapustin
 * Add eBadData type
 *
 * Revision 1.2  2003/11/05 20:25:17  kapustin
 * Update exception code list for splign app exception
 *
 * Revision 1.1  2003/10/30 19:37:20  kapustin
 * Initial toolkit revision
 *
  * ===========================================================================
 */

#endif
