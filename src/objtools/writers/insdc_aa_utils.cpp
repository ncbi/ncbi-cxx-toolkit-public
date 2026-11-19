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
 * Authors: Justin Foley
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seq/seqport_util.hpp>

#include <array>
#include <utility>

#include "insdc_aa_utils.hpp"


using namespace std;

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

string_view g_GetAminoAcid(int aaIndex)
{

    if (aaIndex<0) {
        return "other"sv;
    }

    static constexpr array<string_view, 28> aaNames = {
        "other", // "gap" doesn't apply here
        "alanine",
        "aspartic_acid or asparagine",
        "cysteine",
        "aspartic_acid",
        "glutamic_acid",
        "phenylalanine",
        "glycine",
        "histidine",
        "isoleucine",
        "lysine",
        "leucine",
        "methionine",
        "asparagine",
        "proline",
        "glutamine",
        "arginine",
        "serine",
        "threonine",
        "valine",
        "tryptophan",
        "other",
        "tyrosine",
        "glutamic_acid or glutamine",
        "selenocysteine",
        "stop_codon",
        "pyrrolysine",
        "leucine or isoleucine"
    };

    string_view aa = cmp_less(aaIndex, aaNames.size()) ? aaNames[static_cast<size_t>(aaIndex)] : "other"sv;
    return aa;
}


optional<string_view> g_GetAminoAcid(const CTrna_ext& trnaExt)
{
    if (trnaExt.IsSetAa()) {
        int aaIndex;
        switch (trnaExt.GetAa().Which())
        {
        case CTrna_ext::C_Aa::e_Iupacaa:
            aaIndex = trnaExt.GetAa().GetIupacaa();
            aaIndex = CSeqportUtil::GetMapToIndex(
                CSeq_data::e_Iupacaa, CSeq_data::e_Ncbistdaa, static_cast<CSeqportUtil::TIndex>(aaIndex));
            break;
        case CTrna_ext::C_Aa::e_Ncbieaa:
            aaIndex = trnaExt.GetAa().GetNcbieaa();
            aaIndex = CSeqportUtil::GetMapToIndex(
                CSeq_data::e_Ncbieaa, CSeq_data::e_Ncbistdaa, static_cast<CSeqportUtil::TIndex>(aaIndex));
            break;
        case CTrna_ext::C_Aa::e_Ncbi8aa:
            aaIndex = trnaExt.GetAa().GetNcbi8aa();
            break;
        case CTrna_ext::C_Aa::e_Ncbistdaa:
            aaIndex = trnaExt.GetAa().GetNcbistdaa();
            break;
        default:
            return nullopt;
        }
        return g_GetAminoAcid(aaIndex);
    }
    return nullopt;
}


END_SCOPE(objects)
END_NCBI_SCOPE

