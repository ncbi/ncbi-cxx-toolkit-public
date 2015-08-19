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
 * Authors:  Igor Filippov
 */

#ifndef _EDIT_RNA_EDIT__HPP_
#define _EDIT_RNA_EDIT__HPP_

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <util/line_reader.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/bioseq_ci.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

class NCBI_XOBJEDIT_EXPORT CFindITSParser
{
public:
    CFindITSParser(const char *input, CSeq_entry_Handle tse);
    CRef <CSeq_feat> ParseLine();
    bool AtEOF() {return m_lr->AtEOF();}
    CBioseq_Handle GetBSH() {return m_bsh;}
    bool GetNegative() {return m_negative;}
    string GetMsg() {return m_msg;}
private:
   // Prohibit copy constructor and assignment operator
    CFindITSParser(const CFindITSParser& value);
    CFindITSParser& operator=(const CFindITSParser& value);

    CRef <CSeq_feat> x_ParseLine(const CTempString &line, CSeq_entry_Handle tse, CBioseq_Handle &bsh, bool &negative, string &msg);
    CRef <CSeq_feat> x_CreateMiscRna(const string &accession, const string &comment, CBioseq_Handle bsh);
    CBioseq_Handle x_GetBioseqHandleFromIdGuesser(const string &id_str, objects::CSeq_entry_Handle tse);
    CNcbiIfstream m_istr;
    CRef<ILineReader> m_lr;
    CSeq_entry_Handle m_tse;
    CBioseq_Handle m_bsh;
    bool m_negative;
    string m_msg;
};

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif
        // _EDIT_RNA_EDIT__HPP_
