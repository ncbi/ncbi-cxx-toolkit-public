/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* Author:  Ilya Dondoshansky
*
* ===========================================================================
*/

/// @file: tabular.cpp
/// Formatting of pairwise sequence alignments in tabular form.
/// One line is printed for each alignment with tab-delimited fielalnVec. 
#include <ncbi_pch.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include<objtools/alnmgr/alnmix.hpp>
#include <objtools/align_format/tabular.hpp>
#include <objtools/align_format/showdefline.hpp>
#include <objtools/align_format/align_format_util.hpp>

#include <serial/iterator.hpp>
#include <objects/general/Object_id.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/blastdb/Blast_def_line.hpp>

#include <objmgr/seqdesc_ci.hpp>
#include <objects/seqfeat/Org_ref.hpp>

#include <map>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(align_format)

static const char* NA = "N/A";

void 
CBlastTabularInfo::x_AddDefaultFieldsToShow()
{
    vector<string> format_tokens;
    NStr::Split(kDfltArgTabularOutputFmt, " ", format_tokens);
    ITERATE (vector<string>, iter, format_tokens) {
        _ASSERT(m_FieldMap.count(*iter) > 0);
        x_AddFieldToShow(m_FieldMap[*iter]);
    }
}

void CBlastTabularInfo::x_SetFieldsToShow(const string& format)
{
    for (size_t i = 0; i < kNumTabularOutputFormatSpecifiers; i++) {
        m_FieldMap.insert(make_pair(sc_FormatSpecifiers[i].name,
                                    sc_FormatSpecifiers[i].field));
    }
    
    vector<string> format_tokens;
    NStr::Split(format, " ", format_tokens);

    if (format_tokens.empty())
        x_AddDefaultFieldsToShow();

    ITERATE (vector<string>, iter, format_tokens) {
        if (*iter == kDfltArgTabularOutputFmtTag)
            x_AddDefaultFieldsToShow();
        else if ((*iter)[0] == '-') {
            string field = (*iter).substr(1);
            if (m_FieldMap.count(field) > 0) 
                x_DeleteFieldToShow(m_FieldMap[field]);
        } else {
            if (m_FieldMap.count(*iter) > 0)
                x_AddFieldToShow(m_FieldMap[*iter]);
        }
    }

    if (m_FieldsToShow.empty()) {
        x_AddDefaultFieldsToShow();
    }
}

void CBlastTabularInfo::x_ResetFields()
{
    m_QueryLength = m_SubjectLength = 0U;
    m_Score = m_AlignLength = m_NumGaps = m_NumGapOpens = m_NumIdent =
    m_NumPositives = m_QueryStart = m_QueryEnd = m_SubjectStart =
    m_SubjectEnd = m_QueryFrame = m_SubjectFrame = 0;
    m_BitScore = NcbiEmptyString;
    m_Evalue = NcbiEmptyString;
    m_QuerySeq = NcbiEmptyString;
    m_SubjectSeq = NcbiEmptyString;
    m_BTOP = NcbiEmptyString;
    m_SubjectStrand = NcbiEmptyString;
    m_QueryCovSeqalign = -1;
}

void CBlastTabularInfo::x_SetFieldDelimiter(EFieldDelimiter delim,string customDelim)
{
    switch (delim) {
    case eSpace: m_FieldDelimiter = " "; break;
    case eComma: m_FieldDelimiter = ","; break;
    case eCustom: m_FieldDelimiter = customDelim; break;
    default: m_FieldDelimiter = "\t"; break; // eTab or unsupported value
    }
}

void CBlastTabularInfo::x_CheckTaxDB()
{
   	if( x_IsFieldRequested(eSubjectSciNames) ||
    	x_IsFieldRequested(eSubjectCommonNames) ||
    	x_IsFieldRequested(eSubjectBlastNames) ||
    	x_IsFieldRequested(eSubjectSuperKingdoms))
   	{
   		string resolved = SeqDB_ResolveDbPath("taxdb.bti");
   		if(resolved.empty())
   			ERR_POST(Warning << "Taxonomy name lookup from taxid requires installation of taxdb database with ftp://ftp.ncbi.nlm.nih.gov/blast/db/taxdb.tar.gz");
   	}
}

CBlastTabularInfo::CBlastTabularInfo(CNcbiOstream& ostr, const string& format,
                                     EFieldDelimiter delim, 
                                     bool parse_local_ids)
    : m_Ostream(ostr)
{
    x_SetFieldsToShow(format);
    x_ResetFields();
    x_SetFieldDelimiter(delim);
    SetParseLocalIds(parse_local_ids);
    SetParseSubjectDefline(false);
    SetNoFetch(false);
    m_QueryCovSubject.first = NA;
    m_QueryCovSubject.second = -1;
    m_QueryCovUniqSubject.first = NA;
    m_QueryCovUniqSubject.second = -1;
    m_QueryGeneticCode = 1;
    m_DbGeneticCode = 1;

    x_CheckTaxDB();
}

CBlastTabularInfo::~CBlastTabularInfo()
{
    m_Ostream.flush();
}

static string 
s_GetSeqIdListString(const list<CRef<CSeq_id> >& id, 
                     CBlastTabularInfo::ESeqIdType id_type)
{
    string id_str = NcbiEmptyString;

    switch (id_type) {
    case CBlastTabularInfo::eFullId:
        id_str = CShowBlastDefline::GetSeqIdListString(id, true);
        break;
    case CBlastTabularInfo::eAccession:
    {
        CConstRef<CSeq_id> accid = FindBestChoice(id, CSeq_id::Score);
        accid->GetLabel(&id_str, CSeq_id::eContent, 0);
        break;
    }
    case CBlastTabularInfo::eAccVersion:
    {
        CConstRef<CSeq_id> accid = FindBestChoice(id, CSeq_id::Score);
        accid->GetLabel(&id_str, CSeq_id::eContent, CSeq_id::fLabel_Version);
        break;
    }
    case CBlastTabularInfo::eGi:
        id_str = NStr::NumericToString(FindGi(id));
        break;
    default: break;
    }

    if (id_str == NcbiEmptyString)
        id_str = "Unknown";

    return id_str;
}

void CBlastTabularInfo::x_PrintQuerySeqId() const
{
    m_Ostream << s_GetSeqIdListString(m_QueryId, eFullId);
}

void CBlastTabularInfo::x_PrintQueryGi()
{
    m_Ostream << s_GetSeqIdListString(m_QueryId, eGi);
}

void CBlastTabularInfo::x_PrintQueryAccession()
{
    m_Ostream << s_GetSeqIdListString(m_QueryId, eAccession);
}

void CBlastTabularInfo::x_PrintQueryAccessionVersion()
{
    m_Ostream << s_GetSeqIdListString(m_QueryId, eAccVersion);
}

void CBlastTabularInfo::x_PrintSubjectSeqId()
{
    m_Ostream << s_GetSeqIdListString(m_SubjectId, eFullId);
}

void CBlastTabularInfo::x_PrintSubjectAllSeqIds(void)
{
    ITERATE(vector<list<CRef<CSeq_id> > >, iter, m_SubjectIds) {
        if (iter != m_SubjectIds.begin())
            m_Ostream << ";";
        m_Ostream << s_GetSeqIdListString(*iter, eFullId); 
    }
}

void CBlastTabularInfo::x_PrintSubjectGi(void)
{
    m_Ostream << s_GetSeqIdListString(m_SubjectId, eGi);
}

void CBlastTabularInfo::x_PrintSubjectAllGis(void)
{
    ITERATE(vector<list<CRef<CSeq_id> > >, iter, m_SubjectIds) {
        if (iter != m_SubjectIds.begin())
            m_Ostream << ";";
        m_Ostream << s_GetSeqIdListString(*iter, eGi); 
    }
}

void CBlastTabularInfo::x_PrintSubjectAccession(void)
{
    m_Ostream << s_GetSeqIdListString(m_SubjectId, eAccession);
}

void CBlastTabularInfo::x_PrintSubjectAccessionVersion(void)
{
    m_Ostream << s_GetSeqIdListString(m_SubjectId, eAccVersion);
}

void CBlastTabularInfo::x_PrintSubjectAllAccessions(void)
{
    ITERATE(vector<list<CRef<CSeq_id> > >, iter, m_SubjectIds) {
        if (iter != m_SubjectIds.begin())
            m_Ostream << ";";
        m_Ostream << s_GetSeqIdListString(*iter, eAccession); 
    }
}

void CBlastTabularInfo::x_PrintSubjectTaxId()
{
	if(m_SubjectTaxId == ZERO_TAX_ID) {
		m_Ostream << NA;
		return;
	}
       m_Ostream << m_SubjectTaxId;
}

void CBlastTabularInfo::x_PrintSubjectTaxIds()
{
	if(m_SubjectTaxIds.empty()) {
		m_Ostream << NA;
		return;
	}

    ITERATE(set<TTaxId>, iter, m_SubjectTaxIds) {
        if (iter != m_SubjectTaxIds.begin())
            m_Ostream << ";";
       m_Ostream << *iter;
    }
}

void CBlastTabularInfo::x_PrintSubjectBlastName()
{
	if(m_SubjectBlastName == kEmptyStr) {
		m_Ostream << NA;
		return;
	}
	m_Ostream << (x_IsCsv() ? NStr::Quote(m_SubjectBlastName) : m_SubjectBlastName);
}

void CBlastTabularInfo::x_PrintSubjectBlastNames()
{
    CNcbiOstrstream oss;
	if(m_SubjectBlastNames.empty()) {
		oss << NA;
	} else {
        ITERATE(set<string>, iter, m_SubjectBlastNames) {
            if (iter != m_SubjectBlastNames.begin())
                oss << ";";
            oss << *iter;
        }
    }
    const string output = CNcbiOstrstreamToString(oss);
    m_Ostream << (x_IsCsv() ? NStr::Quote(output) : output);
}

void CBlastTabularInfo::x_PrintSubjectSuperKingdom()
{
	if(m_SubjectSuperKingdom == kEmptyStr) {
		m_Ostream << NA;
		return;
	}
	m_Ostream << m_SubjectSuperKingdom;
}

void CBlastTabularInfo::x_PrintSubjectSuperKingdoms()
{
	if(m_SubjectSuperKingdoms.empty()) {
		m_Ostream << NA;
		return;
	}

	 ITERATE(set<string>, iter, m_SubjectSuperKingdoms) {
		 if (iter != m_SubjectSuperKingdoms.begin())
			 m_Ostream << ";";
		 m_Ostream << *iter;
	}
}

void CBlastTabularInfo::x_PrintSubjectSciName()
{
	if(m_SubjectSciName == kEmptyStr) {
		m_Ostream << NA;
		return;
	}
	m_Ostream << (x_IsCsv() ? NStr::Quote(m_SubjectSciName) : m_SubjectSciName);
}

void CBlastTabularInfo::x_PrintSubjectSciNames()
{
    CNcbiOstrstream oss;
	if(m_SubjectSciNames.empty()) {
		oss << NA;
	} else {
        ITERATE(vector<string>, iter, m_SubjectSciNames) {
             if (iter != m_SubjectSciNames.begin())
                 oss << ";";
             oss << *iter;
        }
    }
    const string output = CNcbiOstrstreamToString(oss);
    m_Ostream << (x_IsCsv() ? NStr::Quote(output) : output);
}

void CBlastTabularInfo::x_PrintSubjectCommonName()
{
	if(m_SubjectCommonName == kEmptyStr) {
		m_Ostream << NA;
		return;
	}
	m_Ostream << (x_IsCsv() ? NStr::Quote(m_SubjectCommonName) : m_SubjectCommonName);
}

void CBlastTabularInfo::x_PrintSubjectCommonNames()
{
    CNcbiOstrstream oss;
	if(m_SubjectCommonNames.empty()) {
		oss << NA;
	} else {
        ITERATE(vector<string>, iter, m_SubjectCommonNames) {
             if (iter != m_SubjectCommonNames.begin())
                 oss << ";";
             oss << *iter;
        }
    }
    const string output = CNcbiOstrstreamToString(oss);
    m_Ostream << (x_IsCsv() ? NStr::Quote(output) : output);
}

void CBlastTabularInfo::x_PrintSubjectAllTitles ()
{
	if(m_SubjectDefline.NotEmpty() && m_SubjectDefline->CanGet() &&
	   m_SubjectDefline->IsSet() && !m_SubjectDefline->Get().empty())
	{
		const list<CRef<CBlast_def_line> > & defline = m_SubjectDefline->Get();
		list<CRef<CBlast_def_line> >::const_iterator iter = defline.begin();
		for(; iter != defline.end(); ++iter)
		{
			if (iter != defline.begin())
						 m_Ostream << "<>";

			if((*iter)->IsSetTitle())
			{
				if((*iter)->GetTitle().empty())
					m_Ostream << NA;
				else
					m_Ostream << (*iter)->GetTitle();
			}
			else
				m_Ostream << NA;
		}
	}
	else
		m_Ostream << NA;

}

void CBlastTabularInfo::x_PrintSubjectTitle()
{
	if(m_SubjectDefline.NotEmpty() && m_SubjectDefline->CanGet() &&
	   m_SubjectDefline->IsSet() && !m_SubjectDefline->Get().empty())
	{
		const list<CRef<CBlast_def_line> > & defline = m_SubjectDefline->Get();

		if(defline.empty())
			m_Ostream << NA;
		else
		{
			if(defline.front()->IsSetTitle())
			{
				if(defline.front()->GetTitle().empty())
					m_Ostream << NA;
				else
					m_Ostream << defline.front()->GetTitle();
			}
			else
				m_Ostream << NA;
		}
	}
	else
		m_Ostream << NA;

}

void CBlastTabularInfo::x_PrintSubjectStrand()
{
	if(m_SubjectStrand != NcbiEmptyString)
		m_Ostream << m_SubjectStrand;
	else
		m_Ostream << NA;
}

void CBlastTabularInfo::x_PrintSubjectCoverage(void)
{
	if(m_QueryCovSubject.second < 0)
		m_Ostream << NA;
	else
		m_Ostream << NStr::IntToString(m_QueryCovSubject.second);
}

void CBlastTabularInfo::x_PrintUniqSubjectCoverage(void)
{
	if(m_QueryCovUniqSubject.second < 0)
		m_Ostream << NA;
	else
		m_Ostream << NStr::IntToString(m_QueryCovUniqSubject.second);
}

void CBlastTabularInfo::x_PrintSeqalignCoverage(void)
{
	if(m_QueryCovSeqalign < 0)
		m_Ostream << NA;
	else
		m_Ostream << NStr::IntToString(m_QueryCovSeqalign);
}

CRef<CSeq_id> s_ReplaceLocalId(const CBioseq_Handle& bh, CConstRef<CSeq_id> sid_in, bool parse_local)
{

        CRef<CSeq_id> retval(new CSeq_id());

        // Local ids are usually fake. If a title exists, use the first token
        // of the title instead of the local id. If no title or if the local id
        // should be parsed, use the local id, but without the "lcl|" prefix.
        if (sid_in->IsLocal()) {
            string id_token;
            vector<string> title_tokens;
            title_tokens = 
                NStr::Split(CAlignFormatUtil::GetTitle(bh), " ", title_tokens);
            if(title_tokens.empty()){
                id_token = NcbiEmptyString;
            } else {
                id_token = title_tokens[0];
            }
            
            if (id_token == NcbiEmptyString || parse_local) {
                const CObject_id& obj_id = sid_in->GetLocal();
                if (obj_id.IsStr())
                    id_token = obj_id.GetStr();
                else 
                    id_token = NStr::IntToString(obj_id.GetId());
            }
            CObject_id* obj_id = new CObject_id();
            obj_id->SetStr(id_token);
            retval->SetLocal(*obj_id);
        } else {
            retval->Assign(*sid_in);
        }

        return retval;
}

void CBlastTabularInfo::SetQueryId(const CBioseq_Handle& bh)
{
    m_QueryId.clear();

    // Create a new list of Seq-ids, substitute any local ids by new fake local 
    // ids, with label set to the first token of this Bioseq's title.
    ITERATE(CBioseq_Handle::TId, itr, bh.GetId()) {
        CRef<CSeq_id> next_id = s_ReplaceLocalId(bh, itr->GetSeqId(), m_ParseLocalIds);
        m_QueryId.push_back(next_id);
    }
}
        
void CBlastTabularInfo::SetSubjectId(const CBioseq_Handle& bh)
{
    m_SubjectId.clear();

    vector<CConstRef<objects::CSeq_id> > subject_id_list;
    ITERATE(CBioseq_Handle::TId, itr, bh.GetId()) {
    	CRef<CSeq_id> next_id = s_ReplaceLocalId(bh, itr->GetSeqId(), !m_ParseSubjectDefline );
    	subject_id_list.push_back(next_id);
    }
    CShowBlastDefline::GetSeqIdList(bh, subject_id_list, m_SubjectId);
}

void CBlastTabularInfo::x_SetSubjectIds(const CBioseq_Handle& bh, const CRef<CBlast_def_line_set> & bdlRef)
{
    m_SubjectIds.clear();

    // Check if this Bioseq handle contains a Blast-def-line-set object.
    // If it does, retrieve Seq-ids from all redundant sequences, and
    // print them separated by commas.
    // Retrieve the CBlast_def_line_set object and save in a CRef, preventing
    // its destruction; then extract the list of CBlast_def_line objects.

    if (bdlRef.NotEmpty() && bdlRef->CanGet() && bdlRef->IsSet() && !bdlRef->Get().empty()){
        vector< CConstRef<CSeq_id> > original_seqids;

        ITERATE(CBlast_def_line_set::Tdata, itr, bdlRef->Get()) {
            original_seqids.clear();
            ITERATE(CBlast_def_line::TSeqid, id, (*itr)->GetSeqid()) {
                original_seqids.push_back(*id);
            }
            list<CRef<objects::CSeq_id> > next_seqid_list;
            // Next call replaces BL_ORD_ID if found.
            CShowBlastDefline::GetSeqIdList(bh,original_seqids,next_seqid_list);
            m_SubjectIds.push_back(next_seqid_list);
        }
    } else {
        // Blast-def-line is not filled, hence retrieve all Seq-ids directly
        // from the Bioseq handle's Seq-id.
        list<CRef<objects::CSeq_id> > subject_id_list;
        ITERATE(CBioseq_Handle::TId, itr, bh.GetId()) {
            CRef<CSeq_id> next_id = s_ReplaceLocalId(bh, itr->GetSeqId(), m_ParseLocalIds);
            subject_id_list.push_back(next_id);
        }
        m_SubjectIds.push_back(subject_id_list);
    }


}

bool s_IsValidName(const string & name)
{
	if(name == "-")
		return false;

	if(name == "unclassified")
		return false;

	return true;
}

void CBlastTabularInfo::x_SetTaxInfo(const CBioseq_Handle & handle, const CRef<CBlast_def_line_set> & bdlRef)
{
    m_SubjectTaxId = ZERO_TAX_ID;
    m_SubjectSciName.clear();
    m_SubjectCommonName.clear();
    m_SubjectBlastName.clear();
    m_SubjectSuperKingdom.clear();

    if (bdlRef.NotEmpty() && bdlRef->CanGet() && bdlRef->IsSet() && !bdlRef->Get().empty()){
    	ITERATE(CBlast_def_line_set::Tdata, itr, bdlRef->Get()) {
            if((*itr)->IsSetTaxid()) {
                    if((*itr)->GetTaxid() != ZERO_TAX_ID) {
                    	m_SubjectTaxId = (*itr)->GetTaxid();
                    	break;
                    }
            }
    	}
    }

    if(m_SubjectTaxId == ZERO_TAX_ID) {
          m_SubjectTaxId = sequence::GetTaxId(handle);
    }

    if(m_SubjectTaxId == ZERO_TAX_ID)
            return;

    if( x_IsFieldRequested(eSubjectSciName) ||
        x_IsFieldRequested(eSubjectCommonName) ||
        x_IsFieldRequested(eSubjectBlastName) ||
        x_IsFieldRequested(eSubjectSuperKingdom)) {

        try {
        	SSeqDBTaxInfo taxinfo;
            CSeqDB::GetTaxInfo(m_SubjectTaxId, taxinfo);
            m_SubjectSciName = taxinfo.scientific_name;
            m_SubjectCommonName = taxinfo.common_name;
            if(s_IsValidName(taxinfo.blast_name)) {
            	m_SubjectBlastName = taxinfo.blast_name;
            }

            if(s_IsValidName(taxinfo.s_kingdom)) {
            	m_SubjectSuperKingdom = taxinfo.s_kingdom;
            }

        } catch (const CException&) {
        	//only put fillers in if we are going to show tax id
            // the fillers are put in so that the name list would
            // match the taxid list
            if(x_IsFieldRequested(eSubjectTaxIds) ) {
            	m_SubjectSciName = NA;
                m_SubjectCommonName = NA;
            }
        }
    }
    return;
}

void CBlastTabularInfo::x_SetTaxInfoAll(const CBioseq_Handle & handle, const CRef<CBlast_def_line_set> & bdlRef)
{
    m_SubjectTaxIds.clear();
    m_SubjectSciNames.clear();
    m_SubjectCommonNames.clear();
    m_SubjectBlastNames.clear();
    m_SubjectSuperKingdoms.clear();

    if (bdlRef.NotEmpty() && bdlRef->CanGet() && bdlRef->IsSet() && !bdlRef->Get().empty()){
    	ITERATE(CBlast_def_line_set::Tdata, itr, bdlRef->Get()) {
    		CBlast_def_line::TTaxIds t = (*itr)->GetTaxIds();
    		m_SubjectTaxIds.insert(t.begin(), t.end());
    	}
    }

    if(m_SubjectTaxIds.empty()) {
            CSeqdesc_CI desc_s(handle, CSeqdesc::e_Source);
            for (;desc_s; ++desc_s) {
                     TTaxId t = desc_s->GetSource().GetOrg().GetTaxId();
                     if(t != ZERO_TAX_ID) {
                    	 m_SubjectTaxIds.insert(t);
                     }
            }

            CSeqdesc_CI desc(handle, CSeqdesc::e_Org);
            for (; desc; ++desc) {
                    TTaxId t= desc->GetOrg().GetTaxId();
                    if(t != ZERO_TAX_ID) {
                    	m_SubjectTaxIds.insert(t);
                    }
            }
    }

    if(m_SubjectTaxIds.empty())
            return;

    if( x_IsFieldRequested(eSubjectSciNames) ||
        x_IsFieldRequested(eSubjectCommonNames) ||
        x_IsFieldRequested(eSubjectBlastNames) ||
        x_IsFieldRequested(eSubjectSuperKingdoms)) {
            set<TTaxId>::iterator itr = m_SubjectTaxIds.begin();

            for(; itr !=  m_SubjectTaxIds.end(); ++itr) {
            	try {
                    SSeqDBTaxInfo taxinfo;
                    CSeqDB::GetTaxInfo(*itr, taxinfo);
                    m_SubjectSciNames.push_back(taxinfo.scientific_name);
                    m_SubjectCommonNames.push_back(taxinfo.common_name);
                    if(s_IsValidName(taxinfo.blast_name))
                            m_SubjectBlastNames.insert(taxinfo.blast_name);

                    if(s_IsValidName(taxinfo.s_kingdom))
                            m_SubjectSuperKingdoms.insert(taxinfo.s_kingdom);

            	} catch (const CException&) {
                    //only put fillers in if we are going to show tax id
                    // the fillers are put in so that the name list would
                    // match the taxid list
                    if(x_IsFieldRequested(eSubjectTaxIds) ) {
                            m_SubjectSciNames.push_back(NA);
                            m_SubjectCommonNames.push_back(NA);
                    }
            	}
            }
    }
    return;
}

void CBlastTabularInfo::x_SetQueryCovSubject(const CSeq_align & align)
{
	int pct = -1;
	if(align.GetNamedScore("seq_percent_coverage", pct))
	{
		m_QueryCovSubject.first = align.GetSeq_id(1).AsFastaString();
		m_QueryCovSubject.second = pct;
	}
	else if(align.GetSeq_id(1).AsFastaString() != m_QueryCovSubject.first)
	{
		m_QueryCovSubject.first = NA;
		m_QueryCovSubject.second = pct;
	}
}

void CBlastTabularInfo::x_SetQueryCovUniqSubject(const CSeq_align & align)
{
	int pct=-1;
	if(align.GetNamedScore("uniq_seq_percent_coverage", pct))
	{
		m_QueryCovUniqSubject.first = align.GetSeq_id(1).AsFastaString();
		m_QueryCovUniqSubject.second = pct;
	}
	else if(align.GetSeq_id(1).AsFastaString() != m_QueryCovUniqSubject.first)
	{
		m_QueryCovUniqSubject.first = NA;
		m_QueryCovUniqSubject.second = pct;
	}
}

void CBlastTabularInfo::x_SetQueryCovSeqalign(const CSeq_align & align, int query_len)
{
	double tmp = 0;
	if(!align.GetNamedScore("hsp_percent_coverage", tmp)) {
		int len = abs((int) (align.GetSeqStop(0) - align.GetSeqStart(0))) + 1;
		tmp = 100.0 * len/(double) query_len;
		if(tmp  < 99)
			tmp +=0.5;
	}
	m_QueryCovSeqalign = (int)tmp;
}

int CBlastTabularInfo::SetFields(const CSeq_align& align, 
                                 CScope& scope, 
                                 CNcbiMatrix<int>* matrix)
{
    const int kQueryRow = 0;
    const int kSubjectRow = 1;

    int num_ident = -1;
    const bool kNoFetchSequence = GetNoFetch();

    // First reset all fields.
    x_ResetFields();

    if (x_IsFieldRequested(eEvalue) || 
        x_IsFieldRequested(eBitScore) ||
        x_IsFieldRequested(eScore) ||
        x_IsFieldRequested(eNumIdentical) ||
        x_IsFieldRequested(eMismatches) ||
        x_IsFieldRequested(ePercentIdentical)) {
        int score = 0, sum_n = 0;
        double bit_score = .0, evalue = .0;
        list<TGi> use_this_gi;
        CAlignFormatUtil::GetAlnScores(align, score, bit_score, evalue, sum_n, 
                                       num_ident, use_this_gi);
        SetScores(score, bit_score, evalue);
    }

    bool bioseqs_found = true;
    // Extract the full query id from the correspondintg Bioseq handle.
    if (x_IsFieldRequested(eQuerySeqId) || x_IsFieldRequested(eQueryGi) ||
        x_IsFieldRequested(eQueryAccession) ||
        x_IsFieldRequested(eQueryAccessionVersion) ||
        x_IsFieldRequested(eQueryCovSeqalign)) {
        try {
            // FIXME: do this only if the query has changed
            const CBioseq_Handle& query_bh = 
                scope.GetBioseqHandle(align.GetSeq_id(0));
            SetQueryId(query_bh);
            if(m_QueryRange.NotEmpty())
            	m_QueryLength = m_QueryRange.GetLength();
            else
            	m_QueryLength = query_bh.GetBioseqLength();
            x_SetQueryCovSeqalign(align, m_QueryLength);
        } catch (const CException&) {
            list<CRef<CSeq_id> > query_ids;
            CRef<CSeq_id> id(new CSeq_id());
            id->Assign(align.GetSeq_id(0));
            query_ids.push_back(id);
            SetQueryId(query_ids);
            bioseqs_found = false;
        }
    }

    if (x_IsFieldRequested(eQueryCovSubject))
    	x_SetQueryCovSubject(align);

    if (x_IsFieldRequested(eQueryCovUniqSubject))
    	x_SetQueryCovUniqSubject(align);

    // Extract the full list of subject ids
    bool setSubjectIds = (x_IsFieldRequested(eSubjectAllSeqIds) ||
    					  x_IsFieldRequested(eSubjectAllGis) ||
                          x_IsFieldRequested(eSubjectAllAccessions));

    bool setSubjectTaxInfo = (x_IsFieldRequested(eSubjectTaxId) ||
        					  x_IsFieldRequested(eSubjectSciName) ||
        					  x_IsFieldRequested(eSubjectCommonName) ||
        					  x_IsFieldRequested(eSubjectBlastName) ||
        					  x_IsFieldRequested(eSubjectSuperKingdom));

    bool setSubjectTaxInfoAll = (x_IsFieldRequested(eSubjectTaxIds) ||
    						     x_IsFieldRequested(eSubjectSciNames) ||
    						     x_IsFieldRequested(eSubjectCommonNames) ||
    						     x_IsFieldRequested(eSubjectBlastNames) ||
    						     x_IsFieldRequested(eSubjectSuperKingdoms));

    bool setSubjectTitle = (x_IsFieldRequested(eSubjectTitle) ||
			  	  	  	  	x_IsFieldRequested(eSubjectAllTitles));

    bool setSubjectId = (x_IsFieldRequested(eSubjectSeqId) ||
    		 	 	 	 x_IsFieldRequested(eSubjectGi) ||
    		 	 	 	 x_IsFieldRequested(eSubjectAccession) ||
    		 	 	 	 x_IsFieldRequested(eSubjAccessionVersion));

    if(setSubjectIds || setSubjectTaxInfo || setSubjectTaxInfoAll || setSubjectTitle ||
       x_IsFieldRequested(eSubjectStrand) || setSubjectId)
    {
        try {
       		const CBioseq_Handle& subject_bh =
                scope.GetBioseqHandle(align.GetSeq_id(1));
            if(setSubjectId) {
            	SetSubjectId(subject_bh);
            }
            m_SubjectLength = subject_bh.GetBioseqLength();

            if(setSubjectIds || setSubjectTaxInfo || setSubjectTitle || setSubjectTaxInfoAll) {
            	CRef<CBlast_def_line_set> bdlRef =
            			CSeqDB::ExtractBlastDefline(subject_bh);
            	if(setSubjectIds) {
            		x_SetSubjectIds(subject_bh, bdlRef);
            	}
            	if(setSubjectTaxInfoAll) {
            		x_SetTaxInfoAll(subject_bh, bdlRef);
            	}
            	if(setSubjectTaxInfo) {
            		x_SetTaxInfo(subject_bh, bdlRef);
            	}
            	if(setSubjectTitle) {
            		m_SubjectDefline.Reset();
            		if(bdlRef.NotEmpty())
            			m_SubjectDefline = bdlRef;
            	}
            }

        } catch (const CException&) {
            list<CRef<CSeq_id> > subject_ids;
            CRef<CSeq_id> id(new CSeq_id());
            id->Assign(align.GetSeq_id(1));
            subject_ids.push_back(id);
            SetSubjectId(subject_ids);
            bioseqs_found = false;
        }

    }

    // If Bioseq has not been found for one or both of the sequences, all
    // subsequent computations cannot proceed. Hence don't set any of the other 
    // fields.
    if (!bioseqs_found)
        return -1;

    if (x_IsFieldRequested(eQueryLength) && m_QueryLength == 0) {
        //_ASSERT(!m_QueryId.empty());
        //_ASSERT(m_QueryId.front().NotEmpty());
        //m_QueryLength = sequence::GetLength(*m_QueryId.front(), &scope);
    	if(m_QueryRange.NotEmpty())
    		m_QueryLength = m_QueryRange.GetLength();
    	else
    		m_QueryLength = sequence::GetLength(align.GetSeq_id(0), &scope);

    }

    if (x_IsFieldRequested(eSubjectLength) && m_SubjectLength == 0) {
        //_ASSERT(!m_SubjectIds.empty());
        //_ASSERT(!m_SubjectIds.front().empty());
        //_ASSERT(!m_SubjectIds.front().front().NotEmpty());
        //m_SubjectLength = sequence::GetLength(*m_SubjectIds.front().front(),
        //                                      &scope);
        m_SubjectLength = sequence::GetLength(align.GetSeq_id(1), &scope);
    }

    if (x_IsFieldRequested(eQueryStart) || x_IsFieldRequested(eQueryEnd) ||
        x_IsFieldRequested(eSubjectStart) || x_IsFieldRequested(eSubjectEnd) ||
        x_IsFieldRequested(eAlignmentLength) || x_IsFieldRequested(eGaps) ||
        x_IsFieldRequested(eGapOpenings) || x_IsFieldRequested(eQuerySeq) ||
        x_IsFieldRequested(eSubjectSeq) || (x_IsFieldRequested(eNumIdentical) && num_ident > 0 ) ||
        x_IsFieldRequested(ePositives) || x_IsFieldRequested(eMismatches) || 
        x_IsFieldRequested(ePercentPositives) || x_IsFieldRequested(ePercentIdentical) ||
        x_IsFieldRequested(eQueryFrame) || x_IsFieldRequested(eBTOP) ||
        x_IsFieldRequested(eSubjectStrand)) {

    CRef<CSeq_align> finalAln(0);
   
    // Convert Std-seg and Dense-diag alignments to Dense-seg.
    // Std-segs are produced only for translated searches; Dense-diags only for 
    // ungapped, not translated searches.
    const bool kTranslated = align.GetSegs().IsStd();
    bool query_is_na = CSeq_inst::IsNa(scope.GetSequenceType(align.GetSeq_id(0)));
    bool subject_is_na = CSeq_inst::IsNa(scope.GetSequenceType(align.GetSeq_id(1)));
    if (kTranslated) {
        CRef<CSeq_align> densegAln = align.CreateDensegFromStdseg();
        // When both query and subject are translated, i.e. tblastx, convert
        // to a special type of Dense-seg.
        if (query_is_na && subject_is_na) {
            finalAln = densegAln->CreateTranslatedDensegFromNADenseg();
        }
        else {
            finalAln = densegAln;
        }
    } else if (align.GetSegs().IsDendiag()) {
        finalAln = CAlignFormatUtil::CreateDensegFromDendiag(align);
    }

    const CDense_seg& ds = (finalAln ? finalAln->GetSegs().GetDenseg() :
                            align.GetSegs().GetDenseg());

    /// @todo code to create CAlnVec is the same as the one used in
    /// blastxml_format.cpp (s_SeqAlignSetToXMLHsps) and also
    /// CDisplaySeqalign::x_GetAlnVecForSeqalign(), these should be refactored
    /// into a single function, possibly in CAlignFormatUtil. Note that
    /// CAlignFormatUtil::GetPercentIdentity() and
    /// CAlignFormatUtil::GetAlignmentLength() also use a similar logic...
    /// @sa s_SeqAlignSetToXMLHsps
    /// @sa CDisplaySeqalign::x_GetAlnVecForSeqalign
    CRef<CAlnVec> alnVec;

    // For non-translated reverse strand alignments, show plus strand on 
    // query and minus strand on subject. To accomplish this, Dense-seg must
    // be reversed.
    if (!kTranslated && ds.IsSetStrands() && 
        ds.GetStrands().front() == eNa_strand_minus) {
        CRef<CDense_seg> reversed_ds(new CDense_seg);
        reversed_ds->Assign(ds);
        reversed_ds->Reverse();
        alnVec.Reset(new CAlnVec(*reversed_ds, scope));   
    } else {
        alnVec.Reset(new CAlnVec(ds, scope));
    }    

    alnVec->SetAaCoding(CSeq_data::e_Ncbieaa);

    int align_length = 0, num_gaps = 0, num_gap_opens = 0;
    if (x_IsFieldRequested(eAlignmentLength) ||
        x_IsFieldRequested(eGaps) ||
        x_IsFieldRequested(eMismatches) ||
        x_IsFieldRequested(ePercentPositives) ||
        x_IsFieldRequested(ePercentIdentical) ||
        x_IsFieldRequested(eGapOpenings)) {
        CAlignFormatUtil::GetAlignLengths(*alnVec, align_length, num_gaps, 
                                          num_gap_opens);
    }

    int num_positives = 0;
    
    if (x_IsFieldRequested(eQuerySeq) || 
        x_IsFieldRequested(eSubjectSeq) ||
        x_IsFieldRequested(ePositives) ||
        x_IsFieldRequested(ePercentPositives) ||
        x_IsFieldRequested(eBTOP) ||
        (x_IsFieldRequested(eNumIdentical) && !kNoFetchSequence) ||
        (x_IsFieldRequested(eMismatches) && !kNoFetchSequence) ||
        (x_IsFieldRequested(ePercentIdentical) && !kNoFetchSequence)) {

        alnVec->SetGapChar('-');
        alnVec->SetGenCode(m_QueryGeneticCode, 0);
        alnVec->SetGenCode(m_DbGeneticCode, 1);
        alnVec->GetWholeAlnSeqString(0, m_QuerySeq);
        alnVec->GetWholeAlnSeqString(1, m_SubjectSeq);
        
        if (x_IsFieldRequested(ePositives) ||
            x_IsFieldRequested(ePercentPositives) ||
            x_IsFieldRequested(eBTOP) ||
            x_IsFieldRequested(eNumIdentical) ||
            x_IsFieldRequested(eMismatches) ||
            x_IsFieldRequested(ePercentIdentical)) {

            string btop_string = "";
            int num_matches = 0;
            num_ident = 0;
            // The query and subject sequence strings must be the same size in a correct
            // alignment, but if alignment extends beyond the end of sequence because of
            // a bug, one of the sequence strings may be truncated, hence it is 
            // necessary to take a minimum here.
            /// @todo FIXME: Should an exception be thrown instead? 
            for (unsigned int i = 0; 
                 i < min(m_QuerySeq.size(), m_SubjectSeq.size());
                 ++i) {
                if (m_QuerySeq[i] == m_SubjectSeq[i]) {
                    ++num_ident;
                    ++num_positives;
                    ++num_matches;
                } else {
                    if(num_matches > 0) {
                        btop_string +=  NStr::Int8ToString(num_matches);
                        num_matches=0; 
                    }
                    btop_string += m_QuerySeq[i];
                    btop_string += m_SubjectSeq[i];
                    if (matrix && !matrix->GetData().empty() &&
                           (*matrix)(m_QuerySeq[i], m_SubjectSeq[i]) > 0) {
                        ++num_positives;
                    }
                }
            }

            if (num_matches > 0) {
                btop_string +=  NStr::Int8ToString(num_matches);
            }
            SetBTOP(btop_string);
        }
    } 

    int q_start = 0, q_end = 0, s_start = 0, s_end = 0;
    if (x_IsFieldRequested(eQueryStart) || x_IsFieldRequested(eQueryEnd) ||
        x_IsFieldRequested(eQueryFrame) || x_IsFieldRequested(eFrames)) {
        // For translated search, for a negative query frame, reverse its start
        // and end offsets.
        if (kTranslated && ds.GetSeqStrand(kQueryRow) == eNa_strand_minus) {
            q_start = alnVec->GetSeqStop(kQueryRow) + 1;
            q_end = alnVec->GetSeqStart(kQueryRow) + 1;
        } else {
            q_start = alnVec->GetSeqStart(kQueryRow) + 1;
            q_end = alnVec->GetSeqStop(kQueryRow) + 1;
        }
    }

    if (x_IsFieldRequested(eSubjectStart) || x_IsFieldRequested(eSubjectEnd) ||
        x_IsFieldRequested(eSubjFrame) || x_IsFieldRequested(eFrames) ||
        x_IsFieldRequested(eSubjectStrand)) {
        // If subject is on a reverse strand, reverse its start and end
        // offsets. Also do that for a nucleotide-nucleotide search, if query
        // is on the reverse strand, because BLAST output always reverses
        // subject, not query.
        if (ds.GetSeqStrand(kSubjectRow) == eNa_strand_minus ||
            (!kTranslated && ds.GetSeqStrand(kQueryRow) == eNa_strand_minus)) {
            s_end = alnVec->GetSeqStart(kSubjectRow) + 1;
            s_start = alnVec->GetSeqStop(kSubjectRow) + 1;
        } else {
            s_start = alnVec->GetSeqStart(kSubjectRow) + 1;
            s_end = alnVec->GetSeqStop(kSubjectRow) + 1;
        }

        if(x_IsFieldRequested(eSubjectStrand))
        {
        	if(!subject_is_na)
        		m_SubjectStrand = NA;
        	else
        		m_SubjectStrand = ((s_start - s_end) > 0 )? "minus":"plus";
        }
    }
    SetEndpoints(q_start, q_end, s_start, s_end);
    
    int query_frame = 1, subject_frame = 1;
    if (kTranslated) {
        if (x_IsFieldRequested(eQueryFrame) || x_IsFieldRequested(eFrames)) {
            query_frame = CAlignFormatUtil::
                GetFrame (q_start - 1, ds.GetSeqStrand(kQueryRow), 
                          scope.GetBioseqHandle(align.GetSeq_id(0)));
        }
    
        if (x_IsFieldRequested(eSubjFrame) || x_IsFieldRequested(eFrames)) {
            subject_frame = CAlignFormatUtil::
                GetFrame (s_start - 1, ds.GetSeqStrand(kSubjectRow), 
                          scope.GetBioseqHandle(align.GetSeq_id(1)));
        }

    }
    else {
    	if (x_IsFieldRequested(eSubjFrame) || x_IsFieldRequested(eFrames)) {
    		if ((s_start - s_end) > 0 ) {
    			subject_frame = -1;
    		}
    	}
    }
    SetCounts(num_ident, align_length, num_gaps, num_gap_opens, num_positives,
              query_frame, subject_frame);
    }

    return 0;
}

void CBlastTabularInfo::Print() 
{
    ITERATE(list<ETabularField>, iter, m_FieldsToShow) {
        // Add tab in front of field, except for the first field.
        if (iter != m_FieldsToShow.begin())
            m_Ostream << m_FieldDelimiter;
        x_PrintField(*iter);
    }
    m_Ostream << "\n";
}

void CBlastTabularInfo::PrintFieldNames(bool is_csv /* = false */)
{
    if (!is_csv) {
      m_Ostream << "# Fields: ";
    }

    ITERATE(list<ETabularField>, iter, m_FieldsToShow) {
        if (iter != m_FieldsToShow.begin()) {
            if (is_csv) {
                m_Ostream << m_FieldDelimiter;
            }
            else {
                m_Ostream << ", ";
            }
        }
            
        switch (*iter) {
        case eQuerySeqId:
            m_Ostream << "query id"; break;
        case eQueryGi:
            m_Ostream << "query gi"; break;
        case eQueryAccession:
            m_Ostream << "query acc."; break;
        case eQueryAccessionVersion:
            m_Ostream << "query acc.ver"; break;
        case eQueryLength:
            m_Ostream << "query length"; break;
        case eSubjectSeqId:
            m_Ostream << "subject id"; break;
        case eSubjectAllSeqIds:
            m_Ostream << "subject ids"; break;
        case eSubjectGi:
            m_Ostream << "subject gi"; break;
        case eSubjectAllGis:
            m_Ostream << "subject gis"; break;
        case eSubjectAccession:
            m_Ostream << "subject acc."; break;
        case eSubjAccessionVersion:
            m_Ostream << "subject acc.ver"; break;
        case eSubjectAllAccessions:
            m_Ostream << "subject accs."; break;
        case eSubjectLength:
            m_Ostream << "subject length"; break;
        case eQueryStart:
            m_Ostream << "q. start"; break;
        case eQueryEnd:
            m_Ostream << "q. end"; break;
        case eSubjectStart:
            m_Ostream << "s. start"; break;
        case eSubjectEnd:
            m_Ostream << "s. end"; break;
        case eQuerySeq:
            m_Ostream << "query seq"; break;
        case eSubjectSeq:
            m_Ostream << "subject seq"; break;
        case eEvalue:
            m_Ostream << "evalue"; break;
        case eBitScore:
            m_Ostream << "bit score"; break;
        case eScore:
            m_Ostream << "score"; break;
        case eAlignmentLength:
            m_Ostream << "alignment length"; break;
        case ePercentIdentical:
            m_Ostream << "% identity"; break;
        case eNumIdentical:
            m_Ostream << "identical"; break;
        case eMismatches:
            m_Ostream << "mismatches"; break;
        case ePositives:
            m_Ostream << "positives"; break;
        case eGapOpenings:
            m_Ostream << "gap opens"; break;
        case eGaps:
            m_Ostream << "gaps"; break;
        case ePercentPositives:
            m_Ostream << "% positives"; break;
        case eFrames:
            m_Ostream << "query/sbjct frames"; break; 
        case eQueryFrame:
            m_Ostream << "query frame"; break; 
        case eSubjFrame:
            m_Ostream << "sbjct frame"; break; 
        case eBTOP:
            m_Ostream << "BTOP"; break;        
        case eSubjectTaxIds:
            m_Ostream << "subject tax ids"; break;
        case eSubjectSciNames:
        	m_Ostream << "subject sci names"; break;
        case eSubjectCommonNames:
        	m_Ostream << "subject com names"; break;
        case eSubjectBlastNames:
        	m_Ostream << "subject blast names"; break;
        case eSubjectSuperKingdoms:
        	m_Ostream << "subject super kingdoms"; break;
        case eSubjectTaxId:
            m_Ostream << "subject tax id"; break;
        case eSubjectSciName:
            m_Ostream << "subject sci name"; break;
        case eSubjectCommonName:
            m_Ostream << "subject com names"; break;
        case eSubjectBlastName:
           	m_Ostream << "subject blast name"; break;
        case eSubjectSuperKingdom:
               	m_Ostream << "subject super kingdom"; break;
        case eSubjectTitle:
        	m_Ostream << "subject title"; break;
        case eSubjectAllTitles:
        	m_Ostream << "subject titles"; break;
        case eSubjectStrand:
        	m_Ostream << "subject strand"; break;
        case eQueryCovSubject:
        	m_Ostream << "% query coverage per subject"; break;
        case eQueryCovUniqSubject:
        	m_Ostream << "% query coverage per uniq subject"; break;
        case eQueryCovSeqalign:
        	m_Ostream << "% query coverage per hsp"; break;
        default:
            _ASSERT(false);
            break;
        }
    }

    m_Ostream << "\n";
}


void CBlastTabularInfo::PrintFieldSpecs(void)
{
    for (auto& it: m_FieldsToShow) {
        if (it != m_FieldsToShow.front()) {
            m_Ostream << m_FieldDelimiter;
        }
        // sc_FormatSpecifiers is ordered by field, but the ordering is not
        // enforced. First assume that sc_FormatSpecifiers is ordered.
        if (sc_FormatSpecifiers[it].field == it) {
            m_Ostream << sc_FormatSpecifiers[it].name;
        }
        else {
            // If the array element is not in the expected location, then
            // search we need to search for it.
            for (size_t i=0; i < kNumTabularOutputFormatSpecifiers;i++) {
                if (sc_FormatSpecifiers[i].field == it) {
                    m_Ostream << sc_FormatSpecifiers[i].name;
                    break;
                }
            }
        }
    }
    m_Ostream << "\n";
}


/// @todo FIXME add means to specify masked database (SB-343)
void 
CBlastTabularInfo::PrintHeader(const string& program_version, 
       const CBioseq& bioseq, 
       const string& dbname, 
       const string& rid /* = kEmptyStr */,
       unsigned int iteration /* = numeric_limits<unsigned int>::max() */,
       const CSeq_align_set* align_set /* = 0 */,
       CConstRef<CBioseq> subj_bioseq /* = CConstRef<CBioseq>() */,
       bool is_csv /* = false */)
{
    x_PrintQueryAndDbNames(program_version, bioseq, dbname, rid, iteration, subj_bioseq);
    // Print number of alignments found, but only if it has been set.
    if (align_set) {
       int num_hits = align_set->Get().size();
       if (num_hits != 0) {
           PrintFieldNames(is_csv);
       }
       m_Ostream << "# " << num_hits << " hits found" << "\n";
    }
}

void 
CBlastTabularInfo::x_PrintQueryAndDbNames(const string& program_version,
       const CBioseq& bioseq,
       const string& dbname,
       const string& rid,
       unsigned int iteration,
       CConstRef<CBioseq> subj_bioseq)
{
    m_Ostream << "# ";
    m_Ostream << program_version << "\n";

    if (iteration != numeric_limits<unsigned int>::max())
        m_Ostream << "# Iteration: " << iteration << "\n";

    const size_t kLineLength(0);
    const bool kHtmlFormat(false);
    const bool kTabularFormat(true);

    // Print the query defline with no html; there is no need to set the 
    // line length restriction, since it's ignored for the tabular case.
    CAlignFormatUtil::AcknowledgeBlastQuery(bioseq, kLineLength, m_Ostream, 
                                            m_ParseLocalIds, kHtmlFormat,
                                            kTabularFormat, rid);
    
    if (dbname != kEmptyStr) {
        m_Ostream << "\n# Database: " << dbname << "\n";
    } else {
        _ASSERT(subj_bioseq.NotEmpty());
        m_Ostream << "\n";
        CAlignFormatUtil::AcknowledgeBlastSubject(*subj_bioseq, kLineLength,
                                                  m_Ostream, m_ParseLocalIds,
                                                  kHtmlFormat, kTabularFormat);
        m_Ostream << "\n";
    }
}

void CBlastTabularInfo::PrintNumProcessed(int num_queries)
{
    m_Ostream << "# BLAST processed " << num_queries << " queries\n";
}

void 
CBlastTabularInfo::SetScores(int score, double bit_score, double evalue)
{
    string total_bit_string, raw_score_string;
    m_Score = score;
    CAlignFormatUtil::GetScoreString(evalue, bit_score, 0, score, m_Evalue, 
                                     m_BitScore, total_bit_string, raw_score_string);

    if ((evalue >= 1.0e-180) && (evalue < 0.0009)){
    	m_Evalue = NStr::DoubleToString(evalue, 2, NStr::fDoubleScientific);
    }
}

void 
CBlastTabularInfo::SetEndpoints(int q_start, int q_end, int s_start, int s_end)
{
    m_QueryStart = q_start;
    m_QueryEnd = q_end;
    m_SubjectStart = s_start;
    m_SubjectEnd = s_end;    
}

void 
CBlastTabularInfo::SetBTOP(string BTOP)
{
    m_BTOP = BTOP;
}

void 
CBlastTabularInfo::SetCounts(int num_ident, int length, int gaps, int gap_opens,
                             int positives, int query_frame, int subject_frame)
{
    m_AlignLength = length;
    m_NumIdent = num_ident;
    m_NumGaps = gaps;
    m_NumGapOpens = gap_opens;
    m_NumPositives = positives;
    m_QueryFrame = query_frame;
    m_SubjectFrame = subject_frame;
}

void
CBlastTabularInfo::SetQueryId(list<CRef<CSeq_id> >& id)
{
    m_QueryId = id;
}

void 
CBlastTabularInfo::SetSubjectId(list<CRef<CSeq_id> >& id)
{
    m_SubjectIds.push_back(id);
}

list<string> 
CBlastTabularInfo::GetAllFieldNames(void)
{
    list<string> field_names;

    for (map<string, ETabularField>::iterator iter = m_FieldMap.begin();
         iter != m_FieldMap.end(); ++iter) {
        field_names.push_back((*iter).first);
    }
    return field_names;
}

void 
CBlastTabularInfo::x_AddFieldToShow(ETabularField field)
{
    if ( !x_IsFieldRequested(field) ) {
        m_FieldsToShow.push_back(field);
    }
}

void 
CBlastTabularInfo::x_DeleteFieldToShow(ETabularField field)
{
    list<ETabularField>::iterator iter; 

    while ((iter = find(m_FieldsToShow.begin(), m_FieldsToShow.end(), field))
           != m_FieldsToShow.end())
        m_FieldsToShow.erase(iter); 
}

void 
CBlastTabularInfo::x_PrintField(ETabularField field)
{
    switch (field) {
    case eQuerySeqId:
        x_PrintQuerySeqId(); break;
    case eQueryGi:
        x_PrintQueryGi(); break;
    case eQueryAccession:
        x_PrintQueryAccession(); break;
    case eQueryAccessionVersion:
        x_PrintQueryAccessionVersion(); break;
    case eQueryLength:
        x_PrintQueryLength(); break;
    case eSubjectSeqId:
        x_PrintSubjectSeqId(); break;
    case eSubjectAllSeqIds:
        x_PrintSubjectAllSeqIds(); break;
    case eSubjectGi:
        x_PrintSubjectGi(); break;
    case eSubjectAllGis:
        x_PrintSubjectAllGis(); break;
    case eSubjectAccession:
        x_PrintSubjectAccession(); break;
    case eSubjAccessionVersion:
        x_PrintSubjectAccessionVersion(); break;
    case eSubjectAllAccessions:
        x_PrintSubjectAllAccessions(); break;
    case eSubjectLength:
        x_PrintSubjectLength(); break;
    case eQueryStart:
        x_PrintQueryStart(); break;
    case eQueryEnd:
        x_PrintQueryEnd(); break;
    case eSubjectStart:
        x_PrintSubjectStart(); break;
    case eSubjectEnd:
        x_PrintSubjectEnd(); break;
    case eQuerySeq:
        x_PrintQuerySeq(); break;
    case eSubjectSeq:
        x_PrintSubjectSeq(); break;
    case eEvalue:
        x_PrintEvalue(); break;
    case eBitScore:
        x_PrintBitScore(); break;
    case eScore:
        x_PrintScore(); break;
    case eAlignmentLength:
        x_PrintAlignmentLength(); break;
    case ePercentIdentical:
        x_PrintPercentIdentical(); break;
    case eNumIdentical:
        x_PrintNumIdentical(); break;
    case eMismatches:
        x_PrintMismatches(); break;
    case ePositives:
        x_PrintNumPositives(); break;
    case eGapOpenings:
        x_PrintGapOpenings(); break;
    case eGaps:
        x_PrintGaps(); break;
    case ePercentPositives:
        x_PrintPercentPositives(); break;
    case eFrames:
        x_PrintFrames(); break;
    case eQueryFrame:
        x_PrintQueryFrame(); break;
    case eSubjFrame:
        x_PrintSubjectFrame(); break;        
    case eBTOP:
        x_PrintBTOP(); break;        
    case eSubjectTaxIds:
    	x_PrintSubjectTaxIds(); break;
    case eSubjectTaxId:
    	x_PrintSubjectTaxId(); break;
    case eSubjectSciNames:
    	x_PrintSubjectSciNames(); break;
    case eSubjectSciName:
    	x_PrintSubjectSciName(); break;
    case eSubjectCommonNames:
    	x_PrintSubjectCommonNames(); break;
    case eSubjectCommonName:
    	x_PrintSubjectCommonName(); break;
    case eSubjectBlastNames:
    	x_PrintSubjectBlastNames(); break;
    case eSubjectBlastName:
    	x_PrintSubjectBlastName(); break;
    case eSubjectSuperKingdoms:
    	x_PrintSubjectSuperKingdoms(); break;
    case eSubjectSuperKingdom:
    	x_PrintSubjectSuperKingdom(); break;
    case eSubjectTitle:
    	x_PrintSubjectTitle(); break;
    case eSubjectAllTitles:
    	x_PrintSubjectAllTitles(); break;
    case eSubjectStrand:
    	x_PrintSubjectStrand(); break;
    case eQueryCovSubject:
    	x_PrintSubjectCoverage(); break;
    case eQueryCovUniqSubject:
    	x_PrintUniqSubjectCoverage(); break;
    case eQueryCovSeqalign:
    	x_PrintSeqalignCoverage(); break;
    default:
        _ASSERT(false);
        break;
    }
}

/// @todo FIXME add means to specify masked database (SB-343)
void 
CIgBlastTabularInfo::PrintHeader(const CConstRef<blast::CIgBlastOptions>& ig_opts,
                                 const string& program_version, 
                                 const CBioseq& bioseq, 
                                 const string& dbname, 
                                 const string& domain_sys,
                                 const string& rid /* = kEmptyStr */,
                                 unsigned int iteration /* = numeric_limits<unsigned int>::max() */,
                                 const CSeq_align_set* align_set /* = 0 */,
                                 CConstRef<CBioseq> subj_bioseq /* = CConstRef<CBioseq>() */)
{
    x_PrintQueryAndDbNames(program_version, bioseq, dbname, rid, iteration, subj_bioseq);
    m_Ostream << "# Domain classification requested: " << domain_sys << endl;
    // Print number of alignments found, but only if it has been set.
    if (align_set) {
        PrintMasterAlign(ig_opts);
       m_Ostream <<  "# Hit table (the first field indicates the chain type of the hit)" << endl;
       int num_hits = align_set->Get().size();
       if (num_hits != 0) {
           PrintFieldNames();
       }
       m_Ostream << "# " << num_hits << " hits found" << "\n";
    } else {
       m_Ostream << "# 0 hits found" << "\n";
    }
}

static void s_FillJunctionalInfo (int left_stop, int right_start, int& junction_len, 
                                  string& junction_seq, const string& query_seq) {
    int np_len = 0;
    int np_start = 0;
    if (right_start <= left_stop) { //overlap junction
        np_len = left_stop - right_start + 1;
        np_start = right_start;
        junction_len = 0;
        junction_seq = "(" + query_seq.substr(np_start, np_len) + ")";
                    
                    
    } else {
        np_len = right_start - left_stop - 1;
        junction_len = np_len;
        if (np_len >= 1) {
            np_start = left_stop + 1;
            junction_seq = query_seq.substr(np_start, np_len);
        }
    }
    
}

static void s_GetCigarString(const CSeq_align& align, string& cigar, int query_len, CScope& scope) {

    cigar = NcbiEmptyString;

    if (align.GetSegs().Which() == CSeq_align::TSegs::e_Denseg) {
        const CDense_seg& denseg = align.GetSegs().GetDenseg();
        const CDense_seg::TStarts& starts = denseg.GetStarts();
        const CDense_seg::TLens& lens = denseg.GetLens();
        CRange<TSeqPos> qrange = align.GetSeqRange(0);
        CRange<TSeqPos> srange = align.GetSeqRange(1);
        const CBioseq_Handle& subject_handle = scope.GetBioseqHandle(align.GetSeq_id(1));
        int subject_len = subject_handle.GetBioseqLength();
        //query
        if (align.GetSeqStrand(0) == eNa_strand_plus) {
            if (qrange.GetFrom() > 0) {
                cigar += NStr::IntToString(qrange.GetFrom());
                cigar += "S";
            }
        }
        else {
            if ((int)qrange.GetToOpen() < query_len) {
                cigar += NStr::IntToString(query_len - qrange.GetToOpen());
                cigar += "S";
            }
        }
        //subject
        if (align.GetSeqStrand(1) == eNa_strand_plus) {
            if (srange.GetFrom() > 0) {
                cigar += NStr::IntToString(srange.GetFrom());
                cigar += "N";
            }
        }
        else {
            if ((int)srange.GetToOpen() < subject_len) {
                cigar += NStr::IntToString(subject_len - srange.GetToOpen());
                cigar += "N";
            }
        }
        for (size_t i=0;i < starts.size();i+=2) {
            cigar += NStr::IntToString(lens[i/2]);
            if (starts[i] >= 0 && starts[i + 1] >= 0) {
                cigar += "M";
            }
            else if (starts[i] < 0) {
                if (lens[i/2] < 10) {
                    cigar += "D";
                }
                else {
                    cigar += "N";
                }
            }
            else {
                cigar += "I";
            }
        }
        if (align.GetSeqStrand(0) == eNa_strand_plus) {
            if ((int)qrange.GetToOpen() < query_len) {
                cigar += NStr::IntToString(query_len - qrange.GetToOpen());
                cigar += "S";
            }
        }
        else {
            if (qrange.GetFrom() > 0) {
                cigar += NStr::IntToString(qrange.GetFrom());
                cigar += "S";
            }
        }
        //subject
        if (align.GetSeqStrand(1) == eNa_strand_plus) {
            if ((int)srange.GetToOpen() < subject_len) {
                cigar += NStr::IntToString(subject_len - srange.GetToOpen());
                cigar += "N";
            }
        }
        else {
            if (srange.GetFrom() > 0) {
                cigar += NStr::IntToString(srange.GetFrom());
                cigar += "N";
            }
        }
    }
}
  
static string s_InsertGap(const string& nuc_without_gap, const string& nuc, const string& prot, char gap_char) {
    SIZE_TYPE new_prot_size = nuc.size()/3 + ((nuc.size()%3 ==2)?1:0);
    string new_prot (new_prot_size, ' ');
    int num_gaps = 0;
    int num_bases = 0;
    int total_inserted_gap = 0;
    int gap_hold = 0;
    for (int i = 0; i < (int)nuc.size(); i++) {
        if (nuc[i] == gap_char) {
            num_gaps ++;
        } else {
            num_bases ++;
        } 
        int index_new_prot = (i+1)/3 - 1;
        int index_original_prot = index_new_prot - total_inserted_gap;

        if (num_gaps == 3) {
            if (index_new_prot < (int)new_prot.size()) {
                total_inserted_gap ++;
                num_gaps = 0;
                if (num_bases == 0) {
                    new_prot[index_new_prot] = gap_char;
                } else {
                    //not add gap yet since it needs to be printed after amino acid is printed
                    gap_hold ++;
                }     
            }
            
        } else if (num_bases == 3) {
            
            index_new_prot -= gap_hold;
            if (index_new_prot < (int)new_prot.size() && index_original_prot < (int)prot.size()) {
                new_prot[index_new_prot] = prot[index_original_prot];
                num_bases = 0;
                if (gap_hold > 0) {
                    for (int j = 0; j < gap_hold; j++) {
                        int position = index_new_prot + 1 + j;
                        if (position < (int) new_prot.size()) {
                            new_prot[position] = gap_char;
                        }
                    }
                    gap_hold = 0;
                }
            }

        }
    }  
    if ((int)nuc_without_gap.size()%3 > 0) {
        if (prot.size() > nuc_without_gap.size()/3) {
            //last two partial bases as toolkit Translate may translate partial codon 
            new_prot[new_prot.size() - 1] = prot[prot.size() - 1];
        } else if (new_prot[new_prot.size() - 1] == ' ') {
            new_prot.pop_back();
        }
    }
    return new_prot;
}
     
static void s_GetGermlineTranslation(const CRef<blast::CIgAnnotation> &annot, CAlnVec& alnvec, 
                                     const string& aligned_query_string, const string& aligned_germline_string,
                                     string& query_translation_string,
                                     string& germline_translation_string){

    if (annot->m_FrameInfo[0] >=0) {
        string aligned_vdj_query = NcbiEmptyString;
        alnvec.GetSeqString(aligned_vdj_query, 0, alnvec.GetSeqStart(0), alnvec.GetSeqStop(0)); 
        int query_trans_offset = ((alnvec.GetSeqStart(0) + 3) - annot->m_FrameInfo[0])%3;
        int query_trans_start =  query_trans_offset > 0?(3 - query_trans_offset):0;

 
        CAlnVec::TResidue gap_char = alnvec.GetGapChar(0);
        string gap_str = NcbiEmptyString;
        gap_str.push_back(gap_char);
        //make sure both query and germline are non-gaps.
        for (int i = query_trans_start; i < (int)aligned_vdj_query.size(); i = i + 3) {
            int query_aln_pos = alnvec.GetAlnPosFromSeqPos(0, alnvec.GetSeqStart(0) + i, CAlnMap::eRight);
            
            if (query_aln_pos < (int)aligned_germline_string.size() && 
                query_aln_pos< (int)aligned_query_string.size() && 
                aligned_germline_string[query_aln_pos] != gap_char &&
                aligned_query_string[query_aln_pos] != gap_char){
                
                string query_translation_template = aligned_query_string.substr(query_aln_pos);
                string final_query_translation_template = NcbiEmptyString;
                
                NStr::Replace(query_translation_template, gap_str, NcbiEmptyString, final_query_translation_template);
                CSeqTranslator::Translate(final_query_translation_template, 
                                          query_translation_string, 
                                          CSeqTranslator::fIs5PrimePartial, NULL, NULL);
                
                query_translation_string = s_InsertGap(final_query_translation_template, query_translation_template, query_translation_string, gap_char);

                string germline_translation_template = aligned_germline_string.substr(query_aln_pos);
                
                //remove internal gap
                string final_germline_translation_template = NcbiEmptyString;
                NStr::Replace(germline_translation_template, gap_str, NcbiEmptyString, final_germline_translation_template);
                CSeqTranslator::Translate(final_germline_translation_template, 
                                          germline_translation_string, 
                                          CSeqTranslator::fIs5PrimePartial, NULL, NULL);
                germline_translation_string = s_InsertGap(final_germline_translation_template, germline_translation_template, germline_translation_string, gap_char);
           
                break; 
            }
        }
    }
}

static void s_SetAirrAlignmentInfo(const CRef<CSeq_align>& align_v,
                                   const CRef<CSeq_align>& align_d,
                                   const CRef<CSeq_align>& align_j,
                                   const CRef<CSeq_align>& align_c,
                                   const CRef<blast::CIgAnnotation> &annot,
                                   CScope& scope,
                                   map<string, string>& airr_data){

    string v_query_alignment = NcbiEmptyString;
    string d_query_alignment = NcbiEmptyString;
    string j_query_alignment = NcbiEmptyString;
    string c_query_alignment = NcbiEmptyString;
    string v_germline_alignment = NcbiEmptyString;
    string d_germline_alignment = NcbiEmptyString;
    string j_germline_alignment = NcbiEmptyString;
    string c_germline_alignment = NcbiEmptyString;
    string v_identity_str = NcbiEmptyString;
    string d_identity_str = NcbiEmptyString;
    string j_identity_str = NcbiEmptyString;
    string c_identity_str = NcbiEmptyString;
 
    
    CAlnMix mix(scope);

    if (align_v) {
        mix.Add(align_v->GetSegs().GetDenseg(), CAlnMix::fPreserveRows);
        double identity = 0;
        string query = NcbiEmptyString;
        string subject = NcbiEmptyString;
        const CDense_seg& ds = (align_v->GetSegs().GetDenseg());
        CAlnVec alnvec(ds, scope);
        alnvec.SetGapChar('-');
        alnvec.GetWholeAlnSeqString(0, query);
        alnvec.GetWholeAlnSeqString(1, subject);

        int num_ident = 0;
        SIZE_TYPE length = min(query.size(), subject.size());

        for (SIZE_TYPE i = 0; i < length; ++i) {
            if (query[i] == subject[i]) {
                ++num_ident;
            }
        }
        if (length > 0) {
            identity = ((double)num_ident)/length;
        }
        NStr::DoubleToString(v_identity_str, identity*100, 3);
        v_query_alignment = query;
        v_germline_alignment = subject;
        s_GetGermlineTranslation(annot, alnvec, v_query_alignment, v_germline_alignment,
                                 airr_data["v_sequence_alignment_aa"], airr_data["v_germline_alignment_aa"]);
    }
    if (align_d) {
        
        mix.Add(align_d->GetSegs().GetDenseg(), CAlnMix::fPreserveRows);
        double identity = 0;
        string query = NcbiEmptyString;
        string subject = NcbiEmptyString;
        const CDense_seg& ds = (align_d->GetSegs().GetDenseg());
        CAlnVec alnvec(ds, scope);
        alnvec.SetGapChar('-');
        alnvec.GetWholeAlnSeqString(0, query);
        alnvec.GetWholeAlnSeqString(1, subject);

        int num_ident = 0;
        SIZE_TYPE length = min(query.size(), subject.size());

        for (SIZE_TYPE i = 0; i < length; ++i) {
            if (query[i] == subject[i]) {
                ++num_ident;
            }
        }
        if (length > 0) {
            identity = ((double)num_ident)/length;
        }
        NStr::DoubleToString(d_identity_str, identity*100, 3);
        d_query_alignment = query;
        d_germline_alignment = subject;
        s_GetGermlineTranslation(annot, alnvec, d_query_alignment, d_germline_alignment,
                                 airr_data["d_sequence_alignment_aa"], airr_data["d_germline_alignment_aa"]);

    }

    if (align_j){
        mix.Add(align_j->GetSegs().GetDenseg(), CAlnMix::fPreserveRows);
        double identity = 0;
        string query = NcbiEmptyString;
        string subject = NcbiEmptyString;
        const CDense_seg& ds = (align_j->GetSegs().GetDenseg());
        CAlnVec alnvec(ds, scope);
        alnvec.SetGapChar('-');
        alnvec.GetWholeAlnSeqString(0, query);
        alnvec.GetWholeAlnSeqString(1, subject);
        
        int num_ident = 0;
        SIZE_TYPE length = min(query.size(), subject.size());
        
        for (SIZE_TYPE i = 0; i < length; ++i) {
            if (query[i] == subject[i]) {
                ++num_ident;
            }
        }
        if (length > 0) {
            identity = ((double)num_ident)/length;
        }
        NStr::DoubleToString(j_identity_str, identity*100, 3);
        j_query_alignment = query;
        j_germline_alignment = subject;
        s_GetGermlineTranslation(annot, alnvec, j_query_alignment, j_germline_alignment,
                                 airr_data["j_sequence_alignment_aa"], airr_data["j_germline_alignment_aa"]);
           
    }

    
    airr_data["v_identity"] = v_identity_str;
    airr_data["d_identity"] = d_identity_str;
    airr_data["j_identity"] = j_identity_str;
   
 
    airr_data["v_sequence_alignment"] = v_query_alignment;
    airr_data["d_sequence_alignment"] = d_query_alignment;
    airr_data["j_sequence_alignment"] = j_query_alignment;
    airr_data["v_germline_alignment"] = v_germline_alignment;
    airr_data["d_germline_alignment"] = d_germline_alignment;
    airr_data["j_germline_alignment"] = j_germline_alignment;
   
  
    //get whole alignment string
    //account for overlapping junction
    string whole_query_alignment = NcbiEmptyString;
    string whole_v_germline_alignment = NcbiEmptyString;
    string whole_d_germline_alignment = NcbiEmptyString;
    string whole_j_germline_alignment = NcbiEmptyString;

    mix.Merge(CAlnMix::fMinGap | CAlnMix::fQuerySeqMergeOnly | 
              CAlnMix::fRemoveLeadTrailGaps | CAlnMix::fFillUnalignedRegions); 
    CAlnVec alnvec(mix.GetDenseg(), scope); 
    alnvec.SetGapChar('-');
    alnvec.GetWholeAlnSeqString(0, whole_query_alignment);
    airr_data["sequence_alignment"] = whole_query_alignment;

    alnvec.GetWholeAlnSeqString(1, whole_v_germline_alignment);
    airr_data["germline_alignment"] += NStr::TruncateSpaces(whole_v_germline_alignment);

    airr_data["v_alignment_start"] = NStr::IntToString(alnvec.GetSeqAlnStart(1) + 1);
    airr_data["v_alignment_end"] = NStr::IntToString(alnvec.GetSeqAlnStop(1) + 1);

    if (align_d) {
        alnvec.GetWholeAlnSeqString(2, whole_d_germline_alignment);
        if (alnvec.GetSeqAlnStart(2) > alnvec.GetSeqAlnStop(1)){
            for (int i = alnvec.GetSeqAlnStop(1) + 1; i < alnvec.GetSeqAlnStart(2); i ++) {
                airr_data["germline_alignment"] += "N";
            }
            airr_data["germline_alignment"] +=  NStr::TruncateSpaces(whole_d_germline_alignment);
        } else {//v-d overlap
           
            int start_pos = min(((int)whole_d_germline_alignment.size() - 1), (int)alnvec.GetSeqAlnStop(1) - (int)alnvec.GetSeqAlnStart(2) + 1);
            string seq = NStr::TruncateSpaces(whole_d_germline_alignment);
            airr_data["germline_alignment"] += seq.substr(start_pos);            
        }
        airr_data["d_alignment_start"] = NStr::IntToString(alnvec.GetSeqAlnStart(2) + 1);
        airr_data["d_alignment_end"] = NStr::IntToString(alnvec.GetSeqAlnStop(2) + 1);

        if (align_j) {
            alnvec.GetWholeAlnSeqString(3, whole_j_germline_alignment);
            if (alnvec.GetSeqAlnStart(3) > alnvec.GetSeqAlnStop(2)) {
                for (int i = alnvec.GetSeqAlnStop(2) + 1; i < alnvec.GetSeqAlnStart(3); i ++) {
                    airr_data["germline_alignment"] += "N";
                }
                airr_data["germline_alignment"] += NStr::TruncateSpaces(whole_j_germline_alignment);
            } else {//d-j overlap
                
                int start_pos = min(((int)whole_j_germline_alignment.size() - 1), (int)alnvec.GetSeqAlnStop(2) - (int)alnvec.GetSeqAlnStart(3) + 1);
                string seq = NStr::TruncateSpaces(whole_j_germline_alignment);
                airr_data["germline_alignment"] += seq.substr(start_pos);
            }
            airr_data["j_alignment_start"] = NStr::IntToString(alnvec.GetSeqAlnStart(3) + 1);
            airr_data["j_alignment_end"] = NStr::IntToString(alnvec.GetSeqAlnStop(3) + 1);

        }
    } else {
        if (align_j) {//light chain
            alnvec.GetWholeAlnSeqString(2, whole_j_germline_alignment);
            if (alnvec.GetSeqAlnStart(2) > alnvec.GetSeqAlnStop(1)) {
                for (int i = alnvec.GetSeqAlnStop(1) + 1; i < alnvec.GetSeqAlnStart(2); i ++) {
                    airr_data["germline_alignment"] += "N";
                }
                airr_data["germline_alignment"] +=  NStr::TruncateSpaces(whole_j_germline_alignment);
            } else { //v-j
               
                int start_pos = min(((int)whole_j_germline_alignment.size() - 1), (int)alnvec.GetSeqAlnStop(1) - (int)alnvec.GetSeqAlnStart(2) + 1);
                string seq = NStr::TruncateSpaces(whole_j_germline_alignment);
                airr_data["germline_alignment"] += seq .substr(start_pos);
            }
            airr_data["j_alignment_start"] = NStr::IntToString(alnvec.GetSeqAlnStart(2) + 1);
            airr_data["j_alignment_end"] = NStr::IntToString(alnvec.GetSeqAlnStop(2) + 1);

        }
    }

    if (align_c) {
      
        double identity = 0;
        string query = NcbiEmptyString;
        string subject = NcbiEmptyString;
        const CDense_seg& ds = (align_c->GetSegs().GetDenseg());
        CAlnVec alnvec_c(ds, scope);
        alnvec_c.SetGapChar('-');
        alnvec_c.GetWholeAlnSeqString(0, query);
        alnvec_c.GetWholeAlnSeqString(1, subject);
        
        int num_ident = 0;
        SIZE_TYPE length = min(query.size(), subject.size());
        
        for (SIZE_TYPE i = 0; i < length; ++i) {
            if (query[i] == subject[i]) {
                ++num_ident;
            }
        }
        if (length > 0) {
            identity = ((double)num_ident)/length;
        }
        NStr::DoubleToString(c_identity_str, identity*100, 3);
        c_query_alignment = query;
        c_germline_alignment = subject;
        s_GetGermlineTranslation(annot, alnvec_c, c_query_alignment, c_germline_alignment,
                                 airr_data["c_sequence_alignment_aa"], airr_data["c_germline_alignment_aa"]);
        
        airr_data["c_identity"] = c_identity_str;
        airr_data["c_sequence_alignment"] = c_query_alignment;
        airr_data["c_germline_alignment"] = c_germline_alignment;

        CAlnMix mix2(scope);
        mix2.Add(align_v->GetSegs().GetDenseg(), CAlnMix::fPreserveRows);
        if (align_d && align_j) {
            mix2.Add(align_d->GetSegs().GetDenseg(), CAlnMix::fPreserveRows);
            mix2.Add(align_j->GetSegs().GetDenseg(), CAlnMix::fPreserveRows);
            mix2.Add(align_c->GetSegs().GetDenseg(), CAlnMix::fPreserveRows);
        } else if (align_j) {
            mix2.Add(align_j->GetSegs().GetDenseg(), CAlnMix::fPreserveRows);
            mix2.Add(align_c->GetSegs().GetDenseg(), CAlnMix::fPreserveRows);
        }
        mix2.Merge(CAlnMix::fMinGap | CAlnMix::fQuerySeqMergeOnly | 
                   CAlnMix::fRemoveLeadTrailGaps | CAlnMix::fFillUnalignedRegions); 
        CAlnVec alnvec2(mix2.GetDenseg(), scope); 
        alnvec2.SetGapChar('-');
        if (align_d && align_j) {
            airr_data["c_alignment_start"] = NStr::IntToString(alnvec2.GetSeqAlnStart(4) + 1);
            airr_data["c_alignment_end"] = NStr::IntToString(alnvec2.GetSeqAlnStop(4) + 1);
        } else if (align_j) {
            airr_data["c_alignment_start"] = NStr::IntToString(alnvec2.GetSeqAlnStart(3) + 1);
            airr_data["c_alignment_end"] = NStr::IntToString(alnvec2.GetSeqAlnStop(3) + 1);
        }
    }   

    //query vdj part translation 
    {
       
        s_GetGermlineTranslation(annot, alnvec, airr_data["sequence_alignment"], airr_data["germline_alignment"],
                                 airr_data["sequence_alignment_aa"], airr_data["germline_alignment_aa"]);
    }
}

void CIgBlastTabularInfo::SetAirrFormatData(CScope& scope,
                                            const CRef<blast::CIgAnnotation> &annot,
                                            const CBioseq_Handle& query_handle, 
                                            CConstRef<CSeq_align_set> align_result,
                                            const CConstRef<blast::CIgBlastOptions>& ig_opts) {
    map <string, string> locus_name =  {{"VH", "IGH"}, {"VK", "IGK"}, {"VL", "IGL"}, {"VB", "TRB"}, 
                                                     {"VD", "TRD"}, {"VA", "TRA"}, {"VG", "TRG"}};
    int index = 1;
    bool found_v = false;
    bool found_d = false;
    bool found_j = false;
    bool found_c = false;
    m_TopAlign_V = 0;
    m_TopAlign_D = 0;
    m_TopAlign_J = 0; 
    m_TopAlign_C = 0;

    if (align_result && !align_result.Empty() && align_result->IsSet() && align_result->CanGet()) {
        ITERATE (CSeq_align_set::Tdata, iter, align_result->Get()) {
            
            if (annot->m_ChainType[index] == "V" && !found_v) {
                m_TopAlign_V = (*iter);
                found_v = true;
            }
            if (annot->m_ChainType[index] == "D" && !found_d) {
                if ((*iter)->GetSeqStrand(0) == eNa_strand_minus){
                    CRef<CSeq_align> temp_align (new CSeq_align);
                    temp_align->Assign(**iter);
                    temp_align->Reverse();  
                    m_TopAlign_D = temp_align;
                } else {
                    m_TopAlign_D = (*iter);
                }
                found_d = true;
            }
            if (annot->m_ChainType[index] == "J" && !found_j) {
                m_TopAlign_J = (*iter);
                found_j = true;
            }
            if (annot->m_ChainType[index] == "C" && !found_c) {
                m_TopAlign_C = (*iter);
                found_c = true;
            }
            
            index ++;
            
        }
    }

    m_AirrData.clear();
    ITERATE (list<string>, iter, ig_opts->m_AirrField) {
        m_AirrData[*iter] = NcbiEmptyString;
    }
 
    if (align_result && !align_result.Empty() && align_result->IsSet() && align_result->CanGet() && !(align_result->Get().empty())) {
        string query_id = NcbiEmptyString;
        
        const list<CRef<CSeq_id> > query_seqid = GetQueryId();
        CRef<CSeq_id> wid = FindBestChoice(query_seqid, CSeq_id::WorstRank); 
        wid->GetLabel(&query_id, CSeq_id::eContent);        
        m_AirrData["sequence_id"] = query_id;
        m_AirrData["sequence"] = m_Query;
        if (annot->m_FrameInfo[0] >= 0) {
            string seq_data(m_Query, annot->m_FrameInfo[0], m_Query.length() - annot->m_FrameInfo[0]);
            CSeqTranslator::Translate(seq_data, m_AirrData["sequence_aa"], 
                                      CSeqTranslator::fIs5PrimePartial, NULL, NULL);
        }
        if (m_OtherInfo[4] == "Yes") {
            m_AirrData["productive"] = "T"; 
        } else if (m_OtherInfo[4] == "No") {
            m_AirrData["productive"] = "F"; 
        } 
        if(locus_name.find(annot->m_ChainTypeToShow) != locus_name.end()) {
            m_AirrData["locus"] = locus_name[annot->m_ChainTypeToShow];
        } else {
            m_AirrData["locus"] = NcbiEmptyString;
        }
           
        if (m_FrameInfo == "IF") {
            m_AirrData["vj_in_frame"] = "T";
        } else if (m_FrameInfo == "OF") {
            m_AirrData["vj_in_frame"] = "F";
        } else if (m_FrameInfo == "IP") {
            m_AirrData["vj_in_frame"] = "T";
        }
        
        if (m_VFrameShift == "Yes") {
            m_AirrData["v_frameshift"] = "T";
        } else if (m_VFrameShift == "No") {
            m_AirrData["v_frameshift"] = "F";
        } 

        if (m_OtherInfo[3] == "Yes") {
            m_AirrData["stop_codon"] = "T";
        } else if (m_OtherInfo[3] == "No") {
            m_AirrData["stop_codon"] = "F";
        }

        m_AirrData["rev_comp"] = m_IsMinusStrand?"T":"F";
        m_AirrData["v_call"] = m_VGene.sid;
        if (m_DGene.sid != "N/A") {
            m_AirrData["d_call"] = m_DGene.sid;
        }
        if (m_JGene.sid != "N/A") {
            m_AirrData["j_call"] = m_JGene.sid;
        }
        if (m_CGene.sid != "N/A") {
            m_AirrData["c_call"] = m_CGene.sid;
        }

        if (m_AirrCdr3Seq != NcbiEmptyString) {
            m_AirrData["junction"] =  m_AirrCdr3Seq; //10th element  
            m_AirrData["junction_length"] = NStr::IntToString((int)m_AirrCdr3Seq.length());
            m_AirrData["junction_aa"] =  m_AirrCdr3SeqTrans;
            m_AirrData["junction_aa_length"] =  NStr::IntToString((int)m_AirrCdr3SeqTrans.length());
            
        } 
        m_AirrData["fwr1"] =  m_Fwr1Seq;
        m_AirrData["fwr1_aa"] = m_Fwr1SeqTrans;
        m_AirrData["cdr1"] =  m_Cdr1Seq;
        m_AirrData["cdr1_aa"] = m_Cdr1SeqTrans;
        m_AirrData["fwr2"] =  m_Fwr2Seq;
        m_AirrData["fwr2_aa"] = m_Fwr2SeqTrans;
        m_AirrData["cdr2"] =  m_Cdr2Seq;
        m_AirrData["cdr2_aa"] = m_Cdr2SeqTrans;
        m_AirrData["fwr3"] =  m_Fwr3Seq;
        m_AirrData["fwr3_aa"] = m_Fwr3SeqTrans;

        m_AirrData["cdr3"] =  m_Cdr3Seq;
        m_AirrData["cdr3_aa"] = m_Cdr3SeqTrans;
        m_AirrData["fwr4"] =  m_Fwr4Seq;
        m_AirrData["fwr4_aa"] = m_Fwr4SeqTrans;

        
        double v_score = 0;
        double d_score = 0;
        double j_score = 0;
        double c_score = 0;
        double v_evalue = 0;
        double d_evalue = 0;
        double j_evalue = 0;
        double c_evalue = 0;
        string v_score_str = NcbiEmptyString;
        string d_score_str = NcbiEmptyString;
        string j_score_str = NcbiEmptyString;
        string c_score_str = NcbiEmptyString;
        string v_evalue_str = NcbiEmptyString;
        string d_evalue_str = NcbiEmptyString;
        string j_evalue_str = NcbiEmptyString;
        string c_evalue_str = NcbiEmptyString;

        m_AirrData["complete_vdj"] = "F";
        if (m_TopAlign_V) {
            m_TopAlign_V->GetNamedScore(CSeq_align::eScore_BitScore, v_score);
            m_TopAlign_V->GetNamedScore(CSeq_align::eScore_EValue, v_evalue);
            NStr::DoubleToString(v_score_str, v_score, 3);
            NStr::DoubleToString(v_evalue_str, v_evalue, 3, NStr::fDoubleScientific);

        }
        if (m_TopAlign_D) {
            m_TopAlign_D->GetNamedScore(CSeq_align::eScore_BitScore, d_score);
            m_TopAlign_D->GetNamedScore(CSeq_align::eScore_EValue, d_evalue);
            NStr::DoubleToString(d_score_str, d_score, 3);
            NStr::DoubleToString(d_evalue_str, d_evalue, 3, NStr::fDoubleScientific);
           
        }
        if (m_TopAlign_J) {
            m_TopAlign_J->GetNamedScore(CSeq_align::eScore_BitScore, j_score);
            m_TopAlign_J->GetNamedScore(CSeq_align::eScore_EValue, j_evalue);
            NStr::DoubleToString(j_score_str, j_score, 3);
            NStr::DoubleToString(j_evalue_str, j_evalue, 3, NStr::fDoubleScientific);
        }
        if (m_TopAlign_C) {
            m_TopAlign_C->GetNamedScore(CSeq_align::eScore_BitScore, c_score);
            m_TopAlign_C->GetNamedScore(CSeq_align::eScore_EValue, c_evalue);
            NStr::DoubleToString(c_score_str, c_score, 3);
            NStr::DoubleToString(c_evalue_str, c_evalue, 3, NStr::fDoubleScientific);
            m_AirrData["c_score"] = c_score_str;
            m_AirrData["c_support"] = c_evalue_str;
        }

        m_AirrData["v_score"] = v_score_str;
        m_AirrData["d_score"] = d_score_str;
        m_AirrData["j_score"] = j_score_str;
       
        m_AirrData["v_support"] = v_evalue_str;
        m_AirrData["d_support"] = d_evalue_str;
        m_AirrData["j_support"] = j_evalue_str;
        
       
        string cigar = NcbiEmptyString;
        if (m_TopAlign_V) {
            s_GetCigarString(*m_TopAlign_V, cigar, query_handle.GetBioseqLength(), scope);
            m_AirrData["v_cigar"] = cigar;
            m_AirrData["v_sequence_start"] = NStr::IntToString(m_TopAlign_V->GetSeqStart(0) + 1);
            m_AirrData["v_sequence_end"] = NStr::IntToString(m_TopAlign_V->GetSeqStop(0) + 1);
            
            m_AirrData["v_germline_start"] = NStr::IntToString(m_TopAlign_V->GetSeqStart(1) + 1);
            m_AirrData["v_germline_end"] = NStr::IntToString(m_TopAlign_V->GetSeqStop(1) + 1);
            if (m_TopAlign_D) {
                int np_len = 0;
                string np_seq = NcbiEmptyString;
                s_FillJunctionalInfo (m_TopAlign_V->GetSeqStop(0), m_TopAlign_D->GetSeqStart(0), 
                                      np_len, np_seq, m_Query);
                m_AirrData["np1_length"] = NStr::IntToString(np_len);
                m_AirrData["np1"] = np_seq;
                 
            }
        }

        if (m_TopAlign_D) {
            s_GetCigarString(*m_TopAlign_D, cigar, query_handle.GetBioseqLength(), scope);
            m_AirrData["d_sequence_start"] = NStr::IntToString(m_TopAlign_D->GetSeqStart(0) + 1);
            m_AirrData["d_sequence_end"] = NStr::IntToString(m_TopAlign_D->GetSeqStop(0) + 1);
            
            if (m_TopAlign_D->GetSeqStrand(1) == eNa_strand_plus) {
                m_AirrData["d_germline_start"] = NStr::IntToString(m_TopAlign_D->GetSeqStart(1) + 1);
                m_AirrData["d_germline_end"] = NStr::IntToString(m_TopAlign_D->GetSeqStop(1) + 1);
                
                
                // Compute d Frame info, only for plus strand case as in the condition in this block
                //at this point V alignment is already flipped to have a positive query strand
                string d_id = m_TopAlign_D->GetSeq_id(1).AsFastaString();
                string v_id = m_TopAlign_V->GetSeq_id(1).AsFastaString();

                if (annot->m_DframeStart > 0 && annot->m_FrameInfo[2] > 0) {
                    
                    //frame is 0-based
                    int query_d_start =  m_TopAlign_D->GetSeqStart(0);
                    int query_d_frame_start = (m_TopAlign_D->GetSeqStart(1) + 3 - annot->m_DframeStart)%3; //query and slave frame is the same
                    
                    if (annot->m_FrameInfo[2] >= query_d_start) {
                        int d_frame_used = ((annot->m_FrameInfo[2] - query_d_start)%3 + query_d_frame_start)%3; 
                        m_AirrData["d_frame"] = NStr::IntToString(d_frame_used + 1);
                    }
                }
            } else {
                m_AirrData["d_germline_start"] = NStr::IntToString(m_TopAlign_D->GetSeqStop(1) + 1);
                m_AirrData["d_germline_end"] = NStr::IntToString(m_TopAlign_D->GetSeqStart(1) + 1);
            }

            m_AirrData["d_cigar"] = cigar;
            
        }
        
        if (m_TopAlign_J) {
	    int query_length = query_handle.GetBioseqLength();
            s_GetCigarString(*m_TopAlign_J, cigar, query_length, scope);
	    int query_J_stop = m_TopAlign_J->GetSeqStop(0);
            m_AirrData["j_cigar"] = cigar;
            m_AirrData["j_sequence_start"] = NStr::IntToString(m_TopAlign_J->GetSeqStart(0) + 1);
            m_AirrData["j_sequence_end"] = NStr::IntToString(query_J_stop + 1);
            
            m_AirrData["j_germline_start"] = NStr::IntToString(m_TopAlign_J->GetSeqStart(1) + 1);
            m_AirrData["j_germline_end"] = NStr::IntToString(m_TopAlign_J->GetSeqStop(1) + 1);
            const CBioseq_Handle& germline_j_bh = 
                    scope.GetBioseqHandle(m_TopAlign_J->GetSeq_id(1));
            int j_length = germline_j_bh.GetBioseqLength();
            if (m_AirrData["v_germline_start"] != NcbiEmptyString && 
                NStr::StringToInt(m_AirrData["v_germline_start"]) == 1 &&
		(NStr::StringToInt(m_AirrData["j_germline_end"]) >= j_length - max(0, annot->m_JDomain[4]) ||
                (NStr::StringToInt(m_AirrData["j_germline_end"]) >= j_length - max(0, annot->m_JDomain[4]) - 1 &&
		 query_length > query_J_stop + 1))) { //-1 to allow J gene missing the last bp but the query must have at least one base past the query j end

                m_AirrData["complete_vdj"] = "T";
            }
            if (m_TopAlign_D) {
                int np_len = 0;
                string np_seq = NcbiEmptyString;
                s_FillJunctionalInfo (m_TopAlign_D->GetSeqStop(0), m_TopAlign_J->GetSeqStart(0), 
                                      np_len, np_seq, m_Query);
                m_AirrData["np2_length"] = NStr::IntToString(np_len);
                m_AirrData["np2"] = np_seq;
            } else if (m_TopAlign_V){
                int np_len = 0;
                string np_seq = NcbiEmptyString;
                s_FillJunctionalInfo (m_TopAlign_V->GetSeqStop(0), m_TopAlign_J->GetSeqStart(0), 
                                      np_len, np_seq, m_Query);
                m_AirrData["np1_length"] = NStr::IntToString(np_len);
                m_AirrData["np1"] = np_seq;
            }

            if (m_TopAlign_C) {
                s_GetCigarString(*m_TopAlign_C, cigar, query_handle.GetBioseqLength(), scope);
                m_AirrData["c_cigar"] = cigar;
                m_AirrData["c_sequence_start"] = NStr::IntToString(m_TopAlign_C->GetSeqStart(0) + 1);
                m_AirrData["c_sequence_end"] = NStr::IntToString(m_TopAlign_C->GetSeqStop(0) + 1);
                
                m_AirrData["c_germline_start"] = NStr::IntToString(m_TopAlign_C->GetSeqStart(1) + 1);
                m_AirrData["c_germline_end"] = NStr::IntToString(m_TopAlign_C->GetSeqStop(1) + 1);
            }
        }
            
        s_SetAirrAlignmentInfo(m_TopAlign_V, m_TopAlign_D, m_TopAlign_J, m_TopAlign_C, annot, scope, m_AirrData);
        
        
        for (unsigned int i=0; i<m_IgDomains.size(); ++i) {
            if (m_IgDomains[i]->name.find("FR1") !=  string::npos) {
                m_AirrData["fwr1_start"] =  NStr::IntToString(m_IgDomains[i]->start + 1);
                m_AirrData["fwr1_end"] =  NStr::IntToString(m_IgDomains[i]->end);
               
            } 
            if (m_IgDomains[i]->name.find("CDR1") !=  string::npos) {
                m_AirrData["cdr1_start"] =  NStr::IntToString(m_IgDomains[i]->start + 1);
                m_AirrData["cdr1_end"] =  NStr::IntToString(m_IgDomains[i]->end);
            } 
            if (m_IgDomains[i]->name.find("FR2") !=  string::npos) {
                m_AirrData["fwr2_start"] =  NStr::IntToString(m_IgDomains[i]->start + 1);
                m_AirrData["fwr2_end"] =  NStr::IntToString(m_IgDomains[i]->end);
               
            } 
            if (m_IgDomains[i]->name.find("CDR2") !=  string::npos) {
                m_AirrData["cdr2_start"] =  NStr::IntToString(m_IgDomains[i]->start + 1);
                m_AirrData["cdr2_end"] =  NStr::IntToString(m_IgDomains[i]->end);
            } 
            if (m_IgDomains[i]->name.find("FR3") !=  string::npos && annot->m_DomainInfo[9] >=0) {
                m_AirrData["fwr3_start"] =  NStr::IntToString(m_IgDomains[i]->start + 1);
                
                m_AirrData["fwr3_end"] =  NStr::IntToString(min(m_QueryAlignSeqEnd, annot->m_DomainInfo[9]) + 1);
             
            } 
        }

        if (m_Cdr3Start > 0){
            m_AirrData["cdr3_start"] = NStr::IntToString(m_Cdr3Start + 1); 
            if (m_Cdr3End > 0) {
                m_AirrData["cdr3_end"] = NStr::IntToString(m_Cdr3End + 1); 
            }     
        }
        if (m_Fwr4Start > 0){
            m_AirrData["fwr4_start"] = NStr::IntToString(m_Fwr4Start + 1); 
            if (m_Cdr3End > 0) {
                m_AirrData["fwr4_end"] = NStr::IntToString(m_Fwr4End + 1); 
            }     
        }
        

    } else {
        SetQueryId(query_handle);
        string query_id = NcbiEmptyString;
        const list<CRef<CSeq_id> > query_seqid = GetQueryId();
        CRef<CSeq_id> wid = FindBestChoice(query_seqid, CSeq_id::WorstRank); 
        wid->GetLabel(&query_id, CSeq_id::eContent); 
        m_AirrData["sequence_id"] = query_id;
        string query_seq;
        query_handle.GetSeqVector(CBioseq_Handle::eCoding_Iupac)
            .GetSeqData(0, query_handle.GetBioseqLength(), query_seq);
        m_AirrData["sequence"] = query_seq;
        CSeqTranslator::Translate(query_seq, m_AirrData["sequence_aa"], 
                                  CSeqTranslator::fIs5PrimePartial, NULL, NULL);
        
        m_AirrData["rev_comp"] = "F";
    }
}

void CIgBlastTabularInfo::PrintAirrRearrangement(CScope& scope,
                                                 const CRef<blast::CIgAnnotation>& annot,
                                                 const string& program_version, 
                                                 const CBioseq& query_bioseq, 
                                                 const string& dbname, 
                                                 const string& domain_sys,
                                                 const string& rid,
                                                 unsigned int iteration,
                                                 const CSeq_align_set* align_set,
                                                 CConstRef<CBioseq> subj_bioseq,
                                                 CNcbiMatrix<int>* matrix,
                                                 bool print_airr_format_header,
                                                 const CConstRef<blast::CIgBlastOptions>& ig_opts)
{

    bool first = true;
    if (print_airr_format_header) {

        ITERATE(list<string>, iter, ig_opts->m_AirrField) {
            if (!first) {
                m_Ostream << m_FieldDelimiter;
            }
            first = false;
            m_Ostream << *iter;
        }
        m_Ostream << endl;
    }
    
    first = true;
    ITERATE(list<string>, iter, ig_opts->m_AirrField) {
        if (!first) {
            m_Ostream << m_FieldDelimiter;
        }
        first = false;
        m_Ostream << m_AirrData[*iter];
    }
    m_Ostream << endl;
    
    
}

int CIgBlastTabularInfo::SetMasterFields(const CSeq_align& align, 
                                 CScope& scope, 
                                 const string& chain_type,
                                 const string& master_chain_type_to_show,
                                 CNcbiMatrix<int>* matrix)
{
    int retval = 0;
    bool hasSeq = x_IsFieldRequested(eQuerySeq);
    bool hasQuerySeqId = x_IsFieldRequested(eQuerySeqId);
    bool hasQueryStart = x_IsFieldRequested(eQueryStart);

    x_ResetIgFields();

    if (!hasSeq) x_AddFieldToShow(eQuerySeq);
    if (!hasQuerySeqId) x_AddFieldToShow(eQuerySeqId);
    if (!hasQueryStart) x_AddFieldToShow(eQueryStart);
    retval = SetFields(align, scope, chain_type, master_chain_type_to_show, matrix);
    if (!hasSeq) x_DeleteFieldToShow(eQuerySeq);
    if (!hasQuerySeqId) x_DeleteFieldToShow(eQuerySeqId);
    if (!hasQueryStart) x_DeleteFieldToShow(eQueryStart);
    return retval;
};            

int CIgBlastTabularInfo::SetFields(const CSeq_align& align,
                                   CScope& scope, 
                                   const string& chain_type,
                                   const string& master_chain_type_to_show,
                                   CNcbiMatrix<int>* matrix)
{
    m_ChainType = chain_type;
    m_MasterChainTypeToShow = master_chain_type_to_show;
    if (m_ChainType == "NA") m_ChainType = "N/A";
    return CBlastTabularInfo::SetFields(align, scope, matrix);
};

void CIgBlastTabularInfo::SetIgCDR3FWR4Annotation(const CRef<blast::CIgAnnotation> &annot)
{
   
    m_Fwr4Start = annot->m_JDomain[2];
    m_Fwr4End = annot->m_JDomain[3];
    m_Cdr3Start = annot->m_JDomain[0];
    m_Cdr3End = annot->m_JDomain[1];

    m_Fwr4Seq = NcbiEmptyString;
    m_Fwr4SeqTrans = NcbiEmptyString;
    m_Cdr3Seq = NcbiEmptyString;
    m_Cdr3SeqTrans = NcbiEmptyString;
    m_AirrCdr3Seq = NcbiEmptyString;
    m_AirrCdr3SeqTrans = NcbiEmptyString;
   
    if (m_Fwr4Start > 0 && m_Fwr4End > m_Fwr4Start) {
        
        m_Fwr4Seq = m_Query.substr(m_Fwr4Start, m_Fwr4End - m_Fwr4Start + 1);
        
        int coding_frame_offset = (m_Fwr4Start - annot->m_FrameInfo[0])%3; 
        if ((int)m_Fwr4Seq.size() >= 3) {
            string fwr4_seq_for_translatioin = m_Fwr4Seq.substr(coding_frame_offset>0?(3-coding_frame_offset):0);
            
            CSeqTranslator::Translate(fwr4_seq_for_translatioin, 
                                      m_Fwr4SeqTrans, 
                                      CSeqTranslator::fIs5PrimePartial, NULL, NULL);
        }
    }

    if (m_Cdr3Start > 0 && m_Cdr3End > m_Cdr3Start) {
       
        m_Cdr3Seq = m_Query.substr(m_Cdr3Start, m_Cdr3End - m_Cdr3Start + 1);
        
        int coding_frame_offset = (m_Cdr3Start - annot->m_FrameInfo[0])%3; 
        if ((int)m_Cdr3Seq.size() >= 3) {
            string cdr3_seq_for_translatioin = m_Cdr3Seq.substr(coding_frame_offset>0?(3-coding_frame_offset):0);
            
            CSeqTranslator::Translate(cdr3_seq_for_translatioin, 
                                      m_Cdr3SeqTrans, 
                                      CSeqTranslator::fIs5PrimePartial, NULL, NULL);
        }
        SIZE_TYPE query_length = m_Query.length();
        int airrcdr3start = max(m_Cdr3Start -3, 0);
        m_AirrCdr3Seq = m_Query.substr(airrcdr3start, min(m_Cdr3End - m_Cdr3Start + 7, 
                                                          (int)(query_length - airrcdr3start)));
        if ((int)m_AirrCdr3Seq.size() >= 3) {
            string airr_cdr3_seq_for_translatioin = m_AirrCdr3Seq.substr(coding_frame_offset>0?(3-coding_frame_offset):0);
            
            CSeqTranslator::Translate(airr_cdr3_seq_for_translatioin, 
                                      m_AirrCdr3SeqTrans, 
                                      CSeqTranslator::fIs5PrimePartial, NULL, NULL);
        }
    }
    
       

};

static void SetCdrFwrSeq (const string& nuc_seq, string& translated_seq, bool is_first_domain, int region_start, int frame_start, 
                          string& next_trans_addition, bool& next_trans_substract, string extra_from_next_region) {
    
    string seq_for_translatioin = NcbiEmptyString;
    if (is_first_domain) {
         //+3 to avoid negative value but does not affect frame
        int coding_frame_offset = ((region_start%3 + 3) - frame_start%3)%3;;
        int start_pos = coding_frame_offset>0?(3-coding_frame_offset):0;
                
        if (start_pos < (int)nuc_seq.size()){ 
            seq_for_translatioin = nuc_seq.substr(start_pos);
        }
    } else {
        seq_for_translatioin = next_trans_addition + nuc_seq;
        next_trans_addition = NcbiEmptyString;
    }
    if (next_trans_substract) {
        seq_for_translatioin.erase(0, 1);
        next_trans_substract = false;
    }
    int next_trans_offset = seq_for_translatioin.length()%3;
    if (next_trans_offset == 2) {//take the first base of the next region
        seq_for_translatioin = seq_for_translatioin + extra_from_next_region;;
        next_trans_substract = true;
    } else if (next_trans_offset == 1) {//move the last base to next region
        next_trans_addition = seq_for_translatioin.substr(seq_for_translatioin.length() - next_trans_offset);
        seq_for_translatioin = seq_for_translatioin.substr(0, seq_for_translatioin.length() - next_trans_offset);
    }
    
    CSeqTranslator::Translate(seq_for_translatioin, translated_seq, CSeqTranslator::fIs5PrimePartial, NULL, NULL);
} 


void CIgBlastTabularInfo::SetIgAnnotation(const CRef<blast::CIgAnnotation> &annot,
                                          const CConstRef<blast::CIgBlastOptions> &ig_opts,
                                          CConstRef<CSeq_align_set>& align_result,
                                          CScope& scope)
{
    
    CRef<CSeq_align> align(0);
    m_QueryAlignSeqEnd = 0;
    m_QueryVAlign = NcbiEmptyString;
    m_VAlign = NcbiEmptyString;
    int index = 1; 
    if (align_result && !align_result.Empty() && align_result->IsSet() && align_result->CanGet()) {
        ITERATE (CSeq_align_set::Tdata, iter, align_result->Get()) {
            if (!align) {
                align = (*iter);
                const CBioseq_Handle& query_bh = 
                    scope.GetBioseqHandle(align->GetSeq_id(0));
                int length = query_bh.GetBioseqLength();
                query_bh.GetSeqVector(CBioseq_Handle::eCoding_Iupac).GetSeqData(0, length, m_Query);
                SetQueryId(query_bh);
                const CDense_seg& ds = align->GetSegs().GetDenseg();
                CRef<CAlnVec> alnVec (new CAlnVec(ds, scope));
                alnVec->SetGapChar('-');
                alnVec->GetWholeAlnSeqString(0, m_QueryVAlign);
                alnVec->GetWholeAlnSeqString(1, m_VAlign);
                m_QueryVAlignStart = alnVec->GetSeqStart(0) + 1;
                m_VAlignStart = alnVec->GetSeqStart(1) + 1;
                m_QueryVAlignEnd = alnVec->GetSeqStop(0) + 1;
            }
            if (annot->m_ChainType[index] == "V" || annot->m_ChainType[index] == "D" || annot->m_ChainType[index] == "J") {
                m_QueryAlignSeqEnd = max(m_QueryAlignSeqEnd, (int)(*iter)->GetSeqStop(0));
            }
        }
    }

   

    bool is_protein = ig_opts->m_IsProtein;
    SetSeqType(!is_protein);
    SetMinusStrand(annot->m_MinusStrand);

    // Gene info coordinates are half inclusive
    SetVGene(annot->m_TopGeneIds[0], annot->m_GeneInfo[0], annot->m_GeneInfo[1]);
    SetDGene(annot->m_TopGeneIds[1], annot->m_GeneInfo[2], annot->m_GeneInfo[3]);
    SetJGene(annot->m_TopGeneIds[2], annot->m_GeneInfo[4], annot->m_GeneInfo[5]);
    SetCGene(annot->m_TopGeneIds[3], annot->m_GeneInfo[6], annot->m_GeneInfo[7]);



    // Compute v j Frame info
    if (annot->m_FrameInfo[1] >= 0 && annot->m_FrameInfo[2] >= 0) {
        int off = annot->m_FrameInfo[1];
        int len = annot->m_FrameInfo[2] - off;
        string seq_trans;
        
        if (annot->m_FrameInfo[0] >= 0) {
            m_VFrameShift = (annot->m_FrameInfo[1] - annot->m_FrameInfo[0])%3 == 0 ? "No" : "Yes";
        }

        if ( len % 3 == 0) {
            string seq_data(m_Query, off, len);
            CSeqTranslator::Translate(seq_data, seq_trans, 
                                      CSeqTranslator::fIs5PrimePartial, NULL, NULL);
            if (seq_trans.find('*') != string::npos) {
                SetFrame("IP");
            } else {
                SetFrame("IF");
            }
        } else  {
            SetFrame("OF");
        }

    } else {
        SetFrame("N/A");
    }

 
    //stop codon anywhere between start of V and end of J
    //This checks for stop codon between start of top matched V and and end of top matched J only 
    if (annot->m_FrameInfo[0] >= 0) {
        int v_start = annot->m_FrameInfo[0];
        int v_j_length = max(max(annot->m_GeneInfo[5], annot->m_GeneInfo[3]), annot->m_GeneInfo[1]) - annot->m_FrameInfo[0];
  
        string seq_data(m_Query, v_start, v_j_length);
        string seq_trans;
        CSeqTranslator::Translate(seq_data, seq_trans, 
                                  CSeqTranslator::fIs5PrimePartial, NULL, NULL);
       
        if (seq_trans.find('*') == string::npos) {
            m_OtherInfo[3] = "No";  //index 3
            if (m_FrameInfo == "IF" || m_FrameInfo == "IP") {
                if (m_VFrameShift == "No") {
                    m_OtherInfo[4] = "Yes"; //index 4,productive or not
                } else {
                    m_OtherInfo[4] = "No"; //index 4
                }
            } else if (m_FrameInfo == "OF"){
                m_OtherInfo[4] = "No"; //index 4
            } else {
               m_OtherInfo[4] = "N/A"; //index 4
            }
        } else {
            m_OtherInfo[3] = "Yes"; //index 3
            m_OtherInfo[4] = "No"; //index 4
        }
        
    } else {
        m_OtherInfo[3] = "N/A";
        m_OtherInfo[4] = "N/A";
    }
    
  

    // Domain info coordinates are inclusive (and always on positive strand)
    AddIgDomain((ig_opts->m_DomainSystem == "kabat")?"FR1":"FR1-IMGT", 
                annot->m_DomainInfo[0], annot->m_DomainInfo[1]+1,
                annot->m_DomainInfo_S[0], annot->m_DomainInfo_S[1]+1);
    AddIgDomain((ig_opts->m_DomainSystem == "kabat")?"CDR1":"CDR1-IMGT",
                annot->m_DomainInfo[2], annot->m_DomainInfo[3]+1,
                annot->m_DomainInfo_S[2], annot->m_DomainInfo_S[3]+1);
    AddIgDomain((ig_opts->m_DomainSystem == "kabat")?"FR2":"FR2-IMGT",
                annot->m_DomainInfo[4], annot->m_DomainInfo[5]+1,
                annot->m_DomainInfo_S[4], annot->m_DomainInfo_S[5]+1);
    AddIgDomain((ig_opts->m_DomainSystem == "kabat")?"CDR2":"CDR2-IMGT",
                annot->m_DomainInfo[6], annot->m_DomainInfo[7]+1,
                annot->m_DomainInfo_S[6], annot->m_DomainInfo_S[7]+1);
    AddIgDomain((ig_opts->m_DomainSystem == "kabat")?"FR3":"FR3-IMGT",
                annot->m_DomainInfo[8], annot->m_DomainInfo[9]+1,
                annot->m_DomainInfo_S[8], annot->m_DomainInfo_S[9]+1);
    AddIgDomain((ig_opts->m_DomainSystem == "kabat")?"CDR3 (V gene only)":"CDR3-IMGT (germline)",
                annot->m_DomainInfo[10], annot->m_DomainInfo[11]+1);
 
    m_Fwr1Seq = NcbiEmptyString;
    m_Fwr1SeqTrans = NcbiEmptyString;
    m_Cdr1Seq = NcbiEmptyString;
    m_Cdr1SeqTrans = NcbiEmptyString;
    m_Fwr2Seq = NcbiEmptyString;
    m_Fwr2SeqTrans = NcbiEmptyString;
    m_Cdr2Seq = NcbiEmptyString;
    m_Cdr2SeqTrans = NcbiEmptyString;
    m_Fwr3Seq = NcbiEmptyString;
    m_Fwr3SeqTrans = NcbiEmptyString;
  
    string next_trans_addition = NcbiEmptyString;
    bool is_first_domain = true;
    bool next_trans_substract = false;

    for (unsigned int i=0; i<m_IgDomains.size(); ++i) {
        if (m_IgDomains[i]->name.find("FR1") !=  string::npos) {
            m_Fwr1Seq = m_Query.substr(m_IgDomains[i]->start, m_IgDomains[i]->end - m_IgDomains[i]->start);
            SetCdrFwrSeq (m_Fwr1Seq, m_Fwr1SeqTrans, is_first_domain, m_IgDomains[i]->start, annot->m_FrameInfo[0],
                          next_trans_addition, next_trans_substract, 
                          (m_IgDomains.size() > i + 1) ? m_Query.substr(m_IgDomains[i+1]->start, 1) : NcbiEmptyString);
            
            is_first_domain = false;   
            
        }
        if (m_IgDomains[i]->name.find("CDR1") !=  string::npos) {
            m_Cdr1Seq = m_Query.substr(m_IgDomains[i]->start, m_IgDomains[i]->end - m_IgDomains[i]->start);
            SetCdrFwrSeq (m_Cdr1Seq, m_Cdr1SeqTrans, is_first_domain, m_IgDomains[i]->start, annot->m_FrameInfo[0],
                          next_trans_addition, next_trans_substract, 
                          (m_IgDomains.size() > i + 1) ? m_Query.substr(m_IgDomains[i+1]->start, 1) : NcbiEmptyString);
            is_first_domain = false;
        }
        
        if (m_IgDomains[i]->name.find("FR2") !=  string::npos) {
            m_Fwr2Seq = m_Query.substr(m_IgDomains[i]->start, m_IgDomains[i]->end - m_IgDomains[i]->start);
            SetCdrFwrSeq (m_Fwr2Seq, m_Fwr2SeqTrans, is_first_domain, m_IgDomains[i]->start, annot->m_FrameInfo[0],
                          next_trans_addition, next_trans_substract, 
                          (m_IgDomains.size() > i + 1) ? m_Query.substr(m_IgDomains[i+1]->start, 1) : NcbiEmptyString);
            is_first_domain = false;
        } 
        if (m_IgDomains[i]->name.find("CDR2") !=  string::npos) {
            m_Cdr2Seq = m_Query.substr(m_IgDomains[i]->start, m_IgDomains[i]->end - m_IgDomains[i]->start);

            SetCdrFwrSeq (m_Cdr2Seq, m_Cdr2SeqTrans, is_first_domain, m_IgDomains[i]->start, annot->m_FrameInfo[0],
                          next_trans_addition, next_trans_substract, 
                          (m_IgDomains.size() > i + 1) ? m_Query.substr(m_IgDomains[i+1]->start, 1) : NcbiEmptyString);
 
            is_first_domain = false;
        } 
        if (m_IgDomains[i]->name.find("FR3") !=  string::npos) {
            if (annot->m_DomainInfo[9] >=0) {
                //fwr3 is special since it may extends past end of v
                m_Fwr3Seq = m_Query.substr(m_IgDomains[i]->start, min(m_QueryAlignSeqEnd, annot->m_DomainInfo[9]) - m_IgDomains[i]->start + 1);
                SetCdrFwrSeq (m_Fwr3Seq, m_Fwr3SeqTrans, is_first_domain, m_IgDomains[i]->start, annot->m_FrameInfo[0],
                              next_trans_addition, next_trans_substract, NcbiEmptyString);
               
            }
        }
    }

    SetIgCDR3FWR4Annotation(annot);
};

void CIgBlastTabularInfo::Print(void)
{
    m_Ostream << m_ChainType << m_FieldDelimiter;
    CBlastTabularInfo::Print();
};

void CIgBlastTabularInfo::PrintMasterAlign(const CConstRef<blast::CIgBlastOptions>& ig_opts, const string &header) const
{
    m_Ostream << endl;
    if (m_IsNucl) {
        if (m_IsMinusStrand) {
            m_Ostream << header << "Note that your query represents the minus strand "
                      << "of a V gene and has been converted to the plus strand. "
                      << "The sequence positions refer to the converted sequence. " << endl << endl;
        }
        m_Ostream << header << "V-(D)-J rearrangement summary for query sequence ";
        m_Ostream << "(Top V gene match, ";
        if (m_ChainType == "VH" || m_ChainType == "VD" || 
            m_ChainType == "VB") m_Ostream << "Top D gene match, ";
        m_Ostream << "Top J gene match, ";
        if (ig_opts->m_Db[4]) { 
            m_Ostream << "Top C gene match, ";
        }
        m_Ostream << "Chain type, stop codon, ";
        m_Ostream << "V-J frame, Productive, Strand, V Frame shift).  "; 
        m_Ostream <<"Multiple equivalent top matches, if present, are separated by a comma." << endl;
        m_Ostream << m_VGene.sid << m_FieldDelimiter;
        if (m_ChainType == "VH"|| m_ChainType == "VD" || 
            m_ChainType == "VB") m_Ostream << m_DGene.sid << m_FieldDelimiter;
        m_Ostream << m_JGene.sid << m_FieldDelimiter;
        if (ig_opts->m_Db[4]) {
            m_Ostream << m_CGene.sid << m_FieldDelimiter;
        }
        m_Ostream << m_MasterChainTypeToShow << m_FieldDelimiter;
        m_Ostream << m_OtherInfo[3] << m_FieldDelimiter;
        if (m_FrameInfo == "IF") m_Ostream << "In-frame";
        else if (m_FrameInfo == "OF") m_Ostream << "Out-of-frame";
        else if (m_FrameInfo == "IP") m_Ostream << "In-frame";
        else m_Ostream << "N/A";
        m_Ostream << m_FieldDelimiter << m_OtherInfo[4];
        m_Ostream << m_FieldDelimiter << ((m_IsMinusStrand) ? '-' : '+' );
        m_Ostream << m_FieldDelimiter << m_VFrameShift << endl << endl;
        x_PrintIgGenes(false, header);
    }

    int length = 0;
    for (unsigned int i=0; i<m_IgDomains.size(); ++i) {
        if (m_IgDomains[i]->length > 0) {
            length += m_IgDomains[i]->length;
        }
    }
    if (!length) return;

    m_Ostream << header << "Alignment summary between query and top germline V gene hit ";
    m_Ostream << "(from, to, length, matches, mismatches, gaps, percent identity)" << endl;

    int num_match = 0;
    int num_mismatch = 0;
    int num_gap = 0;
    for (unsigned int i=0; i<m_IgDomains.size(); ++i) {
        x_PrintIgDomain(*(m_IgDomains[i]));
        m_Ostream << endl;
        if (m_IgDomains[i]->length > 0) {
            num_match += m_IgDomains[i]->num_match;
            num_mismatch += m_IgDomains[i]->num_mismatch;
            num_gap += m_IgDomains[i]->num_gap;
        }
    }
    m_Ostream << "Total" 
              << m_FieldDelimiter << "N/A"
              << m_FieldDelimiter << "N/A"
              << m_FieldDelimiter << length  
              << m_FieldDelimiter << num_match 
              << m_FieldDelimiter << num_mismatch
              << m_FieldDelimiter << num_gap
              << m_FieldDelimiter << std::setprecision(3) << num_match*100.0/length
              << endl << endl;
};

void CIgBlastTabularInfo::PrintHtmlSummary(const CConstRef<blast::CIgBlastOptions>& ig_opts) const
{
    if (m_IsNucl) {
        if (m_IsMinusStrand) {
            m_Ostream << "<br>Note that your query represents the minus strand "
                      << "of a V gene and has been converted to the plus strand. "
                      << "The sequence positions refer to the converted sequence.\n\n";
        }
        m_Ostream << "<br>V-(D)-J rearrangement summary for query sequence (multiple equivalent top matches, if present, are separated by a comma):\n";
        m_Ostream << "<table border=1>\n";
        m_Ostream << "<tr><td>Top V gene match</td>";
        if (m_ChainType == "VH" || m_ChainType == "VD" || 
            m_ChainType == "VB") {  
            m_Ostream << "<td>Top D gene match</td>";
        }
        m_Ostream << "<td>Top J gene match</td>";
        if (ig_opts->m_Db[4]) {
            m_Ostream << "<td>Top C gene match</td>";
        }
        m_Ostream  << "<td>Chain type</td>"
                   << "<td>stop codon</td>"
                   << "<td>V-J frame</td>"
                   << "<td>Productive</td>"
                   << "<td>Strand</td>"
                   << "<td>V frame shift</td></tr>\n";

        m_Ostream << "<tr><td>"  << m_VGene.sid;
        if (m_ChainType == "VH" || m_ChainType == "VD" || 
            m_ChainType == "VB") { 
            m_Ostream << "</td><td>" << m_DGene.sid;
        }
        m_Ostream << "</td><td>" << m_JGene.sid;
        if (ig_opts->m_Db[4]) {
           m_Ostream << "</td><td>" << m_CGene.sid;
        }
        m_Ostream << "</td><td>" << m_MasterChainTypeToShow 
                  << "</td><td>";
        m_Ostream << (m_OtherInfo[3]!="N/A" ? m_OtherInfo[3] : "") << "</td><td>";
        if (m_FrameInfo == "IF") {
            m_Ostream << "In-frame";
        } else if (m_FrameInfo == "OF") {
            m_Ostream << "Out-of-frame";
        } else if (m_FrameInfo == "IP") {
            m_Ostream << "In-frame";
        } 
        m_Ostream << "</td><td>" << (m_OtherInfo[4]!="N/A" ? m_OtherInfo[4] : "");
        m_Ostream << "</td><td>" << ((m_IsMinusStrand) ? '-' : '+');
        m_Ostream << "</td><td>" << m_VFrameShift
                  << "</td></tr></table>\n";
        x_PrintIgGenes(true, "");
    }

    int length = 0;
    for (unsigned int i=0; i<m_IgDomains.size(); ++i) {
        if (m_IgDomains[i]->length > 0) {
            length += m_IgDomains[i]->length;
        }
    }
    if (!length) return;

    m_Ostream << "<br>Alignment summary between query and top germline V gene hit:\n";
    m_Ostream << "<table border=1>";
    m_Ostream << "<tr><td> </td><td> from </td><td> to </td><td> length </td>"
              << "<td> matches </td><td> mismatches </td><td> gaps </td>"
              << "<td> identity(%) </td></tr>\n";

    int num_match = 0;
    int num_mismatch = 0;
    int num_gap = 0;
    for (unsigned int i=0; i<m_IgDomains.size(); ++i) {
        x_PrintIgDomainHtml(*(m_IgDomains[i]));
        if (m_IgDomains[i]->length > 0) {
            num_match += m_IgDomains[i]->num_match;
            num_mismatch += m_IgDomains[i]->num_mismatch;
            num_gap += m_IgDomains[i]->num_gap;
        }
    }
    m_Ostream << "<tr><td> Total </td><td> </td><td> </td><td> " << length 
              << " </td><td> " << num_match 
              << " </td><td> " << num_mismatch
              << " </td><td> " << num_gap
              << " </td><td> " << std::setprecision(3) << num_match*100.0/length
              << " </td></tr>";
    m_Ostream << "</table>\n";
};

void CIgBlastTabularInfo::x_ResetIgFields()
{
    for (unsigned int i=0; i<m_IgDomains.size(); ++i) {
        delete m_IgDomains[i];
    }
    m_IgDomains.clear();
    m_FrameInfo = "N/A";
    m_VFrameShift = "N/A";
    m_ChainType = "N/A";
    m_IsMinusStrand = false;
    m_VGene.Reset();
    m_DGene.Reset();
    m_JGene.Reset();
    m_CGene.Reset();
    for (int i = 0; i < num_otherinfo; i ++) {
        m_OtherInfo[i] = "N/A";
    }
    m_Cdr3Start = -1;
    m_Cdr3End =  -1;
    m_Fwr4Start = -1;
    m_Fwr4End = -1;
    m_Fwr1Seq = NcbiEmptyString;
    m_Fwr1SeqTrans = NcbiEmptyString;
    m_Cdr1Seq = NcbiEmptyString;
    m_Cdr1SeqTrans = NcbiEmptyString;
    m_Fwr2Seq = NcbiEmptyString;
    m_Fwr2SeqTrans = NcbiEmptyString;
    m_Cdr2Seq = NcbiEmptyString;
    m_Cdr2SeqTrans = NcbiEmptyString;
    m_Fwr3Seq = NcbiEmptyString;
    m_Fwr3SeqTrans = NcbiEmptyString;
    m_QueryAlignSeqEnd = 0;
    m_Cdr3Seq = NcbiEmptyString;
    m_Cdr3SeqTrans = NcbiEmptyString;
    m_Fwr4Seq = NcbiEmptyString;
    m_Fwr4SeqTrans = NcbiEmptyString;
};

void CIgBlastTabularInfo::x_PrintPartialQuery(int start, int end, bool isHtml) const
{
    const bool isOverlap = (start > end);

    if (start <0 || end <0 || start==end) {
        if (isHtml) {
            m_Ostream << "<td></td>";
        } else {
            m_Ostream << "N/A";
        }
        return;
    }

    if (isHtml) m_Ostream << "<td>";
    if (isOverlap) {
        int tmp = end;
        end = start;
        start = tmp;
        m_Ostream << '(';
    }
    for (int pos = start; pos < end; ++pos) {
        m_Ostream << m_Query[pos];
    }
    if (isOverlap)  m_Ostream << ')';
    if (isHtml) m_Ostream << "</td>";
};

void CIgBlastTabularInfo::x_PrintIgGenes(bool isHtml, const string& header) const 
{
    int     a1, a2, a3, a4;
    int b0, b1, b2, b3, b4, b5;

    if (m_VGene.start <0) return;

    a2 = a3 = 0;
    b0 = m_VGene.start;
    b1 = m_VGene.end;
    b2 = m_DGene.start;
    b3 = m_DGene.end;
    b4 = m_JGene.start;
    b5 = m_JGene.end;

    if (b2 < 0) {
        b2 = b1;
        b3 = b1;
        if (b3 > b4 && b4 > 0 && (m_ChainType == "VH" || 
                                  m_ChainType == "VD" || 
                                  m_ChainType == "VB")) {
            b4 = b3;
        }
    }

    if (b4 < 0) {
        b4 = b3;
        b5 = b3;
    }

    if (m_ChainType == "VH" || m_ChainType == "VD" || 
            m_ChainType == "VB") {
        a1 = min(b1, b2);
        a2 = max(b1, b2);
        a3 = min(b3, b4);
        a4 = max(b3, b4);
    } else {
        a1 = min(b1, b4);
        a4 = max(b1, b4);
    }

    if (isHtml) {
        m_Ostream << "<br>V-(D)-J junction details based on top germline gene matches:\n";
        m_Ostream << "<table border=1>\n";
        m_Ostream << "<tr><td>V region end</td>";
        if (m_ChainType == "VH" || m_ChainType == "VD" || 
            m_ChainType == "VB") {
            m_Ostream << "<td>V-D junction*</td>"
                      << "<td>D region</td>"
                      << "<td>D-J junction*</td>";
        } else {
            m_Ostream << "<td>V-J junction*</td>";
        }
        m_Ostream << "<td>J region start</td></tr>\n<tr>";
    } else {
        m_Ostream << header << "V-(D)-J junction details based on top germline gene matches (V end, ";
        if (m_ChainType == "VH" || m_ChainType == "VD" || 
            m_ChainType == "VB")  m_Ostream << "V-D junction, D region, D-J junction, ";
        else m_Ostream << "V-J junction, ";
        m_Ostream << "J start).  Note that possible overlapping nucleotides at VDJ junction (i.e, nucleotides that could be assigned to either rearranging gene) are indicated in parentheses (i.e., (TACT)) but"
                  << " are not included under the V, D, or J gene itself" << endl;
    }

    x_PrintPartialQuery(max(b0, a1 - 5), a1, isHtml); m_Ostream << m_FieldDelimiter;
    if (m_ChainType == "VH" || m_ChainType == "VD" || m_ChainType == "VB") {
        x_PrintPartialQuery(b1, b2, isHtml); m_Ostream << m_FieldDelimiter;
        x_PrintPartialQuery(a2, a3, isHtml); m_Ostream << m_FieldDelimiter;
        x_PrintPartialQuery(b3, b4, isHtml); m_Ostream << m_FieldDelimiter;
    } else {
        x_PrintPartialQuery(b1, b4, isHtml); m_Ostream << m_FieldDelimiter;
    }
    x_PrintPartialQuery(a4, min(b5, a4 + 5), isHtml); m_Ostream << m_FieldDelimiter;

    if (isHtml) {
        m_Ostream << "</tr>\n</table>";

        m_Ostream << "*: Overlapping nucleotides may exist"
                  << " at V-D-J junction (i.e, nucleotides that could be assigned \nto either rearranging gene). "
                  << " Such nucleotides are indicated inside a parenthesis (i.e., (TACAT))\n"
                  << " but are not included under the V, D or J gene itself.\n";
    }
    m_Ostream << endl << endl;

    //cdr3 sequence output
    if (m_Cdr3Seq != NcbiEmptyString){
        if (isHtml) {
            m_Ostream << "Sub-region sequence details:\n";
            m_Ostream << "<table border=1>\n";
            m_Ostream << "<tr><td> </td><td>Nucleotide sequence</td>";
            m_Ostream << "<td>Translation</td>";
            m_Ostream << "<td>Start</td>";
            m_Ostream << "<td>End</td>";
        } else {
            m_Ostream << header << "Sub-region sequence details (nucleotide sequence, translation, start, end)" << endl;
        }
        if (isHtml) {
            m_Ostream << "<tr><td>CDR3</td><td>";
        } else {
            m_Ostream << "CDR3" << m_FieldDelimiter;
        }
        m_Ostream << m_Cdr3Seq << m_FieldDelimiter;
        if (isHtml) {
            m_Ostream << "</td><td>";
        }
        m_Ostream << m_Cdr3SeqTrans << m_FieldDelimiter;
        if (isHtml) {
            m_Ostream << "</td><td>";
        }
        m_Ostream << m_Cdr3Start + 1 << m_FieldDelimiter;
        if (isHtml) {
            m_Ostream << "</td><td>";
        }
        m_Ostream << m_Cdr3End + 1 << m_FieldDelimiter;

        if (isHtml) {
            m_Ostream << "</td></tr>\n</table>";
        }
        
        m_Ostream << endl << endl;
    }
   
};

void CIgBlastTabularInfo::x_ComputeIgDomain(SIgDomain &domain)
{
    int q_pos = 0, s_pos = 0;  // query and subject co-ordinate (translated)
    unsigned int i = 0;  // i is the alignment co-ordinate
    // m_QueryStart and m_SubjectStart are 1-based
    if (domain.start < m_QueryVAlignStart-1) domain.start = m_QueryVAlignStart-1;
    while ( (q_pos < domain.start - m_QueryVAlignStart +1 
          || s_pos < domain.s_start - m_VAlignStart +1)
          && i < m_QueryVAlign.size()) {
        if (m_QueryVAlign[i] != '-') ++q_pos;
        if (m_VAlign[i] != '-') ++s_pos;
        ++i;
    }
    while ( (q_pos < domain.end - m_QueryVAlignStart +1 
          || s_pos < domain.s_end - m_VAlignStart +1)
          && i < m_QueryVAlign.size()) {
        if (m_QueryVAlign[i] != '-') {
            ++q_pos;
            if (m_QueryVAlign[i] == m_VAlign[i]) {
                ++s_pos;
                ++domain.num_match;
            } else if (m_SubjectSeq[i] != '-') {
                ++s_pos;
                ++domain.num_mismatch;
            } else {
                ++domain.num_gap;
            }
        } else {
            ++s_pos;
            ++domain.num_gap;
        }
        ++domain.length;
        ++i;
    }
    if (domain.end > m_QueryVAlignEnd) domain.end = m_QueryVAlignEnd;
};

void CIgBlastTabularInfo::x_PrintIgDomain(const SIgDomain &domain) const
{
    m_Ostream << domain.name 
              << m_FieldDelimiter
              << domain.start +1
              << m_FieldDelimiter
              << domain.end
              << m_FieldDelimiter;
    if (domain.length > 0) {
        m_Ostream  << domain.length
              << m_FieldDelimiter
              << domain.num_match
              << m_FieldDelimiter
              << domain.num_mismatch
              << m_FieldDelimiter
              << domain.num_gap
              << m_FieldDelimiter
              << std::setprecision(3)
              << domain.num_match*100.0/domain.length;
    } else {
        m_Ostream  << "N/A" << m_FieldDelimiter
              <<  "N/A" << m_FieldDelimiter
              <<  "N/A" << m_FieldDelimiter
              <<  "N/A" << m_FieldDelimiter
              <<  "N/A" << m_FieldDelimiter
              <<  "N/A" << m_FieldDelimiter
              <<  "N/A";
    }
};

void CIgBlastTabularInfo::x_PrintIgDomainHtml(const SIgDomain &domain) const
{
    m_Ostream << "<tr><td> " << domain.name << " </td>"
              <<     "<td> " << domain.start+1 << " </td>"
              <<     "<td> " << domain.end << " </td>";
    if (domain.length > 0) {
        m_Ostream  << "<td> " << domain.length << " </td>"
                   << "<td> " << domain.num_match << " </td>"
                   << "<td> " << domain.num_mismatch << " </td>"
                   << "<td> " << domain.num_gap << " </td>"
                   << "<td> " << std::setprecision(3) 
                   << domain.num_match*100.0/domain.length << " </td></tr>\n";
    } else {
        m_Ostream  << "<td> </td><td> </td><td> </td><td> </td></tr>\n";
    }
};

END_SCOPE(align_format)
END_NCBI_SCOPE
