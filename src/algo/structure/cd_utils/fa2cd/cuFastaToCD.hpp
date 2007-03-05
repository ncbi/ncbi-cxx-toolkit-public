/*  $Id$
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
* Authors:  Chris Lanczycki
*
* File Description:
*           application to generate a CD from a mFASTA text file
*
* ===========================================================================
*/

#ifndef CU_FA2CD__HPP
#define CU_FA2CD__HPP

#include <map>
#include <list>
#include <vector>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiapp.hpp>

#include <algo/structure/cd_utils/cuCdFromFasta.hpp>
//#include "cuCdFromFasta.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(cd_utils);

//#define ERROR_MESSAGE_CL(s) ERR_POST(ncbi::Error << "CFeaturePatternMatch: " << s << '!')
//#define WARNING_MESSAGE_CL(s) ERR_POST(ncbi::Warning << "CFeaturePatternMatch: " << s)
//#define INFO_MESSAGE_CL(s) LOG_POST(s)
//#define TRACE_MESSAGE_CL(s) ERR_POST(ncbi::Trace << "CFeaturePatternMatch: " << s)
//#define THROW_MESSAGE_CL(str) throw ncbi::CException(__FILE__, __LINE__, NULL, ncbi::CException::eUnknown, (str))



// class for standalone application
class CFasta2CD : public ncbi::CNcbiApplication
{

public:

    CFasta2CD() {};

    ~CFasta2CD() {};

    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

private:

    ostream* m_outStream;
    CCdFromFasta* m_ccdFromFasta;

    bool OutputCD(const string& filename, string& err);
    string BuildOutputFilename(const CArgs& args);
    string BuildCDName(const CArgs& args);
    string BuildCDAccession(const CArgs& args);
    void PrintArgs(ostream& os);
    

};

#endif // CU_FA2CD__HPP
