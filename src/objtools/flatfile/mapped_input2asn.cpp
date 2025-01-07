/* $Id$
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
 * File Name: mapped_input2asn.cpp
 *
 * Author: Justin Foley
 *
 * File Description:
 *      Base class for generating ASN.1 from mapped input
 *
 */

#include <ncbi_pch.hpp>
#include <objtools/flatfile/flatfile_parse_info.hpp>
#include "mapped_input2asn.hpp" 

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CMappedInput2Asn::CMappedInput2Asn(Parser& parser) :
    mParser(parser) {
    mParser.curindx = 0;  
}

CRef<CSeq_entry> CMappedInput2Asn::operator()()
{
    auto pEntry = xGetEntry();
    while (!pEntry && (mParser.curindx < mParser.indx)) {
        pEntry = xGetEntry();
    }
    return pEntry;
}

END_NCBI_SCOPE
