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
*      templates for reading/writing ASN data from/to a file, or copying
*
* ===========================================================================
*/

#ifndef CN3D_ASN_READER__HPP
#define CN3D_ASN_READER__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbistre.hpp>
#include <serial/serial.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <serial/objostrasnb.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>

#include <string>
#include <memory>

#ifndef NS_AUTO_PTR
#ifdef HAVE_NO_AUTO_PTR
#define NS_AUTO_PTR ncbi::auto_ptr
#else
#define NS_AUTO_PTR std::auto_ptr
#endif
#endif


BEGIN_SCOPE(Cn3D)

// a utility function for reading different types of ASN data from a file
template < class ASNClass >
bool ReadASNFromFile(const char *filename, ASNClass *ASNobject, bool isBinary, std::string *err)
{
    err->erase();

    // initialize the binary input stream
    ncbi::CNcbiIfstream inStream(filename, IOS_BASE::in | IOS_BASE::binary);
    if (!inStream) {
        *err = "Cannot open file for reading";
        return false;
    }

    NS_AUTO_PTR<ncbi::CObjectIStream> inObject;
    if (isBinary) {
        // Associate ASN.1 binary serialization methods with the input
        inObject.reset(new ncbi::CObjectIStreamAsnBinary(inStream));
    } else {
        // Associate ASN.1 text serialization methods with the input
        inObject.reset(new ncbi::CObjectIStreamAsn(inStream));
    }

    // Read the asn data
    bool okay = true;
    ncbi::SetDiagTrace(ncbi::eDT_Disable);
    try {
        *inObject >> *ASNobject;
    } catch (std::exception& e) {
        *err = e.what();
        okay = false;
    }
    ncbi::SetDiagTrace(ncbi::eDT_Default);

    inStream.close();
    return okay;
}

// for writing ASN data
template < class ASNClass >
bool WriteASNToFile(const char *filename, const ASNClass& ASNobject, bool isBinary,
    std::string *err, ncbi::EFixNonPrint fixNonPrint = ncbi::eFNP_Default)
{
    err->erase();

    // initialize a binary output stream
    ncbi::CNcbiOfstream outStream(filename,
        isBinary ? (IOS_BASE::out | IOS_BASE::binary) : IOS_BASE::out);
    if (!outStream) {
        *err = "Cannot open file for writing";
        return false;
    }

    NS_AUTO_PTR<ncbi::CObjectOStream> outObject;
    if (isBinary) {
        // Associate ASN.1 binary serialization methods with the input
        outObject.reset(new ncbi::CObjectOStreamAsnBinary(outStream, fixNonPrint));
    } else {
        // Associate ASN.1 text serialization methods with the input
        outObject.reset(new ncbi::CObjectOStreamAsn(outStream, fixNonPrint));
    }

    // write the asn data
    bool okay = true;
    ncbi::SetDiagTrace(ncbi::eDT_Disable);
    try {
        *outObject << ASNobject;
        outStream.flush();
    } catch (std::exception& e) {
        *err = e.what();
        okay = false;
    }
    ncbi::SetDiagTrace(ncbi::eDT_Default);

    outStream.close();
    return okay;
}

// for loading (binary) ASN data via HTTP connection
template < class ASNClass >
bool GetAsnDataViaHTTP(
    const std::string& host, const std::string& path, const std::string& args,
    ASNClass *asnObject, std::string *err,
    bool binaryData = true, unsigned short port = 80)
{
    err->erase();
    bool okay = false;

    // set up registry field to set GET connection method for HTTP
    ncbi::CNcbiRegistry* reg = new ncbi::CNcbiRegistry;
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_DEBUG_PRINTOUT, "FALSE");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_REQ_METHOD,     "GET");
    CORE_SetREG(REG_cxx2c(reg, true));

    try {
        // load data from stream using given URL params
        ncbi::CConn_HttpStream httpStream(host, path, args, kEmptyStr, port);
        NS_AUTO_PTR<ncbi::CObjectIStream> inObject;
        if (binaryData)
            inObject.reset(new ncbi::CObjectIStreamAsnBinary(httpStream));
        else
            inObject.reset(new ncbi::CObjectIStreamAsn(httpStream));
        ncbi::SetDiagTrace(ncbi::eDT_Disable);
        *inObject >> *asnObject;
        okay = true;

    } catch (std::exception&) {
        *err = "Network connection failed or data is not in expected format";
    }
    ncbi::SetDiagTrace(ncbi::eDT_Default);

    CORE_SetREG(NULL);
    return okay;
}

END_SCOPE(Cn3D)

#endif // CN3D_ASN_READER__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.20  2004/06/28 19:31:38  thiessen
* more user-friendly error message
*
* Revision 1.19  2004/05/25 17:48:19  ucko
* Qualify diag-trace manipulation with ncbi::.
*
* Revision 1.18  2004/01/27 22:56:55  thiessen
* remove auto_ptr from stream objects
*
* Revision 1.17  2003/10/20 16:53:54  thiessen
* flush output after writing
*
* Revision 1.16  2003/08/22 14:33:38  thiessen
* remove static decl from templates
*
* Revision 1.15  2003/03/19 14:43:50  thiessen
* disable trace messages in object loaders for now
*
* Revision 1.14  2003/02/03 19:20:00  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.13  2002/10/25 19:00:02  thiessen
* retrieve VAST alignment from vastalign.cgi on structure import
*
* Revision 1.12  2002/06/12 18:45:41  thiessen
* more linux/gcc fixes
*
* Revision 1.11  2002/06/11 16:27:16  thiessen
* use ncbi::auto_ptr
*
* Revision 1.10  2002/06/11 13:18:47  thiessen
* fixes for gcc 3
*
* Revision 1.9  2002/05/29 23:58:06  thiessen
* use text mode ostream for ascii asn
*
* Revision 1.8  2001/09/27 20:58:14  thiessen
* add VisibleString filter option
*
* Revision 1.7  2001/09/24 13:16:53  thiessen
* fix wxPanel issues
*
* Revision 1.6  2001/09/24 11:34:35  thiessen
* add missing ncbi::
*
* Revision 1.5  2001/09/19 22:55:43  thiessen
* add preliminary net import and BLAST
*
* Revision 1.4  2001/09/18 03:09:38  thiessen
* add preliminary sequence import pipeline
*
* Revision 1.3  2001/07/12 17:34:22  thiessen
* change domain mapping ; add preliminary cdd annotation GUI
*
* Revision 1.2  2000/12/22 19:25:46  thiessen
* write cdd output files
*
* Revision 1.1  2000/12/21 23:42:24  thiessen
* load structures from cdd's
*
*/
