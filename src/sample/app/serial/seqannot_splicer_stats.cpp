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
* Author:  David McElhany
*
* File Description:
*   See main application file.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <stack>

#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbiobj.hpp>

#include "seqannot_splicer_stats.hpp"

USING_SCOPE(ncbi);


// Global pointer to access the application statistics singleton.
CSeqAnnotSplicerStats* g_Stats = CSeqAnnotSplicerStats::GetInstance();


typedef unsigned long   TObjCtr;

// Private data used by app stats class.
static TObjCtr  s_NumSa = 0;
static TObjCtr  s_NumSe = 0;
static TObjCtr  s_NumSeUnchanged = 0;
static TObjCtr  s_NumSaSpliced = 0;
static bool     s_Unchanged = false;


///////////////////////////////////////////////////////////////////////////
// Singleton class for encapsulating application statistics

CSeqAnnotSplicerStats* CSeqAnnotSplicerStats::GetInstance(void)
{
    static CSeqAnnotSplicerStats instance;
    return &instance;
}

void CSeqAnnotSplicerStats::Report(void)
{
    NcbiCout << "\nNumber of Seq-annot's processed: " << s_NumSa << NcbiEndl;
    NcbiCout << "Number of Seq-annot's spliced:   " << s_NumSaSpliced << NcbiEndl;
    NcbiCout << "Number of Seq-annot's unspliced: " << s_NumSa - s_NumSaSpliced << NcbiEndl << NcbiEndl;
    NcbiCout << "Number of Seq-entry's processed: " << s_NumSe << NcbiEndl;
    NcbiCout << "Number of Seq-entry's expanded:  " << s_NumSe - s_NumSeUnchanged << NcbiEndl;
    NcbiCout << "Number of Seq-entry's unchanged: " << s_NumSeUnchanged << NcbiEndl;
}


void CSeqAnnotSplicerStats::SeqAnnot_Scan_Pre(void)
{
    if (s_NumSa % 10000 == 0) {
        NcbiCout << "Preprocessing Seq-annot # " << s_NumSa << NcbiEndl;
    }
}


void CSeqAnnotSplicerStats::SeqAnnot_Scan_Post(void)
{
    ++s_NumSa;
}


void CSeqAnnotSplicerStats::SeqAnnot_Spliced(void)
{
    ++s_NumSaSpliced;
}


void CSeqAnnotSplicerStats::SeqEntry_Changed(void)
{
    s_Unchanged = false;
}


void CSeqAnnotSplicerStats::SeqEntry_End(void)
{
    ++s_NumSe;
    if (s_Unchanged) {
        ++s_NumSeUnchanged;
    }
}


void CSeqAnnotSplicerStats::SeqEntry_Start(void)
{
    s_Unchanged = true;
}

