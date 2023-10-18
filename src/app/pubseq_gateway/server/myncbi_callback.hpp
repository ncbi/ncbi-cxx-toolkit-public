#ifndef MY_NCBI_CALLBACK__HPP
#define MY_NCBI_CALLBACK__HPP

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
 * Authors: Sergey Satskiy
 *
 * File Description:
 *
 */

#include <functional>
#include <objtools/pubseq_gateway/impl/myncbi/myncbi_request.hpp>


using TMyNCBIErrorCB = function<void(const string &  cookie,
                                     CRequestStatus::ECode  status,
                                     int  code,
                                     EDiagSev  severity,
                                     const string &  message)>;
using TMyNCBIDataCB = function<void(const string &  cookie,
                                    CPSG_MyNCBIRequest_WhoAmI::SUserInfo info)>;

class IPSGS_Processor;


class CMyNCBIErrorCallback
{
    public:
        CMyNCBIErrorCallback(IPSGS_Processor *  processor,
                             TMyNCBIErrorCB  my_ncbi_error_cb,
                             const string &  cookie);

        void operator()(CRequestStatus::ECode  status,
                        int  code,
                        EDiagSev  severity,
                        const string &  message);

    private:
        IPSGS_Processor *   m_Processor;
        TMyNCBIErrorCB      m_MyNCBIErrorCB;
        string              m_Cookie;

        psg_time_point_t    m_MyNCBITiming;
};



class CMyNCBIDataCallback
{
    public:
        CMyNCBIDataCallback(IPSGS_Processor *  processor,
                            TMyNCBIDataCB  my_ncbi_data_cb,
                            const string &  cookie);

        void operator()(CPSG_MyNCBIRequest_WhoAmI::SUserInfo info);

    private:
        IPSGS_Processor *       m_Processor;
        TMyNCBIDataCB           m_MyNCBIDataCB;
        string                  m_Cookie;

        psg_time_point_t        m_MyNCBITiming;
};


#endif

