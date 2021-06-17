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
* Author:  Aaron Ucko, NCBI
*
* File Description:
*   Simple test of user-object conversion
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <serial/iterator.hpp>
#include <serial/objostrasn.hpp>
#include <serial/serial.hpp>

#include <objects/general/Date.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/uoconv.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

class CUOConvTestApp : public CNcbiApplication
{
public:
    int Run(void);
};

int CUOConvTestApp::Run(void)
{
    CObjectOStreamAsn  out(cout);
    CDate              date, date2;
    CRef<CUser_object> udate;

    date.SetToTime(CurrentTime());
    out << date;
    udate = PackAsUserObject(Begin(date));
    out << *udate;
    UnpackUserObject(*udate, Begin(date2));
    out << date2;

    return 0;
}

int main(int argc, char** argv)
{
    return CUOConvTestApp().AppMain(argc, argv);
}
