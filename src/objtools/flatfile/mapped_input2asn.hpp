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
 *
 * Author: Justin Foley
 *
 * File Description:
 *      Base class for generating ASN from mapped input
 *
 */

#ifndef _MAPPED_INPUT2ASN_HPP_
#define _MAPPED_INPUT2ASN_HPP_
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE

class Parser;
class CObjectOStream;
namespace objects {
    class CSeq_entry;
}


class CMappedInput2Asn
{
public:
    CMappedInput2Asn(Parser& parser);
    CRef<objects::CSeq_entry> operator()();
    virtual ~CMappedInput2Asn(){}

    virtual void PostTotals() = 0;
protected:
    virtual CRef<objects::CSeq_entry> xGetEntry() = 0;
    struct STotals 
    {
        size_t Succeeded{0};
        size_t Long{0};
        size_t Dropped{0};
    };

    Parser& mParser;
    STotals mTotals;
};


END_NCBI_SCOPE
#endif
