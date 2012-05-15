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

/// References for the various BLAST publications
static const string kReferences[(int)CReference::eMaxPublications+1] = {
    // eGappedBlast
    "Stephen F. Altschul, Thomas L. Madden, \
Alejandro A. Sch&auml;ffer, Jinghui Zhang, Zheng Zhang, Webb Miller, and David J. \
Lipman (1997), \"Gapped BLAST and PSI-BLAST: a new generation of protein \
database search programs\", Nucleic Acids Res. 25:3389-3402.",
    // ePhiBlast
    "Zheng Zhang, Alejandro A. Sch&auml;ffer, Webb Miller, \
Thomas L. Madden, David J. Lipman, Eugene V. Koonin, and Stephen F. \
Altschul (1998), \"Protein sequence similarity searches using patterns \
as seeds\", Nucleic Acids Res. 26:3986-3990.",
    // eMegaBlast
    "Zheng Zhang, Scott Schwartz, Lukas Wagner, and Webb Miller (2000), \
\"A greedy algorithm for aligning DNA sequences\", \
J Comput Biol 2000; 7(1-2):203-14.", 
    // eCompBasedStats
    "Alejandro A. Sch&auml;ffer, L. Aravind, Thomas L. Madden, Sergei Shavirin, \
John L. Spouge, Yuri I. Wolf, Eugene V. Koonin, and Stephen F. Altschul \
(2001), \"Improving the accuracy of PSI-BLAST protein database searches \
with composition-based statistics and other refinements\", Nucleic Acids \
Res. 29:2994-3005.",
    // eCompAdjustedMatrices
    "Stephen F. Altschul, John C. Wootton, E. Michael Gertz, Richa Agarwala, \
Aleksandr Morgulis, Alejandro A. Sch&auml;ffer, and Yi-Kuo Yu (2005) \"Protein \
database searches using compositionally adjusted substitution matrices\", \
FEBS J. 272:5101-5109.",
    // eIndexedMegablast
    "Aleksandr Morgulis, George Coulouris, Yan Raytselis, \
Thomas L. Madden, Richa Agarwala, Alejandro A. Sch&auml;ffer \
(2008), \"Database Indexing for Production MegaBLAST Searches\", \
Bioinformatics 24:1757-1764.",
    // eDeltaBlast
    "Grzegorz M. Boratyn, Alejandro A. Schaffer, Richa Agarwala, Stephen F. Altschul, \
David J. Lipman and Thomas L. Madden (2012) \"Domain enhanced lookup time \
accelerated BLAST\", Biology Direct 7:12.",
    // eMaxPublications
    kEmptyStr
};
                 
/// Pubmed URLs to retrieve the references defined above
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
entrez/query.fcgi?db=PubMed&cmd=Retrieve&list_uids=16218944&dopt=Citation",
    // eIndexedMegablast
    "http://www.ncbi.nlm.nih.gov/pubmed/18567917",
    // eDeltaBlast
    "http://www.ncbi.nlm.nih.gov/pubmed/22510480",
    // eMaxPublications
    kEmptyStr
};

string 
CReference::GetString(EPublication pub)
{
    return kReferences[(int) pub];
}

string 
CReference::GetHTMLFreeString(EPublication pub)
{
    string pub_string = GetString(pub);
    string::size_type offset = pub_string.find("&auml;");
    if (offset != string::npos)
        pub_string.replace(offset, 6, "a");

    return pub_string;
}

string
CReference::GetPubmedUrl(EPublication pub)
{
    return kPubMedUrls[(int) pub];
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
