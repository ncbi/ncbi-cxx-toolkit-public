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

#include <util/icanceled.hpp>

#include <objmgr/scope.hpp>
#include <objtools/format/flat_file_config.hpp>

BEGIN_NCBI_SCOPE

class CArgDescriptions;
class CArgs;

BEGIN_SCOPE(objects)

class CBioseqContext;
class CStartSectionItem;
class CHtmlAnchorItem;
class CLocusItem;
class CDeflineItem;
class CAccessionItem;
class CVersionItem;
class CGenomeProjectItem;
class CDBSourceItem;
class CKeywordsItem;
class CSegmentItem;
class CSourceItem;
class CReferenceItem;
class CCacheItem;
class CCommentItem;
class CPrimaryItem;
class CFeatHeaderItem;
class CSourceFeatureItem;
class CFeatureItem;
class CGapItem;
class CBaseCountItem;
class COriginItem;
class CSequenceItem;
class CContigItem;
class CWGSItem;
class CTSAItem;
class CEndSectionItem;
class IFlatItem;
class CSeq_id;
class CSeq_loc;
class CScope;
struct SModelEvidance;

// --- Flat File HTML configuration class

class CHTMLFormatterEx : public IHTMLFormatter
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

private:
    mutable CRef<CScope> m_scope;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_FORMAT___FLAT_FILE_HTML__HPP */
