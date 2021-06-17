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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <misc/hgvs/sequtils.hpp>

#include <objmgr/util/sequence.hpp>
#include <objects/seq/MolInfo.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


Uint4 GetSequenceType(const CBioseq_Handle& bsh)
{
    if (bsh.IsAa()) {
        return CSeq_id::fAcc_prot;
    }

    const CMolInfo* info = sequence::GetMolInfo(bsh);
    if (info) {
        if (info->GetBiomol() == CMolInfo::eBiomol_mRNA  ||
            info->GetBiomol() == CMolInfo::eBiomol_pre_RNA  ||
            info->GetBiomol() == CMolInfo::eBiomol_tRNA  ||
            info->GetBiomol() == CMolInfo::eBiomol_snRNA  ||
            info->GetBiomol() == CMolInfo::eBiomol_scRNA  ||
            info->GetBiomol() == CMolInfo::eBiomol_cRNA  ||
            info->GetBiomol() == CMolInfo::eBiomol_snoRNA  ||
            info->GetBiomol() == CMolInfo::eBiomol_ncRNA  ||
            info->GetBiomol() == CMolInfo::eBiomol_tmRNA) {
            return (CSeq_id::fAcc_nuc | CSeq_id::eAcc_mrna);
        }

        if (info->GetBiomol() == CMolInfo::eBiomol_genomic) {
            return (CSeq_id::fAcc_nuc | CSeq_id::fAcc_genomic);
        }
    }

    CSeq_id_Handle idh = sequence::GetId(*bsh.GetSeqId(), bsh.GetScope(),
                                         sequence::eGetId_Best);
    CSeq_id::EAccessionInfo id_info = idh.GetSeqId()->IdentifyAccession();

    if ((id_info & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_est  ||
        id_info == CSeq_id::eAcc_refseq_mrna  ||
        id_info == CSeq_id::eAcc_refseq_mrna_predicted  ||
        id_info == CSeq_id::eAcc_gpipe_mrna) {
        return (CSeq_id::fAcc_nuc | CSeq_id::eAcc_mrna);
    }
    if (id_info == CSeq_id::eAcc_refseq_chromosome  ||
        id_info == CSeq_id::eAcc_refseq_contig  ||
        id_info == CSeq_id::eAcc_refseq_genomic  ||
        id_info == CSeq_id::eAcc_refseq_genome  ||
        id_info == CSeq_id::eAcc_refseq_wgs_intermed) {
        return (CSeq_id::fAcc_nuc | CSeq_id::fAcc_genomic);
    }

    return (CSeq_id::eAcc_unknown);
}


END_SCOPE(objects)
END_NCBI_SCOPE
