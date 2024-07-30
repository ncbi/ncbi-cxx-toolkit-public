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
 *   message handler/writer for asnvalidate
 *
 */
#ifndef ASNVAL_MESSAGE_HANDLER_HPP
#define ASNVAL_MESSAGE_HANDLER_HPP

#include <corelib/ncbistd.hpp>
#include <objects/valerr/ValidError.hpp>
#include "formatters.hpp"
#include <queue>
#include <atomic>
#include <mutex>
#include <condition_variable>


BEGIN_NCBI_SCOPE
namespace objects {
class CValidErrItem;
class CSeqdesc;
class CSeq_entry;
}


class IMessageHandler : public objects::IValidError
{
public:
    IMessageHandler(const CAppConfig& config, CNcbiOstream& ostr);
    virtual ~IMessageHandler() = default;

    void AddValidErrItem(
        EDiagSev             sev,     // severity
        unsigned int         ec,      // error code
        const string&        msg,     // specific error message
        const string&        desc,    // offending object's description
        const CSerialObject& obj,     // offending object
        const string&        acc,     // accession of object.
        const int            ver,     // version of object.
        const string&        location = kEmptyStr, // formatted location of object
        const int            seq_offset = 0) override;

    void AddValidErrItem(
        EDiagSev             sev,     // severity
        unsigned int         ec,      // error code
        const string&        msg,     // specific error message
        const string&        desc,    // offending object's description
        const string&        acc,     // accession of object.
        const int            ver,     // version of object.
        const string&        location = kEmptyStr, // formatted location of object
        const int            seq_offset = 0) override;

    void AddValidErrItem(
        EDiagSev             sev,     // severity
        unsigned int         ec,      // error code
        const string&        msg,     // specific error message
        const string&        desc,    // offending object's description
        const objects::CSeqdesc&      seqdesc, // offending object
        const objects::CSeq_entry&    ctx,     // place of packaging
        const string&        acc,     // accession of object or context.
        const int            ver,     // version of object.
        const int            seq_offset = 0) override;

    void AddValidErrItem(
        EDiagSev             sev,     // severity
        unsigned int         ec,      // error code
        const string&        msg) override;

    virtual void AddValidErrItem(CRef<objects::CValidErrItem> item) = 0;

    virtual void Write(bool ignoreInferences = true) = 0;
    virtual void RequestStop() = 0;
    virtual bool InvokeWrite() const = 0;
    virtual void SetInvokeWrite(bool invokeWrite) = 0;
    virtual size_t GetNumReported() const = 0;

protected:
    bool Ignore(const objects::CValidErrItem& item) const;
    bool Report(const objects::CValidErrItem& item) const;

    unique_ptr<IFormatter> m_pFormat;
    CNcbiOstream& m_Ostr;
private:
    const CAppConfig& mAppConfig;
};


class CSerialMessageHandler : public IMessageHandler
{
public:
    CSerialMessageHandler(const CAppConfig& config, CNcbiOstream& ostr);

    ~CSerialMessageHandler();
    
    void AddValidErrItem(CRef<objects::CValidErrItem> item) override;

    void Write(bool ignoreInferences = true) override {}
    void RequestStop() override {}
    bool InvokeWrite() const override { return false; }
    void SetInvokeWrite(bool /* dummy */) override {}
    size_t GetNumReported() const override { return m_NumReported; }

private:
    size_t m_NumReported{0};
};

class CAsyncMessageHandler : public IMessageHandler
{
public:
    CAsyncMessageHandler(const CAppConfig& config, CNcbiOstream& ostr);

    ~CAsyncMessageHandler();
    
    void AddValidErrItem(CRef<objects::CValidErrItem> item) override;

    void Write(bool ignoreInferences = true) override;
    void RequestStop() override;
    bool InvokeWrite() const override { return m_InvokeWrite; }
    void SetInvokeWrite(bool invokeWrite) override { m_InvokeWrite=invokeWrite; }
    size_t GetNumReported() const override { return m_NumReported; }
private:
    condition_variable m_Cv;
    mutex m_Mutex;
    queue<CRef<objects::CValidErrItem>> m_Items;
    bool m_InvokeWrite = true;
    atomic<size_t> m_NumReported{0};
};

END_NCBI_SCOPE
#endif
