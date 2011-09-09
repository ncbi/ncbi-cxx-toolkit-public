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
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>

#include <algo/gnomon/id_handler.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

USING_SCOPE(objects);

CIdHandler::CIdHandler(CScope& scope)
    : m_Scope(scope)
{
}

CConstRef<CSeq_id> CIdHandler::ToCanonical(const CSeq_id& id) const
{
    CConstRef<CSeq_id> canonical_id;
    try {
        CSeq_id_Handle idh = sequence::GetId(id, m_Scope, sequence::eGetId_Canonical | sequence::eGetId_ThrowOnError);
        canonical_id = idh.GetSeqId();
    } catch (sequence::CSeqIdFromHandleException& e) {
        if (e.GetErrCode() != sequence::CSeqIdFromHandleException::eRequestedIdNotFound)
            throw;
        canonical_id.Reset(&id);
    }
    return canonical_id;
}

string CIdHandler::ToString(const CSeq_id& id)
{
    return id.AsFastaString();
}

CRef<CSeq_id> CIdHandler::ToSeq_id(const string& str)
{
    return CRef<CSeq_id>(new CSeq_id(str));
}

CRef<CSeq_id> CIdHandler::GnomonMRNA(Int8 id)
{
    return CRef<CSeq_id>(new CSeq_id("gnl|GNOMON|" + NStr::Int8ToString(id) + ".m"));
}

CRef<CSeq_id> CIdHandler::GnomonProtein(Int8 id)
{
    return CRef<CSeq_id>(new CSeq_id("gnl|GNOMON|" + NStr::Int8ToString(id) + ".p"));
}

END_SCOPE(gnomon)
END_NCBI_SCOPE
