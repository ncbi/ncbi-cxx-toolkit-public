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
 * Author:  Vasilchenko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#if NCBI_OS_MSWIN
# include <stdio.h>
#else
# include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <test/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;

void Remove(const string& file)
{
    unlink(file.c_str());
}

bool Exists(const string& file)
{
    struct stat st;
    if ( stat(file.c_str(), &st) == 0 )
        return true;
    if ( errno == ENOENT )
        return false;
    ERR_POST("Unexpected errno after stat(): " << errno);
    return false;
}

long FileSize(const string& file)
{
    struct stat st;
    if ( stat(file.c_str(), &st) == 0 )
        return st.st_size;
    ERR_POST("Cannot get size of file: " << errno);
    return 0;
}

int main()
{
    SetDiagStream(&NcbiCerr);
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostPrefix("test_overwrite");
    
    string fileName = "test_file.tmp";
    Remove(fileName);
    if ( Exists(fileName) ) {
        ERR_POST("cannot remove " << fileName);
        return 1;
    }
    {
        // write data
        CNcbiOfstream o(fileName.c_str(), IOS_BASE::app);
        if ( !o ) {
            ERR_POST("Cannot create file " << fileName);
            return 1;
        }
        o.seekp(0, IOS_BASE::end);
        if ( streampos(o.tellp()) != streampos(0) ) {
            ERR_POST("New file returns non zero size" << streampos(o.tellp()));
            return 1;
        }
        o << "TEST";
    }
    if ( FileSize(fileName) != 4 ) {
        ERR_POST("Wrong file size after write" << FileSize(fileName));
        return 1;
    }
    {
        // try to overwrite data
        CNcbiOfstream o(fileName.c_str(), IOS_BASE::app);
        if ( !o ) {
            ERR_POST("Cannot open file " << fileName);
            return 1;
        }
        o.seekp(0, IOS_BASE::end);
        if ( streampos(o.tellp()) == streampos(0) ) {
            ERR_POST("Non empty file returns zero size");
            return 1;
        }
    }
    if ( FileSize(fileName) != 4 ) {
        ERR_POST("File size was changed after test" << FileSize(fileName));
        return 1;
    }
    Remove(fileName);
    ERR_POST(Info << "Test passed successfully");
    return 0;
}


/*
 * ===========================================================================
 * $Log$
 * Revision 6.4  2004/05/14 13:59:51  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 6.3  2002/04/16 18:49:06  ivanov
 * Centralize threatment of assert() in tests.
 * Added #include <test/test_assert.h>. CVS log moved to end of file.
 *
 * ==========================================================================
 */
