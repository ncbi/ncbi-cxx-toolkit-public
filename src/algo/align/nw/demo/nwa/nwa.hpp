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
* File Description:  NWA application class definition
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

    void x_RunOnPair() const
        THROWS((CAppNWAException, CAlgoAlignException));

    CRef<objects::CSeq_id> x_ReadFastaFile(const string& filename,
                                                 vector<char>* sequence) const;
};


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2005/02/23 16:59:38  kapustin
 * +CNWAligner::SetTranscript. Use CSeq_id's instead of strings in CNWFormatter. Modify CNWFormatter::AsSeqAlign to allow specification of alignment's starts and strands.
 *
 * Revision 1.1  2004/12/16 22:38:08  kapustin
 * Move to algo/align/nw/demo/nwa
 *
 * Revision 1.9  2004/05/17 14:50:57  kapustin
 * Add/remove/rearrange some includes and object declarations
 *
 * Revision 1.8  2004/04/30 12:49:47  kuznets
 * throw -> THROWS (fixes warning in MSVC7)
 *
 * Revision 1.7  2003/09/10 19:11:50  kapustin
 * Add eNotSupported exception for multithreading availability checking
 *
 * Revision 1.6  2003/06/17 17:20:44  kapustin
 * CNWAlignerException -> CAlgoAlignException
 *
 * Revision 1.5  2003/01/24 16:49:36  kapustin
 * Add write file exception type
 *
 * Revision 1.4  2003/01/08 15:58:33  kapustin
 * Read offset parameter from fasta reading routine
 *
 * Revision 1.3  2002/12/12 17:59:30  kapustin
 * Enable spliced alignments
 *
 * Revision 1.2  2002/12/09 15:47:36  kapustin
 * Declare exception class before the application
 *
 * Revision 1.1  2002/12/06 17:44:26  ivanov
 * Initial revision
 *
 * ===========================================================================
 */


#endif
