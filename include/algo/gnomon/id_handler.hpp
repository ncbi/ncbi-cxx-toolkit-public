#ifndef ALGO_GNOMON___IDHANDLER__HPP
#define ALGO_GNOMON___IDHANDLER__HPP

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
 * Authors:  Vyacheslav Chetvernin
 *
 * File Description:
 *
 */

#include <corelib/ncbistl.hpp>
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
class CSeq_id;
class CScope;
END_SCOPE(objects)
USING_SCOPE(objects);

BEGIN_SCOPE(gnomon)

class CIdHandler {
public:
    CIdHandler(CScope& scope);

    static string ToString(const CSeq_id& id);

    static CRef<CSeq_id> ToSeq_id(const string& str);

    static CRef<CSeq_id> GnomonMRNA(Int8 id);
    static CRef<CSeq_id> GnomonProtein(Int8 id);

    CConstRef<CSeq_id> ToCanonical(const CSeq_id& id) const;

private:
    CScope& m_Scope;
};

END_SCOPE(gnomon)
END_NCBI_SCOPE

#endif  // ALGO_GNOMON___IDHANDLER__HPP
