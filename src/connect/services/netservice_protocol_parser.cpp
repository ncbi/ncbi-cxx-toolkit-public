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
 * Author:  Pavel Ivanov, Victor Joukov
 *
 * File Description:
 *   Implementation of protocol parser used for NetCache and NetSchedule.
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/netservice_protocol_parser.hpp>
#include <connect/services/netcache_key.hpp>


BEGIN_NCBI_SCOPE


const char*
CNSProtoParserException::GetErrCodeString(void) const
{
    switch (GetErrCode())
    {
    case eNoCommand:        return "eNoCommand";
    case eWrongCommand:     return "eWrongCommand";
    case eBadToken:         return "eBadToken";
    case eArgumentsMissing: return "eArgumentsMissing";
    case eWrongMap:         return "eWrongMap";
    default:                return CException::GetErrCodeString();
    }
}


template <class T>
inline T*
CNetServProtoParserBase::x_GetNextInCmdMap(T* ptr)
{
    return reinterpret_cast<T*>(reinterpret_cast<const char*>(ptr) + m_CmdDefSize);
}


// For internal use only
enum ENSProtoTokenType {
    eNSTT_None  = -1,    // No more tokens
    eNSTT_Int   =  1,    // Avoid 0 as meaningful value
    eNSTT_Str,
    eNSTT_Id,
    eNSTT_NCID,
    eNSTT_Key,
    eNSTT_ICPrefix
};

ENSProtoTokenType
CNetServProtoParserBase::x_GetToken(const char** str,
                                    const char*  str_end,
                                    CTempString* token)
{
    ENSProtoTokenType ttype = eNSTT_None;
    const char* s = *str;

    if (s >= str_end) {
        return ttype;
    }

    const char* tok_start = s;
    char quote_char;
    if (*s == '"' || *s == '\'') {
        quote_char = *s;
        ++s;
        tok_start = s;
        ttype = eNSTT_Str;
    }
    else {
        ttype = eNSTT_Int;
        if (*s == '-') {
            ++s;
        }
    }

    for (; s < str_end; ++s) {
        if (ttype == eNSTT_Str) {
            if (*s == quote_char) {
                break;
            }
            else if (*s == '\\'  &&  (s + 1 < str_end)) {
                ++s;
            }
        }
        else if (*s == '='  ||  isspace(*s))
            break;
        else if (ttype == eNSTT_Int  &&  !isdigit(*s)) {
            ttype = eNSTT_Id;
        }
    }

    if (*s == '=') {
        ttype = eNSTT_Key;
    }
    else if (ttype == eNSTT_Str  &&  *s != quote_char) {
        NCBI_THROW_FMT(CNSProtoParserException, eBadToken,
                       "String is not ended correctly: \""
                       << string(tok_start, s - tok_start));
    }
    else if (ttype == eNSTT_Int  &&  s > *str  &&  *(s - 1) == '-') {
        ttype = eNSTT_Id;
    }

    token->assign(tok_start, s - tok_start);

    if (ttype == eNSTT_Id) {
        if (token->size() > 4  &&  tok_start[0] == 'I'  &&  tok_start[1] == 'C'
            &&  tok_start[2] == '('  &&  *(s - 1) == ')')
        {
            ttype = eNSTT_ICPrefix;
        }
        else if (CNetCacheKey::IsValidKey(tok_start, token->size())) {
            ttype = eNSTT_NCID;
        }
    }
    if (ttype == eNSTT_Key) {
        ++s;
    }
    else {
        if (ttype == eNSTT_Str) {
            ++s;
        }
        while ((s < str_end)  &&  isspace(*s)) {
            ++s;
        }
    }
    *str = s;

    return ttype;
}

void
CNetServProtoParserBase::ParseCommand(CTempString     command,
                                      const void**    match_cmd,
                                      TNSProtoParams* params)
{
    const char* str     = command.data();
    const char* str_end = str + command.size();
    CTempString token, cache_name;
    ENSProtoTokenType ttype;

    ttype = x_GetToken(&str, str_end, &token);
    if (ttype == eNSTT_ICPrefix) {
        cache_name.assign(token.data() + 3, token.size() - 4);
        ttype = x_GetToken(&str, str_end, &token);
    }
    if (ttype != eNSTT_Id) {
        NCBI_THROW_FMT(CNSProtoParserException, eNoCommand,
                       "Command name is absent: '" << command << "'");
    }

    const char* const* cmd_name = m_CmdMapName;
    const SNSProtoArgument* argsDescr = m_CmdMapArgs;
    while (*cmd_name) {
        if (strlen(*cmd_name) == token.size()
            &&  strncmp(*cmd_name, token.data(), token.size()) == 0
            &&  ((!cache_name.empty()  &&  (argsDescr->flags & fNSPA_ICPrefix))
                 ||  (cache_name.empty()  &&  !(argsDescr->flags & fNSPA_ICPrefix))))
        {
            break;
        }
        cmd_name  = x_GetNextInCmdMap(cmd_name );
        argsDescr = x_GetNextInCmdMap(argsDescr);
    }
    if (!*cmd_name) {
        NCBI_THROW_FMT(CNSProtoParserException, eWrongCommand,
                       "Unknown command name '" << token << "' in command '"
                       << command << "'");
    }
    *match_cmd = cmd_name;

    if (!cache_name.empty()) {
        _ASSERT(argsDescr->flags & fNSPA_ICPrefix);

        (*params)[argsDescr->key] = cache_name;
        ++argsDescr;
    }

    try {
        ParseArguments(CTempString(str, str_end - str), argsDescr, params);
    }
    catch (CNSProtoParserException& ex) {
        ex.AddBacklog(DIAG_COMPILE_INFO,
                      FORMAT("Command being parsed is '" << command << "'"));
        throw;
    }
}

static inline void
s_SetDefaultValue(TNSProtoParams*         params,
                  const SNSProtoArgument* arg_descr)
{
    const char* from = arg_descr->dflt;
    if (from) {
        (*params)[arg_descr->key] = from;
    }
}

void
CNetServProtoParserBase::ParseArguments(CTempString             args,
                                        const SNSProtoArgument* arg_descr,
                                        TNSProtoParams*         params)
{
    const char* str     = args.data();
    const char* str_end = str + args.size();
    CTempString key, val;
    ENSProtoTokenType val_type = eNSTT_None; // if arglist is empty, it should be successful

    while (arg_descr->flags != eNSPA_None) // extra arguments are just ignored
    {
        val_type = x_GetToken(&str, str_end, &key);
        if (val_type == eNSTT_Key) {
            val_type = x_GetToken(&str, str_end, &val);
            if (val_type == eNSTT_Key) {
                NCBI_THROW_FMT(CNSProtoParserException, eBadToken,
                               "Second equal sign met in the parameter value: '"
                               << string(val.data(), val.size() + 1) << "'");
            }
        }
        else {
            val = key;
            key.clear();
        }
        if (val_type == eNSTT_None  &&  key.empty()) {
            break;
        }

        // token processing here
        bool matched = false;
        while (arg_descr->flags != eNSPA_None) {
            if (arg_descr->flags & fNSPA_Ellipsis) {
                if (key.empty()) {
                    NCBI_THROW(CNSProtoParserException, eBadToken,
                               "Only key=value pairs allowed at the end of command");
                }
                matched = true;
            }
            else if (arg_descr->flags & fNSPA_Match) {
                matched = strlen(arg_descr->key) == val.size()
                          && strncmp(arg_descr->key, val.data(), val.size()) == 0;
            }
            else {
                matched = x_IsArgumentMatch(key, val_type, arg_descr);
            }
            if (matched) {
                break;
            }

            // Can skip optional arguments
            if (!(arg_descr->flags & fNSPA_Optional)) {
                break;
            }
            do {
                s_SetDefaultValue(params, arg_descr);
                ++arg_descr;
            }
            while (arg_descr->flags != eNSPA_None
                   &&  arg_descr[-1].flags & fNSPA_Chain);
        }

        if (arg_descr->flags & fNSPA_Obsolete) {
            NCBI_THROW_FMT(CNSProtoParserException, eWrongArgument,
                           "Argument " << arg_descr->key << " is obsolete."
                           " It cannot be used in command now.");
        }
        else if (arg_descr->flags == eNSPA_None  ||  !matched) {
            break;
        }

        // accept argument
        if (arg_descr->flags & fNSPA_Ellipsis) {
            (*params)[key] = val;
        } else {
            (*params)[arg_descr->key]
                              = (arg_descr->flags & fNSPA_Match)? "match" : val;
        }
        // Process OR by skipping argument descriptions until OR flags is set
        while (arg_descr->flags != eNSPA_None
               &&  arg_descr->flags & fNSPA_Or)
        {
            ++arg_descr;
            s_SetDefaultValue(params, arg_descr);
        }
        if (!(arg_descr->flags & fNSPA_Ellipsis)) {
            // Ellipsis consumes all the rest, so we increment this only
            // for regular, non-ellipsis arguments.
            ++arg_descr;
        }
    }
    // Check that remaining arg descriptors are optional
    while (arg_descr->flags != eNSPA_None
           &&  arg_descr->flags & fNSPA_Optional)
    {
        s_SetDefaultValue(params, arg_descr);
        ++arg_descr;
    }

    if (arg_descr->flags != eNSPA_None) {
        NCBI_THROW_FMT(CNSProtoParserException, eArgumentsMissing,
                       "Not all required parameters given. "
                       "Next parameter needed is '" << arg_descr->key << "'");
    }
}

// Compare actual token type with argument type.
// Int type IS-A Id type.
bool
CNetServProtoParserBase::x_IsArgumentMatch(const CTempString       key,
                                           ENSProtoTokenType       val_type,
                                           const SNSProtoArgument* arg_descr)
{
    if (arg_descr->flags & fNSPA_Clear) {
        return false;
    }
    if (arg_descr->flags & fNSPA_ICPrefix) {
        NCBI_THROW(CNSProtoParserException, eWrongMap,
                   "IC Prefix can be only the first in the arguments list "
                   "of a command map.");
    }
    if (!key.empty()  &&  !(strlen(arg_descr->key) == key.size()
                           &&  strncmp(arg_descr->key, key.data(), key.size()) == 0))
    {
        return false;
    }
    if (val_type == eNSTT_ICPrefix  ||  val_type == eNSTT_None) {
        val_type = eNSTT_Id;
    }
    if (arg_descr->atype == eNSPT_Str
        ||  (arg_descr->atype == eNSPT_Id  &&  (val_type == eNSTT_Int
                                                ||  val_type == eNSTT_NCID))
        ||  static_cast<int>(arg_descr->atype) == static_cast<int>(val_type))
    {
        return true;
    }

    return false;
}

END_NCBI_SCOPE
