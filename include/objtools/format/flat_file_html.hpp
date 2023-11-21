#ifndef OBJTOOLS_FORMAT___FLAT_FILE_HTML__HPP
#define OBJTOOLS_FORMAT___FLAT_FILE_HTML__HPP

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
 * Authors:  Jonathan Kans, NCBI
 *
 * File Description:
 *    Configuration class for flat-file HTML generator
 */
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objmgr/scope.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

const string sLinkBaseProt = "/protein/";
const string sLinkBaseNuc = "/nuccore/";
const string sLinkBaseNucSearch = "/sites/entrez?db=Nucleotide&amp;cmd=Search&amp;term=";
const string sLinkBaseTaxonomy = "/Taxonomy/Browser/wwwtax.cgi?";
struct SModelEvidance;

// --- Flat File HTML configuration class

class NCBI_FORMAT_EXPORT CHTMLFormatterEx : public IHTMLFormatter
{
public:
    CHTMLFormatterEx(CRef<CScope> scope);
    ~CHTMLFormatterEx() override;

    void FormatProteinId(string&, const CSeq_id& seq_id, const string& prot_id) const override;
    void FormatTranscriptId(string&, const CSeq_id& seq_id, const string& nuc_id) const override;
    void FormatNucSearch(CNcbiOstream& os, const string& id) const override;
    void FormatNucId(string& str, const CSeq_id& seq_id, TIntId gi, const string& acc_id) const override;
    void FormatTaxid(string& str, const TTaxId taxid, const string& taxname) const override;
    void FormatLocation(string& str, const CSeq_loc& loc, TIntId gi, const string& visible_text) const override;
    void FormatModelEvidence(string& str, const SModelEvidance& me) const override;
    void FormatTranscript(string& str, const string& name) const override;
    void FormatGeneralId(CNcbiOstream& os, const string& id) const override;
    void FormatGapLink(CNcbiOstream& os, TSeqPos gap_size, const string& id, bool is_prot) const override;
    void FormatUniProtId(string& str, const string& prot_id) const override;
    void SetNcbiURLBase(const string& path) { m_NcbiURLBase = path; }

private:
    mutable CRef<CScope> m_scope;
    string m_NcbiURLBase;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_FORMAT___FLAT_FILE_HTML__HPP */
