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
 * Author:  Sergiy Gotvyanskyy
 *
 * File Description:
 *   Distance matrix readers (UCSC-style Region format)
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>              
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/stream_utils.hpp>

#include <util/static_map.hpp>
#include <util/line_reader.hpp>

#include <serial/iterator.hpp>
#include <serial/objistrasn.hpp>

// Objects includes
#include <objects/general/Int_fuzz.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp> 
#include <objects/general/Dbtag.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Feat_id.hpp>

#include <objtools/readers/read_util.hpp>
#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/ucscregion_reader.hpp>
#include <objtools/error_codes.hpp>

#include <objmgr/util/feature.hpp>

#include <algorithm>


#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

CUCSCRegionReader::CUCSCRegionReader(unsigned int iflags):
  CReaderBase(iflags)
{
}

CUCSCRegionReader::~CUCSCRegionReader()
{
}

CRef<objects::CSeq_feat> CUCSCRegionReader::xParseFeatureUCSCFormat(const string& Line, int Number)
{
    CRef<CSeq_feat> Feat(new CSeq_feat);
#if 0
    vector<string> Tokens;
	string Delims = ".:- \t";
    {{
        size_t IdSubI = 0;
        for(int I = 0; I < Line.length(); I++) {
            CSeq_id::EAccessionInfo Info = CSeq_id::IdentifyAccession(Line.substr(0, I));
            if(Info > 0) {
                IdSubI = I;
            }
        }
        
        if(IdSubI > 0) {
            string IdStr = Line.substr(0, IdSubI);
            string SubLine = Line.substr(IdSubI );
            //cerr << IdStr << endl;
            //cerr << SubLine << endl;
            NStr::Tokenize(NStr::TruncateSpaces(SubLine), Delims, Tokens, NStr::eMergeDelims);
            Tokens.insert(Tokens.begin(), IdStr);
        } else {
            NStr::Tokenize(NStr::TruncateSpaces(Line), Delims, Tokens, NStr::eMergeDelims);
        }
    }}

	//ITERATE(vector<string>, iter, Tokens) {
	//	cerr << "\t" << *iter << endl;
	//}

    if(Tokens.size() < K_START+1)
        return CRef<CSeq_feat>();

	NStr::ReplaceInPlace(Tokens[K_START], ",", "");
	NStr::ReplaceInPlace(Tokens[K_START], ".", "");
	if(Tokens.size() >= K_STOP+1) {
		NStr::ReplaceInPlace(Tokens[K_STOP], ",", "");
		NStr::ReplaceInPlace(Tokens[K_STOP], ".", "");
    }

	Feat->SetData().SetRegion("region: "+NStr::IntToString(Number)); 

    CRef<CSeq_loc> TopLoc(new CSeq_loc);
    TopLoc->SetInt().SetId().Assign(*CRegionFile::x_ParseId(Tokens[K_ID])); 
    TopLoc->SetInt().SetFrom() = NStr::StringToUInt8(Tokens[K_START])-1;
    if(Tokens.size() >= K_STOP+1) 
		TopLoc->SetInt().SetTo() = NStr::StringToUInt8(Tokens[K_STOP])-1;
   	else
		TopLoc->SetInt().SetTo() = TopLoc->GetInt().GetFrom();

	if(Tokens.size() >= K_STRAND+1) {
		if(Tokens[K_STRAND] == "+")
			TopLoc->SetInt().SetStrand() = eNa_strand_plus;
		else if(Tokens[K_STRAND] == "-")
			TopLoc->SetInt().SetStrand() = eNa_strand_minus;
		else
			TopLoc->SetInt().SetStrand() = eNa_strand_plus;
	} else {
		TopLoc->SetInt().SetStrand() = eNa_strand_plus;
    }
	Feat->SetLocation().Assign(*TopLoc);

    if(!Feat->CanGetTitle())
        Feat->SetTitle() = "Line:"+NStr::IntToString(Number);

//cerr << MSerial_AsnText << *Feat;
#endif

    return Feat;
}
//  ----------------------------------------------------------------------------
void CUCSCRegionReader::xSmartFieldSplit(vector<string>& fields, CTempString line)
{
    NStr::Tokenize(line, " \t.-:", fields, NStr::eMergeDelims);
    if (line[line.length()-1] == '-')
        fields.push_back("-");
    while (fields.size() > 3)
    {
        if (fields.size() == 4 && (fields.back() == "+" || fields.back() == "-"))
            break;
        // try to merge first column
        size_t len = fields[0].length();
        if (line[len] == '.')
        {
            fields[0] += line[len];
            fields[0] += fields[1];
            fields.erase(fields.begin()+1);
        }
    }
}
//  ----------------------------------------------------------------------------
bool CUCSCRegionReader::xParseFeature(
    const vector<string>& fields,
    CRef<CSeq_annot>& annot,
    ILineErrorListener* pEC)
{
    //  assign
    string str_line_number = NStr::IntToString(m_uLineNumber);
    CSeq_annot::C_Data::TFtable& ftable = annot->SetData().SetFtable();
    CRef<CSeq_feat> feature;
    feature.Reset( new CSeq_feat );
    try {
        x_SetFeatureLocation(feature, fields);
        feature->SetData().SetRegion() = "region: "+ str_line_number;
        if(!feature->CanGetTitle())
            feature->SetTitle() = "Line:" + str_line_number;
    }
    catch(CObjReaderLineException& err) {
        ProcessError(err, pEC);
        return false;
    }
    ftable.push_back( feature );
    return true;
}
//  ----------------------------------------------------------------------------                
void CUCSCRegionReader::x_SetFeatureLocation(
    CRef<CSeq_feat>& feature,
    const vector<string>& fields )
//  ----------------------------------------------------------------------------
{
    //
    //  Note:
    //  BED convention for specifying intervals is 0-based, first in, first out.
    //  ASN convention for specifying intervals is 0-based, first in, last in.
    //  Hence, conversion BED->ASN  leaves the first leaves the "from" coordinate
    //  unchanged, and decrements the "to" coordinate by one.
    //

    CRef<CSeq_loc> location(new CSeq_loc);
    int from, to;
    from = to = -1;

    //already established: We got at least three columns
    try {
        from = NStr::StringToInt(fields[1], NStr::fAllowCommas)-1;
    }
    catch(std::exception&) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            m_uLineNumber,
            "Invalid data line: Bad \"SeqStart\" value." ) );
        pErr->Throw();
    }
    to = from;

    if (fields.size()>2)
    try {
        to = NStr::StringToInt(fields[2], NStr::fAllowCommas) - 1;
    }
    catch(std::exception&) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            m_uLineNumber,
            "Invalid data line: Bad \"SeqStop\" value.") );
        pErr->Throw();
    }

    if (from == to) {
        location->SetPnt().SetPoint(from);
    }
    else if (from < to) {
        location->SetInt().SetFrom(from);
        location->SetInt().SetTo(to);
    }
    else {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            m_uLineNumber,
            "Invalid data line: \"SeqStop\" less than \"SeqStart\"." ) );
        pErr->Throw();
    }

    size_t strand_field = 3;
    if (strand_field < fields.size()) {
        string strand = fields[strand_field];
        if (strand != "+"  &&  strand != "-"  &&  strand != ".") {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Error,
                m_uLineNumber,
                "Invalid data line: Invalid strand character." ) );
            pErr->Throw();
        }
        location->SetStrand(( fields[strand_field] == "+" ) ?
                           eNa_strand_plus : eNa_strand_minus );
    }
    try
    {
        CRef<CSeq_id> id = CReadUtil::AsSeqId(fields[0], m_iFlags, false);
        //CRef<CSeq_id> id (new CSeq_id(fields[0], CSeq_id::fParse_AnyRaw | m_iFlags));
        location->SetId(*id);
        feature->SetLocation(*location);
    }
    catch(CSeqIdException&)
    {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            m_uLineNumber,
            "Malformed sequence id:" ) );
        pErr->Throw();
    }
    
}
//  ----------------------------------------------------------------------------
CRef<CSerialObject> CUCSCRegionReader::ReadObject(ILineReader& lr, ILineErrorListener* pErrors)
{
    CRef<CSeq_annot> annot = ReadSeqAnnot(lr, pErrors);
    return CRef<CSerialObject>(annot);
}
//  ----------------------------------------------------------------------------                
CRef<CSeq_annot> CUCSCRegionReader::ReadSeqAnnot(
    ILineReader& lr,
    ILineErrorListener* pEC ) 
{
    const size_t MAX_RECORDS = 100000;

    CRef<CSeq_annot> annot;
    CRef<CAnnot_descr> desc;

    annot.Reset(new CSeq_annot);
    desc.Reset(new CAnnot_descr);
    annot->SetDesc(*desc);
    CSeq_annot::C_Data::TFtable& tbl = annot->SetData().SetFtable();

    int featureCount = 0;

    while (!lr.AtEOF()) {

        ++m_uLineNumber;

        CTempString line = *++lr;

        if (NStr::TruncateSpaces_Unsafe(line).empty()) {
            continue;
        }
        if (xParseComment(line, annot)) {
            continue;
        }
	    CTempString record_copy = NStr::TruncateSpaces_Unsafe(line);

        //  parse
        vector<string> fields;

        xSmartFieldSplit(fields, record_copy);

#if 0
        try {
            xCleanColumnValues(fields);
        }
        catch(CObjReaderLineException& err) {
            ProcessError(err, pEC);
            continue;
        }
#endif

        if (xParseFeature(fields, annot, pEC)) {
            ++featureCount;
            continue;
        }
        if (tbl.size() >= MAX_RECORDS) {
            break;
        }
    }
    //  Only return a valid object if there was at least one feature
    if (0 == featureCount) {
        return CRef<CSeq_annot>();
    }
    //x_AddConversionInfo(annot, pEC);
    //x_AssignTrackData( annot );

#if 0
    if(m_columncount >= 3) {
        CRef<CUser_object> columnCountUser( new CUser_object() );
        columnCountUser->SetType().SetStr( "NCBI_BED_COLUMN_COUNT" );
        columnCountUser->AddField("NCBI_BED_COLUMN_COUNT", int ( m_columncount ) );
    
        CRef<CAnnotdesc> userDesc( new CAnnotdesc() );
        userDesc->SetUser().Assign( *columnCountUser );
        annot->SetDesc().Set().push_back( userDesc );
    }
#endif
    return annot;
}


END_objects_SCOPE
END_NCBI_SCOPE
