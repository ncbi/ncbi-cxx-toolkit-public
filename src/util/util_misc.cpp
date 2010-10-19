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
 * Author:  Sergey Satskiy,
 *          Anton Lavrentiev (providing line by line advices of how it must be
 *          implemented)
 *
 */

#include <ncbi_pch.hpp>
#include <util/util_misc.hpp>
#include <corelib/ncbi_param.hpp>
#include <corelib/ncbifile.hpp>

#if defined(NCBI_OS_UNIX)
#  include <unistd.h>
#if defined(HAVE_READPASSPHRASE)
#  include <readpassphrase.h>
#endif
#elif defined(NCBI_OS_MSWIN)
#  include <conio.h>
#else
#  error  "Unsuported platform"
#endif


BEGIN_NCBI_SCOPE


const char* CGetPasswordFromConsoleException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eGetPassError:         return "eGetPassError";
    case eKeyboardInterrupt:    return "eKeyboardInterrupt";
    default:                    return CException::GetErrCodeString();
    }
}


string g_GetPasswordFromConsole(const string& prompt)
{
    string      password;
    CMutex      lock;
    CMutexGuard guard(lock);

#if defined(NCBI_OS_UNIX)
    // UNIX implementation

#if defined(HAVE_READPASSPHRASE)

    char password_buffer[1024];
    char* raw_password = readpassphrase(prompt.c_str(), password_buffer,
                                        sizeof(password_buffer),
                                        RPP_ECHO_OFF | RPP_REQUIRE_TTY);

#elif defined(HAVE_GETPASSPHRASE)

    char* raw_password = getpassphrase(prompt.c_str());

#elif defined(HAVE_GETPASS)

    char* raw_password = getpass(prompt.c_str());

#else
#  error "Unsupported Unix platform; the getpass, getpassphrase, and readpassphrase functions are all absent"
#endif

    if (!raw_password)
        NCBI_THROW
            (CGetPasswordFromConsoleException, eGetPassError,
             "g_GetPasswordFromConsole(): error getting password");
    password = string(raw_password);

#elif defined(NCBI_OS_MSWIN)
    // Windows implementation

    for (size_t index = 0;  index < prompt.size();  ++index) {
        _putch(prompt[index]);
    }

    for (;;) {
        char ch;
        ch = _getch();
        if (ch == '\r'  ||  ch == '\n')
            break;
        if (ch == '\003')
            NCBI_THROW(CGetPasswordFromConsoleException, eKeyboardInterrupt,
                       "g_GetPasswordFromConsole(): keyboard interrupt");
        if (ch == '\b') {
            if ( !password.empty() ) {
                password.resize(password.size() - 1);
            }
        }
        else
            password.append(1, ch);
    }

    _putch('\r');
    _putch('\n');
#endif

    return password;
}


NCBI_PARAM_DECL  (string, NCBI, DataPath);
NCBI_PARAM_DEF_EX(string, NCBI, DataPath, kEmptyStr, 0, NCBI_DATA_PATH);
typedef   NCBI_PARAM_TYPE(NCBI, DataPath) TNCBIDataPath;

NCBI_PARAM_DECL(string, NCBI, Data);
NCBI_PARAM_DEF (string, NCBI, Data, kEmptyStr);
typedef NCBI_PARAM_TYPE(NCBI, Data) TNCBIDataDir;

string g_FindDataFile(const CTempString& basename)
{
#ifdef NCBI_OS_MSWIN
    static const string kDelim = ";";
#else
    static const string kDelim = ":";
#endif

    TNCBIDataPath path;
    list<string> dirs;

    if ( !path.Get().empty() ) {
        NStr::Split(path.Get(), kDelim, dirs);
    } else {
        TNCBIDataDir dir;
        if ( !dir.Get().empty() ) {
            dirs.push_back(dir.Get());
        }
    }

    CFile file;
    ITERATE (list<string>, dir, dirs) {
        file.Reset(CDirEntry::MakePath(*dir, basename));
        if (file.Exists()) {
            return file.GetPath();
        }
    }

    return kEmptyStr; // not found
}


END_NCBI_SCOPE

