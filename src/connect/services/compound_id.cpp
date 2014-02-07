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
 * Authors:  Dmitry Kazimirov
 *
 * File Description:
 *   Compound IDs are Base64-encoded string that contain application-specific
 *   information to identify and/or locate objects.
 *
 */

/// @file compound_id.cpp
/// Implementations of CCompoundIDPool, CCompoundID, and CCompoundIDField.

#include <ncbi_pch.hpp>

#include "compound_id_impl.hpp"

#include <connect/ncbi_socket.hpp>

#include <iomanip>

#define CIC_GENERICID_CLASS_NAME "CompoundID"
#define CIC_NETCACHEKEY_CLASS_NAME "NetCacheKey"
#define CIC_NETSCHEDULEKEY_CLASS_NAME "NetScheduleKey"
#define CIC_NETSTORAGEOBJECTID_CLASS_NAME "NetStorageObjectID"

#define CIT_ID_TYPE_NAME "id"
#define CIT_INTEGER_TYPE_NAME "int"
#define CIT_SERVICE_NAME_TYPE_NAME "service"
#define CIT_DATABASE_NAME_TYPE_NAME "database"
#define CIT_TIMESTAMP_TYPE_NAME "time"
#define CIT_RANDOM_TYPE_NAME "rand"
#define CIT_IPV4_ADDRESS_TYPE_NAME "ipv4_addr"
#define CIT_HOST_TYPE_NAME "host"
#define CIT_PORT_TYPE_NAME "port"
#define CIT_IPV4_SOCK_ADDR_TYPE_NAME "ipv4_sock_addr"
#define CIT_PATH_TYPE_NAME "path"
#define CIT_STRING_TYPE_NAME "str"
#define CIT_BOOLEAN_TYPE_NAME "bool"
#define CIT_FLAGS_TYPE_NAME "flags"
#define CIT_LABEL_TYPE_NAME "label"
#define CIT_CUE_TYPE_NAME "cue"
#define CIT_SEQ_ID_TYPE_NAME "seq_id"
#define CIT_TAX_ID_TYPE_NAME "tax_id"
#define CIT_NESTED_CID_TYPE_NAME "nested"

BEGIN_NCBI_SCOPE

static const char* s_ClassNames[eCIC_NumberOfClasses] = {
    /* eCIC_GenericID           */  CIC_GENERICID_CLASS_NAME,
    /* eCIC_NetCacheKey         */  CIC_NETCACHEKEY_CLASS_NAME,
    /* eCIC_NetScheduleKey      */  CIC_NETSCHEDULEKEY_CLASS_NAME,
    /* eCIC_NetStorageObjectID  */  CIC_NETSTORAGEOBJECTID_CLASS_NAME
};

static const char* s_TypeNames[eCIT_NumberOfTypes] = {
    /* eCIT_ID                  */  CIT_ID_TYPE_NAME,
    /* eCIT_Integer             */  CIT_INTEGER_TYPE_NAME,
    /* eCIT_ServiceName         */  CIT_SERVICE_NAME_TYPE_NAME,
    /* eCIT_DatabaseName        */  CIT_DATABASE_NAME_TYPE_NAME,
    /* eCIT_Timestamp           */  CIT_TIMESTAMP_TYPE_NAME,
    /* eCIT_Random              */  CIT_RANDOM_TYPE_NAME,
    /* eCIT_IPv4Address         */  CIT_IPV4_ADDRESS_TYPE_NAME,
    /* eCIT_Host                */  CIT_HOST_TYPE_NAME,
    /* eCIT_Port                */  CIT_PORT_TYPE_NAME,
    /* eCIT_IPv4SockAddr        */  CIT_IPV4_SOCK_ADDR_TYPE_NAME,
    /* eCIT_Path                */  CIT_PATH_TYPE_NAME,
    /* eCIT_String              */  CIT_STRING_TYPE_NAME,
    /* eCIT_Boolean             */  CIT_BOOLEAN_TYPE_NAME,
    /* eCIT_Flags               */  CIT_FLAGS_TYPE_NAME,
    /* eCIT_Label               */  CIT_LABEL_TYPE_NAME,
    /* eCIT_Cue                 */  CIT_CUE_TYPE_NAME,
    /* eCIT_SeqID               */  CIT_SEQ_ID_TYPE_NAME,
    /* eCIT_TaxID               */  CIT_TAX_ID_TYPE_NAME,
    /* eCIT_NestedCID           */  CIT_NESTED_CID_TYPE_NAME
};

const char* CCompoundIDException::GetErrCodeString() const
{
    switch (GetErrCode()) {
    case eInvalidType:
        return "eInvalidType";
    default:
        return CException::GetErrCodeString();
    }
}

void SCompoundIDFieldImpl::DeleteThis()
{
    m_CID = NULL;
}

ECompoundIDFieldType CCompoundIDField::GetType()
{
    return m_Impl->m_Type;
}

CCompoundIDField CCompoundIDField::GetNextNeighbor()
{
    SCompoundIDFieldImpl* next = TFieldList::GetNext(m_Impl);
    if (next != NULL)
        next->m_CID = m_Impl->m_CID;
    return next;
}

CCompoundIDField CCompoundIDField::GetNextHomogeneous()
{
    SCompoundIDFieldImpl* next = THomogeneousFieldList::GetNext(m_Impl);
    if (next != NULL)
        next->m_CID = m_Impl->m_CID;
    return next;
}

#define CIF_GET_IMPL(ret_type, method, type, member) \
    ret_type CCompoundIDField::method() const \
    { \
        if (m_Impl->m_Type != type) { \
            NCBI_THROW_FMT(CCompoundIDException, eInvalidType, \
                    "Compound ID field type mismatch (requested: " << \
                    s_TypeNames[type] << "; actual: " << \
                    s_TypeNames[m_Impl->m_Type] << ')'); \
        } \
        return (ret_type) m_Impl->member; \
    }

CIF_GET_IMPL(Uint8, GetID, eCIT_ID, m_Uint8Value);
CIF_GET_IMPL(Int8, GetInteger, eCIT_Integer, m_Int8Value);
CIF_GET_IMPL(string, GetServiceName, eCIT_ServiceName, m_StringValue);
CIF_GET_IMPL(string, GetDatabaseName, eCIT_DatabaseName, m_StringValue);
CIF_GET_IMPL(Int8, GetTimestamp, eCIT_Timestamp, m_Int8Value);
CIF_GET_IMPL(Uint4, GetRandom, eCIT_Random, m_Uint4Value);
CIF_GET_IMPL(Uint4, GetIPv4Address, eCIT_IPv4Address &&
        m_Impl->m_Type != eCIT_IPv4SockAddr, m_IPv4SockAddr.m_IPv4Addr);
CIF_GET_IMPL(string, GetHost, eCIT_Host, m_StringValue);
CIF_GET_IMPL(Uint2, GetPort, eCIT_Port &&
        m_Impl->m_Type != eCIT_IPv4SockAddr, m_IPv4SockAddr.m_Port);
CIF_GET_IMPL(string, GetPath, eCIT_Path, m_StringValue);
CIF_GET_IMPL(string, GetString, eCIT_String, m_StringValue);
CIF_GET_IMPL(bool, GetBoolean, eCIT_Boolean, m_BoolValue);
CIF_GET_IMPL(Uint8, GetFlags, eCIT_Flags, m_Uint8Value);
CIF_GET_IMPL(string, GetLabel, eCIT_Label, m_StringValue);
CIF_GET_IMPL(Uint8, GetCue, eCIT_Cue, m_Uint8Value);
CIF_GET_IMPL(string, GetSeqID, eCIT_SeqID, m_StringValue);
CIF_GET_IMPL(Uint8, GetTaxID, eCIT_TaxID, m_Uint8Value);
CIF_GET_IMPL(const CCompoundID&, GetNestedCID, eCIT_NestedCID, m_NestedCID);


SCompoundIDFieldImpl* SCompoundIDImpl::AppendField(
        ECompoundIDFieldType field_type)
{
    SCompoundIDFieldImpl* new_entry = m_Pool->m_FieldPool.Alloc();
    m_FieldList.Append(new_entry);
    m_HomogeneousFields[field_type].Append(new_entry);
    new_entry->m_Type = field_type;
    ++m_Length;
    return new_entry;
}

void SCompoundIDImpl::DeleteThis()
{
    CCompoundIDPool pool(m_Pool);
    m_Pool = NULL;
    SCompoundIDFieldImpl* field = m_FieldList.m_Head;
    while (field != NULL) {
        SCompoundIDFieldImpl* next_field = TFieldList::GetNext(field);
        pool->m_FieldPool.ReturnToPool(field);
        field = next_field;
    }
    pool->m_CompoundIDPool.ReturnToPool(this);
}

void CCompoundIDField::Remove()
{
    CCompoundID cid(m_Impl->m_CID);
    m_Impl->m_CID = NULL;
    cid->Remove(m_Impl);
}

ECompoundIDClass CCompoundID::GetClass() const
{
    return m_Impl->m_Class;
}

bool CCompoundID::IsEmpty() const
{
    return m_Impl->m_Length == 0;
}

unsigned CCompoundID::GetLength() const
{
    return m_Impl->m_Length;
}

CCompoundIDField CCompoundID::GetFirstField()
{
    SCompoundIDFieldImpl* first = m_Impl->m_FieldList.m_Head;
    first->m_CID = m_Impl;
    return first;
}

CCompoundIDField CCompoundID::GetFirst(ECompoundIDFieldType field_type)
{
    _ASSERT(field_type < eCIT_NumberOfTypes);

    SCompoundIDFieldImpl* first =
            m_Impl->m_HomogeneousFields[field_type].m_Head;
    if (first != NULL)
        first->m_CID = m_Impl;
    return first;
}

#define CID_APPEND_IMPL(method, field_type, val_type, member) \
    void CCompoundID::method(val_type value) \
    { \
        m_Impl->AppendField(field_type)->member = value; \
    }

CID_APPEND_IMPL(AppendID, eCIT_ID, Uint8, m_Uint8Value);
CID_APPEND_IMPL(AppendInteger, eCIT_Integer, Int8, m_Int8Value);
CID_APPEND_IMPL(AppendServiceName, eCIT_ServiceName,
        const string&, m_StringValue);
CID_APPEND_IMPL(AppendDatabaseName, eCIT_DatabaseName,
        const string&, m_StringValue);
CID_APPEND_IMPL(AppendTimestamp, eCIT_Timestamp, Int8, m_Int8Value);

void CCompoundID::AppendCurrentTime()
{
    AppendTimestamp(time(NULL));
}

CID_APPEND_IMPL(AppendRandom, eCIT_Random, Uint4, m_Uint4Value);

void CCompoundID::AppendRandom()
{
    AppendRandom(m_Impl->m_Pool->GetRand());
}

CID_APPEND_IMPL(AppendIPv4Address, eCIT_IPv4Address,
        Uint4, m_IPv4SockAddr.m_IPv4Addr);
CID_APPEND_IMPL(AppendHost, eCIT_Host, const string&, m_StringValue);
CID_APPEND_IMPL(AppendPort, eCIT_Port, Uint2, m_IPv4SockAddr.m_Port);

void CCompoundID::AppendIPv4SockAddr(Uint4 ipv4_address, Uint2 port_number)
{
    SCompoundIDFieldImpl* new_field = m_Impl->AppendField(eCIT_IPv4SockAddr);
    new_field->m_IPv4SockAddr.m_IPv4Addr = ipv4_address;
    new_field->m_IPv4SockAddr.m_Port = port_number;
}

CID_APPEND_IMPL(AppendPath, eCIT_Path, const string&, m_StringValue);
CID_APPEND_IMPL(AppendString, eCIT_String, const string&, m_StringValue);
CID_APPEND_IMPL(AppendBoolean, eCIT_Boolean, bool, m_BoolValue);
CID_APPEND_IMPL(AppendFlags, eCIT_Flags, Uint8, m_Uint8Value);
CID_APPEND_IMPL(AppendLabel, eCIT_Label, const string&, m_StringValue);
CID_APPEND_IMPL(AppendCue, eCIT_Cue, Uint8, m_Uint8Value);
CID_APPEND_IMPL(AppendSeqID, eCIT_SeqID, const string&, m_StringValue);
CID_APPEND_IMPL(AppendTaxID, eCIT_TaxID, Uint8, m_Uint8Value);
CID_APPEND_IMPL(AppendNestedCID, eCIT_NestedCID,
        const CCompoundID&, m_NestedCID);

CCompoundIDField CCompoundID::GetLastField()
{
    SCompoundIDFieldImpl* last = m_Impl->m_FieldList.m_Tail;
    last->m_CID = m_Impl;
    return last;
}

string CCompoundID::ToString()
{
    return m_Impl->PackV0();
}

static void s_Indent(CNcbiOstrstream& sstr,
        int indent_depth, const char* indent)
{
    while (--indent_depth >= 0)
        sstr << indent;
}

static void s_DumpCompoundID(CNcbiOstrstream& sstr, SCompoundIDImpl* cid_impl,
        int indent_depth, const char* indent)
{
    sstr << s_ClassNames[cid_impl->m_Class] << '\n';
    s_Indent(sstr, indent_depth, indent);
    sstr << "{\n";

    SCompoundIDFieldImpl* field = cid_impl->m_FieldList.m_Head;

    if (field != NULL)
        for (;;) {
            s_Indent(sstr, indent_depth + 1, indent);
            sstr << s_TypeNames[field->m_Type] << ' ';
            switch (field->m_Type) {
            case eCIT_ID:
            case eCIT_Cue:
            case eCIT_TaxID:
                sstr << field->m_Uint8Value;
                break;
            case eCIT_Integer:
            case eCIT_Timestamp:
                sstr << field->m_Int8Value;
                break;
            case eCIT_ServiceName:
            case eCIT_DatabaseName:
            case eCIT_Host:
            case eCIT_Path:
            case eCIT_String:
            case eCIT_Label:
            case eCIT_SeqID:
                sstr << '"' <<
                        NStr::PrintableString(field->m_StringValue) << '"';
                break;
            case eCIT_Random:
                sstr << field->m_Uint4Value;
                break;
            case eCIT_IPv4Address:
                sstr << CSocketAPI::ntoa(field->m_IPv4SockAddr.m_IPv4Addr);
                break;
            case eCIT_Port:
                sstr << field->m_IPv4SockAddr.m_Port;
                break;
            case eCIT_IPv4SockAddr:
                sstr << CSocketAPI::ntoa(field->m_IPv4SockAddr.m_IPv4Addr) <<
                        ':' << field->m_IPv4SockAddr.m_Port;
                break;
            case eCIT_Boolean:
                sstr << (field->m_BoolValue ? "true" : "false");
                break;
            case eCIT_Flags:
                if (field->m_Uint8Value <= 0xFF)
                    sstr << "0b" << setw(8) << setfill('0') <<
                            NStr::UInt8ToString(field->m_Uint8Value, 0, 2);
                else if (field->m_Uint8Value <= 0xFFFF)
                    sstr << "0b" << setw(16) << setfill('0') <<
                            NStr::UInt8ToString(field->m_Uint8Value, 0, 2);
                else
                    sstr << "0x" <<
                            NStr::UInt8ToString(field->m_Uint8Value, 0, 16);
                break;
            case eCIT_NestedCID:
                s_DumpCompoundID(sstr, field->m_NestedCID,
                        indent_depth + 1, indent);
                break;

            case eCIT_NumberOfTypes:
                _ASSERT(0);
                break;
            }
            field = cid_impl->m_FieldList.GetNext(field);
            if (field != NULL)
                sstr << ",\n";
            else {
                sstr << '\n';
                break;
            }
        }

    s_Indent(sstr, indent_depth, indent);
    sstr << '}';
}

string CCompoundID::Dump()
{
    CNcbiOstrstream sstr;
    s_DumpCompoundID(sstr, m_Impl, 0, "    ");
    sstr << '\n' << NcbiEnds;
    return sstr.str();
}

CCompoundIDPool::CCompoundIDPool() : m_Impl(new SCompoundIDPoolImpl)
{
    m_Impl->m_RandomGen.Randomize();
}

CCompoundID CCompoundIDPool::NewID(ECompoundIDClass new_id_class)
{
    CCompoundID new_cid(m_Impl->m_CompoundIDPool.Alloc());
    new_cid->Reset(m_Impl, new_id_class);
    return new_cid;
}

CCompoundID CCompoundIDPool::FromString(const string& cid)
{
    return m_Impl->UnpackV0(cid);
}

class CCompoundIDDumpParser
{
public:
    CCompoundIDDumpParser(CCompoundIDPool::TInstance cid_pool,
            const string& cid_dump) :
        m_Pool(cid_pool),
        m_Dump(cid_dump),
        m_Ch(m_Dump.c_str()),
        m_Line(1),
        m_LineBegin(m_Ch)
    {
    }

    CCompoundID ParseID();

    void SkipSpace();
    void SkipSpaceToNextToken();
    void CheckEOF();

private:
    void x_SaveErrPos() {m_ErrLine = m_Line; m_ErrPos = m_Ch;}
    bool x_EOF() const {return *m_Ch == '\0';}
    string x_ReadString();
    Uint8 x_ReadUint8();
    Uint8 x_ReadInt8();
    Uint4 x_ReadIPv4Address();
    Uint2 x_ReadPortNumber();

    CCompoundIDPool m_Pool;
    string m_Dump;
    const char* m_Ch;
    size_t m_Line;
    const char* m_LineBegin;
    size_t m_ErrLine;
    const char* m_ErrPos;
};

#define CID_PARSER_EXCEPTION(message) \
    NCBI_THROW_FMT(CCompoundIDException, eInvalidDumpSyntax, \
            "line " << m_ErrLine << ", column " << (m_ErrPos - m_LineBegin + 1) << ": " << message)

CCompoundID CCompoundIDDumpParser::ParseID()
{
    SkipSpace();
    x_SaveErrPos();

    if (x_EOF() || !isalpha(*m_Ch)) {
        CID_PARSER_EXCEPTION("missing compound ID class name");
    }

    const char* token_begin = m_Ch;

    do
        ++m_Ch;
    while (!x_EOF() && isalpha(*m_Ch));

    CTempString new_id_class_name(token_begin, m_Ch - token_begin);

    ECompoundIDClass new_id_class = eCIC_NumberOfClasses;

    switch (*token_begin) {
    case 'C':
        if (new_id_class_name == CIC_GENERICID_CLASS_NAME)
            new_id_class = eCIC_GenericID;
        break;
    case 'N':
        if (new_id_class_name == CIC_NETCACHEKEY_CLASS_NAME)
            new_id_class = eCIC_NetCacheKey;
        else if (new_id_class_name == CIC_NETSCHEDULEKEY_CLASS_NAME)
            new_id_class = eCIC_NetScheduleKey;
        else if (new_id_class_name == CIC_NETSTORAGEOBJECTID_CLASS_NAME)
            new_id_class = eCIC_NetStorageObjectID;
    }

    if (new_id_class == eCIC_NumberOfClasses) {
        CID_PARSER_EXCEPTION("unknown component ID class '" <<
                new_id_class_name << '\'');
    }

    SkipSpace();

    if (x_EOF() || *m_Ch != '{') {
        x_SaveErrPos();
        CID_PARSER_EXCEPTION("missing '{'");
    }

    ++m_Ch;

    SkipSpaceToNextToken();

    CCompoundID result(m_Pool.NewID(new_id_class));

    if (*m_Ch != '}')
        for (;;) {
            token_begin = m_Ch;
            x_SaveErrPos();

            do
                ++m_Ch;
            while (isalnum(*m_Ch) || *m_Ch == '_');

            CTempString field_type_name(token_begin, m_Ch - token_begin);

            ECompoundIDFieldType field_type = eCIT_NumberOfTypes;

            switch (*token_begin) {
            case '}':
                CID_PARSER_EXCEPTION("a field type name is required");
            case 'b':
                if (field_type_name == CIT_BOOLEAN_TYPE_NAME)
                    field_type = eCIT_Boolean;
                break;
            case 'd':
                if (field_type_name == CIT_DATABASE_NAME_TYPE_NAME)
                    field_type = eCIT_DatabaseName;
                break;
            case 'f':
                if (field_type_name == CIT_FLAGS_TYPE_NAME)
                    field_type = eCIT_Flags;
                break;
            case 'h':
                if (field_type_name == CIT_HOST_TYPE_NAME)
                    field_type = eCIT_Host;
                break;
            case 'i':
                if (field_type_name == CIT_ID_TYPE_NAME)
                    field_type = eCIT_ID;
                else if (field_type_name == CIT_INTEGER_TYPE_NAME)
                    field_type = eCIT_Integer;
                else if (field_type_name == CIT_IPV4_ADDRESS_TYPE_NAME)
                    field_type = eCIT_IPv4Address;
                else if (field_type_name == CIT_IPV4_SOCK_ADDR_TYPE_NAME)
                    field_type = eCIT_IPv4SockAddr;
                break;
            case 'n':
                if (field_type_name == CIT_NESTED_CID_TYPE_NAME)
                    field_type = eCIT_NestedCID;
                else if (field_type_name == CIT_CUE_TYPE_NAME)
                    field_type = eCIT_Cue;
                break;
            case 'p':
                if (field_type_name == CIT_PATH_TYPE_NAME)
                    field_type = eCIT_Path;
                else if (field_type_name == CIT_PORT_TYPE_NAME)
                    field_type = eCIT_Port;
                break;
            case 'r':
                if (field_type_name == CIT_RANDOM_TYPE_NAME)
                    field_type = eCIT_Random;
                break;
            case 's':
                if (field_type_name == CIT_SEQ_ID_TYPE_NAME)
                    field_type = eCIT_SeqID;
                else if (field_type_name == CIT_SERVICE_NAME_TYPE_NAME)
                    field_type = eCIT_ServiceName;
                else if (field_type_name == CIT_STRING_TYPE_NAME)
                    field_type = eCIT_String;
                break;
            case 't':
                if (field_type_name == CIT_LABEL_TYPE_NAME)
                    field_type = eCIT_Label;
                else if (field_type_name == CIT_TAX_ID_TYPE_NAME)
                    field_type = eCIT_TaxID;
                else if (field_type_name == CIT_TIMESTAMP_TYPE_NAME)
                    field_type = eCIT_Timestamp;
            }

            if (field_type == eCIT_NumberOfTypes) {
                CID_PARSER_EXCEPTION("unknown field type '" <<
                        field_type_name << '\'');
            }

            SkipSpaceToNextToken();

            switch (field_type) {
            case eCIT_ID:
                result.AppendID(x_ReadUint8());
                break;
            case eCIT_Integer:
                result.AppendInteger(x_ReadInt8());
                break;
            case eCIT_ServiceName:
                result.AppendServiceName(x_ReadString());
                break;
            case eCIT_DatabaseName:
                result.AppendDatabaseName(x_ReadString());
                break;
            case eCIT_Timestamp:
                result.AppendTimestamp(x_ReadInt8());
                break;
            case eCIT_Random:
                {
                    x_SaveErrPos();
                    Uint8 random_number = x_ReadUint8();
                    if (random_number >= ((Uint8) 1) << 8 * sizeof(Uint4)) {
                        CID_PARSER_EXCEPTION(
                                "random number exceeds maximum allowed value");
                    }
                    result.AppendRandom((Uint4) random_number);
                }
                break;
            case eCIT_IPv4Address:
                result.AppendIPv4Address(x_ReadIPv4Address());
                break;
            case eCIT_Host:
                result.AppendHost(x_ReadString());
                break;
            case eCIT_Port:
                result.AppendPort(x_ReadPortNumber());
                break;
            case eCIT_IPv4SockAddr:
                {
                    Uint4 ipv4_address = x_ReadIPv4Address();
                    if (x_EOF() || *m_Ch != ':') {
                        x_SaveErrPos();
                        CID_PARSER_EXCEPTION("missing ':'");
                    }
                    ++m_Ch;
                    result.AppendIPv4SockAddr(ipv4_address, x_ReadPortNumber());
                }
                break;
            case eCIT_Path:
                result.AppendPath(x_ReadString());
                break;
            case eCIT_String:
                result.AppendString(x_ReadString());
                break;
            case eCIT_Boolean:
                {
                    token_begin = m_Ch;
                    x_SaveErrPos();

                    while (!x_EOF() && isalpha(*m_Ch))
                        ++m_Ch;

                    CTempString bool_val(token_begin, m_Ch - token_begin);

                    if (bool_val == "false")
                        result.AppendBoolean(false);
                    else if (bool_val == "true")
                        result.AppendBoolean(true);
                    else {
                        CID_PARSER_EXCEPTION("invalid boolean value \"" <<
                                bool_val << '\"');
                    }
                }
                break;
            case eCIT_Flags:
                result.AppendFlags(x_ReadUint8());
                break;
            case eCIT_Label:
                result.AppendLabel(x_ReadString());
                break;
            case eCIT_Cue:
                result.AppendCue(x_ReadUint8());
                break;
            case eCIT_SeqID:
                result.AppendSeqID(x_ReadString());
                break;
            case eCIT_TaxID:
                result.AppendTaxID(x_ReadUint8());
                break;
            case eCIT_NestedCID:
                result.AppendNestedCID(ParseID());
                break;
            case eCIT_NumberOfTypes:
                break;
            }

            SkipSpaceToNextToken();

            if (*m_Ch == ',')
                ++m_Ch;
            else if (*m_Ch == '}')
                break;
            else {
                x_SaveErrPos();
                CID_PARSER_EXCEPTION("either ',' or '}' expected");
            }

            SkipSpaceToNextToken();
        }
    ++m_Ch;
    return result;
}

void CCompoundIDDumpParser::SkipSpace()
{
    while (!x_EOF() && isspace(*m_Ch))
        if (*m_Ch++ == '\n') {
            m_LineBegin = m_Ch;
            ++m_Line;
        }
}

void CCompoundIDDumpParser::SkipSpaceToNextToken()
{
    for (;;)
        if (x_EOF()) {
            x_SaveErrPos();
            CID_PARSER_EXCEPTION("unterminated compound ID");
        } else if (!isspace(*m_Ch))
            break;
        else if (*m_Ch++ == '\n') {
            m_LineBegin = m_Ch;
            ++m_Line;
        }
}

void CCompoundIDDumpParser::CheckEOF()
{
    if (!x_EOF()) {
        x_SaveErrPos();
        CID_PARSER_EXCEPTION("extra characters past component ID definition");
    }
}

string CCompoundIDDumpParser::x_ReadString()
{
    char quote_char;

    if (x_EOF() || ((quote_char = *m_Ch) != '"' && quote_char != '\'')) {
        x_SaveErrPos();
        CID_PARSER_EXCEPTION("string must start with a quote character");
    }

    const char* str_begin = ++m_Ch;
    bool escaped = false;

    while (!x_EOF())
        if (*m_Ch == quote_char && !escaped)
            return NStr::ParseEscapes(
                    CTempString(str_begin, m_Ch++ - str_begin));
        else if (*m_Ch == '\\') {
            escaped = !escaped;
            ++m_Ch;
        } else {
            escaped = false;
            if (*m_Ch++ == '\n') {
                m_LineBegin = m_Ch;
                ++m_Line;
            }
        }

    x_SaveErrPos();
    CID_PARSER_EXCEPTION("unterminated quoted string");
}

Uint8 CCompoundIDDumpParser::x_ReadUint8()
{
    x_SaveErrPos();

    if (x_EOF() || !isdigit(*m_Ch)) {
        CID_PARSER_EXCEPTION("missing integer value");
    }

    const char* token_begin;
    int base;

    if (*m_Ch != '0') {
        token_begin = m_Ch;
        ++m_Ch;  // A digit other than '0', moving on...
        base = 10;
    } else {
        ++m_Ch;  // Skip the leading zero.

        if (x_EOF())
            return 0;

        switch (*m_Ch) {
        case 'b':
        case 'B':
            token_begin = ++m_Ch;
            base = 2;
            break;
        case 'x':
        case 'X':
            token_begin = ++m_Ch;
            base = 16;
            break;
        default:
            if (!isdigit(*m_Ch))
                return 0;
            token_begin = m_Ch;
            ++m_Ch;  // It's a digit; move on to the next character.
            base = 8;
        }
    }

    while (!x_EOF() && isalnum(*m_Ch))
        ++m_Ch;

    Uint8 result = NStr::StringToUInt8(CTempString(token_begin,
            m_Ch - token_begin), NStr::fConvErr_NoThrow, base);

    if (result == 0 && errno != 0) {
        CID_PARSER_EXCEPTION("invalid Uint8 number specification");
    }

    return result;
}

Uint8 CCompoundIDDumpParser::x_ReadInt8()
{
    const char* token_begin = m_Ch;
    x_SaveErrPos();

    if (!x_EOF() && *m_Ch == '-')
        ++m_Ch;

    if (x_EOF() || !isdigit(*m_Ch)) {
        x_SaveErrPos();
        CID_PARSER_EXCEPTION("missing integer value");
    }

    do
        ++m_Ch;
    while (!x_EOF() && isdigit(*m_Ch));

    Int8 result = NStr::StringToInt8(CTempString(token_begin,
            m_Ch - token_begin), NStr::fConvErr_NoThrow);

    if (result == 0 && errno != 0) {
        CID_PARSER_EXCEPTION("integer overflow");
    }

    return result;
}

Uint4 CCompoundIDDumpParser::x_ReadIPv4Address()
{
    x_SaveErrPos();
    Uint4 ipv4_address = 0;
    unsigned char* octet = reinterpret_cast<unsigned char*>(&ipv4_address);
    unsigned number;
    int dots = 3;
    do {
        if (x_EOF())
            goto IPv4ParsingError;
        number = *m_Ch - '0';
        if (number > 9)
            goto IPv4ParsingError;
        for (;;) {
            ++m_Ch;
            if (x_EOF())
                goto IPv4ParsingError;
            unsigned digit = *m_Ch - '0';
            if (digit <= 9) {
                if ((number = number * 10 + digit) > 255)
                    goto IPv4ParsingError;
            } else if (*m_Ch == '.') {
                ++m_Ch;
                break;
            } else
                goto IPv4ParsingError;
        }
        *octet++ = (unsigned char) number;
    } while (--dots > 0);
    if (x_EOF())
        goto IPv4ParsingError;
    number = *m_Ch - '0';
    if (number > 9)
        goto IPv4ParsingError;
    for (;;) {
        ++m_Ch;
        if (x_EOF())
            break;
        unsigned digit = *m_Ch - '0';
        if (digit <= 9) {
            if ((number = number * 10 + digit) > 255)
                goto IPv4ParsingError;
        } else {
            if (*m_Ch == '.')
                ++m_Ch;
            break;
        }
    }
    *octet = (unsigned char) number;
    return ipv4_address;

IPv4ParsingError:
    CID_PARSER_EXCEPTION("invalid IPv4 address");
}

Uint2 CCompoundIDDumpParser::x_ReadPortNumber()
{
    x_SaveErrPos();
    Uint8 port_number = x_ReadUint8();
    if (port_number >= 1 << 8 * sizeof(Uint2)) {
        CID_PARSER_EXCEPTION("port number exceeds maximum value");
    }
    return (Uint2) port_number;
}

CCompoundID CCompoundIDPool::FromDump(const string& cid_dump)
{
    CCompoundIDDumpParser dump_parser(m_Impl, cid_dump);

    CCompoundID result(dump_parser.ParseID());

    dump_parser.SkipSpace();
    dump_parser.CheckEOF();

    return result;
}

#define SCRAMBLE_PASS() \
    pos = seq; \
    counter = seq_len - 1; \
    do { \
        pos[1] ^= *pos ^ length_factor--; \
        ++pos; \
    } while (--counter > 0);

static void s_Scramble(unsigned char* seq, size_t seq_len)
{
    if (seq_len > 1) {
        unsigned char length_factor = ((unsigned char) seq_len << 1) - 1;
        unsigned char* pos;
        size_t counter;

        SCRAMBLE_PASS();

        *seq ^= *pos ^ length_factor--;

        SCRAMBLE_PASS();
    }
}

void g_PackID(void* binary_id, size_t binary_id_len, string& packed_id)
{
    s_Scramble((unsigned char*) binary_id, binary_id_len);

    size_t packed_id_len;

    base64url_encode(NULL, binary_id_len, NULL, 0, &packed_id_len);

    packed_id.resize(packed_id_len);

    packed_id[0] = '\0';

    base64url_encode(binary_id, binary_id_len,
            const_cast<char*>(packed_id.data()), packed_id_len, NULL);
}

#define UNSCRAMBLE_PASS() \
    counter = seq_len - 1; \
    do { \
        *pos ^= pos[-1] ^ ++length_factor; \
        --pos; \
    } while (--counter > 0);

static void s_Unscramble(unsigned char* seq, size_t seq_len)
{
    if (seq_len > 1) {
        unsigned char length_factor = 0;
        unsigned char* pos = seq + seq_len - 1;
        size_t counter;

        UNSCRAMBLE_PASS();

        pos = seq + seq_len - 1;

        *seq ^= *pos ^ ++length_factor;

        UNSCRAMBLE_PASS();
    }
}

bool g_UnpackID(const string& packed_id, string& binary_id)
{
    size_t binary_id_len;

    base64url_decode(NULL, packed_id.length(), NULL, 0, &binary_id_len);

    binary_id.resize(binary_id_len);
    binary_id[0] = '\0';

    unsigned char* ptr = (unsigned char*) const_cast<char*>(binary_id.data());

    if (base64url_decode(packed_id.data(), packed_id.length(),
            ptr, binary_id_len, NULL) != eBase64_OK)
        return false;

    s_Unscramble(ptr, binary_id_len);

    return true;
}

END_NCBI_SCOPE
