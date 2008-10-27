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
 * Author:  Pavel Ivanov
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
    case eNoCommand:     return "eNoCommand";
    case eWrongCommand:  return "eWrongCommand";
    case eBadToken:      return "eBadToken";
    case eArgumentsLack: return "eArgumentsLack";
    case eWrongMap:      return "eWrongMap";
    default:             return CException::GetErrCodeString();
    }
}


CNetServProtoParserBase::CNetServProtoParserBase
(
    const char* const*      cmd_name,
    const SNSProtoArgument* cmd_args,
    size_t                  cmddef_size
)
{
    SetCommandMap(cmd_name, cmd_args, cmddef_size);
}

void
CNetServProtoParserBase::SetCommandMap(const char* const*      cmd_name,
                                       const SNSProtoArgument* cmd_args,
                                       size_t                  cmddef_size)
{
    m_CmdMapName = cmd_name;
    m_CmdMapArgs = cmd_args;
    m_CmdDefSize = cmddef_size;
}

template <class T>
inline T*
CNetServProtoParserBase::x_GetNextInCmdMap(T* ptr)
{
    return (T*)((Int1*)ptr + m_CmdDefSize);
}

ENSProtoTokenType
CNetServProtoParserBase::x_GetToken(const char** str,
                                    const char** tok,
                                    size_t*      size)
{
    ENSProtoTokenType ttype = eNSTT_None;
    const char* s = *str;

    if (!*s) {
        return ttype;
    }

    const char* tok_start = s;
    if (*s == '"') {
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

    for (; *s; ++s) {
        if (ttype == eNSTT_Str) {
            if (*s == '"') {
                break;
            }
            else if (*s == '\\'  &&  *(s + 1)) {
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
    else if (ttype == eNSTT_Str  &&  *s != '"') {
        NCBI_THROW(CNSProtoParserException, eBadToken,
                   "String is not ended correctly: \""
                   + string(tok_start, s - tok_start));
    }
    else if (ttype == eNSTT_Int  &&  s > *str  &&  *(s - 1) == '-') {
        ttype = eNSTT_Id;
    }

    *tok = tok_start;
    *size = s - tok_start;

    if (ttype == eNSTT_Id) {
        if (*size > 4  &&  tok_start[0] == 'I'  &&  tok_start[1] == 'C'
            &&  tok_start[2] == '('  &&  *(s - 1) == ')')
        {
            ttype = eNSTT_ICPrefix;
        }
        else if (CNetCacheKey::IsValidKey(string(tok_start, *size))) {
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
        while (*s  &&  isspace(*s)) {
            ++s;
        }
    }
    *str = s;

    return ttype;
}

void
CNetServProtoParserBase::ParseCommand(CTempString          command,
                                      const void**         match_cmd,
                                      map<string, string>* params)
{
    const char* s = command.data();
    const char* token;
    const char* cache_name = NULL;
    size_t tsize, cache_name_sz = 0;
    ENSProtoTokenType ttype;

    ttype = x_GetToken(&s, &token, &tsize);
    if (ttype == eNSTT_ICPrefix) {
        cache_name    = token + 3;
        cache_name_sz = tsize - 4;
        ttype = x_GetToken(&s, &token, &tsize);
    }
    if (ttype != eNSTT_Id) {
        NCBI_THROW(CNSProtoParserException, eNoCommand,
                   "Command absent in request: '" + string(command) + "'");
    }

    const char* const* cmd_name = m_CmdMapName;
    const SNSProtoArgument* argsDescr = m_CmdMapArgs;
    while (*cmd_name) {
        if (strlen(*cmd_name) == tsize
            &&  strncmp(token, *cmd_name, tsize) == 0
            &&  (cache_name  &&  argsDescr->flags & fNSPA_ICPrefix
                 || !cache_name  &&  !(argsDescr->flags & fNSPA_ICPrefix)))
        {
            break;
        }
        cmd_name  = x_GetNextInCmdMap(cmd_name );
        argsDescr = x_GetNextInCmdMap(argsDescr);
    }
    if (!*cmd_name) {
        NCBI_THROW(CNSProtoParserException, eWrongCommand,
                   "Unknown command in request: '"
                   + string(token, tsize) + "'");
    }
    *match_cmd = cmd_name;

    if (cache_name) {
        _ASSERT(argsDescr->flags & fNSPA_ICPrefix);

        (*params)[argsDescr->key] = string(cache_name, cache_name_sz);
        ++argsDescr;
    }

    ParseArguments(s, argsDescr, params);
}

inline void s_SafeSetDefault(map<string, string>*    params,
                             const SNSProtoArgument* arg_descr)
{
    string& str = (*params)[arg_descr->key];
    const char* from = arg_descr->dflt;
    if (from) {
        str = from;
    }
    else {
        str.clear();
    }
}

void
CNetServProtoParserBase::ParseArguments(CTempString             str,
                                        const SNSProtoArgument* arg_descr,
                                        map<string, string>*    params)
{
    const char* s = str.data();
    const char* key;
    const char* val;
    size_t key_size, val_size;
    ENSProtoTokenType val_type = eNSTT_None; // if arglist is empty, it should be successful

    while (arg_descr->flags != eNSPA_None) // extra arguments are just ignored
    {
        val_type = x_GetToken(&s, &key, &key_size);
        if (val_type == eNSTT_Key) {
            val_type = x_GetToken(&s, &val, &val_size);
            if (val_type == eNSTT_Key) {
                NCBI_THROW(CNSProtoParserException, eBadToken,
                           "Second equal sign met in the parameter val: '"
                           + string(val, val_size + 1) + "'");
            }
        }
        else {
            val      = key;
            val_size = key_size;
            key      = NULL;
            key_size = 0;
        }
        if (val_type == eNSTT_None  &&  key == NULL) {
            break;
        }

        // token processing here
        bool matched = false;
        while (arg_descr->flags != eNSPA_None) {
            if (arg_descr->flags & fNSPA_Match) {
                matched = key != NULL
                          &&  strlen(arg_descr->key) == val_size
                          &&  strncmp(arg_descr->key, val, val_size) == 0;
            }
            else {
                matched = x_ArgumentMatch(key, key_size, val_type, arg_descr);
            }
            if (matched) {
                break;
            }

            // Can skip optional arguments
            if ((arg_descr->flags & fNSPA_Optional) == 0) {
                break;
            }
            do {
                if (arg_descr->dflt) {
                    s_SafeSetDefault(params, arg_descr);
                }
                ++arg_descr;
            }
            while (arg_descr->flags != eNSPA_None
                   &&  arg_descr[-1].flags & fNSPA_Chain);
        }

        if (arg_descr->flags == eNSPA_None  ||  !matched) {
            break;
        }

        // accept argument
        if ((arg_descr->flags & fNSPA_Match) == 0)
            (*params)[arg_descr->key] = string(val, val_size);
        // Process OR by skipping argument descriptions until OR flags is set
        while (arg_descr->flags != eNSPA_None
               &&  arg_descr->flags & fNSPA_Or)
        {
            ++arg_descr;
            if (arg_descr->dflt) {
                s_SafeSetDefault(params, arg_descr);
            }
        }
        ++arg_descr;
    }
    // Check that remaining arg descriptors are optional
    while (arg_descr->flags != eNSPA_None
           &&  arg_descr->flags & fNSPA_Optional)
    {
        s_SafeSetDefault(params, arg_descr);
        ++arg_descr;
    }
    
    if (arg_descr->flags != eNSPA_None) {
        NCBI_THROW(CNSProtoParserException, eArgumentsLack,
                   "Not all required parameters given. "
                   "Next parameter needed - '"
                   + string(arg_descr->key) + "'");
    }
}

// Compare actual token type with argument type.
// Int type IS-A Id type.
bool
CNetServProtoParserBase::x_ArgumentMatch(const char*             key,
                                         size_t                  key_size,
                                         ENSProtoTokenType       val_type,
                                         const SNSProtoArgument* arg_descr)
{
    if (arg_descr->flags & fNSPA_Clear) {
        return false;
    }
    if (arg_descr->flags & fNSPA_ICPrefix) {
        NCBI_THROW(CNSProtoParserException, eWrongMap,
                   "IC Prefix can be only first in arguments list "
                   "in command map.");
    }
    if (key != NULL  &&  !(strlen(arg_descr->key) == key_size
                           &&  strncmp(arg_descr->key, key, key_size) == 0))
    {
        return false;
    }
    if (val_type == eNSTT_ICPrefix  ||  val_type == eNSTT_None) {
        val_type = eNSTT_Id;
    }
    if (arg_descr->atype == eNSPT_Str
        ||  arg_descr->atype == eNSPT_Id  &&  (val_type == eNSTT_Int
                                               ||  val_type == eNSTT_NCID)
        ||  static_cast<int>(arg_descr->atype) == static_cast<int>(val_type))
    {
        return true;
    }

    return false;
}

END_NCBI_SCOPE
