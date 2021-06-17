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
 * Author: Frank Ludwig
 *
 * File Description:  Iterate through file names matching a given glob pattern
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Annotdesc.hpp>

#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <objtools/import/import_error.hpp>
#include "feat_annot_assembler.hpp"
#include "../annot_import_data.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ============================================================================
CFeatAnnotAssembler::CFeatAnnotAssembler(
    CImportMessageHandler& errorReporter):
//  ============================================================================
    mErrorReporter(errorReporter)
{
}

//  ============================================================================
CFeatAnnotAssembler::~CFeatAnnotAssembler()
//  ============================================================================
{
}

//  ============================================================================
void
CFeatAnnotAssembler::InitializeAnnot(
    CSeq_annot& annot)
//  ============================================================================
{
    annot.Reset();
    annot.SetData().SetFtable();
}


//  ============================================================================
void
CFeatAnnotAssembler::FinalizeAnnot(
    const CAnnotImportData&,
    CSeq_annot&)
//  ============================================================================
{
}
