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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2001/09/18 03:09:38  thiessen
* add preliminary sequence import pipeline
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

// C stuff
#include <stdio.h>
#include <asn.h>
#include <ncbibs.h>


BEGIN_SCOPE(Cn3D)

// a utility function for converting ASN data structures from C to C++
template < class ASNClass >
static bool ConvertAsnFromCToCPP(Pointer from, AsnWriteFunc writeFunc, ASNClass *to, std::string *err)
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

        if (!((*writeFunc)(from, aibp->aip, NULL))) throw "C data -> AsnIoBS failed";

        AsnIoBSClose(aibp);
        aibp = NULL;
        int dataSize = Nlm_BSLen(bsp);
        char *asnDataBlock = new char[dataSize];
        if (!asnDataBlock) throw "block allocation failed";

        Nlm_BSSeek(bsp, 0, 0);
        if (Nlm_BSRead(bsp, (void *) asnDataBlock, dataSize) != dataSize)
            throw "AsnIoBS -> datablock failed";

        CNcbiIstrstream asnIstrstream(asnDataBlock, dataSize);
        CObjectIStreamAsnBinary objIstream(asnIstrstream);
        objIstream >> *to;
        retval = true;

    } catch (const char *msg) {
        *err = msg;
    } catch (exception& e) {
        *err = std::string("uncaught exception: ") + e.what();
    }

    if (asnDataBlock) delete asnDataBlock;
    if (aibp) AsnIoBSClose(aibp);
    if (bsp) Nlm_BSFree(bsp);
    return retval;
}

END_SCOPE(Cn3D)

#endif // CN3D_ASN_CONVERTER__HPP
