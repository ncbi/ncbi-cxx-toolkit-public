/*
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
* Author:  Alexey Dobronadezhdin
*
* File Description:
*
* ===========================================================================
*/

#ifndef BASE_REPORT_H
#define BASE_REPORT_H

#include <string>

namespace pub_report
{

class CBaseReport
{
public:
    virtual ~CBaseReport() {};

    const std::string& GetCurrentSeqId() const {
        return m_current_seq_id;
    }

    virtual void SetCurrentSeqId(const std::string& name) {
        m_current_seq_id = name;
    }

    virtual void ClearCurrentSeqId() {
        m_current_seq_id.clear();
    }

    virtual void CompleteReport()
    {}

    virtual void ClearData()
    {}

private:

    std::string m_current_seq_id;
};

}

#endif // BASE_REPORT_H