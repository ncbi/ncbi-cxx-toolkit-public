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
* Authors:  Paul Thiessen
*
* File Description:
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/06/27 20:09:39  thiessen
* initial checkin
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/serial.hpp>            
#include <serial/objistrasnb.hpp>       
#include <objects/ncbimime/Ncbi_mime_asn1.hpp>

#include "cn3d/main.hpp" 
#include "cn3d/structure_set.hpp"

USING_NCBI_SCOPE;
using namespace objects;
using namespace Cn3D;

int CCn3DApp::Run() {

    // initialize the binary input stream 
    auto_ptr<CNcbiIstream> inStream;
    inStream.reset(new CNcbiIfstream("input.asn1", IOS_BASE::in | IOS_BASE::binary));
    if ( !*inStream )
      ERR_POST(Fatal << "Cannot open input file");

    // Associate ASN.1 binary serialization methods with the input 
    auto_ptr<CObjectIStream> inObject;
    inObject.reset(new CObjectIStreamAsnBinary(*inStream));

    // Read the CNcbi_mime_asn1 data 
    CNcbi_mime_asn1 mime;
    *inObject >> mime;

    // Create StructureSet from mime data
    StructureSet strucSet = StructureSet(mime);

    //strucSet.Draw();

    return 0;
}

int main(int argc, const char* argv[]) 
{
#ifdef _DEBUG
    // Initialize the diagnostic stream 
    CNcbiOfstream diag("Cn3D.log");
    SetDiagStream(&diag);
#endif

    // Run the application 
    CCn3DApp theApp;
    return theApp.AppMain(argc, argv);
}
