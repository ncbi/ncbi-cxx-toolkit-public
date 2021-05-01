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
 * Authors:  Frank Ludwig
 *
 * File Description: Common file reader utility functions.
 *
 */

#include <ncbi_pch.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objtools/readers/read_util.hpp>
#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/line_error.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
void CReadUtil::Tokenize(
    const string& str,
    const string& delim,
    vector< string >& parts )
//  ----------------------------------------------------------------------------
{
    string temp;
    bool inQuote(false);
    const char joiner('#');

    for (size_t i=0; i < str.size(); ++i) {
        switch(str[i]) {

        default:
            break;
        case '\"':
            inQuote = inQuote ^ true;
            break;
        case ' ':
            if (inQuote) {
                if (temp.empty())
                    temp = str;
                temp[i] = joiner;
            }
            break;
        }
    }
    if (temp.empty()) {
        NStr::Split(str, delim, parts, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
        return;
    }
    NStr::Split(temp, delim, parts, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
    for (size_t j=0; j < parts.size(); ++j) {
        for (size_t i=0; i < parts[j].size(); ++i) {
            if (parts[j][i] == joiner) {
                parts[j][i] = ' ';
            }
        }
    }
}

//  -----------------------------------------------------------------
CRef<CSeq_id> CReadUtil::AsSeqId(
    const string& givenId,
    unsigned int flags,
    bool localInts)
//  -----------------------------------------------------------------
{
    string rawId(NStr::URLDecode(givenId, NStr::eUrlDec_Percent));

    if (flags & CReaderBase::fAllIdsAsLocal) {
        CRef<CSeq_id> pId(new CSeq_id);
        bool idSet = false;
        if (string::npos == rawId.find_first_not_of("0987654321")  &&
                localInts) {
            // GEO-1239 : if id is numeric but doesn't fit into an integer, use
            // string representation.
            try {
                pId->SetLocal().SetId(NStr::StringToInt(rawId));
                idSet = true;
            } catch(CException&) {}
        }
        if (!idSet) {
            pId->SetLocal().SetStr(rawId);
        }
        return pId;
    }
    try {
        CRef<CSeq_id> pId(new CSeq_id(rawId));
        if (!pId) {
            pId.Reset(new CSeq_id(CSeq_id::e_Local, rawId));
            return pId;
        }
        if (pId->IsGi()) {
            if (flags & CReaderBase::fNumericIdsAsLocal  ||
                    pId->GetGi() < GI_CONST(500)) {
                pId.Reset(new CSeq_id);
                if (!localInts) {
                    pId->SetLocal().SetStr(rawId);
                }
                else {
                    pId->SetLocal().SetId(NStr::StringToInt(rawId));
                }
                return pId;
            }
        }
        return pId;
    }
    catch(CSeqIdException&) {
    }
    return CRef<CSeq_id>(new CSeq_id(CSeq_id::e_Local, rawId));
}


//  ----------------------------------------------------------------------------
bool CReadUtil::GetTrackName(
    const CSeq_annot& annot,
    string& value)
//  ----------------------------------------------------------------------------
{
    return GetTrackValue(annot, "name", value);
}

//  ----------------------------------------------------------------------------
bool CReadUtil::GetTrackAssembly(
    const CSeq_annot& annot,
    string& value)
//  ----------------------------------------------------------------------------
{
    return GetTrackValue(annot, "db", value);
}

//  ----------------------------------------------------------------------------
bool CReadUtil::GetTrackOffset(
    const CSeq_annot& annot,
    int& value)
//  ----------------------------------------------------------------------------
{
    string offset;
    if (!GetTrackValue(annot, "offset", offset)) {
        value = 0;
    }
    else {
        value = NStr::StringToInt(offset);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CReadUtil::GetTrackValue(
    const CSeq_annot& annot,
    const string& key,
    string& value)
//  -----------------------------------------------------------------------------
{
    const string sTrackDataClass("Track Data");

    if (!annot.IsSetDesc()) {
        return false;
    }
    const CAnnot_descr::Tdata& descr = annot.GetDesc().Get();
    for (CAnnot_descr::Tdata::const_iterator ait = descr.begin();
            ait != descr.end(); ++ait) {
        const CAnnotdesc& desc = **ait;
        if (!desc.IsUser()) {
            continue;
        }
        const CUser_object& user = desc.GetUser();
        if (!user.IsSetType()) {
            continue;
        }
        const CObject_id& oid = user.GetType();
        if (!oid.IsStr()  ||  oid.GetStr() != sTrackDataClass) {
            continue;
        }
        if (!user.IsSetData()) {
            continue;
        }
        const CUser_object::TData& fields = user.GetData();
        for (CUser_object::TData::const_iterator fit = fields.begin();
                fit != fields.end(); ++fit) {
            const CUser_field& field = **fit;
            if (!field.IsSetLabel()  ||  !field.GetLabel().IsStr()) {
                continue;
            }
            if (field.GetLabel().GetStr() != key) {
                continue;
            }
            if (!field.IsSetData()  ||  !field.GetData().IsStr()) {
                return false;
            }
            value = field.GetData().GetStr();
            return true;
        }
    }
    return false;
}

//  ----------------------------------------------------------------------------
void sParseGeneOntologyTerm(
    const CTempString& qual,
    const CTempString& rawVal,
    string& comment,
    string& goId,
    string& pmId,
    string& evidenceCodes)
//  ----------------------------------------------------------------------------
{
    unique_ptr<CLineError> pErr(CLineError::Create(
        ILineError::eProblem_QualifierBadValue,
        eDiag_Error,
        "",
        0,
        "",
        qual,
        rawVal,
        ""));

    vector<CTempString> fields; fields.reserve(4);
    NStr::Split(rawVal, "|", fields);
    while (fields.size() < 4) {
        fields.push_back("");
    }
    if (fields.size() != 4) {
        pErr->PatchErrorMessage("GO term contains too many columns");
        pErr->Throw();
    }
    for (auto& field: fields) {
        NStr::TruncateSpacesInPlace(field);
    }

    //column 0: comment, anything goes:
    comment = fields[0];

    //column 1: GO id, must be seven digits if present at all:
    goId = fields[1];
    NStr::ToLower(goId);
    if (NStr::StartsWith(goId, "go:")) {
        goId = goId.substr(3);
    }
    if (goId.size() > 7) {
        pErr->PatchErrorMessage("GO ID (column 2) must be at most seven digits long");
        pErr->Throw();
    }
    if (goId.find_first_not_of("0123456789") != string::npos) {
        pErr->PatchErrorMessage("GO ID (column 2) can only contain digits");
        pErr->Throw();
    }
    if (!goId.empty()) {
        while (goId.size() < 7) {
            goId = string("000000") + goId;
            goId = goId.substr(goId.size() - 7);
        }
    }

    //column 2: PMID or GO_REF:
    pmId = fields[2];
    NStr::ToLower(pmId);
    if (pmId.find_first_not_of("0123456789") != string::npos) {
        pErr->PatchErrorMessage("Pubmed ID (column 3) may only contain digits");
        pErr->Throw();
    }

    //column 3: comma separated string of evidence codes. We don't validate it yet (rw-621)
    evidenceCodes = fields[3];

}

//  ----------------------------------------------------------------------------
void CReadUtil::AddGeneOntologyTerm(
    CSeq_feat& feature,
    const CTempString& qual,
    const CTempString& val)
//  ----------------------------------------------------------------------------
{
    static const string k_GoQuals[] = {"go_process", "go_component", "go_function"};
    static const int k_NumGoQuals = sizeof (k_GoQuals) / sizeof (string);

    if (qual.empty ()) return;

    int j = 0;
    while (j < k_NumGoQuals && !NStr::EqualNocase(k_GoQuals[j], qual)) {
        j++;
    }
    if (j == k_NumGoQuals) {
        unique_ptr<CLineError> pErr(CLineError::Create(
            ILineError::eProblem_InvalidQualifier,
            eDiag_Error,
            "",
            0,
            "",
            qual,
            val,
            "GO_ qualifier not recognized"));
        pErr->Throw();
    }

    string comment, goId, pmId, evidenceCodes;
    sParseGeneOntologyTerm(qual, val, comment, goId, pmId, evidenceCodes);

    string label;
    qual.Copy(label, 3, CTempString::npos); // prefix 'go_' should be eliminated
    label[0] = toupper(label[0]);

    feature.SetExt().SetType().SetStr("GeneOntology");
    CUser_field& wrap = feature.SetExt().SetField(label);
    CRef<CUser_field> new_field(new CUser_field);
    CUser_field& field = *new_field;
    wrap.SetData().SetFields().push_back(new_field);
    field.SetLabel().SetId(0); // compatible with C-toolkit

    CRef<CUser_field> text_field(new CUser_field());
    text_field->SetLabel().SetStr("text string");
    text_field->SetData().SetStr(CUtf8::AsUTF8(comment, eEncoding_Ascii));
    field.SetData().SetFields().push_back(text_field);

    if (!NStr::IsBlank(goId)) {
        CRef<CUser_field> goid (new CUser_field());
        goid->SetLabel().SetStr("go id");
        goid->SetData().SetStr(CUtf8::AsUTF8(goId, eEncoding_Ascii));
        field.SetData().SetFields().push_back(goid);
    }

    //if (NStr::StartsWith(pmId, "0")) {
    //    //pmId really is goRef
    //    CRef<CUser_field> goref (new CUser_field());
    //    goref->SetLabel().SetStr("go ref");
    //    goref->SetData().SetStr(pmId);
    //    field.SetData().SetFields().push_back(goref);
    //}

    if (!NStr::IsBlank(pmId)) {
        CRef<CUser_field> pubmed (new CUser_field());
        pubmed->SetLabel().SetStr("pubmed id");
        pubmed->SetData().SetInt(NStr::StringToInt(pmId));
        field.SetData().SetFields().push_back(pubmed);
    }

    if (!NStr::IsBlank(evidenceCodes)) {
        CRef<CUser_field> evidence (new CUser_field());
        evidence->SetLabel().SetStr("evidence");
        evidence->SetData().SetStr( CUtf8::AsUTF8(evidenceCodes, eEncoding_Ascii) );
        field.SetData().SetFields().push_back(evidence);
    }
}


END_objects_SCOPE
END_NCBI_SCOPE
