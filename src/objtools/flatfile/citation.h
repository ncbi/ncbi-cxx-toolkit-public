/* citation.h
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
* File Name:  citation.h
*
* Author: Alexey Dobronadezhdin
*
* File Description:
* -----------------
*
*/

#ifndef CITATION_H
#define CITATION_H

#include "ftablock.h"

BEGIN_NCBI_SCOPE

class CPubInfo
{
    int cit_num_;
    const objects::CBioseq* bioseq_;
    const objects::CPub_equiv* pub_equiv_;
    const objects::CPub* pub_;

public:
    CPubInfo();

    int GetSerial() const { return cit_num_; }
    const objects::CBioseq* GetBioseq() const { return bioseq_; }

    const objects::CPub_equiv* GetPubEquiv() const;
    const objects::CPub* GetPub() const { return pub_; }

    void SetBioseq(const objects::CBioseq* bioseq);
    void SetPubEquiv(const objects::CPub_equiv* pub_equiv);

    void SetPub(const objects::CPub* pub);
};

void ProcessCitations(TEntryList& seq_entries);
void SetMinimumPub(const CPubInfo& pub_info, TPubList& pubs);

END_NCBI_SCOPE

#endif //CITATION_H
