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
* File Name:  $Id$
*
* File Description: Implementation of dbapi language call
* $Log$
* Revision 1.3  2004/05/18 18:24:52  gorelenk
* Added PCH - <ncbi_pch.hpp> .
*
* Revision 1.2  2002/07/18 19:51:04  starchen
* fixed some error
*
* Revision 1.1  2002/07/18 15:49:39  starchen
* first entry
*
*
*============================================================================
*/

#include <ncbi_pch.hpp>

// the following function  do not describe any DBAPI usage

 char* getParam(char tag, int argc, char* argv[], bool* flag)
{
    for(int i= 1; i < argc; i++) {
        if(((*argv[i] == '-') || (*argv[i] == '/')) && 
           (*(argv[i]+1) == tag)) { // tag found
            if(flag) *flag= true;
            if(*(argv[i]+2) == '\0')  { // tag is a separate arg
                if(i == argc - 1) return 0;
                if(*argv[i+1] != *argv[i]) return argv[i+1];
                else return 0;
            }
            else return argv[i]+2;
        }
    }
    if(flag) *flag= false;
    return 0;
}

