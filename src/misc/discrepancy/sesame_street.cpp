/*  $Id$
 * =========================================================================
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
 * =========================================================================
 *
 * Authors: Sema
 *
 */

#include <ncbi_pch.hpp>
#include "discrepancy_core.hpp"
#include "utils.hpp"
#include <objects/seqfeat/BioSource.hpp>
//#include <objects/seqfeat/Imp_feat.hpp>
//#include <objects/seqfeat/SeqFeatXref.hpp>
//#include <objects/macro/String_constraint.hpp>
//#include <objects/misc/sequence_util_macros.hpp>
//#include <objects/seq/seqport_util.hpp>
//#include <objmgr/bioseq_ci.hpp>
//#include <objmgr/feat_ci.hpp>
//#include <objmgr/seq_vector.hpp>
//#include <objmgr/util/feature.hpp>
//#include <objmgr/util/sequence.hpp>
//#include <sstream>

BEGIN_NCBI_SCOPE;
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(sesame_street);

// Some animals are more equal than others...

DISCREPANCY_CASE(SOURCE_QUALS, CBioSource, eAll, "Some animals are more equal than others...")
{
    //cout << "CBioSource" << endl;
    /*
    CSeq_inst::TMol mol = obj.GetMol();
    if (mol != CSeq_inst::eMol_dna && mol != CSeq_inst::eMol_rna && mol != CSeq_inst::eMol_na) {
        return;
    }
    m_Objs["[n] nucleotide Bioseq[s] [is] present"].Add(*new CDiscrepancyObject(context.GetCurrentBioseq(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
    */
}


DISCREPANCY_SUMMARIZE(SOURCE_QUALS)
{
    m_Objs["[n] nucleotide Bioseq[s] [is] present"];
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_ALIAS(SOURCE_QUALS, SOURCE_QUALS_ASNDISC)


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
