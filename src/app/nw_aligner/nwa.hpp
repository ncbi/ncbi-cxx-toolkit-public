#ifndef ALGO_ALIGN_NW__NWA_APP__HPP
#define ALGO_ALIGN_NW__NWA_APP__HPP

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
* File Description:  Global alignment application class definition
*                   
* ===========================================================================
*/

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <algo/align/nw/align_exception.hpp>


BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CSeq_id;
END_SCOPE(objects)

// Exceptions
//

class CAppNWAException : public CException 
{
public:

    enum EErrCode {
        eCannotReadFile,
        eCannotWriteFile,
        eInconsistentParameters,
	eNotSupported
    };

    virtual const char* GetErrCodeString(void) const {
        switch ( GetErrCode() ) {
        case eCannotReadFile:
            return "Cannot read from file";
        case eCannotWriteFile:
            return "Cannot write to file";
        case eInconsistentParameters:
            return "Two or more parameters are inconsistent";
	case eNotSupported:
	    return "Feature not supported";
        default:
            return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CAppNWAException, CException);
};


// application class
//

class CAppNWA : public CNcbiApplication
{
public:

    virtual void Init();
    virtual int  Run();
    virtual void Exit();

private:

    void                   x_RunOnPair() const;
    CRef<objects::CSeq_id> x_ReadFastaFile(const string& filename,
                                           vector<char>* sequence) const;
};


END_NCBI_SCOPE

#endif
