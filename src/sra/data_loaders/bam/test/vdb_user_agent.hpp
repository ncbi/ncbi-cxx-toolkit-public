#ifndef SRA__VDB_USER_AGENT__HPP
#define SRA__VDB_USER_AGENT__HPP
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
* Author:  Eugene Vasilchenko
*
* File Description:
*   Monitor VDB User-Agent
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);

class CVDBUserAgentMonitor
{
public:
    static void Initialize();
    
    enum EErrorFlags {
        fNoErrors                = 0,
        fErrorParse              = 1<<0,
        fErrorHTTPLine           = 1<<1,
        fErrorParseHTTPLine      = fErrorParse | fErrorHTTPLine,
        fErrorUserAgentLine      = 1<<2,
        fErrorParseUserAgentLine = fErrorParse | fErrorUserAgentLine,
        fErrorUnknownFile        = 1<<3,
        fErrorWrongValues        = 1<<4,
        fErrorNoHTTPLine         = 1<<5,
    };
    typedef int TErrorFlags;
    static int GetErrorFlags();
    
    typedef vector<string> TErrorMessages;
    static TErrorMessages GetErrorMessages();

    static void ReportErrors();
    static void ResetErrors();
    
    struct SUserAgentValues {
        string client_ip;
        string session_id;
        string page_hit_id;

        static SUserAgentValues Any()
            {
                return { "*", "*", "*" };
            }

        bool operator==(const SUserAgentValues& values) const;
        bool operator!=(const SUserAgentValues& values) const
            {
                return !(*this == values);
            }
    };
    static void SetExpectedUserAgentValues(const string& file_name, const SUserAgentValues& values);
    static const SUserAgentValues* GetExpectedUserAgentValues(const string& file_name);

    static CVDBUserAgentMonitor& GetInstance();
    void Append(const char* buffer, size_t size);
    
protected:
    void x_CheckLine();
    static void x_AddError(const string& message, TErrorFlags flags);

private:
    string m_Line; // current line
    string m_FileName; // file name in http request
};
std::ostream& operator<<(std::ostream& out, const CVDBUserAgentMonitor::SUserAgentValues& values);

END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif //SRA__VDB_USER_AGENT__HPP
