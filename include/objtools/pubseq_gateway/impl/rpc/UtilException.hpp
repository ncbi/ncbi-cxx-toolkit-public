#ifndef UTILEXCEPTION__HPP
#define UTILEXCEPTION__HPP

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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 */

#include <stdexcept>
#include <sstream>

#include "uv.h"

class EException: public std::runtime_error {
protected:
    const int m_Code;
public:
    EException(const char * const msg, const int code) noexcept :
        runtime_error{msg},
        m_Code{code}
    {}
    int code() const noexcept {
        return m_Code;
    }
    [[noreturn]] static inline void raise(const char* msg, int code = 0) {
        throw EException{msg, code};
    }
    [[noreturn]] static inline void raise(const std::string& msg, int code = 0) {
        raise(msg.c_str(), code);
    }
};

class EUvException: public EException {
public:
    EUvException(const char * const msg, const int code) noexcept : EException(msg, code) {}
    [[noreturn]] static inline void raise(const char* msg, int code = 0) {
        if (code) {
            std::stringstream ss;
            ss << msg << ": " << uv_strerror(code);
            throw EUvException(ss.str().c_str(), code);
        }
        else
            throw EUvException{msg, code};
    }
    [[noreturn]] static inline void raise(const std::string& msg, int code = 0) {
        raise(msg.c_str(), code);
    }
};


#endif
