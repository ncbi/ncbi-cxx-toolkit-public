#ifndef DDRPCDATAPACKER__HPP
#define DDRPCDATAPACKER__HPP

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

#include <string>
#include <memory>
#include <cstdint>
#include <vector>
#include <istream>
#include <iostream>
#include <thread>
#include <unordered_map>

#include "DdRpcCommon.hpp"

namespace DDRPC {

namespace DataType {
    enum Type {
        CVARCHAR,      // zero-terminated
        SHORT_VARCHAR, // string, length is 1 byte up to 255
        LONG_VARCHAR,  // string, length is 4 bytes, up to 4B
        BIT,
        UINT1,
        INT1,
        UINT2,
        INT2,
        UINT4,
        INT4,
        UINT8,
        INT8,
        FLOAT,
        DATETIME
    };
    static thread_local std::unordered_map<std::string, Type> s_NameToType;
    bool NameToDataType(const std::string& Name, Type* dt);
}

struct VarData {
public:
    DataType::Type Type;
    bool IsNull;
    union {
        struct {
            uint32_t Len;
            uint32_t Ofs;
        } AsString;
        int8_t AsInt1;
        uint8_t AsUint1;
        int16_t AsInt2;
        uint16_t AsUint2;
        int32_t AsInt4;
        uint32_t AsUint4;
        int64_t AsInt8;
        uint64_t AsUint8;
        double AsFloat;
        time_t AsDateTime;
    };
};

struct DataColumn {
    std::string Name;
    DataType::Type Type;
    bool IsKey;
    bool IsNullable;
    DataColumn() :
        Type(DataType::CVARCHAR),
        IsKey(false),
        IsNullable(false)
    {}
    DataColumn(const std::string& Descr) : DataColumn() {
        AssignAsText(Descr);
    }
    DataColumn(std::istream& is) : DataColumn() {
        AssignAsBin(is);
    }
    void AssignAsText(const std::string& Descr);
    void AssignAsBin(std::istream& is);
    void GetAsText(std::ostream& os) const;
    void GetAsBin(std::ostream& os) const;
};

class DataRow;

class DataColumns {
private:
    std::vector<DataColumn> m_Elements;
public:
    DataColumns() = default;
    DataColumns(std::istream& is) {
        AssignAsBin(is);
    }
    DataColumns(const std::string& Descr) {
        AssignAsText(Descr);
    }
    void AssignAsBin(std::istream& is);
    void AssignAsText(const std::string& Descr);
    void GetAsBin(std::ostream& os) const;
    void GetAsText(std::string& Descr) const;

    DataColumns ExtractKeyColumns() const;
    DataColumns ExtractDataColumns() const;
    size_t Size() const {
        return m_Elements.size();
    }
    std::vector<DataColumn>::const_iterator begin() const {
        return m_Elements.begin();
    }
    std::vector<DataColumn>::const_iterator end() const {
        return m_Elements.end();
    }
    size_t KeyColumns() const;
    DataColumn& operator[](std::size_t n) {
        return m_Elements[n];
    }
    const DataColumn& operator[](std::size_t n) const {
        return m_Elements[n];
    }
    void ParseBCP(const std::string& Line, char Delimiter, const DataColumns& KeyClms, const DataColumns& DataClms, std::string& Key, std::string& Data);
};

class DataRow {
private:
    std::vector<VarData> m_Data;
    void UpdateColumnCount(const DataColumns& Clms);
    void UnpackDirectKey(const DataColumn& Clm);
    void LoadPresenceBits(size_t& DataPos, bool IsKey, const DataColumns& Clms);
    bool FetchBit(size_t& BitPos, const char* RawDataPtr, size_t RawDataLen) const;
    uint8_t FetchU1(size_t& BitPos, const char* RawDataPtr, size_t RawDataLen) const;
    uint32_t FetchU4(size_t& BitPos, const char* RawDataPtr, size_t RawDataLen) const;
    void PostBit(size_t& BitPos, std::string& Data, bool Value);
    void PostU1(size_t& BitPos, std::string& Data, uint8_t Value);
    void PostU4(size_t& BitPos, std::string& Data, uint32_t Value);

    void PackDirectKey(const std::string& Value, const DataColumn& Clm);
public:
    DataRow() = default;
    void Unpack(const std::string &Raw, bool IsKey, const DataColumns& Clms);
    void Pack(const std::vector<std::string>& Values, bool IsKey, const DataColumns& Clms);
    void GetAsTsv(std::ostream& os, char Delimiter) const;
    VarData& operator[](std::size_t n) {
        return m_Data[n];
    }
    const VarData& operator[](std::size_t n) const {
        return m_Data[n];
    }
    std::string RawData;
};

std::string DateTimeToStr(time_t t);


}

#endif
