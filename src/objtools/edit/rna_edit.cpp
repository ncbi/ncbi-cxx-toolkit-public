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
* Author: Igor Filippov
*
* File Description:
*   functions for parsing FindITS output
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objtools/edit/seqid_guesser.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objtools/edit/rna_edit.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

CFindITSParser::CFindITSParser(const char *input, CSeq_entry_Handle tse) :  m_istr(input), m_tse(tse)
{
    m_lr.Reset(ILineReader::New(m_istr));
 
    if (m_lr.Empty()) 
    {
        NCBI_THROW(CException, eUnknown, "Unable to read Label RNA|ITS results");
    }
}

CRef <CSeq_feat> CFindITSParser::ParseLine()
{
   const CTempString& line = *++*m_lr;
   return x_ParseLine(line, m_tse, m_bsh, m_negative, m_msg);  
}

// Dear Future self: https://xkcd.com/1421/
CRef <CSeq_feat> CFindITSParser :: x_ParseLine(const CTempString &line, CSeq_entry_Handle tse, CBioseq_Handle &bsh, bool &negative, string &msg)
{
    CRef <CSeq_feat> null_mrna(NULL);
    vector<string> arr;
    NStr::Split(line,"\t",arr);
    if (arr.size() != 9)  
    {
        return null_mrna;
    }
    string accession = arr[0];
    string ssu = arr[2];
    string its1 = arr[3];
    string r58S = arr[4];
    string its2 = arr[5];
    string lsu = arr[6];
    string error = arr[7];  
    string strand = arr[8]; 

    bsh = x_GetBioseqHandleFromIdGuesser(accession,tse);
    if (!bsh) 
    {
        return null_mrna;
    }

    arr.clear();
    NStr::TruncateSpacesInPlace(error);  
    if (!error.empty() && error !=  "Broken or partial sequence, no 5.8S!" && error !=  "Broken or partial sequence, only partial 5.8S!")
    {
        msg = "Error returned for: "+accession+" "+error;
        return null_mrna;
    }

    NStr::Split(ssu,":",arr);
    ssu = arr.back();
    NStr::TruncateSpacesInPlace(ssu);
    arr.clear();

    NStr::Split(its1,":",arr);
    its1 = arr.back();
    NStr::TruncateSpacesInPlace(its1);
    arr.clear();

    NStr::Split(r58S,":",arr);
    r58S = arr.back();
    NStr::TruncateSpacesInPlace(r58S);
    arr.clear();

    NStr::Split(its2,":",arr);
    its2 = arr.back();
    NStr::TruncateSpacesInPlace(its2);
    arr.clear();

    NStr::Split(lsu,":",arr);
    lsu = arr.back();
    NStr::TruncateSpacesInPlace(lsu);
    arr.clear();

    bool ssu_present(false);
    bool lsu_present(false);
    bool ssu_too_large(false);
    bool lsu_too_large(false);
    bool r58S_too_large(false);
    bool its1_span(false);
    bool its2_span(false);
   
    vector<string> comments;
    if (ssu != "Not found")
    {
        comments.push_back("small subunit ribosomal RNA");
        ssu_present = true;
        ssu_too_large = IsLengthTooLarge(ssu, 2200);
    }
    if (its1 != "Not found")
    {
        comments.push_back("internal transcribed spacer 1");
        NStr::Split(its1,"-",arr);
        its1_span =  (arr.size() == 2);
        arr.clear();
    }
    if (r58S != "Not found")
    {
        comments.push_back("5.8S ribosomal RNA");
        r58S_too_large = IsLengthTooLarge(r58S, 200);
    }
    if (its2 != "Not found")
    {
        comments.push_back("internal transcribed spacer 2");
        NStr::Split(its2,"-",arr);
        its2_span =  (arr.size() == 2);
        arr.clear();
    }
    if (lsu != "Not found")
    {
        comments.push_back("large subunit ribosomal RNA");
        lsu_present = true;
        lsu_too_large = IsLengthTooLarge(lsu, 5100);
    }

    if (its1_span && its2_span && (r58S == "Not found" || r58S == "No end" || r58S == "No start"))
    {
        msg = "5.8S is not found while ITS1 and ITS2 spans exist in: "+accession;
        return null_mrna;
    }
    if (ssu_too_large)
    {
        msg = "SSU too large in: "+accession;
        return null_mrna;
    }
    if (lsu_too_large)
    {
        msg = "LSU too large in: "+accession;
        return null_mrna;
    }
    if (r58S_too_large)
    {
        msg = "5.8S too large in: "+accession;
        return null_mrna;
    }
    

    string comment;
    switch(comments.size())
    {
    case 0 : comment = "does not contain rna label";break;
    case 1 : 
    {
        if (!ssu_present && !lsu_present) 
        {
            comment = "contains "+comments.front();
        } 
    }
    break;
    case 2 : comment = "contains " + comments[0]+" and "+comments[1];break;
    default : comment = "contains "+comments[0]; for (unsigned int j=1; j<comments.size()-1;j++) comment += ", "+comments[j]; comment += ", and "+comments.back();break;
    }
    negative = strand == "1";
    if (comments.size() == 1 && (ssu_present || lsu_present))
        return x_CreateRRna(comments.front(), bsh);
    return x_CreateMiscRna(comment,bsh);
}

bool CFindITSParser :: IsLengthTooLarge(const string& str, int max_length)
{
    vector<string> arr;
    NStr::Split(str,"-",arr);
    if (arr.size() == 2)
    {
        int start = NStr::StringToInt(arr.front(), NStr::fConvErr_NoThrow);
        int end = NStr::StringToInt(arr.back(), NStr::fConvErr_NoThrow);
        int length = end - start + 1;
        return length > max_length;
    }
    return false;
}

CRef <CSeq_feat> CFindITSParser :: x_CreateMiscRna(const string &comment, CBioseq_Handle bsh)
{
    CRef <CSeq_feat> new_mrna (new CSeq_feat());
    new_mrna->SetData().SetRna().SetType(CRNA_ref::eType_miscRNA);
    new_mrna->SetComment(comment);

    CRef<CSeq_loc> loc(new CSeq_loc());
    loc->SetInt().SetFrom(0);
    loc->SetInt().SetTo(bsh.GetBioseqLength()-1);
    loc->SetInt().SetStrand(eNa_strand_plus);
    loc->SetPartialStart(true, eExtreme_Positional); 
    loc->SetPartialStop(true, eExtreme_Positional); 
    loc->SetId(*bsh.GetSeqId());
    new_mrna->SetLocation(*loc);
    
    new_mrna->SetPartial(true);
    return new_mrna;
}

CRef <CSeq_feat> CFindITSParser :: x_CreateRRna(const string &comment, CBioseq_Handle bsh)
{
    CRef <CSeq_feat> new_rrna (new CSeq_feat());
    new_rrna->SetData().SetRna().SetType(CRNA_ref::eType_rRNA);
    string remainder;
    new_rrna->SetData().SetRna().SetRnaProductName(comment, remainder);
  
    CRef<CSeq_loc> loc(new CSeq_loc());
    loc->SetInt().SetFrom(0);
    loc->SetInt().SetTo(bsh.GetBioseqLength()-1);
    loc->SetInt().SetStrand(eNa_strand_plus);
    loc->SetPartialStart(true, eExtreme_Positional); 
    loc->SetPartialStop(true, eExtreme_Positional); 
    loc->SetId(*bsh.GetSeqId());
    new_rrna->SetLocation(*loc);
    
    new_rrna->SetPartial(true);
    return new_rrna;
}

CBioseq_Handle CFindITSParser :: x_GetBioseqHandleFromIdGuesser(const string &id_str, objects::CSeq_entry_Handle tse)
{
    CRef<edit::CStringConstraint> constraint(new edit::CStringConstraint(id_str, edit::CStringConstraint::eMatchType_Equals));
    CBioseq_CI bi(tse, CSeq_inst::eMol_na);
    while (bi) 
    {
        if (edit::CSeqIdGuesser::DoesSeqMatchConstraint(*bi,constraint))
            return *bi;
        ++bi;
    }

    return CBioseq_Handle();
}


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

