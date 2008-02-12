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
* Author: Aleksey Grichenko
*
* File Description:
*   EFetch request
*
*/

#include <ncbi_pch.hpp>
#include <objtools/eutils/api/efetch.hpp>
#include <cgi/cgi_util.hpp>


BEGIN_NCBI_SCOPE


CEFetch_Request::CEFetch_Request(CRef<CEUtils_ConnContext>& ctx)
    : CEUtils_Request(ctx, "efetch.fcgi"),
      m_RetStart(0),
      m_RetMax(0),
      m_RetMode(eRetMode_none)
{
}


CEFetch_Request::~CEFetch_Request(void)
{
}


inline
const char* CEFetch_Request::x_GetRetModeName(void) const
{
    static const char* s_RetModeName[] = {
        "none", "xml", "html", "text", "asn.1"
    };
    return s_RetModeName[m_RetMode];
}


string CEFetch_Request::GetQueryString(void) const
{
    string args = TParent::GetQueryString();
    string ids = m_Id.AsQueryString();
    if ( !ids.empty() ) {
        args += "&" + ids;
    }
    if (m_RetStart > 0) {
        args += "&retstart=" + NStr::IntToString(m_RetStart);
    }
    if (m_RetMax > 0) {
        args += "&retmax=" + NStr::IntToString(m_RetMax);
    }
    if ( m_RetMode != eRetMode_none ) {
        args += "&retmode=" +
            URL_EncodeString(x_GetRetModeName(),
            eUrlEncode_ProcessMarkChars);
    }
    return args;
}


ESerialDataFormat CEFetch_Request::GetSerialDataFormat(void) const
{
    switch (m_RetMode) {
    case eRetMode_xml: return eSerial_Xml;
    case eRetMode_asn: return eSerial_AsnText;
    default: break;
    }
    return eSerial_None;
}


CRef<uilist::CIdList> CEFetch_Request::FetchIdList(int chunk_size)
{
    int retstart = GetRetStart();
    int orig_retmax = GetRetMax();
    int retmax = orig_retmax > 0 ? orig_retmax : numeric_limits<int>::max();
    if (chunk_size <= 0) {
        chunk_size = retmax;
    }
    // Get in XML mode
    SetRetMode(eRetMode_xml);
    // Get chunks
    CRef<uilist::CIdList> id_list(new uilist::CIdList);
    uilist::CIdList::TId& ids = id_list->SetId();
    uilist::CIdList tmp;
    try {
        for (int i = retstart; i < retmax; i += chunk_size) {
            SetRetStart(i);
            SetRetMax(chunk_size < (retmax - i) ? chunk_size : (retmax - i));
            *GetObjectIStream() >> tmp;
            if ( tmp.SetId().empty() ) {
                break;
            }
            ids.splice(ids.end(), tmp.SetId());
        }
    }
    catch (CSerialException) {
    }
    catch (...) {
        SetRetStart(retstart);
        SetRetMax(orig_retmax);
        throw;
    }
    SetRetStart(retstart);
    SetRetMax(orig_retmax);
    return id_list;
}


CEFetch_Literature_Request::
CEFetch_Literature_Request(ELiteratureDB db,
                           CRef<CEUtils_ConnContext>& ctx)
    : CEFetch_Request(ctx),
      m_RetType(eRetType_none)
{
    static const char* s_LitDbName[] = {
        "pubmed", "pmc", "journals", "omim"
    };
    SetDatabase(s_LitDbName[db]);
}


inline
const char* CEFetch_Literature_Request::x_GetRetTypeName(void) const
{
    static const char* s_LitRetTypeName[] = {
        "", "uilist", "abstract", "citation", "medline", "full"
    };
    return s_LitRetTypeName[m_RetType];
}


string CEFetch_Literature_Request::GetQueryString(void) const
{
    string args = TParent::GetQueryString();
    if (m_RetType != eRetType_none) {
        args += "&rettype=";
        args += x_GetRetTypeName();
    }
    return args;
}


CRef<uilist::CIdList>
CEFetch_Literature_Request::FetchIdList(int chunk_size)
{
    SetRetType(eRetType_uilist);
    return TParent::FetchIdList(chunk_size);
}


CEFetch_Sequence_Request::
CEFetch_Sequence_Request(ESequenceDB db,
                         CRef<CEUtils_ConnContext>& ctx)
    : CEFetch_Request(ctx),
      m_RetType(eRetType_none),
      m_Complexity(eComplexity_none),
      m_Strand(eStrand_none),
      m_SeqStart(0),
      m_SeqStop(0)
{
    static const char* s_SeqDbName[] = {
        "gene", "genome", "nucleotide", "nuccore", "nucest", "nucgss",
        "protein", "popset", "snp", "sequences"
    };
    SetDatabase(s_SeqDbName[db]);
}


inline
const char* CEFetch_Sequence_Request::x_GetRetTypeName(void) const
{
    static const char* s_SeqRetTypeName[] = {
        "", "native", "fasta", "gb", "gbc", "gbwithparts", "est", "gss",
        "gp", "gpc", "seqid", "acc", "chr", "flt", "rsr", "brief", "docset"
    };
    return s_SeqRetTypeName[m_RetType];
}


string CEFetch_Sequence_Request::GetQueryString(void) const
{
    string args = TParent::GetQueryString();
    if (m_RetType != eRetType_none) {
        args += "&rettype=";
        args += x_GetRetTypeName();
    }
    if (m_Complexity != eComplexity_none) {
        args += "&complexity=";
        args += NStr::IntToString(m_Complexity);
    }
    if (m_Strand != eStrand_none) {
        args += "&strand=";
        args += NStr::IntToString(m_Strand);
    }
    if (m_SeqStart > 0) {
        args += "&seq_start=";
        args += NStr::IntToString(m_SeqStart);
    }
    if (m_SeqStop > 0) {
        args += "&seq_stop=";
        args += NStr::IntToString(m_SeqStop);
    }
    return args;
}


CEFetch_Taxonomy_Request::
CEFetch_Taxonomy_Request(CRef<CEUtils_ConnContext>& ctx)
    : CEFetch_Request(ctx),
      m_Report(eReport_none)
{
    SetDatabase("taxonomy");
}


inline
const char* CEFetch_Taxonomy_Request::x_GetReportName(void) const
{
    static const char* s_TaxReportName[] = {
        "", "uilist", "brief", "docsum", "xml"
    };
    return s_TaxReportName[m_Report];
}


string CEFetch_Taxonomy_Request::GetQueryString(void) const
{
    string args = TParent::GetQueryString();
    if (m_Report != eReport_none) {
        args += "&report=";
        args += x_GetReportName();
    }
    return args;
}


CRef<uilist::CIdList>
CEFetch_Taxonomy_Request::FetchIdList(int chunk_size)
{
    SetReport(eReport_uilist);
    return TParent::FetchIdList(chunk_size);
}


END_NCBI_SCOPE
