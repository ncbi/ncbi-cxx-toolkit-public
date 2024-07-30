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
 * Author:  Justin Foley
 *
 * File Description:
 *   asnvalidate output formatters
 *
 */

#include <ncbi_pch.hpp>
#include <objects/valerr/ValidError.hpp>
#include <objects/valerr/ValidErrItem.hpp>
#include <objects/seq/Seqdesc.hpp>
#include "message_handler.hpp" 
#include "app_config.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


IMessageHandler::IMessageHandler(const CAppConfig& config, CNcbiOstream& ostr) :
    mAppConfig(config), m_pFormat(g_CreateFormatter(config, ostr)), m_Ostr(ostr) {}


bool IMessageHandler::Ignore(const CValidErrItem& item) const 
{
    const auto severity = item.GetSeverity();

    if (severity < mAppConfig.mLowCutoff ||
        severity > mAppConfig.mHighCutoff) {
        return true;
    }

    if (!mAppConfig.mOnlyError.empty() && !(NStr::EqualNocase(mAppConfig.mOnlyError,item.GetErrCode()))) {
        return true;
    }

    return false;
}

void IMessageHandler::AddValidErrItem(
    EDiagSev             sev,
    unsigned int         ec,
    const string&        msg,
    const string&        desc,
    const CSerialObject& obj,
    const string&        acc,
    const int            ver,
    const string&        location,
    const int            seq_offset) 
{
    CRef<CValidErrItem> item(new CValidErrItem(sev, ec, msg, desc, &obj, nullptr, acc, ver, seq_offset));
    if (!NStr::IsBlank(location)) {
        item->SetLocation(location);
    }

    AddValidErrItem(item);
}


void IMessageHandler::AddValidErrItem(
    EDiagSev             sev,
    unsigned int         ec,
    const string&        msg,
    const string&        desc,
    const string&        acc,
    const int            ver,
    const string&        location,
    const int            seq_offset) 
{
    CRef<CValidErrItem> item(new CValidErrItem(sev, ec, msg, desc, nullptr, nullptr, acc, ver, seq_offset));
    if (!NStr::IsBlank(location)) {
        item->SetLocation(location);
    }

    AddValidErrItem(item);
}


void IMessageHandler::AddValidErrItem(
    EDiagSev          sev,
    unsigned int      ec,
    const string&     msg,
    const string&     desc,
    const CSeqdesc&   seqdesc,
    const CSeq_entry& ctx,
    const string&     acc,
    const int         ver,
    const int         seq_offset)
{
    CRef<CValidErrItem> item(new CValidErrItem(sev, ec, msg, desc, &seqdesc, &ctx, acc, ver, seq_offset));
    AddValidErrItem(item);
}


void IMessageHandler::AddValidErrItem(EDiagSev sev, unsigned int ec, const string& msg)
{
    CRef<CValidErrItem> item(new CValidErrItem());
    item->SetSev(sev);
    item->SetErrIndex(ec);
    item->SetMsg(msg);
    item->SetErrorName(CValidErrItem::ConvertErrCode(ec));
    item->SetErrorGroup(CValidErrItem::ConvertErrGroup(ec));

    AddValidErrItem(item);
}


bool IMessageHandler::Report(const CValidErrItem& item) const
{
    return (mAppConfig.mReportLevel <= item.GetSeverity());
}

CSerialMessageHandler::CSerialMessageHandler(const CAppConfig& config, CNcbiOstream& ostr) :
    IMessageHandler(config, ostr) {
        m_pFormat->Start();
}


CSerialMessageHandler::~CSerialMessageHandler() {
    m_pFormat->Finish();
}


void CSerialMessageHandler::AddValidErrItem(CRef<CValidErrItem> pItem)
{
    if (pItem.Empty() || Ignore(*pItem)) {
        return;
    }
    if (Report(*pItem)) {
        ++m_NumReported;
    }
    (*m_pFormat)(*pItem);
}


CAsyncMessageHandler::CAsyncMessageHandler(const CAppConfig& config, CNcbiOstream& ostr) :
    IMessageHandler(config, ostr) {
    m_pFormat->Start();
}


CAsyncMessageHandler::~CAsyncMessageHandler() 
{
    // Make sure that all messages are written
    { 
        lock_guard<mutex> lock(m_Mutex);
        if (m_Items.empty()) {
            m_pFormat->Finish();
            return;
        }
        if (m_Items.back().NotEmpty()) {
            m_Items.push(CRef<CValidErrItem>());
        }
    }

    Write();
    m_pFormat->Finish();
}



void CAsyncMessageHandler::AddValidErrItem(CRef<CValidErrItem> pItem) {
   
    const auto notEmpty = pItem.NotEmpty();
    if (notEmpty) {
        if (Ignore(*pItem)) {
            return;
        }
        if (Report(*pItem)) {
            ++m_NumReported;
        }
    }

        
    {
        lock_guard<mutex> lock(m_Mutex);
        m_Items.push(pItem);
    }
    m_Cv.notify_all();
}


void CAsyncMessageHandler::Write(bool ignoreInferences) {
    while (true) {
        unique_lock<mutex> lock(m_Mutex);
        m_Cv.wait(lock, [this]()->bool
                {
                    return !m_Items.empty();
                });
        auto pItem = m_Items.front();
        m_Items.pop();
        if (!pItem) {
            break;
        }
        if (ignoreInferences && pItem->GetErrGroup() == "SEQ_FEAT" && pItem->GetErrCode() == "InvalidInferenceValue") {
            continue;
        }
        (*m_pFormat)(*pItem);
    }
}


void CAsyncMessageHandler::RequestStop() {
    AddValidErrItem(CRef<CValidErrItem>());
}




END_NCBI_SCOPE
