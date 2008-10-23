#ifndef CONNECT_SERVICES___NETSERVICE_PROTOCOL_PARSER__HPP
#define CONNECT_SERVICES___NETSERVICE_PROTOCOL_PARSER__HPP

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
 * Authors:  Pavel Ivanov
 *
 * File Description:
 *   Protocol parser used for NetCache and NetSchedule.
 *
 */

#include <connect/connect_export.h>


BEGIN_NCBI_SCOPE


enum ENSProtoArgFlags {
    fNSPA_Required  = 1 << 0, // argument must be present
    fNSPA_Optional  = 1 << 1, // the argument may be omited
    fNSPA_Chain     = 1 << 2, // if the argument is absent, whole
                              // chain is ignored
    fNSPA_Or        = 1 << 3, // This argument is ORed to next a|b
                              // as opposed to default sequence a b
    fNSPA_Match     = 1 << 4, // This argument is exact match with "key" field
                              // and uses "dflt" field as value
                              // as opposed to default using the arg as value
    fNSPA_Clear     = 1 << 5, // do not match this arg, just set to default
    fNSPA_ICPrefix  = 1 << 6, // special value for IC prefix that precedes
                              // command itself
    // Typical values
    eNSPA_None      = 0,      // end of arg list, intentionally set to 0
    eNSPA_Required  = fNSPA_Required,
    eNSPA_Optional  = fNSPA_Optional,
    eNSPA_ICPrefix  = fNSPA_Required | fNSPA_ICPrefix,
    eNSPA_Optchain  = fNSPA_Optional | fNSPA_Chain,
    eNSPA_ClearOnly = fNSPA_Optional | fNSPA_Clear
};
typedef unsigned int TNSProtoArgFlags;

enum ENSProtoArgType {
    eNSPT_Int = 1,    // Avoid 0 as meaningful value
    eNSPT_Str,
    eNSPT_Id,
    eNSPT_NCID
};

struct SNSProtoArgument {
    const char*      key;
    ENSProtoArgType  atype;
    TNSProtoArgFlags flags;
    const char*      dflt;
};

enum {
    kNSProtoMaxArgs = 9
};

template <class Extra>
struct SNSProtoCmdDef {
    const char*      cmd;
    Extra            extra;
    SNSProtoArgument args[kNSProtoMaxArgs + 1]; // + end of record
};

template <class Extra>
struct SNSProtoParsedCmd {
    const SNSProtoCmdDef<Extra>* command;
    map<string, string>          params;
};


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


class NCBI_XCONNECT_EXPORT CNSProtoParserException : public CException
{
public:
    enum EErrCode {
        /// Empty line in protocol or invalid command
        eNoCommand,
        eWrongCommand,
        eBadToken,
        eArgumentsLack,
        eWrongMap
    };

    virtual const char* GetErrCodeString(void) const;

    NCBI_EXCEPTION_DEFAULT(CNSProtoParserException, CException);
};


class NCBI_XCONNECT_EXPORT CNetServProtoParserBase {
public:
    void ParseArguments(CTempString             str,
                        const SNSProtoArgument* arg_descr,
                        map<string, string>*    params);

protected:
    CNetServProtoParserBase(const char* const*      cmd_name,
                            const SNSProtoArgument* cmd_args,
                            size_t                  cmddef_size);

    void SetCommandMap(const char* const*      cmd_name,
                       const SNSProtoArgument* cmd_args,
                       size_t                  cmddef_size);

    void ParseCommand(CTempString          command,
                      const void**         match_cmd,
                      map<string, string>* params);

private:
    template <class T> T* x_GetNextInCmdMap(T* ptr);

    ENSProtoTokenType x_GetToken(const char** str,
                                 const char** tok,
                                 size_t*      size);
    bool x_ArgumentMatch(const char*             key,
                         size_t                  key_size,
                         ENSProtoTokenType       val_type,
                         const SNSProtoArgument* arg_descr);


    const char* const*      m_CmdMapName;
    const SNSProtoArgument* m_CmdMapArgs;
    size_t                  m_CmdDefSize;
};

template <class Extra>
class CNetServProtoParser : public CNetServProtoParserBase {
public:
    typedef SNSProtoCmdDef<Extra>    TCmdDef;
    typedef SNSProtoParsedCmd<Extra> TParsedCmd;

    CNetServProtoParser(const TCmdDef* cmd_map);

    void SetCommandMap(const TCmdDef* cmd_map);

    TParsedCmd ParseCommand(CTempString command);
};


//////////////////////////////////////////////////////////////////////////
// Inline functions
//////////////////////////////////////////////////////////////////////////

template <class Extra>
inline
CNetServProtoParser<Extra>::CNetServProtoParser(const TCmdDef* cmd_map)
    : CNetServProtoParserBase(&cmd_map->cmd, cmd_map->args, sizeof(TCmdDef))
{}

template <class Extra>
inline void
CNetServProtoParser<Extra>::SetCommandMap(const TCmdDef* cmd_map)
{
    CNetServProtoParserBase::SetCommandMap(
                              &cmd_map->cmd, cmd_map->args, sizeof(TCmdDef));
}

template <class Extra>
inline typename CNetServProtoParser<Extra>::TParsedCmd
CNetServProtoParser<Extra>::ParseCommand(CTempString command)
{
    TParsedCmd result;
    CNetServProtoParserBase::ParseCommand(command,
                                          (const void**)&result.command,
                                          &result.params);
    return result;
}


END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES___NETSERVICE_PROTOCOL_PARSER__HPP */
