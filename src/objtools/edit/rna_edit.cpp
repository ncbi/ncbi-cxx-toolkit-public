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
    NStr::Tokenize(line,"\t",arr);
    if (arr.size() != 9)  
    {
        msg = "Unable to parse extractor line ";
        msg += line;
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
    arr.clear();
    NStr::TruncateSpacesInPlace(error);  
    if (!error.empty() && error !=  "Broken or partial sequence, no 5.8S!" && error !=  "Broken or partial sequence, only partial 5.8S!")
    {
        msg = "Error returned for: "+accession+" "+error;
        return null_mrna;
    }

    NStr::Tokenize(ssu,":",arr);
    ssu = arr.back();
    NStr::TruncateSpacesInPlace(ssu);
    arr.clear();

    NStr::Tokenize(its1,":",arr);
    its1 = arr.back();
    NStr::TruncateSpacesInPlace(its1);
    arr.clear();

    NStr::Tokenize(r58S,":",arr);
    r58S = arr.back();
    NStr::TruncateSpacesInPlace(r58S);
    arr.clear();

    NStr::Tokenize(its2,":",arr);
    its2 = arr.back();
    NStr::TruncateSpacesInPlace(its2);
    arr.clear();

    NStr::Tokenize(lsu,":",arr);
    lsu = arr.back();
    NStr::TruncateSpacesInPlace(lsu);
    arr.clear();

    vector<string> comments;
    if (ssu != "Not found")
        comments.push_back("18S ribosomal RNA");
    if (its1 != "Not found")
        comments.push_back("internal transcribed spacer 1");
    if (r58S != "Not found")
        comments.push_back("5.8S ribosomal RNA");
    if (its2 != "Not found")
        comments.push_back("internal transcribed spacer 2");
    if (lsu != "Not found")
        comments.push_back("28S ribosomal RNA");

/*    if ((r58S == "Not found"  || r58S == "No start" || r58S == "No end")&& 
        (ssu  != "Not found" || its1  != "Not found" || its2  != "Not found" || lsu  != "Not found"))
    {
        msg = "5.8S not found, but flanking regions have been found.";
        return null_mrna;
    }
    */
    string comment;
    switch(comments.size())
    {
    case 0 : comment = "does not contain rna label";break;
    case 1 : comment = "contains "+comments.front();break;
    case 2 : comment = "contains " + comments[0]+" and "+comments[1];break;
    default : comment = "contains "+comments[0]; for (unsigned int j=1; j<comments.size()-1;j++) comment += ", "+comments[j]; comment += ", and "+comments.back();break;
    }
    negative = strand == "1";
    bsh = x_GetBioseqHandleFromIdGuesser(accession,tse);
    if (!bsh) return null_mrna;
    return x_CreateMiscRna(accession,comment,bsh);
}


CRef <CSeq_feat> CFindITSParser :: x_CreateMiscRna(const string &accession, const string &comment, CBioseq_Handle bsh)
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

