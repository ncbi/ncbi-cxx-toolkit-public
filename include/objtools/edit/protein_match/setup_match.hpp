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
 *  and reliability of the software and data,  the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties,  express or implied,  including
 *  warranties of performance,  merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Justin Foley
 */

#ifndef _SETUP_MATCH_H_
#define _SETUP_MATCH_H_

#include <corelib/ncbistd.hpp>
#include <objmgr/seq_entry_handle.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CMatchSetup {

public:
    CMatchSetup(void);
    virtual ~CMatchSetup(void) {};

    static void GetNucProtSets(
        CSeq_entry_Handle seh,
        list<CSeq_entry_Handle>& nucprot_sets);

    CSeq_entry_Handle GetGenBankTopLevelEntry(CSeq_entry_Handle nucleotide_seh);
    CSeq_entry_Handle GetNucleotideSEH(CSeq_entry_Handle seh) const;
    bool GetNucSeqIdFromCDSs(CSeq_entry_Handle& seh, CRef<CSeq_id>& nucseq_id);
    bool UpdateNucSeqIds(CRef<CSeq_id>& new_id,
        CSeq_entry_Handle& nucleotide_seh,
        CSeq_entry_Handle& nuc_prot_seh);

private:
    CRef<CScope> m_GBScope;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif
