#ifndef MLA__HPP
#define MLA__HPP

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

#include <corelib/ncbistd.hpp>
#include <corelib/ncbithr.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <objects/mla/Mla_back.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CMedline_si;
class CMla_request;
class CTitle_msg;

class CMLAClient
{
public:
    CMLAClient(void) : m_Stream(0) { }
    ~CMLAClient(void) { Fini(); }

    class CException : public runtime_error
    {
    public:
        CException(const CMla_back& response)
            : runtime_error("MLA client exception"), m_Type(response.Which()),
              m_Error(response.IsError() ? response.GetError() : (EError_val)0)
            {}
        virtual const char* what(void) const THROWS_NONE;

    private:
        CMla_back::E_Choice m_Type;
        EError_val          m_Error;
    };

    // Normally called automatically (upon first request and
    // destruction, respectively), but available for explicit use
    void Init(void);
    void Fini(void);

    // Other possible request types; see mla.asn
    CRef<CMedline_entry>  GetMle      (int);
    CRef<CPub>            GetPub      (int);
    CRef<CTitle_msg_list> GetTitle    (const CTitle_msg&);
    int                   CitMatch    (const CPub&);
    CMla_back::TGetuids   GetMriUids  (int);
    CMla_back::TGetuids   GetAccUids  (const CMedline_si&);
    int                   UidToPmid   (int);
    int                   PmidToUid   (int);
    CRef<CMedline_entry>  GetMlePmid  (int);
    CRef<CPub>            GetPubPmid  (int);
    int                   CitMatchPmid(const CPub&);
    CMla_back::TGetpmids  GetMriPmids (int);
    CMla_back::TGetpmids  GetAccPmids (const CMedline_si&);
    CMla_back::TGetpmids  CitLstPmids (const CPub&);
    CRef<CMedline_entry>  GetMleUid   (int);
    CRef<CMedlars_entry>  GetMlrPmid  (int);
    CRef<CMedlars_entry>  GetMlrUid   (int);

private:
    CRef<CMla_back> SendRequest(const CMla_request& request);

    CConn_ServiceStream* m_Stream; // Initialized on first use
    CMutex               m_Mutex;
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.2  2002/04/22 19:00:15  ucko
* Switch from CFastMutex to CMutex because SendRequest can call itself via Init.
* Move log to end.
*
* Revision 1.1  2002/03/06 22:07:11  ucko
* Add simple code to communicate with the medarch server.
*
* ===========================================================================
*/
#endif  /* MLA__HPP */
