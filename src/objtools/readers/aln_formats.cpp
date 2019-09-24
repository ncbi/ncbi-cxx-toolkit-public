/*
 * $Id$
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
 * Authors: Frank Ludwig
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <objtools/readers/aln_formats.hpp>

BEGIN_NCBI_SCOPE

//  ----------------------------------------------------------------------------
string EAlignFormatToString(
    EAlignFormat alnFmt)
//  ----------------------------------------------------------------------------
{
    static const map<EAlignFormat, string> sFormatMap{{
        {EAlignFormat::UNKNOWN, "Unknown"},
        {EAlignFormat::CLUSTAL, "Clustal"},
        {EAlignFormat::FASTAGAP, "FASTA-Gap"},
        {EAlignFormat::MULTALIN, "Multalin"},
        {EAlignFormat::NEXUS, "NEXUS"},
        {EAlignFormat::PHYLIP, "PHYLIP"},
        {EAlignFormat::SEQUIN, "Sequin"},
    }};
    auto it = sFormatMap.find(alnFmt);
    if (it == sFormatMap.end()) {
        return "Unknown";
    }
    return it->second;
}


//  ----------------------------------------------------------------------------
EAlignFormat StringToEAlignFormat(
    const string& strFmt)
//  ----------------------------------------------------------------------------
{
    static const map<string, EAlignFormat> sFormatMap{{
        {"clustal", EAlignFormat::CLUSTAL},
        {"fasta-gap", EAlignFormat::FASTAGAP},
        {"multalign", EAlignFormat::MULTALIN},
        {"nexus", EAlignFormat::NEXUS},
        {"phylip", EAlignFormat::PHYLIP},
        {"sequin", EAlignFormat::SEQUIN},
    }};
    string normalized(strFmt);
    NStr::ToLower(normalized);
    auto it = sFormatMap.find(normalized);
    if (it == sFormatMap.end()) {
        return EAlignFormat::UNKNOWN;
    }
    return it->second;
}

END_NCBI_SCOPE
