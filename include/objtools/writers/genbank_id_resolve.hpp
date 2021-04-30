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
 * File Description:  Formatter, Genbank to BED.
 *
 */

#ifndef OBJTOOLS_WRITERS___GENBANKID_RESOLVE__HPP
#define OBJTOOLS_WRITERS___GENBANKID_RESOLVE__HPP

#include <corelib/ncbistd.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objmgr/util/feature.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ---------------------------------------------------------------------------
class NCBI_XOBJWRITE_EXPORT CGenbankIdResolve
//  ---------------------------------------------------------------------------
{
public:
    static CGenbankIdResolve&
    Get();

    ~CGenbankIdResolve();

    void
    SetThrowOnUnresolvedGi(
        bool doThrow) { mThrowOnUnresolvedGi = doThrow; };

    void
    SetLabelType(
        CSeq_id::ELabelType labelType) { mLabelType = labelType; };

    bool
    GetBestId(
        CSeq_id_Handle,
        CScope&,
        string&);

    bool
    GetBestId(
        const CMappedFeat&,
        string&);

    bool
    GetBestId(
        const CSeq_loc&,
        string&);

private:
    CGenbankIdResolve();

    CScope&
    xGetDefaultScope();

private:
    CRef<CScope> mpDefaultScope;
    bool mThrowOnUnresolvedGi;
    CSeq_id::ELabelType mLabelType;
};

END_objects_SCOPE
END_NCBI_SCOPE


#endif  // OBJTOOLS_WRITERS___GENBANKID_RESOLVE__HPP

