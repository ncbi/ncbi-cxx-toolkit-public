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
 * Authors:  Christiam Camacho
 *
 */

/// @file version.cpp
/// Implementation of the BLAST engine's version and reference classes

#include <ncbi_pch.hpp>
#include <algo/blast/core/blast_engine.h>
#include <algo/blast/api/version.hpp>
#include <sstream>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CVersion::CVersion(int major, int minor, int patch, 
                             const string& date)
    : CVersionInfo(major, minor, patch), m_ReleaseDate(date)
{
}

string
CVersion::Print(void) const
{
    ostringstream os;
    os << GetMajor() << "." << GetMinor() << "." << GetPatchLevel();
    return os.str();
}

static const string kReferences[(int)CReference::eMaxPublications+1] = {
    // eGappedBlast
    "Altschul, Stephen F., Thomas L. Madden, \
Alejandro A. Schäffer, Jinghui Zhang, Zheng Zhang, Webb Miller, and David J. \
Lipman (1997), \"Gapped BLAST and PSI-BLAST: a new generation of protein \
database search programs\", Nucleic Acids Res. 25:3389-3402.",
    // ePhiBlast
    "Zhang, Zheng, Alejandro A. Schäffer, Webb Miller, \
Thomas L. Madden, David J. Lipman, Eugene V. Koonin, and Stephen F. \
Altschul (1998), \"Protein sequence similarity searches using patterns \
as seeds\", Nucleic Acids Res. 26:3986-3990.",
    // eMegaBlast
    "Zheng Zhang, Scott Schwartz, Lukas Wagner, and Webb Miller (2000), \
\"A greedy algorithm for aligning DNA sequences\", \
J Comput Biol 2000; 7(1-2):203-14.", 
    // eCompBasedStats
    "Schäffer, Alejandro A., L. Aravind, Thomas L. Madden, Sergei Shavirin, \
John L. Spouge, Yuri I. Wolf, Eugene V. Koonin, and Stephen F. Altschul \
(2001), \"Improving the accuracy of PSI-BLAST protein database searches \
with composition-based statistics and other refinements\", Nucleic Acids \
Res. 29:2994-3005.",
    // eCompAdjustedMatrices
    "John C. Wootton, E. Michael Gertz, Richa Agarwala, Aleksandr Morgulis, \
Alejandro A. Schäffer, and Yi-Kuo Yu \"Protein database searches using \
compositionally adjusted substitution matrices\", submitted.",
    // eMaxPublications
    kEmptyStr
};
                 
static const string kPubMedUrls[(int)CReference::eMaxPublications+1] = {
    // eGappedBlast
    "http://www.ncbi.nlm.nih.gov/\
entrez/query.fcgi?db=PubMed&cmd=Retrieve&list_uids=9254694&dopt=Citation",
    // ePhiBlast
    "http://www.ncbi.nlm.nih.gov/\
entrez/query.fcgi?db=PubMed&cmd=Retrieve&list_uids=9705509&dopt=Citation",
    // eMegaBlast
    "http://www.ncbi.nlm.nih.gov/\
entrez/query.fcgi?db=PubMed&cmd=Retrieve&list_uids=10890397&dopt=Citation",
    // eCompBasedStats
    "http://www.ncbi.nlm.nih.gov/\
entrez/query.fcgi?db=PubMed&cmd=Retrieve&list_uids=11452024&dopt=Citation",
    // eCompAdjustedMatrices
    "http://www.ncbi.nlm.nih.gov/\
entrez/query.fcgi",
    // eMaxPublications
    kEmptyStr
};

string 
CReference::GetString(EPublication pub)
{
    return kReferences[(int) pub];
}

string
CReference::GetPubmedUrl(EPublication pub)
{
    return kPubMedUrls[(int) pub];
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
