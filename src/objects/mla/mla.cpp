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
* Author:  Aaron Ucko
*
* File Description:
*   Client for Medline archive server
*/

#include <objects/mla/mla.hpp>

#include <corelib/ncbiexpt.hpp>
#include <serial/enumvalues.hpp>
#include <serial/exception.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/serial.hpp>

#include <objects/biblio/PubMedId.hpp>
#include <objects/medlars/Medlars_entry.hpp>
#include <objects/medline/Medline_entry.hpp>
#include <objects/mla/mla__.hpp>
#include <objects/pub/Pub.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


const char* CMLAClient::CException::what(void) const THROWS_NONE
{
    string s;
    if (m_Type == CMla_back::e_Error) {
        s = "MLA error: "
            + GetTypeInfo_enum_EError_val()->FindName(m_Error, true);
    } else {
        s = "Unexpected MLA response type " + CMla_back::SelectionName(m_Type);
    }
    return s.c_str();
}


inline
CRef<CMla_back> CMLAClient::SendRequest(const CMla_request& request)
{
    CMutexGuard guard(m_Mutex);
    if ( !m_Stream ) {
        Init();
    }

    CObjectOStreamAsnBinary out(*m_Stream);
    CObjectIStreamAsnBinary in (*m_Stream);
    CRef<CMla_back>         response(new CMla_back);

    out << request;
    in  >> *response;
    return response;
}


void CMLAClient::Init(void)
{
    if ( m_Stream ) { // Already initialized
        return;
    }
    m_Stream = new CConn_ServiceStream("MedArch");

    CMla_request request;
    request.SetInit();
    CRef<CMla_back> response = SendRequest(request);
    if ( !response->IsInit() ) {
        THROW1_TRACE(runtime_error, "Failure initializing MLA client");
    }
}


void CMLAClient::Fini(void)
{
    if ( !m_Stream ) {
        return;
    }

    CMla_request request;
    request.SetFini();
    CRef<CMla_back> response = SendRequest(request);
    if ( !response->IsFini() ) {
        // Don't throw a exception, since this is normally called by
        // the destructor
        ERR_POST("Failure deinitializing MLA client");
    }

    m_Stream = 0;
}


#define METHOD(out_t, name, in_t, setter, result)    \
out_t CMLAClient::name(in_t in)                      \
{                                                    \
    CMla_request request;                            \
    request.setter;                                  \
    CRef<CMla_back> response = SendRequest(request); \
    try {                                            \
        return result;                               \
    } catch (CInvalidChoiceSelection) {              \
        THROW0_TRACE(CException(*response));         \
    }                                                \
}

METHOD(CRef<CMedline_entry>, GetMle, int, SetGetmle(in),
       &response->SetGetmle())
METHOD(CRef<CPub>, GetPub, int, SetGetpub(in), &response->SetGetpub())
METHOD(CRef<CTitle_msg_list>, GetTitle, const CTitle_msg&, SetGettitle(in),
       &response->SetGettitle())
METHOD(int, CitMatch, const CPub&, SetCitmatch(in), response->GetCitmatch())
METHOD(CMla_back::TGetuids, GetMriUids, int, SetGetmriuids(in),
       response->GetGetuids())
METHOD(CMla_back::TGetuids, GetAccUids, const CMedline_si&, SetGetaccuids(in),
       response->GetGetuids())
METHOD(int, UidToPmid, int, SetUidtopmid(in), response->GetOutpmid().Get())
METHOD(int, PmidToUid, int, SetPmidtouid().Set(in), response->GetOutuid())
METHOD(CRef<CMedline_entry>, GetMlePmid, int, SetGetmlepmid().Set(in),
       &response->SetGetmle())
METHOD(CRef<CPub>, GetPubPmid, int, SetGetpubpmid().Set(in),
       &response->SetGetpub())
METHOD(int, CitMatchPmid, const CPub&, SetCitmatchpmid(in),
       response->GetOutpmid().Get())
METHOD(CMla_back::TGetpmids, GetMriPmids, int, SetGetmripmids(in),
       response->GetGetpmids())
METHOD(CMla_back::TGetpmids, GetAccPmids, const CMedline_si&,
       SetGetaccpmids(in), response->GetGetpmids())
METHOD(CMla_back::TGetpmids, CitLstPmids, const CPub&, SetCitlstpmids(in),
       response->GetGetpmids())
METHOD(CRef<CMedline_entry>, GetMleUid, int, SetGetmleuid(in),
       &response->SetGetmle())
METHOD(CRef<CMedlars_entry>, GetMlrPmid, int, SetGetmlrpmid().Set(in),
       &response->SetGetmlr())
METHOD(CRef<CMedlars_entry>, GetMlrUid, int, SetGetmlruid(in),
       &response->SetGetmlr())

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 6.2  2002/04/22 19:01:19  ucko
* Switch from CFastMutex to CMutex because SendRequest can call itself via Init.
* Throw runtime_error rather than string from Init.
* Move log to end.
*
* Revision 6.1  2002/03/06 22:07:12  ucko
* Add simple code to communicate with the medarch server.
*
* ===========================================================================
*/
