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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  BDB libarary types implementations.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_bswap.hpp>
#include <bdb/bdb_types.hpp>
#include <db.h>

BEGIN_NCBI_SCOPE


static
const unsigned char* s_GetLString(const unsigned char* str, 
                                  bool  check_legacy, 
                                  int*  str_len)
{
    _ASSERT(str);
    _ASSERT(str_len);

    // string length reconstruction
    *str_len = (str[0])       | 
               (str[1] << 8)  |
               (str[2] << 16) |
               (str[3] << 24);
    
    if (check_legacy) {
        if (*str_len < 0) { // true L-string
            *str_len = -(*str_len);
            str += 4;
        } else {
            *str_len = ::strlen((const char*)str);
        }
    } else {  // no legacy strings
        if (*str_len < 0) { // true L-string
            *str_len = -(*str_len);
            str += 4;
        } else {
            _ASSERT(0); // positive length !
        }
    }

    return str;

}

static 
const unsigned char* s_GetLString(const DBT* val, 
                                  bool  check_legacy, 
                                  int*  str_len)
{
    const unsigned char* str = (const unsigned char*)val->data;

    if (val->size <= 4) {  // looks like legacy C-string
        _ASSERT(check_legacy);
        *str_len = ::strlen((const char*)str);
        return str;
    }

    return s_GetLString(str, check_legacy, str_len);
}



extern "C"
{

/////////////////////////////////////////////////////////////////////////////
//  BDB comparison functions
//

int BDB_UintCompare(DB*, const DBT* val1, const DBT* val2)
{
    unsigned int v1, v2;
    ::memcpy(&v1, val1->data, sizeof(unsigned));
    ::memcpy(&v2, val2->data, sizeof(unsigned));
    return (v1 < v2) ? -1
                     : ((v2 < v1) ? 1 : 0);
}

int BDB_Int8Compare(DB*, const DBT* val1, const DBT* val2)
{
    Int8 v1, v2;
    ::memcpy(&v1, val1->data, sizeof(Int8));
    ::memcpy(&v2, val2->data, sizeof(Int8));
    return (v1 < v2) ? -1
                     : ((v2 < v1) ? 1 : 0);
}

int BDB_IntCompare(DB*, const DBT* val1, const DBT* val2)
{
    int v1, v2;
    ::memcpy(&v1, val1->data, sizeof(int));
    ::memcpy(&v2, val2->data, sizeof(int));
    return (v1 < v2) ? -1
                     : ((v2 < v1) ? 1 : 0);
}

int BDB_Int2Compare(DB*, const DBT* val1, const DBT* val2)
{
    Int2 v1, v2;
    ::memcpy(&v1, val1->data, sizeof(Int2));
    ::memcpy(&v2, val2->data, sizeof(Int2));
    return (v1 < v2) ? -1
                     : ((v2 < v1) ? 1 : 0);
}

int BDB_UCharCompare(DB*, const DBT* val1, const DBT* val2)
{
    const unsigned char& v1=*static_cast<unsigned char*>(val1->data);
    const unsigned char& v2=*static_cast<unsigned char*>(val2->data);

    return (v1 < v2) ? -1
                     : ((v2 < v1) ? 1 : 0);
}

int BDB_FloatCompare(DB*, const DBT* val1, const DBT* val2)
{
    float v1, v2;
    ::memcpy(&v1, val1->data, sizeof(v1));
    ::memcpy(&v2, val2->data, sizeof(v2));
    return (v1 < v2) ? -1
                     : ((v2 < v1) ? 1 : 0);
}

int BDB_DoubleCompare(DB*, const DBT* val1, const DBT* val2)
{
    double v1, v2;
    ::memcpy(&v1, val1->data, sizeof(v1));
    ::memcpy(&v2, val2->data, sizeof(v2));
    return (v1 < v2) ? -1
                     : ((v2 < v1) ? 1 : 0);
}

int BDB_StringCompare(DB*, const DBT* val1, const DBT* val2)
{
    return ::strcmp((const char*)val1->data, (const char*)val2->data);
}



int BDB_LStringCompare(DB* db, const DBT* val1, const DBT* val2)
{
    const CBDB_BufferManager* fbuf1 =
          static_cast<CBDB_BufferManager*> (db->app_private);

    bool check_legacy = fbuf1->IsLegacyStrings();
    
    const unsigned char* str1;
    const unsigned char* str2;
    int str_len1; 
    int str_len2;

    str1 = s_GetLString(val1, check_legacy, &str_len1); 
    str2 = s_GetLString(val2, check_legacy, &str_len2); 

    int cmp_len = min(str_len1, str_len2);
    int r = ::memcmp(str1, str2, cmp_len);
    if (r == 0) {
        return (str_len1 < str_len2) ? -1
                                     : ((str_len2 < str_len1) ? 1 : 0);
    }
    return r;
}


int BDB_StringCaseCompare(DB*, const DBT* val1, const DBT* val2)
{
    return NStr::strcasecmp((const char*)val1->data, (const char*)val2->data);
}

int BDB_Compare(DB* db, const DBT* val1, const DBT* val2)
{
    const CBDB_BufferManager* fbuf1 =
          static_cast<CBDB_BufferManager*> (db->app_private);

    bool byte_swapped = fbuf1->IsByteSwapped();

    _ASSERT(fbuf1);

    const char* p1 = static_cast<char*> (val1->data);
    const char* p2 = static_cast<char*> (val2->data);

    unsigned int cmp_limit = fbuf1->GetFieldCompareLimit();
    if (cmp_limit == 0) {
        cmp_limit = fbuf1->FieldCount();
    } else {
        _ASSERT(cmp_limit <= fbuf1->FieldCount());
    }

    for (unsigned int i = 0;  i < cmp_limit;  ++i) {
        const CBDB_Field& fld1 = fbuf1->GetField(i);
        int ret = fld1.Compare(p1, p2, byte_swapped);
        if ( ret )
            return ret;

        p1 += fld1.GetDataLength(p1);
        p2 += fld1.GetDataLength(p2);
    }

    return 0;
}



int BDB_ByteSwap_UintCompare(DB*, const DBT* val1, const DBT* val2)
{
    unsigned int v1, v2;
    v1 = (unsigned int) CByteSwap::GetInt4((unsigned char*)val1->data);
    v2 = (unsigned int) CByteSwap::GetInt4((unsigned char*)val2->data);
    return (v1 < v2) ? -1
                     : ((v2 < v1) ? 1 : 0);
}

int BDB_ByteSwap_Int8Compare(DB*, const DBT* val1, const DBT* val2)
{
    Int8 v1, v2;
    v1 = CByteSwap::GetInt8((unsigned char*)val1->data);
    v2 = CByteSwap::GetInt8((unsigned char*)val2->data);
    return (v1 < v2) ? -1
                     : ((v2 < v1) ? 1 : 0);
}

int BDB_ByteSwap_IntCompare(DB*, const DBT* val1, const DBT* val2)
{
    int v1, v2;
    v1 = CByteSwap::GetInt4((unsigned char*)val1->data);
    v2 = CByteSwap::GetInt4((unsigned char*)val2->data);
    return (v1 < v2) ? -1
                     : ((v2 < v1) ? 1 : 0);
}

int BDB_ByteSwap_Int2Compare(DB*, const DBT* val1, const DBT* val2)
{
    Int2 v1, v2;
    v1 = CByteSwap::GetInt2((unsigned char*)val1->data);
    v2 = CByteSwap::GetInt2((unsigned char*)val2->data);
    return (v1 < v2) ? -1
                     : ((v2 < v1) ? 1 : 0);
}


int BDB_ByteSwap_FloatCompare(DB*, const DBT* val1, const DBT* val2)
{
    float v1, v2;
    v1 = CByteSwap::GetFloat((unsigned char*)val1->data);
    v2 = CByteSwap::GetFloat((unsigned char*)val2->data);
    return (v1 < v2) ? -1
                     : ((v2 < v1) ? 1 : 0);
}


int BDB_ByteSwap_DoubleCompare(DB*, const DBT* val1, const DBT* val2)
{
    double v1, v2;
    v1 = CByteSwap::GetDouble((unsigned char*)val1->data);
    v2 = CByteSwap::GetDouble((unsigned char*)val2->data);
    return (v1 < v2) ? -1
                     : ((v2 < v1) ? 1 : 0);

}




} // extern "C"

/////////////////////////////////////////////////////////////////////////////
//  IBDB_Field::
//

IBDB_Field::~IBDB_Field() 
{
}

/////////////////////////////////////////////////////////////////////////////
//  CBDB_Field::
//

CBDB_Field::CBDB_Field(ELengthType length_type)
: m_BufferManager(0),
  m_Buffer(0),
  m_BufferSize(0),
  m_BufferIdx(0)
{   
    m_Flags.VariableLength = (length_type == eFixedLength) ? 0 : 1;
    m_Flags.Attached = 0;
    m_Flags.Nullable = 0;
}

BDB_CompareFunction 
CBDB_Field::GetCompareFunction(bool /*byte_swapped*/) const
{ 
    return BDB_Compare; 
}


/////////////////////////////////////////////////////////////////////////////
//  CBDB_BufferManager::
//


CBDB_BufferManager::CBDB_BufferManager()
  : m_Buffer(0),
    m_BufferSize(0),
    m_PackedSize(0),
    m_DBT_Size(0),
    m_Packable(false),
    m_ByteSwapped(false),
    m_Nullable(false),
    m_NullSetSize(0),
    m_CompareLimit(0),
    m_LegacyString(false),
	m_OwnFields(false),
    m_PackOptComputed(false),
    m_FirstVarFieldIdx(0),
    m_FirstVarFieldIdxOffs(0)
{
}

CBDB_BufferManager::~CBDB_BufferManager()
{
    delete [] m_Buffer;
	
	if (m_OwnFields) {
    	for (size_t i = 0;  i < m_Fields.size(); ++i) {
        	CBDB_Field* field = m_Fields[i];
			delete field;
    	}
	}
}

void CBDB_BufferManager::Bind(CBDB_Field* field, ENullable is_nullable)
{
    m_Fields.push_back(field);
    m_Ptrs.push_back(0);

    unsigned field_idx = (unsigned)(m_Fields.size() - 1);
    field->SetBufferIdx(field_idx);

    if ( !m_Packable ) {
        // If we bind any var length field, then record becomes packable
        m_Packable = field->IsVariableLength();
    }

    if (is_nullable == eNullable)
        field->SetNullable();
}

int CBDB_BufferManager::GetFieldIndex(const string& name) const
{
    for (size_t i = 0;  i < m_Fields.size();  ++i) {
        const CBDB_Field& field = *m_Fields[i];
        const string& fname = field.GetName();
        if (NStr::CompareNocase(name, fname) == 0) {
            return i;
        }
    }
    return -1;
}

size_t CBDB_BufferManager::ComputeBufferSize() const
{
    size_t buf_len = 0;
    for (size_t i = 0;  i < m_Fields.size();  ++i) {
        const CBDB_Field& field = *m_Fields[i];
        buf_len += field.GetBufferSize();
    }
    return buf_len;
}


void CBDB_BufferManager::CheckNullConstraint() const
{
    if ( !IsNullable() )
        return;

    for (size_t i = 0;  i < m_Fields.size();  ++i) {
        const CBDB_Field& fld = *m_Fields[i];
        if (!fld.IsNullable()  &&  TestNullBit((unsigned)i)) {
            string message("NULL field in database operation.");
            const string& field_name = fld.GetName();
            if ( !field_name.empty() ) {
                message.append("(Field:");
                message.append(field_name);
                message.append(")");
            }
            BDB_THROW(eNull, message);
        }
    }
}


void CBDB_BufferManager::Construct()
{
    _ASSERT(m_Fields.size());

    // Buffer construction: fields size calculation.
    m_BufferSize = ComputeBufferSize();

    if ( IsNullable() ) {
        m_NullSetSize = ComputeNullSetSize();
        m_BufferSize += m_NullSetSize;
    }

    delete [] m_Buffer; m_Buffer = 0;
    m_Buffer = new char[m_BufferSize];
    ::memset(m_Buffer, 0, m_BufferSize);

    // Record construction: set element offsets(pointers)
    char*  buf_ptr = (char*) m_Buffer;
    buf_ptr += m_NullSetSize;

    for (size_t i = 0;  i < m_Fields.size();  ++i) {
        CBDB_Field& df = *m_Fields[i];
        m_Ptrs[i] = buf_ptr;

        df.SetBufferManager(this);
        df.SetBuffer(buf_ptr);

        buf_ptr += df.GetBufferSize();
    }

    m_PackedSize = 0;  // not packed
}


BDB_CompareFunction 
CBDB_BufferManager::GetCompareFunction() const
{
    if (m_Fields.size() > 1)
        return BDB_Compare;
    bool byte_swapped = IsByteSwapped();
    return m_Fields[0]->GetCompareFunction(byte_swapped);
}


void CBDB_BufferManager::ArrangePtrsPacked()
{
    _ASSERT(m_Fields.size());

    if ( !IsPackable() ) {
        m_PackedSize = m_BufferSize;
        return;
    }

    char* buf_ptr = m_Buffer;
    buf_ptr += m_NullSetSize;
    m_PackedSize = m_NullSetSize;

    for (size_t i = 0;  i < m_Fields.size();  ++i) {
        CBDB_Field& df = *m_Fields[i];
        df.SetBuffer(buf_ptr);
        size_t len = df.GetDataLength(buf_ptr);
        buf_ptr += len;
        m_PackedSize += len;
    }
}

void CBDB_BufferManager::x_ComputePackOpt()
{
    unsigned int offset = m_NullSetSize;

    for (TFieldVector::size_type i = 0;  i < m_Fields.size();  ++i) {
        CBDB_Field& df = *m_Fields[i];

        if (df.IsVariableLength()) {
            m_FirstVarFieldIdx = i;
            break;
        }

        size_t actual_len = df.GetLength();
        offset += actual_len;
    } // for

    m_FirstVarFieldIdxOffs = offset;
    m_PackOptComputed = true;
}


unsigned int CBDB_BufferManager::Pack()
{
    _ASSERT(m_Fields.size());
    if (m_PackedSize != 0)
        return (unsigned)m_PackedSize;
    if ( !IsPackable() ) {
        m_PackedSize = m_BufferSize;
        return (unsigned)m_PackedSize;
    }

    if (!m_PackOptComputed) {
        x_ComputePackOpt();
    }

    char* new_ptr = m_Buffer;
    new_ptr += m_FirstVarFieldIdxOffs; 
    m_PackedSize = m_FirstVarFieldIdxOffs; 

    for (size_t i = m_FirstVarFieldIdx;  i < m_Fields.size();  ++i) {
        CBDB_Field& df = *m_Fields[i];
        size_t actual_len = df.GetLength();
        void* old_ptr = m_Ptrs[i];

        if (new_ptr != old_ptr) {
            ::memmove(new_ptr, old_ptr, actual_len);
            df.SetBuffer(new_ptr);
        }

        if ( m_NullSetSize ) {
            if (df.IsVariableLength()  &&  TestNullBit((unsigned)i)) {
                actual_len = 1;
                *new_ptr = '\0'; // for string it will guarantee it is empty
            }
        }

        new_ptr      += actual_len;
        m_PackedSize += actual_len;
    }

    return (unsigned)m_PackedSize;
}


unsigned int CBDB_BufferManager::Unpack()
{
    _ASSERT(m_Fields.size());
    if (m_PackedSize == 0)
        return (unsigned)m_BufferSize;
    if ( !IsPackable() ) {
        m_PackedSize = 0;
        return (unsigned)m_PackedSize;
    }
    if (!m_PackOptComputed) {
        x_ComputePackOpt();
    }

    _ASSERT(!m_Fields.empty());
    for (size_t i = m_Fields.size() - 1;  true;  --i) {
        CBDB_Field& df = *m_Fields[i];
        size_t actual_len = df.GetLength();
        void* new_ptr = m_Ptrs[i];
        const void* old_ptr = df.GetBuffer();
        if (new_ptr != old_ptr) {
            ::memmove(new_ptr, old_ptr, actual_len);
            df.SetBuffer(new_ptr);
        }
        m_PackedSize -= actual_len;

        if (i == m_FirstVarFieldIdx) {
            m_PackedSize -= m_FirstVarFieldIdxOffs;
            break;
        }
    }


    _ASSERT(m_PackedSize == 0);
    return (unsigned)m_BufferSize;
}

void CBDB_BufferManager::PrepareDBT_ForWrite(DBT* dbt)
{
    Pack();
    dbt->data = m_Buffer;
    dbt->size = (unsigned)m_PackedSize;
}

void CBDB_BufferManager::PrepareDBT_ForRead(DBT* dbt)
{
    dbt->data = m_Buffer;
    dbt->size = dbt->ulen = (unsigned)m_BufferSize;
    dbt->flags = DB_DBT_USERMEM;
}

int CBDB_BufferManager::Compare(const CBDB_BufferManager& buf_mgr,
                                unsigned int              field_count)
    const
{
    if ( !field_count ) {
        field_count = FieldCount();
    }
    _ASSERT(field_count <= FieldCount());

    for (unsigned int i = 0;  i < field_count;  ++i) {
        const CBDB_Field& df1 = GetField(i);
        const CBDB_Field& df2 = buf_mgr.GetField(i);

        int ret = df1.CompareWith(df2);
        if ( ret )
            return ret;
    }

    return 0;
}


void CBDB_BufferManager::DuplicateStructureFrom(const CBDB_BufferManager& buf_mgr)
{
    _ASSERT(FieldCount() == 0);
    for (unsigned int i = 0;  i < buf_mgr.FieldCount();  ++i) {
        const CBDB_Field& src_fld = buf_mgr.GetField(i);
		
		// "0" value selects the same buffer size as in src_fld
        auto_ptr<CBDB_Field> dst_fld(src_fld.Construct(0));
		
        dst_fld->SetName(src_fld.GetName().c_str());
        Bind(dst_fld.get());
        dst_fld.release();
    }
    m_LegacyString = buf_mgr.IsLegacyStrings();
}


void CBDB_BufferManager::CopyFrom(const CBDB_BufferManager& buf_mgr)
{
	CopyFieldsFrom(buf_mgr);
}

/////////////////////////////////////////////////////////////////////////////
//  CBDB_FieldLString::
//

CBDB_FieldLString::CBDB_FieldLString() 
 : CBDB_FieldStringBase()
{
    SetBufferSize(256 + 4);
}

CBDB_Field* CBDB_FieldLString::Construct(size_t buf_size) const
{
    CBDB_FieldLString* fld = new CBDB_FieldLString();
    fld->SetBufferSize(buf_size ? buf_size : GetBufferSize());
    return fld;
}


const unsigned char* 
CBDB_FieldLString::GetLString(const unsigned char* str,
                              bool                 check_legacy,
                              int*                 str_len) const
{
    size_t DBT_size = m_BufferManager->GetDBT_Size();

    if (DBT_size > 0 && DBT_size <= 4) {  // looks like legacy C-string
        _ASSERT(check_legacy);
        *str_len = (int)::strlen((const char*)str);
    } else {
        str = s_GetLString(str, check_legacy, str_len);
    }
    return str;
}

/*
CBDB_FieldLString::operator const char* () const
{
    const unsigned char* str = (const unsigned char*)GetBuffer();
    bool check_legacy = m_BufferManager->IsLegacyStrings();

    int str_len;
    size_t DBT_size = m_BufferManager->GetDBT_Size();

    if (DBT_size <= 4) {  // looks like legacy C-string
        _ASSERT(check_legacy);
        str_len = ::strlen((const char*)str);
    } else {
        str = GetLString(str, check_legacy, &str_len);
    }
    return (const char*) str;
}
*/

const CBDB_FieldLString& 
CBDB_FieldLString::operator=(const CBDB_FieldLString& str)
{
    void* buf = GetBuffer();
    if (this == &str)
        return *this;

    size_t len = str.GetDataLength(buf);
    if ( len > (GetBufferSize() - 4) ) {
        // TODO: allow partial string assignment?
        BDB_THROW(eOverflow, "String field overflow.");
    }
    Unpack();
    ::memcpy(buf, str.GetBuffer(), len);

    if ( str.IsNull() ) {
        SetNull();
    } else {
        SetNotNull();
    }

    return *this;
}


void CBDB_FieldLString::Set(const char* str, EOverflowAction if_overflow)
{
    if ( !str )
        str = kEmptyCStr;

    unsigned int new_len = ::strlen(str) + 1;

    // check overflow
    if ( new_len > (GetBufferSize() - 4) ) {
        if (if_overflow == eTruncateOnOverflow) {
            new_len = GetBufferSize();
        } else {
            string message("String field overflow."); 
            // TODO: add info what caused overflow, what length expected
            BDB_THROW(eOverflow, message);
        }
    }
    Unpack();
    unsigned char* str_buf = (unsigned char*) GetBuffer();

    int s_len = -(int)new_len;  // always store it negative
    str_buf[0] = (unsigned char) (s_len);
    str_buf[1] = (unsigned char) (s_len >> 8);
    str_buf[2] = (unsigned char) (s_len >> 16);
    str_buf[3] = (unsigned char) (s_len >> 24);

    str_buf += 4;

    ::memcpy(str_buf, str, new_len);

    SetNotNull();
}

void CBDB_FieldLString::SetString(const char* str)
{
    operator=(str);
}


int CBDB_FieldLString::Compare(const void* p1, 
                               const void* p2, 
                               bool /*byte_swapped*/) const
{
    _ASSERT(p1 && p2);

    bool check_legacy = m_BufferManager->IsLegacyStrings();
    
    const unsigned char* str1;
    const unsigned char* str2;
    int str_len1; 
    int str_len2;

    str1 = GetLString((const unsigned char*)p1, check_legacy, &str_len1);
    str2 = GetLString((const unsigned char*)p2, check_legacy, &str_len2); 

    int cmp_len = min(str_len1, str_len2);
    int r = ::memcmp(str1, str2, cmp_len);
    if (r == 0) {
        return (str_len1 < str_len2) ? -1
                                     : ((str_len2 < str_len1) ? 1 : 0);
    }
    return r;
}


void CBDB_FieldLString::SetMinVal()
{
    Set("", eTruncateOnOverflow);
}


void CBDB_FieldLString::SetMaxVal()
{
    void* buf = Unpack();
    int buf_size = GetBufferSize();

    ::memset(buf, 0x7F, buf_size); // 0xFF for international
    ((char*) buf)[buf_size - 1] = '\0';

    int s_len = -(buf_size - 4);  // always store it negative
    unsigned char* str_buf = (unsigned char*) buf;

    str_buf[0] = (unsigned char) (s_len);
    str_buf[1] = (unsigned char) (s_len >> 8);
    str_buf[2] = (unsigned char) (s_len >> 16);
    str_buf[3] = (unsigned char) (s_len >> 24);

    SetNotNull();
}


bool CBDB_FieldLString::IsEmpty() const
{
    const unsigned char* str = (const unsigned char*) GetBuffer();
    bool check_legacy = m_BufferManager->IsLegacyStrings();
    int str_len;

    str = GetLString(str, check_legacy, &str_len);

    return (str_len == 0);
}

bool CBDB_FieldLString::IsBlank() const
{
    const unsigned char* str = (const unsigned char*) GetBuffer();
    bool check_legacy = m_BufferManager->IsLegacyStrings();
    int str_len;

    str = GetLString(str, check_legacy, &str_len);

    for (int i = 0; i < str_len; ++i) {
        if (!isspace(str[i]))
            return false;
    }

    return true;
}


size_t CBDB_FieldLString::GetDataLength(const void* buf) const
{
    const unsigned char* str = (const unsigned char*) buf;
    bool check_legacy = m_BufferManager->IsLegacyStrings();
    int str_len;

    str = GetLString(str, check_legacy, &str_len);
    if (str != (const unsigned char*) buf)
        str_len += 4;
    return str_len + 1;
}

const CBDB_FieldLString& CBDB_FieldLString::operator= (const char* str)
{ 
    Set(str, eThrowOnOverflow); 
    return *this;
}

const CBDB_FieldLString& CBDB_FieldLString::operator= (const string& str)
{
    SetStdString(str);
    return *this;
}


string CBDB_FieldLString::Get() const
{
    _ASSERT(!IsNull());
	
    const unsigned char* buf = (const unsigned char*) GetBuffer();
    bool check_legacy = m_BufferManager->IsLegacyStrings();
    
    const unsigned char* str;
    int str_len; 

    str = GetLString(buf, check_legacy, &str_len);
    if (str_len == 0) {
        return kEmptyStr;
    } 
    string ret((const char*) str, str_len);
    return ret;
}

void CBDB_FieldLString::ToString(string& ostr) const
{
    const unsigned char* buf = (const unsigned char*) GetBuffer();
    bool check_legacy = m_BufferManager->IsLegacyStrings();
    
    const unsigned char* str;
    int str_len;

    ostr.resize(0);

    str = GetLString(buf, check_legacy, &str_len);
    if (str_len == 0) {
        return;
    }
    ostr.append((const char*) str, str_len);
}

void CBDB_FieldLString::SetStdString(const string& str)
{
    unsigned int str_len = str.length();
    if (str_len == 0) {
        Set("", eThrowOnOverflow);
        return;
    }

    // check overflow
    if (str_len > (GetBufferSize() - 4 - 1)) {
        string message("String field overflow."); 
        // TODO: add info what caused overflow, what length expected
        BDB_THROW(eOverflow, message);
    }

    const char* str_data = str.data();

    /* void* buf = */ Unpack();

    unsigned char* str_buf = (unsigned char*) GetBuffer();

    int s_len = -(int)str_len;  // always store it negative
    str_buf[0] = (unsigned char) (s_len);
    str_buf[1] = (unsigned char) (s_len >> 8);
    str_buf[2] = (unsigned char) (s_len >> 16);
    str_buf[3] = (unsigned char) (s_len >> 24);

    str_buf += 4;

    ::memcpy(str_buf, str_data, str_len);

    str_buf[str_len] = 0;

    SetNotNull();

}

/////////////////////////////////////////////////////////////////////////////
//  CBDB_FieldFactory::
//

CBDB_FieldFactory::CBDB_FieldFactory()
{}

CBDB_FieldFactory::EType CBDB_FieldFactory::GetType(const string& type) const
{
	if (NStr::CompareNocase(type, "string")==0) {
		return eString;
	} else 
	if (NStr::CompareNocase(type, "lstring")==0) {
		return eLString;
	} else 
	if (NStr::CompareNocase(type, "int8")==0) {
		return eInt8;		
	} else 
	if (NStr::CompareNocase(type, "int4")==0) {
		return eInt4;		
	} else 
	if (NStr::CompareNocase(type, "uint4")==0) {
		return eUint4;
	} else 
	if (NStr::CompareNocase(type, "int2")==0) {
		return eInt2;
	} else 
	if (NStr::CompareNocase(type, "uint1")==0) {
		return eUint1;
	} else 
	if (NStr::CompareNocase(type, "float")==0) {
		return eFloat;
	} else 
	if (NStr::CompareNocase(type, "double")==0) {
		return eDouble;
	} else 
	if (NStr::CompareNocase(type, "uchar")==0) {
		return eUChar;
	} else 
	if (NStr::CompareNocase(type, "blob")==0) {
		return eBlob;
	} else 
	if (NStr::CompareNocase(type, "lob")==0) {
		return eBlob;
	} else {
		return eUnknown;
	}
}		

CBDB_Field* CBDB_FieldFactory::Create(EType etype) const
{
	switch (etype) 
	{
	case eString:
		return new CBDB_FieldString();
	case eLString:
		return new CBDB_FieldLString();
	case eInt8:
		return new CBDB_FieldInt8();
	case eInt4:
		return new CBDB_FieldInt4();
	case eInt2:
		return new CBDB_FieldInt2();
	case eUint1:
		return new CBDB_FieldUint1();
	case eFloat:
		return 	new CBDB_FieldFloat();
	case eDouble:
		return new CBDB_FieldDouble();
	case eUChar:
		return new CBDB_FieldUChar();
	default:
		BDB_THROW(eInvalidType, "Type is not supported.");			
	};
	
	return 0;
}

CBDB_Field* CBDB_FieldFactory::Create(const string& type) const
{
	EType et = GetType(type);	
	try 
	{
		return Create(et);
	} 
	catch (CBDB_LibException& )
	{
		BDB_THROW(eInvalidType, type);
	}
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.33  2005/03/15 14:46:45  kuznets
 * Optimization in record packing
 *
 * Revision 1.32  2004/09/02 15:37:02  rotmistr
 * Fixed name for UInt1 to Uint1
 *
 * Revision 1.31  2004/07/09 15:14:49  kuznets
 * Fixed warnings (MSVC 7)
 *
 * Revision 1.30  2004/06/29 12:25:40  kuznets
 * CBDB_BufferManager added functions to bulk copy fields
 *
 * Revision 1.29  2004/06/28 12:14:53  kuznets
 * Added uint1 to the types class factory
 *
 * Revision 1.28  2004/06/24 20:52:25  rotmistr
 * Added Int8 and UInt1 field types.
 *
 * Revision 1.27  2004/06/24 19:25:11  kuznets
 * Added ASSERT when somebody gets a NULL field
 *
 * Revision 1.26  2004/06/21 15:06:45  kuznets
 * Added BLOB (eBlob) to the list of types
 *
 * Revision 1.25  2004/06/17 16:25:50  kuznets
 * + ownership flag to BufferManager
 *
 * Revision 1.24  2004/06/17 13:38:02  kuznets
 * +CBDB_FieldFactory
 *
 * Revision 1.23  2004/05/17 20:55:11  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.22  2004/05/06 15:42:19  rotmistr
 * Changed Char type to UChar
 *
 * Revision 1.21  2004/05/05 19:18:21  rotmistr
 * CBDB_FieldChar added
 *
 * Revision 1.20  2004/03/08 13:30:39  kuznets
 * + ToString
 *
 * Revision 1.19  2004/02/17 19:05:21  kuznets
 * GCC warnings fix
 *
 * Revision 1.18  2004/02/12 19:54:39  kuznets
 * CBDB_BufferManager::GetFieldIndex()
 *
 * Revision 1.17  2004/02/04 17:03:39  kuznets
 * Fixed bug in LString manipulation.
 *
 * Revision 1.16  2003/12/22 18:54:14  kuznets
 * Implemeneted length prefixed string field (CBDB_FieldLString)
 *
 * Revision 1.15  2003/11/06 14:06:02  kuznets
 * Removing auto_ptr from CBDB_BufferManager
 *
 * Revision 1.14  2003/10/16 19:25:38  kuznets
 * Added field comparison limit to the fields manager
 *
 * Revision 1.13  2003/09/29 16:27:06  kuznets
 * Cleaned up 64-bit compilation warnings
 *
 * Revision 1.12  2003/09/17 13:31:12  kuznets
 * Implemented Int2Compare family of functions
 *
 * Revision 1.11  2003/09/11 16:34:35  kuznets
 * Implemented byte-order independence.
 *
 * Revision 1.10  2003/07/25 15:47:15  kuznets
 * Added support for double field type
 *
 * Revision 1.9  2003/07/23 20:21:43  kuznets
 * Implemented new improved scheme for setting BerkeleyDB comparison function.
 * When table has non-segmented key the simplest(and fastest) possible function
 * is assigned (automatically without reloading CBDB_File::SetCmp function).
 *
 * Revision 1.8  2003/07/02 17:55:35  kuznets
 * Implementation modifications to eliminated direct dependency from <db.h>
 *
 * Revision 1.7  2003/06/10 20:08:27  kuznets
 * Fixed function names.
 *
 * Revision 1.6  2003/05/27 18:43:45  kuznets
 * Fixed some compilation problems with GCC 2.95
 *
 * Revision 1.5  2003/05/02 14:12:11  kuznets
 * Bug fix
 *
 * Revision 1.4  2003/04/29 19:07:22  kuznets
 * Cosmetics..
 *
 * Revision 1.3  2003/04/29 19:04:47  kuznets
 * Fixed a bug in buffers management
 *
 * Revision 1.2  2003/04/28 14:51:55  kuznets
 * #include directives changed to conform the NCBI policy
 *
 * Revision 1.1  2003/04/24 16:34:30  kuznets
 * Initial revision
 *
 * ===========================================================================
 */
