#ifndef SEQANNOT_SPLICER_STATS__HPP
#define SEQANNOT_SPLICER_STATS__HPP

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

// Singleton class for encapsulating application statistics.
class CSeqAnnotSplicerStats
{
public:
    static CSeqAnnotSplicerStats* GetInstance(void);

    void Report(void);

    void SeqAnnot_Scan_Pre(void);
    void SeqAnnot_Scan_Post(void);
    void SeqAnnot_Spliced(void);

    void SeqEntry_Changed(void);
    void SeqEntry_End(void);
    void SeqEntry_Start(void);
};

// Global pointer to access the application statistics singleton.
extern CSeqAnnotSplicerStats* g_Stats;


#endif  /* SEQANNOT_SPLICER_STATS__HPP */
