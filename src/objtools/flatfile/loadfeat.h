/* loadfeat.h
 *
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
 * File Name:  loadfeat.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 *
 */

#ifndef _LOADFEAT_
#define _LOADFEAT_

#include <objects/seqfeat/Seq_feat.hpp>

#include "xgbfeat.h"
#include "asci_blk.h"

BEGIN_NCBI_SCOPE

typedef struct feature_block {
    Int4      num;
    char*   key;
    char*   location;

    TQualVector quals;

    feature_block() :
        num(0),
        key(NULL),
        location(NULL)
    {
    }

} FeatBlk, *FeatBlkPtr;

void LoadFeat(ParserPtr pp, DataBlkPtr entry, objects::CBioseq& bioseq);
int  ParseFeatureBlock(IndexblkPtr ibp, bool deb, DataBlkPtr dbp, Int2 source, Parser::EFormat format);

void GetFlatBiomol(int& biomol, Uint1 tech, char* molstr, ParserPtr pp, DataBlkPtr entry, const objects::COrg_ref* org_ref);

bool GetSeqLocation(objects::CSeq_feat& feat, char* location, TSeqIdList& ids,
                    bool* hard_err, ParserPtr pp, char* name);

END_NCBI_SCOPE

#endif
