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
 * File Name: loadfeat.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 *
 */

#ifndef _LOADFEAT_
#define _LOADFEAT_

#include <objects/seqfeat/Seq_feat.hpp>

#include "xgbfeat.h"
#include "asci_blk.h"

BEGIN_NCBI_SCOPE
struct FeatBlk {
    Int4   num      = 0;
    string key;
    char*  location = nullptr;
    Int2   spindex;

    TQualVector quals;

    const char*   key_or(const char* pch) const { return key.empty() ? pch : key.c_str(); }

    void        location_assign(string_view);
    bool        location_isset() const { return location != nullptr; }
    const char* location_get() const { return location; }
    const char* location_c_str() const { return location; }
    const char* location_or(const char* pch) const { return location ? location : pch; }

    ~FeatBlk();
};

using FeatBlkPtr = FeatBlk*;

void LoadFeat(ParserPtr pp, const DataBlk& entry, objects::CBioseq& bioseq);
int  ParseFeatureBlock(IndexblkPtr ibp, bool deb, TDataBlkList& dbl, Int2 source, Parser::EFormat format);

void GetFlatBiomol(int& biomol, int tech, char* molstr, ParserPtr pp, const DataBlk& entry, const objects::COrg_ref* org_ref);

bool GetSeqLocation(objects::CSeq_feat& feat, string_view location, const objects::CSeq_id& seqid, bool* hard_err, ParserPtr pp, string_view name);

END_NCBI_SCOPE

#endif
