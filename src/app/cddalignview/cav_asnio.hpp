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
*      template for reading/writing ASN data of any type from/to a file
*
* ===========================================================================
*/

#ifndef CAV_ASN_IO__HPP
#define CAV_ASN_IO__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbistre.hpp>
#include <serial/serial.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <serial/objostrasnb.hpp>

#include <string>


BEGIN_NCBI_SCOPE

// a utility function for reading different types of ASN data from a file
template < class ASNClass >
bool ReadASNFromIstream(CNcbiIstream& inStream, ASNClass& ASNobject, bool isBinary, string& err)
{
    auto_ptr<CObjectIStream> inObject;
    if (isBinary) {
        // Associate ASN.1 binary serialization methods with the input
        inObject.reset(new CObjectIStreamAsnBinary(inStream));
    } else {
        // Associate ASN.1 text serialization methods with the input
        inObject.reset(new CObjectIStreamAsn(inStream));
    }

    // Read the data
    try {
        err.erase();
        *inObject >> ASNobject;
    } catch (exception& e) {
        err = e.what();
        return false;
    }

    return true;
}

// for writing ASN data
template < class ASNClass >
bool WriteASNToFile(const char *filename, ASNClass& mime, bool isBinary, string& err)
{
    // initialize a binary output stream
    auto_ptr<CNcbiOstream> outStream;
    outStream.reset(new CNcbiOfstream(filename, IOS_BASE::out | IOS_BASE::binary));
    if (!(*outStream)) {
        err = "Cannot open file for writing";
        return false;
    }

    auto_ptr<CObjectOStream> outObject;
    if (isBinary) {
        // Associate ASN.1 binary serialization methods with the input
        outObject.reset(new CObjectOStreamAsnBinary(*outStream));
    } else {
        // Associate ASN.1 text serialization methods with the input
        outObject.reset(new CObjectOStreamAsn(*outStream));
    }

    // Read the CNcbi_mime_asn1 data
    try {
        *outObject << mime;
    } catch (exception& e) {
        err = e.what();
        return false;
    }

    return true;
}

END_NCBI_SCOPE

#endif // CAV_ASN_IO__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/03/19 05:33:43  thiessen
* move to src/app/cddalignview
*
* Revision 1.6  2003/02/03 17:52:03  thiessen
* move CVS Log to end of file
*
* Revision 1.5  2003/01/21 12:31:55  thiessen
* move includes into src dir
*
* Revision 1.4  2001/01/25 20:18:39  thiessen
* fix in-memory asn read/write
*
* Revision 1.3  2001/01/25 00:50:51  thiessen
* add command-line args; can read asn data from stdin
*
* Revision 1.2  2001/01/22 15:54:11  thiessen
* correctly set up ncbi namespacing
*
* Revision 1.1  2001/01/22 13:15:52  thiessen
* initial checkin
*
*/
