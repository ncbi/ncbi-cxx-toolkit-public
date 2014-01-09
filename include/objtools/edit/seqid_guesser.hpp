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
 * Authors:  Colleen Bollin
 */
#ifndef _SEQID_GUESSER_H_
#define _SEQID_GUESSER_H_

#include <corelib/ncbistd.hpp>
#include <corelib/hash_map.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objtools/edit/string_constraint.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)


class NCBI_XOBJEDIT_EXPORT CSeqIdGuesser
{
public:
    CSeqIdGuesser(CSeq_entry_Handle entry);
    ~CSeqIdGuesser() {} ;

    CRef<CSeq_id> Guess(const string& id_str);
    vector<CBioseq_Handle> FindMatches(CRef<CStringConstraint> string_constraint);
    static bool DoesSeqMatchConstraint(CBioseq_Handle bsh, CRef<CStringConstraint> string_constraint);
    static vector<string> GetIdStrings(CBioseq_Handle bsh);

    typedef hash_map<string, CRef<CSeq_id> > TStringIdHash;

private:
    CSeq_entry_Handle m_SeqEntry;

    TStringIdHash m_StringIdHash;
};


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif
// _SEQID_GUESSER_H_

