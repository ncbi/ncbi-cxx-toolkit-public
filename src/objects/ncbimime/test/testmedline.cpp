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
* Author: Eugene Vasilchenko
*
* File Description:
*   testmedline test program for checking size of application which uses
*   generated objects.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.6  2000/04/12 15:38:05  vasilche
* Added copyright header.
* testmedline code moved out of ncbi namespace.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/serial.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objostrasn.hpp>

#define Ncbi_mime_asn11 0
#define Medline1 1

#if Ncbi_mime_asn11
#include <objects/ncbimime/Ncbi_mime_asn1.hpp>
#define Struct1 CNcbi_mime_asn1
#define Module1 ncbimime
#define File1 "ncbimime"
#endif
#if Medline1
#include <objects/medline/Medline_entry.hpp>
#define Struct1 CMedline_entry
#define Module1 medline
#define File1 "medline"
#endif
#include <objects/medlars/Medlars_entry.hpp>
#define Struct2 CMedlars_entry
#define Module2 medlars
#define File2 "medlars"


USING_NCBI_SCOPE;
using namespace objects;

int main(void)
{
    SetDiagStream(&NcbiCerr);

    Struct1 object1;
    try {
        CNcbiIfstream in(File1 ".ent");
        CObjectIStreamAsn objIn(in);
        objIn >> object1;
    }
    catch (exception& exc) {
        ERR_POST("Exception: " << exc.what());
    }
    try {
        CNcbiOfstream out(File1 ".out");
        CObjectOStreamAsn objOut(out);
        objOut << object1;
    }
    catch (exception& exc) {
        ERR_POST("Exception: " << exc.what());
    }
    Struct2 object2;
    try {
        CNcbiIfstream in(File2 ".ent");
        CObjectIStreamAsn objIn(in);
        objIn >> object2;
    }
    catch (exception& exc) {
        ERR_POST("Exception: " << exc.what());
    }
    try {
        CNcbiOfstream out(File2 ".out");
        CObjectOStreamAsn objOut(out);
        objOut << object2;
    }
    catch (exception& exc) {
        ERR_POST("Exception: " << exc.what());
    }
}
