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
*      templates for converting ASN data of any type from/to a file
*
* ===========================================================================
*/

#ifndef CN3D_ASN_CONVERTER__HPP
#define CN3D_ASN_CONVERTER__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbistre.hpp>
#include <serial/serial.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasnb.hpp>

#include <string>
#include <memory>

// C stuff
#include <stdio.h>
#include <asn.h>
#include <ncbibs.h>

#ifndef NS_AUTO_PTR
#ifdef HAVE_NO_AUTO_PTR
#define NS_AUTO_PTR ncbi::auto_ptr
#else
#define NS_AUTO_PTR std::auto_ptr
#endif
#endif


BEGIN_SCOPE(Cn3D)

// a utility function for converting ASN data structures from C to C++
template < class ASNClass >
bool ConvertAsnFromCToCPP(Pointer from, AsnWriteFunc writeFunc, ASNClass *to, std::string *err)
{
    err->erase();
    Nlm_ByteStorePtr bsp = NULL;
    AsnIoBSPtr aibp = NULL;
    char *asnDataBlock = NULL;
    bool retval = false;

    try {
        bsp = Nlm_BSNew(1024);
        aibp = AsnIoBSOpen("wb", bsp);
        if (!bsp || !aibp) throw "AsnIoBS creation failed";

        if (!((*writeFunc)(from, aibp->aip, NULL))) throw "C object -> AsnIoBS failed";

        AsnIoBSClose(aibp);
        aibp = NULL;
        int dataSize = Nlm_BSLen(bsp);
        asnDataBlock = new char[dataSize];
        if (!asnDataBlock) throw "block allocation failed";

        Nlm_BSSeek(bsp, 0, 0);
        if (Nlm_BSRead(bsp, (void *) asnDataBlock, dataSize) != dataSize)
            throw "AsnIoBS -> datablock failed";
        Nlm_BSFree(bsp);
        bsp = NULL;

        ncbi::CNcbiIstrstream asnIstrstream(asnDataBlock, dataSize);
        ncbi::CObjectIStreamAsnBinary objIstream(asnIstrstream);
        ncbi::SetDiagTrace(ncbi::eDT_Disable);
        objIstream >> *to;
        retval = true;

    } catch (const char *msg) {
        *err = msg;
    } catch (std::exception& e) {
        *err = std::string("uncaught exception: ") + e.what();
    }
    ncbi::SetDiagTrace(ncbi::eDT_Default);

    if (asnDataBlock) delete[] asnDataBlock;
    if (aibp) AsnIoBSClose(aibp);
    if (bsp) Nlm_BSFree(bsp);
    return retval;
}

// a utility function for converting ASN data structures from C++ to C
template < class ASNClass >
Pointer ConvertAsnFromCPPToC(const ASNClass& from, AsnReadFunc readFunc, std::string *err)
{
    err->erase();
    Pointer cObject = NULL;
    AsnIoMemPtr aimp = NULL;

    try {
        ncbi::CNcbiOstrstream asnOstrstream;
        ncbi::CObjectOStreamAsnBinary objOstream(asnOstrstream);
        ncbi::SetDiagTrace(ncbi::eDT_Disable);
        objOstream << from;

        NS_AUTO_PTR<char> strData(asnOstrstream.str()); // to make sure data gets freed
        aimp = AsnIoMemOpen("rb", (unsigned char *) asnOstrstream.str(), asnOstrstream.pcount());
        if (!aimp || !(cObject = (*readFunc)(aimp->aip, NULL)))
            throw "AsnIoMem -> C object failed";

    } catch (const char *msg) {
        *err = msg;
    } catch (std::exception& e) {
        *err = std::string("uncaught exception: ") + e.what();
    }
    ncbi::SetDiagTrace(ncbi::eDT_Default);

    if (aimp) AsnIoMemClose(aimp);
    return cObject;
}

// create a new copy of a C++ ASN data object
template < class ASNClass >
ASNClass * CopyASNObject(const ASNClass& originalObject, std::string *err)
{
    err->erase();
    NS_AUTO_PTR<ASNClass> newObject;

    try {
        newObject.reset(new ASNClass());
        newObject->Assign(originalObject);
    } catch (std::exception& e) {
        *err = e.what();
        return NULL;
    }

    return newObject.release();
}

END_SCOPE(Cn3D)

#endif // CN3D_ASN_CONVERTER__HPP


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.12  2004/05/27 19:43:24  thiessen
* prefix diag trace stuff with ncbi::
*
* Revision 1.11  2003/08/22 14:33:38  thiessen
* remove static decl from templates
*
* Revision 1.10  2003/03/19 14:43:49  thiessen
* disable trace messages in object loaders for now
*
* Revision 1.9  2003/02/03 19:20:00  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.8  2002/10/08 12:35:42  thiessen
* use delete[] for arrays
*
* Revision 1.7  2002/06/12 18:45:41  thiessen
* more linux/gcc fixes
*
* Revision 1.6  2002/06/11 16:27:16  thiessen
* use ncbi::auto_ptr
*
* Revision 1.5  2002/06/11 13:18:47  thiessen
* fixes for gcc 3
*
* Revision 1.4  2002/01/24 18:12:33  thiessen
* fix another leak
*
* Revision 1.3  2002/01/23 21:08:37  thiessen
* fix memory leak
*
* Revision 1.2  2001/09/19 22:55:43  thiessen
* add preliminary net import and BLAST
*
* Revision 1.1  2001/09/18 03:09:38  thiessen
* add preliminary sequence import pipeline
*
*/
