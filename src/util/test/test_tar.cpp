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
 * Author:  Vladimir Ivanov
 *
 * File Description:  Test program for the Compression API
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <util/compress/tar.hpp>
#include <test/test_assert.h>  // This header must go last


USING_NCBI_SCOPE;


//////////////////////////////////////////////////////////////////////////////
//
// Test application
//

class CTest : public CNcbiApplication
{
public:
    void Init(void);
    int  Run(void);
};


void CTest::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);
}


int CTest::Run(void)
{
    //CTar tar("e:\\tar\\f_cvf.tar");
    //CTar tar("e:\\tar\\f_cvf_dir.tar");
    //CTar tar("e:\\tar\\cmd.tar");
    //CTar tar("e:\\tar\\0021-9193-19910101_173.19-208369-1100877641.58635.tar");
    //CTar tar("e:\\tar\\0021-9193-19940101_176.12-205570-1100884818.17906.tar");

    //CNcbiIfstream is("e:\\tar\\cmd.tar",
    //    IOS_BASE::in | IOS_BASE::binary);
    CNcbiIfstream is("e:\\tar\\0021-9193-19940101_176.12-205570-1100884818.17906.tar",
        IOS_BASE::in | IOS_BASE::binary);
    CTar tar(is);

    //CTar tar("/home/ivanov/tar/0021-9193-19940101_176.12-205570-1100884818.17906.tar");
    
    //tar.Extract("e:\\tar\\out");
    tar.Extract("/home/ivanov/tar/out");

    return 0;
}



//////////////////////////////////////////////////////////////////////////////
//
// MAIN
//

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CTest().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2005/01/31 15:54:14  ivanov
 * Fixed path to tar.hpp
 *
 * Revision 1.1  2004/12/02 17:49:16  ivanov
 * Initial draft revision
 *
 * ===========================================================================
 */
