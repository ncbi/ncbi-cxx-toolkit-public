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
 *      Write hooks for flatfile parser
 *
 */
#ifndef _FLAT2ASN_WRITER_HPP_
#define _FLAT2ASN_WRITER_HPP_

#include <corelib/ncbiobj.hpp>
#include <serial/objostr.hpp>
#include <serial/objectio.hpp>
#include <functional>

BEGIN_NCBI_SCOPE

class CObjectOStream;
namespace objects {
    class CSeq_entry;
    class CBioseq_set;
    class CSeq_submit;
};

class CFlat2AsnWriter
{
public:
    using FGetEntry = function<CRef<objects::CSeq_entry>(void)>;

    CFlat2AsnWriter(CObjectOStream& objOstr)
        : m_ObjOstr(objOstr) {}
    void Write(const objects::CBioseq_set* pBioseqSet, FGetEntry getEntry);
    void Write(const objects::CSeq_submit* pSubmit, FGetEntry getEntry);
private:
    CObjectOStream& m_ObjOstr;
};

END_NCBI_SCOPE

#endif
