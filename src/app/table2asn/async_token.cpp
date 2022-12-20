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
* Authors:  Jonathan Kans, Clifford Clausen,
*           Aaron Ucko, Sergiy Gotvyanskyy
*
* File Description:
*   Converter of various files into ASN.1 format, main application function
*
*/

#include <ncbi_pch.hpp>
#include <common/ncbi_source_ver.h>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_mask.hpp>

#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>

#include <util/line_reader.hpp>
#include <objtools/edit/remote_updater.hpp>
#include <objtools/cleanup/cleanup.hpp>

#include "multireader.hpp"
#include "table2asn_context.hpp"
#include "struc_cmt_reader.hpp"
#include "feature_table_reader.hpp"
#include "fcs_reader.hpp"
#include "src_quals.hpp"

#include <objects/seq/Seq_descr.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/general/Date.hpp>

#include <objects/seq/Linkage_evidence.hpp>
#include <objects/seq/Seq_gap.hpp>

#include <objtools/edit/seq_entry_edit.hpp>
#include <objects/valerr/ValidError.hpp>
#include <objtools/validator/validator.hpp>

#include <objtools/readers/message_listener.hpp>

#include "table2asn_validator.hpp"

#include <objmgr/feat_ci.hpp>
#include "visitors.hpp"

#include <objtools/readers/fasta_exception.hpp>

#include <misc/data_loaders_util/data_loaders_util.hpp>

#include <objtools/format/flat_file_generator.hpp>
#include <objtools/logging/listener.hpp>
#include <objtools/writers/async_writers.hpp>
#include <objtools/cleanup/cleanup_pub.hpp>

#include <common/test_assert.h>  /* This header must go last */

#include "async_token.hpp"

using namespace ncbi;
using namespace objects;

using namespace ncbi;
using namespace objects;

TAsyncToken::operator CConstRef<CSeq_entry>() const
{
    if (entry)
        return entry;
    if (seh)
        return seh.GetCompleteSeq_entry();

    return {};
}


CRef<feature::CFeatTree> TAsyncToken::FeatTree()
{
    if (feat_tree.Empty())
    {
        feat_tree.Reset(new feature::CFeatTree());
        feat_tree->AddFeatures(CFeat_CI(scope->GetBioseqHandle(*bioseq)));
    }
    return feat_tree;
}


void TAsyncToken::Clear()
{
    map_locus_to_gene.clear();
    map_protein_to_mrna.clear();
    map_transcript_to_mrna.clear();
    feat_tree.ReleaseOrNull();
}


