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
 * Authors:  Frank Ludwig
 *
 * File Description:  Write gff file
 *
 */

#include <ncbi_pch.hpp>

#include <objmgr/util/sequence.hpp>

#include <objtools/writers/writer_exception.hpp>
#include <objtools/writers/genbank_id_resolve.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
CGenbankIdResolve::CGenbankIdResolve():
    mThrowOnUnresolvedGi(false),
    mLabelType(CSeq_id::eContent)
//  ----------------------------------------------------------------------------
{}

//  ----------------------------------------------------------------------------
CGenbankIdResolve::~CGenbankIdResolve()
//  ----------------------------------------------------------------------------
{}

//  ----------------------------------------------------------------------------
CGenbankIdResolve&
CGenbankIdResolve::Get()
//  ----------------------------------------------------------------------------
{
    static CGenbankIdResolve resolver;
    return resolver;
}

//  ----------------------------------------------------------------------------
bool CGenbankIdResolve::GetBestId(
    CSeq_id_Handle idh,
    CScope& scope,
    string& best_id)
//  ----------------------------------------------------------------------------
{
    if (!idh) {
        return false;
    }
    CSeq_id_Handle best_idh = sequence::GetId(idh, scope, sequence::eGetId_Best);
    if (!best_idh) {
        best_idh = idh;
    }
    if (best_idh.IsGi()  &&  mThrowOnUnresolvedGi) {
        // we are not supposed to let any GI numbers see the light of day:
        string msg("Unable to resolve GI number ");
        msg += NStr::NumericToString(best_idh.GetGi());
        NCBI_THROW(CObjWriterException, eBadInput, msg);
    }
    string backup = best_id;
    try {
        best_idh.GetSeqId()->GetLabel(&best_id, mLabelType);
    }
    catch (...) {
        best_id = backup;
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGenbankIdResolve::GetBestId(
    const CMappedFeat& mf,
    string& best_id)
//  ----------------------------------------------------------------------------
{
    CSeq_id_Handle idh = mf.GetLocationId();
    if (idh) {
        return GetBestId(idh, mf.GetScope(), best_id);
    }
    const CSeq_loc& loc = mf.GetLocation();
    idh = sequence::GetIdHandle(loc, &mf.GetScope());
    return  GetBestId(idh, mf.GetScope(), best_id);
}

//  -----------------------------------------------------------------------------
bool CGenbankIdResolve::GetBestId(
    const CSeq_loc& loc,
    string& best_id)
//  -----------------------------------------------------------------------------
{
    const CSeq_id* pId = loc.GetId();
    if (!pId) {
        NCBI_THROW(CObjWriterException, eBadInput,
            "CGenbankIdResolve: Location without good ID");
    }
    return GetBestId(
        CSeq_id_Handle::GetHandle(*pId),
        xGetDefaultScope(),
        best_id);
}

//  -----------------------------------------------------------------------------
CScope& CGenbankIdResolve::xGetDefaultScope()
//  -----------------------------------------------------------------------------
{
    if (!mpDefaultScope) {
        mpDefaultScope.Reset(new CScope(*CObjectManager::GetInstance()));
        mpDefaultScope->AddDefaults();
    }
    return *mpDefaultScope;
}

END_NCBI_SCOPE
