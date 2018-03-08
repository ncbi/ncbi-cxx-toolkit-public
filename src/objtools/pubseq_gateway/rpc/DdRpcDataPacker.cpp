
#include <ncbi_pch.hpp>

#include <cstring>
#include <sstream>
#include <cstdint>
#include <cassert>

#include <objtools/pubseq_gateway/impl/rpc/UtilException.hpp>
#include <objtools/pubseq_gateway/impl/rpc/DdRpcCommon.hpp>
#include <objtools/pubseq_gateway/impl/rpc/DdRpcDataPacker.hpp>

using namespace std;

namespace DDRPC {
    
#define COLUMN_TYPE_MASK    0x1F
#define COLUMN_IS_NULLABLE  0x80
#define COLUMN_IS_KEY       0x40
#define DT_FMT              "%b %d %Y %H:%M:%S"

namespace DataType {

static const Type All[] =          {CVARCHAR,   SHORT_VARCHAR,   LONG_VARCHAR,     BIT,   UINT1,   INT1,   UINT2,   INT2,   UINT4,   INT4,   UINT8,   INT8,   FLOAT,   DATETIME};
static const char* Names[] =       {"CVARCHAR", "SHORT_VARCHAR", "LONG_VARCHAR",   "BIT", "UINT1", "INT1", "UINT2", "INT2", "UINT4", "INT4", "UINT8", "INT8", "FLOAT", "DATETIME"};
//  static const uint8_t BitLength[] = {0,          sizeof(uint8_t), sizeof(uint32_t), 1,     0,       0,      1,       1,      1,       1,      1,       1,      0,       1};
static constexpr unsigned int Size = countof(All);

bool NameToDataType(const std::string& Name, Type* dt) {
    if (s_NameToType.size() == 0) {
        for (size_t i = 0; i < Size; ++i)
            s_NameToType.emplace(Names[i], All[i]);
    }
    auto it = s_NameToType.find(Name);
    if (it == s_NameToType.end())
        return false;
    if (dt)
        *dt = it->second;
    return true;
}
}

static string Fetch(istream& is) {
    string rv;
    char ch;
    if (is.eof())
        return rv;

    while (is.get(ch) && ch == ' ');
    if (ch != ' ')
        is.unget();
    if (getline(is, rv, ' '))
        return rv;
    EDdRpcException::raise("Failed to fetch string");
}

static time_t StrToDateTime(const string& str, const char* fmt) {
    static bool has_tm_now = false;
    static struct tm tm_now = {0};
    struct tm tm = {0};
    if (strptime(str.c_str(), fmt, &tm) == NULL)
        return 0;
    if (!has_tm_now) {
        time_t t = time(0);
        localtime_r(&t, &tm_now);
        has_tm_now = true;
    }
    tm.tm_isdst = -1;
    tm.tm_gmtoff = tm_now.tm_gmtoff;
    tm.tm_zone = tm_now.tm_zone;
    return mktime(&tm);
}

string DateTimeToStr(time_t t) {
    char buf[32];
    struct tm tm;
    if (t != 0) {
        localtime_r(&t, &tm);
        strftime(buf, sizeof(buf), /*"%m/%d/%Y %H:%M:%S"*/ "%b %e %Y %H:%M:%S", &tm);
        return string(buf);
    }
    return "NULL";
}

/** DataColumn */

void DataColumn::AssignAsText(const string& Descr) {
    stringstream ss(Descr);
    string tok;
    IsNullable = true;
    Name = Fetch(ss);
    if (Name.empty())
        EDdRpcException::raise("Column name is expected");
    tok = Fetch(ss);
    if (tok.empty())
        EDdRpcException::raise("Column type is expected");
    bool found = DataType::NameToDataType(tok, &Type);
    if (!found)
        EDdRpcException::raise("Column type is not recognized");

    while (!ss.eof()) {
        tok = Fetch(ss);
        if (!IsKey && tok == "KEY")
            IsKey = true;
        else if (IsNullable && tok == "NOTNULL")
            IsNullable = false;
        else
            EDdRpcException::raise("Unexpected token");
    }
}

void DataColumn::AssignAsBin(istream& is) {
    if (!getline(is, Name, '\0'))
        EDdRpcException::raise("Column name is expected");
    if (is.eof())
        EDdRpcException::raise("Column type is expected");
    uint8_t Flags = (uint8_t)is.get();
    Type = (DataType::Type)(Flags & COLUMN_TYPE_MASK);
    if (Type < 0 || Type >= DataType::Size)
        EDdRpcException::raise("Column type is not recognized");
    IsNullable = Flags & COLUMN_IS_NULLABLE;
    IsKey = Flags & COLUMN_IS_KEY;
}

void DataColumn::GetAsText(std::ostream& os) const {
    os << Name << " " << DataType::Names[Type];
    if (!IsNullable)
        os << " NOTNULL";
    if (IsKey)
        os << " KEY";
}

void DataColumn::GetAsBin(ostream& os) const {
    os << Name;
    os.put('\0');
    uint8_t Flags = (uint8_t)Type | (IsNullable ? COLUMN_IS_NULLABLE : 0) | (IsKey ? COLUMN_IS_KEY : 0);
    os.put((char)Flags);
}


/** DataColumns */
    
void DataColumns::AssignAsBin(istream& is) {
    m_Elements.clear();
    uint8_t n = is.get();
    for (uint8_t i = 0; i < n; ++i)
        m_Elements.push_back(DataColumn(is));
}

void DataColumns::AssignAsText(const string& Descr) {
    m_Elements.clear();
    stringstream ss(Descr);
    string OneDescr;
    while (getline(ss, OneDescr, ','))
        m_Elements.push_back(DataColumn(OneDescr));
}

void DataColumns::GetAsBin(ostream& os) const {
    os.put(m_Elements.size());
    for (const auto& el : m_Elements)
        el.GetAsBin(os);
}

void DataColumns::GetAsText(string& Descr) const {
    stringstream os;
    for (const auto& el : m_Elements) {
        el.GetAsText(os);
        if (&el != &m_Elements.back())
            os << ", ";
    }
    Descr = os.str();
}

DataColumns DataColumns::ExtractKeyColumns() const {
    DataColumns rv;
    if (m_Elements.size() == 0)
        EDdRpcException::raise("Columns object is not initialized");
    for (const auto& it : m_Elements) {
        if (it.IsKey)
            rv.m_Elements.push_back(it);
    }
    return rv;
}

size_t DataColumns::KeyColumns() const {
    size_t rv = 0;
    for (const auto& it : m_Elements) {
        if (it.IsKey)
            ++rv;
    }
    return rv;
}

DataColumns DataColumns::ExtractDataColumns() const {
    DataColumns rv;
    if (m_Elements.size() == 0)
        EDdRpcException::raise("Columns object is not initialized");
    rv.m_Elements.reserve(m_Elements.size());
    for (const auto& it : m_Elements) {
        if (!it.IsKey)
            rv.m_Elements.push_back(it);
    }
    return rv;
}

void DataColumns::ParseBCP(const string& Line, char Delimiter, const DataColumns& KeyClms, const DataColumns& DataClms, string& Key, string& Data) {
    stringstream ss(Line);
    vector<string> DataValues;
    vector<string> KeyValues;
    string tok;
    DataValues.reserve(32);
    for (const auto& it : m_Elements) {
        if(getline(ss, tok, Delimiter)) {
            if (it.IsKey)
                KeyValues.push_back(tok);
            else
                DataValues.push_back(tok);
        }
        else
            EDdRpcException::raise("Line contains less columns than defined");
    }
    if (getline(ss, tok, Delimiter))
        EDdRpcException::raise("Line contains more columns than defined");


    DataRow KeyRow;
    DataRow DataRow;

    KeyRow.Pack(KeyValues, true, KeyClms);
    DataRow.Pack(DataValues, false, DataClms);

    Key = move(KeyRow.RawData);
    Data = move(DataRow.RawData);
}

/** DataRow */

bool DataRow::FetchBit(size_t& BitPos, const char* RawDataPtr, size_t RawDataLen) const {
    size_t Pos = BitPos;
    ++BitPos;
    size_t Ofs = Pos / 8;
    if (Ofs >= RawDataLen)
        EDdRpcException::raise("Data corruption (data truncated)");
    return ((uint8_t)RawDataPtr[Ofs] & (0x01 << (Pos % 8))) != 0;
}

uint8_t DataRow::FetchU1(size_t& BitPos, const char* RawDataPtr, size_t RawDataLen) const {
    size_t Pos = BitPos;
    BitPos += 8;
    size_t Ofs = Pos / 8;
    size_t Shift = (Pos % 8);
    if (Shift == 0) {
        if (Ofs >= RawDataLen)
            EDdRpcException::raise("Data corruption (data truncated)");
        return (uint8_t)RawDataPtr[Ofs];
    }

    if (Ofs + 1 >= RawDataLen)
        EDdRpcException::raise("Data corruption (data truncated)");
    return ((uint8_t)RawDataPtr[Ofs] >> Shift) | ((uint8_t)RawDataPtr[Ofs + 1] << (8 - Shift));
}

uint32_t DataRow::FetchU4(size_t& BitPos, const char* RawDataPtr, size_t RawDataLen) const {
    size_t Pos = BitPos;
    BitPos += 32;
    size_t Ofs = Pos / 8;
    size_t Shift = (Pos % 8);
    if (Shift == 0) {
        if (Ofs + 3 >= RawDataLen)
            EDdRpcException::raise("Data corruption (data truncated)");
        return *((uint32_t*)&RawDataPtr[Ofs]);
    }

    if (Ofs + 4 >= RawDataLen)
        EDdRpcException::raise("Data corruption (data truncated)");
    return ((*(uint32_t*)&RawDataPtr[Ofs]) >> Shift) | ((uint32_t)((uint8_t)RawDataPtr[Ofs + 4] << (8 - Shift))) << 24;
}

void DataRow::PostBit(size_t& BitPos, string& Data, bool Value) {
    size_t Pos = BitPos;
    ++BitPos;
    size_t Ofs = Pos / 8;
    if (Ofs >= Data.length())
        Data.resize(Ofs + 1);
    if (Value)
        Data[Ofs] |= (char)(1 << (Pos % 8));
}

void DataRow::PostU1(size_t& BitPos, string& Data, uint8_t Value) {
    size_t Pos = BitPos;
    BitPos += 8;
    size_t Ofs = Pos / 8;
    size_t Shift = Pos % 8;
    if (Shift == 0) {
        if (Ofs >= Data.length())
            Data.resize(Ofs + 1);
        Data[Ofs] = (char)Value;
    }
    else {
        if (Ofs + 1 >= Data.length())
            Data.resize(Ofs + 2);
        Data[Ofs] |= (char)(Value << Shift);
        Data[Ofs + 1] |= (char)(Value >> (8 - Shift));
    }
}

void DataRow::PostU4(size_t& BitPos, string& Data, uint32_t Value) {
    size_t Pos = BitPos;
    BitPos += 32;
    size_t Ofs = Pos / 8;
    size_t Shift = Pos % 8;
    if (Shift == 0) {
        if (Ofs + 3 >= Data.length())
            Data.resize(Ofs + 4);
        uint32_t *p = (uint32_t*)&Data[Ofs];
        *p = Value;
    }
    else {
        if (Ofs + 4 >= Data.length())
            Data.resize(Ofs + 5);
        uint32_t *p = (uint32_t*)&Data[Ofs];
        *p |= (Value << Shift);
        Data[Ofs + 4] |= (char)((Value >> (8 - Shift)) >> 24);
    }
}

void DataRow::UpdateColumnCount(const DataColumns& Clms) {
    size_t Columns = Clms.Size();
    m_Data.reserve(Columns);
    if (m_Data.size() != Columns) {
        while (m_Data.size() < Columns)
            m_Data.emplace_back();
        if (m_Data.size() > Columns)
            m_Data.erase(m_Data.begin() + Columns, m_Data.end());
        assert(m_Data.size() == Columns);
    }
}

void DataRow::LoadPresenceBits(size_t& DataPos, bool IsKey, const DataColumns& Clms) {
    size_t ClmIdx = 0;
    size_t BitPos = 0;
    assert(!IsKey || Clms.Size() != 0); // no direct key here
    const char* RawDataPtr = RawData.c_str();
    size_t RawDataLen = RawData.length();
    for (auto& it : m_Data) {
        const DataColumn& Clm = Clms[ClmIdx];
        it.Type = Clm.Type;
        it.IsNull = Clm.IsNullable && FetchBit(BitPos, RawDataPtr, RawDataLen);
        if (!it.IsNull) {
            switch (Clm.Type) {
                case DataType::CVARCHAR:      // no presense bits
                    break;
                case DataType::SHORT_VARCHAR:
                    it.AsUint1 = FetchU1(BitPos, RawDataPtr, RawDataLen);
                    break;
                case DataType::LONG_VARCHAR:
                    it.AsUint4 = FetchU4(BitPos, RawDataPtr, RawDataLen);
                    break;
                case DataType::BIT:
                    it.AsUint1 = FetchBit(BitPos, RawDataPtr, RawDataLen);
                    break;
                case DataType::UINT1:         // no presense bits
                case DataType::INT1:          // no presense bits
                    break;
                case DataType::UINT2:
                case DataType::INT2:
                case DataType::UINT4:
                case DataType::INT4:
                case DataType::UINT8:
                case DataType::INT8:
                    it.AsUint1 = FetchBit(BitPos, RawDataPtr, RawDataLen);
                    break;
                case DataType::FLOAT:         // no presense bits
                    break;
                case DataType::DATETIME:
                    it.AsUint1 = FetchBit(BitPos, RawDataPtr, RawDataLen);
                    break;
                default:
                    EDdRpcException::raise("Unexpected/unsupported type");
            }
        }
        ClmIdx++;
    }
    DataPos = (BitPos + 7) / 8;
}

void DataRow::UnpackDirectKey(const DataColumn& Clm) {
    VarData& Data = m_Data[0];
    Data.Type = Clm.Type;
    Data.IsNull = false;
    if (Clm.IsNullable)
        EDdRpcException::raise("Nullable single keys are not supported");

    const char* RawDataPtr = RawData.c_str();
    size_t RawDataLen = RawData.length();
    
    if (RawDataLen == 0)
        EDdRpcException::raise("Null key found");

    switch (Clm.Type) {
        case DataType::CVARCHAR:
        case DataType::SHORT_VARCHAR:
        case DataType::LONG_VARCHAR:
            Data.AsString.Len = RawDataLen;
            Data.AsString.Ofs = 0;
            break;
        case DataType::BIT:
            EDdRpcException::raise("BIT is not acceptable as a key column");
        case DataType::UINT1:
            switch (RawDataLen) {
                case 1: 
                    Data.AsUint1 = *(uint8_t*)RawDataPtr;
                default:
                    if (RawDataLen > 1)
                        EDdRpcException::raise("Key data is too long");
                    else
                        EDdRpcException::raise("Key data of invalid/unsupported length");
            }
            break;
        case DataType::INT1:
            switch (RawDataLen) {
                case 1: 
                    Data.AsInt1 = *(int8_t*)RawDataPtr;
                default:
                    if (RawDataLen > 1)
                        EDdRpcException::raise("Key data is too long");
                    else
                        EDdRpcException::raise("Key data of invalid/unsupported length");
            }
            break;
        case DataType::UINT2:
            switch (RawDataLen) {
                case 2:
                    Data.AsUint2 = *(uint16_t*)RawDataPtr;
                case 1: 
                    Data.AsUint2 = *(uint8_t*)RawDataPtr;
                default:
                    if (RawDataLen > 2)
                        EDdRpcException::raise("Key data is too long");
                    else
                        EDdRpcException::raise("Key data of invalid/unsupported length");
            }
            break;
        case DataType::INT2:
            switch (RawDataLen) {
                case 2:
                    Data.AsInt2 = *(int16_t*)RawDataPtr;
                case 1: 
                    Data.AsInt2 = *(int8_t*)RawDataPtr;
                default:
                    if (RawDataLen > 2)
                        EDdRpcException::raise("Key data is too long");
                    else
                        EDdRpcException::raise("Key data of invalid/unsupported length");
            }
            break;
        case DataType::UINT4:
            switch (RawDataLen) {
                case 4:
                    Data.AsUint4 = *(uint32_t*)RawDataPtr;
                    break;
                case 2:
                    Data.AsUint4 = *(uint16_t*)RawDataPtr;
                    break;
                case 1:
                    Data.AsUint4 = *(uint8_t*)RawDataPtr;
                    break;
                default:
                    if (RawDataLen > 4)
                        EDdRpcException::raise("Key data is too long");
                    else
                        EDdRpcException::raise("Key data of invalid/unsupported length");
            }
            break;
        case DataType::INT4:
            switch (RawDataLen) {
                case 4:
                    Data.AsInt4 = *(int32_t*)RawDataPtr;
                    break;
                case 2:
                    Data.AsInt4 = *(int16_t*)RawDataPtr;
                    break;
                case 1:
                    Data.AsInt4 = *(int8_t*)RawDataPtr;
                    break;
                default:
                    if (RawDataLen > 4)
                        EDdRpcException::raise("Key data is too long");
                    else
                        EDdRpcException::raise("Key data of invalid/unsupported length");
            }
            break;
        case DataType::UINT8:
            switch (RawDataLen) {
                case 8:
                    Data.AsUint8 = *(uint64_t*)RawDataPtr;
                    break;
                case 4:
                    Data.AsUint8 = *(uint32_t*)RawDataPtr;
                    break;
                case 2:
                    Data.AsUint8 = *(uint16_t*)RawDataPtr;
                    break;
                case 1:
                    Data.AsUint8 = *(uint8_t*)RawDataPtr;
                    break;
                default:
                    if (RawDataLen > 8)
                        EDdRpcException::raise("Key data is too long");
                    else
                        EDdRpcException::raise("Key data of invalid/unsupported length");
            }
            break;
        case DataType::INT8:
            switch (RawDataLen) {
                case 8:
                    Data.AsInt8 = *(int64_t*)RawDataPtr;
                    break;
                case 4:
                    Data.AsInt8 = *(int32_t*)RawDataPtr;
                    break;
                case 2:
                    Data.AsInt8 = *(int16_t*)RawDataPtr;
                    break;
                case 1:
                    Data.AsInt8 = *(int8_t*)RawDataPtr;
                    break;
                default:
                    if (RawDataLen > 8)
                        EDdRpcException::raise("Key data is too long");
                    else
                        EDdRpcException::raise("Key data of invalid/unsupported length");
            }
            break;
        case DataType::FLOAT:
            switch (RawDataLen) {
                case 8:
                    Data.AsFloat = *(double*)RawDataPtr;
                    break;
                case 4:
                    Data.AsFloat = *(float*)RawDataPtr;
                    break;
                default:
                    if (RawDataLen > 8)
                        EDdRpcException::raise("Key data is too long");
                    else
                        EDdRpcException::raise("Key data of invalid/unsupported length");
            }
            break;
        case DataType::DATETIME:
            switch (RawDataLen) {
                case 8:
                    Data.AsDateTime = *(uint64_t*)RawDataPtr;
                    break;
                case 4:
                    Data.AsFloat = *(uint32_t*)RawDataPtr;
                    break;
                default:
                    if (RawDataLen > 8)
                        EDdRpcException::raise("Key data is too long");
                    else
                        EDdRpcException::raise("Key data of invalid/unsupported length");
            }
            break;
        default:
            EDdRpcException::raise("Unexpected/unsupported type");
    }
}

void DataRow::Unpack(const string &Raw, bool IsKey, const DataColumns& Clms) {

    RawData = Raw;
    size_t Columns = Clms.Size();
    bool DirectKey = (IsKey && Columns == 1);

    UpdateColumnCount(Clms);

    if (DirectKey) {
        UnpackDirectKey(Clms[0]);
    }
    else {
        size_t DataPos = 0;
        LoadPresenceBits(DataPos, IsKey, Clms);
        const char* RawDataPtr = RawData.c_str() + DataPos;
        size_t RawDataOfs = DataPos;
        size_t RawDataLen = RawData.length() - DataPos;

        size_t DataIdx = 0;
        for (const auto& Clm : Clms) {
            auto& Data = m_Data[DataIdx++];
            if (Data.IsNull)
                continue;

            size_t DataLen = 0;
                
            switch (Clm.Type) {
                case DataType::CVARCHAR: {
                    size_t len = strlen(RawDataPtr);
                    Data.AsString.Len = len;
                    Data.AsString.Ofs = RawDataOfs;
                    DataLen = len + 1;
                    break;
                }
                case DataType::SHORT_VARCHAR:
                    Data.AsString.Len = Data.AsUint1;
                    Data.AsString.Ofs = RawDataOfs;
                    DataLen = Data.AsUint1;
                    break;
                case DataType::LONG_VARCHAR:
                    Data.AsString.Len = Data.AsUint4;
                    Data.AsString.Ofs = RawDataOfs;
                    DataLen = Data.AsUint4;
                    break;
                case DataType::BIT: // fetched among the presence bits
                    break;
                case DataType::UINT1:
                    Data.AsUint1 = *(uint8_t*)RawDataPtr;
                    DataLen = sizeof(uint8_t);
                    break;
                case DataType::INT1:
                    Data.AsInt1 = *(int8_t*)RawDataPtr;
                    DataLen = sizeof(int8_t);
                    break;
                case DataType::UINT2:
                    if (Data.AsUint1) {
                        Data.AsUint2 = *(uint8_t*)RawDataPtr;
                        DataLen = sizeof(uint8_t);
                    }
                    else {
                        Data.AsUint2 = *(uint16_t*)RawDataPtr;
                        DataLen = sizeof(uint16_t);
                    }
                    break;
                case DataType::INT2:
                    if (Data.AsUint1) {
                        Data.AsInt2 = *(int8_t*)RawDataPtr;
                        DataLen = sizeof(int8_t);
                    }
                    else {
                        Data.AsInt2 = *(int16_t*)RawDataPtr;
                        DataLen = sizeof(int16_t);
                    }
                    break;
                case DataType::UINT4:
                    if (Data.AsUint1) {
                        Data.AsUint4 = *(uint16_t*)RawDataPtr;
                        DataLen = sizeof(uint16_t);
                    }
                    else {
                        Data.AsUint4 = *(uint32_t*)RawDataPtr;
                        DataLen = sizeof(uint32_t);
                    }
                    break;
                case DataType::INT4:
                    if (Data.AsUint1) {
                        Data.AsInt4 = *(int16_t*)RawDataPtr;
                        DataLen = sizeof(int16_t);
                    }
                    else {
                        Data.AsInt4 = *(int32_t*)RawDataPtr;
                        DataLen = sizeof(int32_t);
                    }
                    break;
                case DataType::UINT8:
                    if (Data.AsUint1) {
                        Data.AsUint8 = *(uint32_t*)RawDataPtr;
                        DataLen = sizeof(uint32_t);
                    }
                    else {
                        Data.AsUint8 = *(uint64_t*)RawDataPtr;
                        DataLen = sizeof(uint64_t);
                    }
                    break;
                case DataType::INT8:
                    if (Data.AsUint1) {
                        Data.AsInt8 = *(int32_t*)RawDataPtr;
                        DataLen = sizeof(int32_t);
                    }
                    else {
                        Data.AsInt8 = *(int64_t*)RawDataPtr;
                        DataLen = sizeof(int64_t);
                    }
                    break;
                case DataType::FLOAT:
                    Data.AsFloat = *(double*)RawDataPtr;
                    DataLen = sizeof(double);
                    break;
                case DataType::DATETIME:
                    if (Data.AsUint1) {
                        Data.AsDateTime = *(int32_t*)RawDataPtr;
                        DataLen = sizeof(int32_t);
                    }
                    else {
                        Data.AsDateTime = *(int64_t*)RawDataPtr;
                        DataLen = sizeof(int64_t);
                    }
                    break;
                default:
                    EDdRpcException::raise("Unexpected/unsupported type");
            }

            if (DataLen > 0) {
                RawDataPtr += DataLen;
                RawDataOfs += DataLen;
                if (RawDataLen < DataLen)
                    EDdRpcException::raise("Data corruption (data truncated)");
                RawDataLen -= DataLen;
            }
        }
        if (RawDataLen > 0)
            EDdRpcException::raise("Data corruption (packed data remained)");
    }
}

void DataRow::PackDirectKey(const string& Value, const DataColumn& Clm) {
    switch (Clm.Type) {
        case DataType::CVARCHAR:
        case DataType::SHORT_VARCHAR:
        case DataType::LONG_VARCHAR:
            RawData = Value;
            break;
        case DataType::BIT:
            EDdRpcException::raise("BIT is not acceptable as a key column");
        case DataType::UINT1:
        case DataType::UINT2:
        case DataType::UINT4:
        case DataType::UINT8: {
            uint64_t v = stoull(Value);
            if (v > UINT32_MAX) {
                if (Clm.Type != DataType::UINT8)
                    EDdRpcException::raise("Too large integer value for a key");
                RawData.assign((const char*)&v, sizeof(v));
            }
            else if (v > UINT16_MAX) {
                if (Clm.Type != DataType::UINT8 && Clm.Type != DataType::UINT4)
                    EDdRpcException::raise("Too large integer value for a key");
                uint32_t vv = v;
                RawData.assign((const char*)&vv, sizeof(vv));
            }
            else if (v > UINT8_MAX) {
                if (Clm.Type != DataType::UINT8 && Clm.Type != DataType::UINT4 && Clm.Type != DataType::UINT2)
                    EDdRpcException::raise("Too large integer value for a key");
                uint16_t vv = v;
                RawData.assign((const char*)&vv, sizeof(vv));
            }
            else {
                uint8_t vv = v;
                RawData.assign((const char*)&vv, sizeof(vv));
            }
            break;
        }
        case DataType::INT1:
        case DataType::INT2:
        case DataType::INT4:
        case DataType::INT8: {
            int64_t v = stoll(Value);
            if (v > INT32_MAX || v < INT32_MIN) {
                if (Clm.Type != DataType::INT8)
                    EDdRpcException::raise("Too large integer value for a key");
                RawData.assign((const char*)&v, sizeof(v));
            }
            else if (v > INT16_MAX || v < INT16_MIN) {
                if (Clm.Type != DataType::INT8 && Clm.Type != DataType::INT4)
                    EDdRpcException::raise("Too large integer value for a key");
                int32_t vv = v;
                RawData.assign((const char*)&vv, sizeof(vv));
            }
            else if (v > INT8_MAX || v < INT8_MIN) {
                if (Clm.Type != DataType::INT8 && Clm.Type != DataType::INT4 && Clm.Type != DataType::INT2)
                    EDdRpcException::raise("Too large integer value for a key");
                int16_t vv = v;
                RawData.assign((const char*)&vv, sizeof(vv));
            }
            else {
                int8_t vv = v;
                RawData.assign((const char*)&vv, sizeof(vv));
            }
            break;
        }
        case DataType::FLOAT: {
            char *end = nullptr;
            double d = strtod(Value.c_str(), &end);
            RawData.assign((const char*)&d, sizeof(d));
            break;
        }
            
        case DataType::DATETIME: {
            time_t t = StrToDateTime(Value, DT_FMT);
            if (t <= UINT32_MAX) {
                uint32_t v = t;
                RawData.assign((const char*)&v, sizeof(v));
            }
            else 
                RawData.assign((const char*)&t, sizeof(t));
            break;
        }
        default:
            EDdRpcException::raise("Unexpected/unsupported type");
    }
    
}

void DataRow::Pack(const vector<string>& Values, bool IsKey, const DataColumns& Clms) {
    if (Clms.Size() != Values.size())
        EDdRpcException::raise("Number of columns does not match");
    bool DirectKey = IsKey && Clms.Size() == 1;

    UpdateColumnCount(Clms);

    if (DirectKey) {
        PackDirectKey(Values[0], Clms[0]);
    }
    else {
        string AccumulatedData;
        size_t DataIdx = 0;
        size_t PresenceBitPos = 0;
        
        for (const auto& Clm : Clms) {
            auto& Data = m_Data[DataIdx];
            auto& Value = Values[DataIdx++];
            bool IsEmpty = Value.empty();

            if (IsEmpty && !Clm.IsNullable)
                EDdRpcException::raise("Empty value is not allwoed for non-nullable column");
            if (Clm.IsNullable)
                PostBit(PresenceBitPos, RawData, IsEmpty);
            Data.IsNull = IsEmpty;
            Data.Type = Clm.Type;
            if (!IsEmpty) {
                size_t DataLen = 0;

                uint64_t uv;
                int64_t sv;
                double dv;
                time_t tv;

                const char* DataPtr = nullptr;
                switch (Clm.Type) {
                    case DataType::CVARCHAR:
                        Data.AsString.Len = Value.length();
                        Data.AsString.Ofs = AccumulatedData.length();
                        DataLen = Data.AsString.Len + 1; // include last 0
                        DataPtr = Value.c_str();
                        break;

                    case DataType::SHORT_VARCHAR:
                        if (Value.length() > 255)
                            EDdRpcException::raise("Too large string for SHORT_VARCHAR");
                        DataLen = Data.AsString.Len = Value.length();
                        Data.AsString.Ofs = AccumulatedData.length();
                        DataPtr = Value.c_str();
                        PostU1(PresenceBitPos, RawData, DataLen);
                        break;

                    case DataType::LONG_VARCHAR:
                        DataLen = Data.AsString.Len = Value.length();
                        Data.AsString.Ofs = AccumulatedData.length();
                        DataPtr = Value.c_str();
                        PostU4(PresenceBitPos, RawData, DataLen);
                        break;

                    case DataType::BIT: // fetched among the presence bits
                        PostBit(PresenceBitPos, RawData, stoll(Value) != 0);
                        break;

                    case DataType::UINT1:
                        uv = stoull(Value);
                        if (uv > UINT8_MAX)
                            EDdRpcException::raise("Too large integer value for a column");
                        Data.AsUint1 = uv;
                        DataLen = sizeof(uint8_t);
                        DataPtr = (const char*)&uv;
                        break;

                    case DataType::UINT2:
                        uv = stoull(Value);
                        if (uv > UINT16_MAX)
                            EDdRpcException::raise("Too large integer value for a column");

                        Data.AsUint2 = uv;
                        if (uv <= UINT8_MAX) {
                            PostBit(PresenceBitPos, RawData, 1);
                            DataLen = sizeof(uint8_t);
                            DataPtr = (const char*)&uv;
                        }
                        else {
                            PostBit(PresenceBitPos, RawData, 0);
                            DataLen = sizeof(uint16_t);
                            DataPtr = (const char*)&uv;
                        }
                        break;

                    case DataType::UINT4:
                        uv = stoull(Value);
                        if (uv > UINT32_MAX)
                            EDdRpcException::raise("Too large integer value for a column");

                        Data.AsUint4 = uv;
                        if (uv <= UINT16_MAX) {
                            PostBit(PresenceBitPos, RawData, 1);
                            DataLen = sizeof(uint16_t);
                            DataPtr = (const char*)&uv;
                        }
                        else {
                            PostBit(PresenceBitPos, RawData, 0);
                            DataLen = sizeof(uint32_t);
                            DataPtr = (const char*)&uv;
                        }
                        break;

                    case DataType::UINT8:
                        uv = stoull(Value);

                        Data.AsUint8 = uv;
                        if (uv <= UINT32_MAX) {
                            PostBit(PresenceBitPos, RawData, 1);
                            DataLen = sizeof(uint32_t);
                            DataPtr = (const char*)&uv;
                        }
                        else {
                            PostBit(PresenceBitPos, RawData, 0);
                            DataLen = sizeof(uint64_t);
                            DataPtr = (const char*)&uv;
                        }
                        break;

                    case DataType::INT1:
                        sv = stoll(Value);
                        if (sv > INT8_MAX || sv < INT8_MIN)
                            EDdRpcException::raise("Too large integer value for a column");
                        Data.AsInt1 = sv;
                        DataLen = sizeof(int8_t);
                        DataPtr = (const char*)&sv;
                        break;

                    case DataType::INT2:
                        sv = stoll(Value);
                        if (sv > INT16_MAX || sv < INT16_MIN)
                            EDdRpcException::raise("Too large integer value for a column");

                        Data.AsInt2 = sv;
                        if (sv <= INT8_MAX && sv >= INT8_MIN) {
                            PostBit(PresenceBitPos, RawData, 1);
                            DataLen = sizeof(int8_t);
                            DataPtr = (const char*)&sv;
                        }
                        else {
                            PostBit(PresenceBitPos, RawData, 0);
                            DataLen = sizeof(int16_t);
                            DataPtr = (const char*)&sv;
                        }
                        break;

                    case DataType::INT4:
                        sv = stoll(Value);
                        if (sv > INT32_MAX || sv < INT32_MIN)
                            EDdRpcException::raise("Too large integer value for a column");

                        Data.AsInt4 = sv;
                        if (sv <= INT16_MAX && sv >= INT16_MIN) {
                            PostBit(PresenceBitPos, RawData, 1);
                            DataLen = sizeof(int16_t);
                            DataPtr = (const char*)&sv;
                        }
                        else {
                            PostBit(PresenceBitPos, RawData, 0);
                            DataLen = sizeof(int32_t);
                            DataPtr = (const char*)&sv;
                        }
                        break;

                    case DataType::INT8:
                        sv = stoll(Value);

                        Data.AsInt8 = sv;
                        if (sv <= INT32_MAX && sv >= INT32_MIN) {
                            PostBit(PresenceBitPos, RawData, 1);
                            DataLen = sizeof(int32_t);
                            DataPtr = (const char*)&sv;
                        }
                        else {
                            PostBit(PresenceBitPos, RawData, 0);
                            DataLen = sizeof(int64_t);
                            DataPtr = (const char*)&sv;
                        }
                        break;

                    case DataType::FLOAT: {
                        char *end = nullptr;
                        dv = strtod(Value.c_str(), &end);

                        Data.AsFloat = dv;
                        DataPtr = (const char*)&dv;
                        DataLen = sizeof(dv);
                        break;
                    }

                    case DataType::DATETIME:
                        tv = StrToDateTime(Value, DT_FMT);
                        if (tv <= UINT32_MAX) {
                            PostBit(PresenceBitPos, RawData, 1);
                            DataLen = sizeof(uint32_t);
                            DataPtr = (const char*)&tv;
                        }
                        else {
                            PostBit(PresenceBitPos, RawData, 0);
                            DataLen = sizeof(tv);
                            DataPtr = (const char*)&tv;
                        }
                        break;

                    default:
                        EDdRpcException::raise("Unexpected/unsupported type");
                }
                if (DataLen != 0) {
                    AccumulatedData.append(DataPtr, DataLen);
                }
            }
        }
        
        // Adjust Data.AsString.Ofs to PresenseBit's offset
        size_t PresenseBitSize = RawData.length();
        for (auto& Data : m_Data) {
            if (!Data.IsNull) {
                switch(Data.Type) {
                    case DataType::CVARCHAR:
                    case DataType::SHORT_VARCHAR:
                    case DataType::LONG_VARCHAR:
                        Data.AsString.Ofs += PresenseBitSize;
                        break;
                    default:;
                }
            }
        }

        RawData.append(AccumulatedData);
        
    }
}

void DataRow::GetAsTsv(std::ostream& os, char Delimiter) const {
    for (const auto& Data : m_Data) {
        if (!Data.IsNull) {
            switch (Data.Type) {
                case DataType::LONG_VARCHAR:
                case DataType::SHORT_VARCHAR:
                case DataType::CVARCHAR:
                    os.write(&RawData[Data.AsString.Ofs], Data.AsString.Len);
                    break;

                case DataType::BIT:
                    os << (Data.AsUint1 ? "1" : "0");
                    break;

                case DataType::UINT1:
                    os << (int)Data.AsUint1;
                    break;

                case DataType::UINT2:
                    os << Data.AsUint2;
                    break;

                case DataType::UINT4:
                    os << Data.AsUint4;
                    break;

                case DataType::UINT8:
                    os << Data.AsUint8;
                    break;

                case DataType::INT1:
                    os << (int)Data.AsInt1;
                    break;

                case DataType::INT2:
                    os << Data.AsInt2;
                    break;

                case DataType::INT4:
                    os << Data.AsInt4;
                    break;

                case DataType::INT8:
                    os << Data.AsInt8;
                    break;

                case DataType::FLOAT: {
                    os << Data.AsFloat;
                    break;
                }

                case DataType::DATETIME:
                    os << DateTimeToStr(Data.AsDateTime);
                    break;

                default:
                    EDdRpcException::raise("Unexpected/unsupported type");
            }
        }
//        if (&Data != &m_Data.back())
            os << Delimiter;
    }
}


static thread_local unique_ptr<DataColumns> s_accver_resolver_data_columns;

void AccVerResolverUnpackData(DataRow& row, const std::string& data)
{
    constexpr const char kACCVER_RESOLVER_COLUMNS[] =
        "ACCVER "   "CVARCHAR NOTNULL KEY, "
        "GI "       "INT8, "
        "LEN "      "UINT4, "
        "SAT "      "UINT1, "
        "SAT_KEY "  "UINT4, "
        "TAXID "    "UINT4, "
        "DATE "     "DATETIME, "
        "SUPPRESS " "BIT NOTNULL";

    if (!s_accver_resolver_data_columns) {
        DataColumns clm;
        clm.AssignAsText(kACCVER_RESOLVER_COLUMNS);
        s_accver_resolver_data_columns.reset(new DataColumns(clm.ExtractDataColumns()));
    }
    row.Unpack(data, false, *s_accver_resolver_data_columns);
}

};
