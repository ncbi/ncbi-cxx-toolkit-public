#ifndef SRA__READER__SRA__VDBREAD__HPP
#define SRA__READER__SRA__VDBREAD__HPP
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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Access to VDB files
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiobj.hpp>

#include <sra/readers/sra/sraread.hpp>

#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <kfg/config.h>
#include <vfs/manager.h>
#include <vfs/path.h>
#include <vfs/resolver.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

SPECIALIZE_SRA_REF_TRAITS(VDBManager, const);
SPECIALIZE_SRA_REF_TRAITS(VDatabase, const);
SPECIALIZE_SRA_REF_TRAITS(VTable, const);
SPECIALIZE_SRA_REF_TRAITS(VCursor, const);
SPECIALIZE_SRA_REF_TRAITS(KConfig, );
SPECIALIZE_SRA_REF_TRAITS(VFSManager, );
SPECIALIZE_SRA_REF_TRAITS(VPath, );
SPECIALIZE_SRA_REF_TRAITS(VPath, const);
SPECIALIZE_SRA_REF_TRAITS(VResolver, );


class CVFSManager;
class CVPath;
class CVResolver;

class CVDBMgr;
class CVDB;
class CVDBTable;
class CVDBColumn;
class CVDBCursor;
class CVDBStringValue;

class NCBI_SRAREAD_EXPORT CKConfig
    : public CSraRef<KConfig>
{
public:
    CKConfig(void);
    CKConfig(ENull /*null*/)
        {
        }
};


class NCBI_SRAREAD_EXPORT CVFSManager
    : public CSraRef<VFSManager>
{
public:
    CVFSManager(void);
    CVFSManager(ENull /*null*/)
        {
        }
};


class NCBI_SRAREAD_EXPORT CVPath
    : public CSraRef<VPath>
{
public:
    CVPath(const string& path);
};


class NCBI_SRAREAD_EXPORT CVPathRet
    : public CSraRef<const VPath>
{
public:
    CVPathRet(void)
        {
        }
    
    operator string() const;

protected:
    friend class CVResolver;

    TObject** x_InitPtr(void)
        {
            return CSraRef<TObject>::x_InitPtr();
        }
};


class NCBI_SRAREAD_EXPORT CVResolver
    : public CSraRef<VResolver>
{
public:
    CVResolver(const CVFSManager& mgr, const CKConfig& cfg);
    CVResolver(ENull /*null*/)
        {
        }

    string Resolve(const string& acc) const;
};


class NCBI_SRAREAD_EXPORT CVDBMgr
    : public CSraRef<const VDBManager>
{
public:
    CVDBMgr(void);

    string FindAccPath(const string& acc) const;

protected:
    void x_Init(void);

private:
    mutable CVResolver m_Resolver;
};


class NCBI_SRAREAD_EXPORT CVDB
    : public CSraRef<const VDatabase>
{
public:
    CVDB(void)
        {
        }
    CVDB(const CVDBMgr& mgr, const string& acc_or_path)
        {
            Init(mgr, acc_or_path);
        }

    void Init(const CVDBMgr& mgr, const string& acc_or_path);
};


class NCBI_SRAREAD_EXPORT CVDBTable
    : public CSraRef<const VTable>
{
public:
    CVDBTable(void)
        {
        }
    CVDBTable(const CVDB& db, const char* table_name)
        {
            Init(db, table_name);
        }
    CVDBTable(const CVDBMgr& mgr, const string& acc_or_path)
        {
            Init(mgr, acc_or_path);
        }

    void Init(const CVDB& db, const char* table_name);
    void Init(const CVDBMgr& mgr, const string& acc_or_path);
};


class NCBI_SRAREAD_EXPORT CVDBCursor
    : public CSraRef<const VCursor>
{
public:
    CVDBCursor(const CVDBTable& table)
        : m_RowOpened(false)
        {
            Init(table);
        }

    bool RowIsOpened(void) const
        {
            return m_RowOpened;
        }
    rc_t OpenRowRc(uint64_t row_id);
    void OpenRow(uint64_t row_id);
    bool TryOpenRow(uint64_t row_id)
        {
            return OpenRowRc(row_id) == 0;
        }
    void CloseRow(void);

    // returns first id, and count of ids in the range
    pair<int64_t, uint64_t> GetRowIdRange(uint32_t index = 0) const;

    uint64_t GetMaxRowId(void) const;

    void SetParam(const char* name, const CTempString& value) const;

protected:
    void Init(const CVDBTable& table);

private:
    bool m_RowOpened;
};


class NCBI_SRAREAD_EXPORT CVDBObjectCacheBase
{
public:
    CVDBObjectCacheBase(void);
    ~CVDBObjectCacheBase(void);

protected:
    CRef<CObject> Get(void);
    void Put(CRef<CObject>& curs);

private:
    typedef vector< CRef<CObject> > TObjects;
    TObjects m_Objects;

private:
    CVDBObjectCacheBase(const CVDBObjectCacheBase&);
    void operator=(const CVDBObjectCacheBase&);
};


template<class Object>
class CVDBObjectCache : public CVDBObjectCacheBase
{
public:
    CRef<Object> Get(void) {
        CRef<Object> obj;
        CRef<CObject> obj2 = CVDBObjectCacheBase::Get();
        obj = static_cast<Object*>(obj2.GetPointerOrNull());
        return obj;
    }
    void Put(CRef<Object>& obj) {
        CRef<CObject> obj2(obj.GetPointer());
        CVDBObjectCacheBase::Put(obj2);
    }
};


class NCBI_SRAREAD_EXPORT CVDBColumn
{
public:
    enum EMissing {
        eMissingDisallow,
        eMissingAllow
    };

    CVDBColumn(const CVDBCursor& cursor,
               const char* name,
               EMissing missing)
        {
            Init(cursor, 0, name, 0, missing);
        }
    CVDBColumn(const CVDBCursor& cursor,
               const char* name,
               const char* backup_name = 0,
               EMissing missing = eMissingDisallow)
        {
            Init(cursor, 0, name, backup_name, missing);
        }
    CVDBColumn(const CVDBCursor& cursor,
               size_t element_bit_size,
               const char* name,
               const char* backup_name = 0,
               EMissing missing = eMissingDisallow)
        {
            Init(cursor, element_bit_size, name, backup_name, missing);
        }
    
    enum {
        kInvalidIndex = uint32_t(~0)
    };
    DECLARE_SAFE_BOOL_METHOD(m_Index != kInvalidIndex);

    uint32_t GetIndex(void) const
        {
            return m_Index;
        }

    // returns first id, and count of ids in the range
    pair<int64_t, uint64_t> GetRowIdRange(CVDBCursor& cursor) const
        {
            return cursor.GetRowIdRange(GetIndex());
        }

protected:
    void Init(const CVDBCursor& cursor,
              size_t element_bit_size,
              const char* name,
              const char* backup_name,
              EMissing missing);

private:
    uint32_t m_Index;
};


template<size_t ElementBitSize>
class CVDBColumnBits : public CVDBColumn
{
public:
    CVDBColumnBits(const CVDBCursor& cursor,
                   const char* name,
                   CVDBColumn::EMissing missing)
        : CVDBColumn(cursor, ElementBitSize, name, 0, missing)
        {
        }
    CVDBColumnBits(const CVDBCursor& cursor,
                   const char* name,
                   const char* backup_name = 0,
                   CVDBColumn::EMissing missing = CVDBColumn::eMissingDisallow)
        : CVDBColumn(cursor, ElementBitSize, name, backup_name, missing)
        {
        }
};


// DECLARE_VDB_COLUMN is helper macro to declare accessor to VDB column
#define DECLARE_VDB_COLUMN(name)                                        \
    CVDBValue::SValueIndex name(uint64_t row) const {                   \
        return CVDBValue::SValueIndex(m_Cursor, row, NCBI_NAME2(m_,name)); \
    }                                                                   \
    CVDBColumn NCBI_NAME2(m_, name)

// DECLARE_VDB_COLUMN is helper macro to declare accessor to VDB column
#define DECLARE_VDB_COLUMN_AS(type, name)                               \
    CVDBValueFor<type> name(uint64_t row) const {                       \
        return CVDBValueFor<type>(m_Cursor, row, NCBI_NAME2(m_,name));  \
    }                                                                   \
    CVDBColumnBits<sizeof(type)*8> NCBI_NAME2(m_, name)

// DECLARE_VDB_COLUMN is helper macro to declare accessor to VDB column
#define DECLARE_VDB_COLUMN_AS_STRING(name)                              \
    CVDBStringValue name(uint64_t row) const {                          \
        return CVDBStringValue(m_Cursor, row, NCBI_NAME2(m_,name));     \
    }                                                                   \
    CVDBColumnBits<8> NCBI_NAME2(m_, name)

// DECLARE_VDB_COLUMN is helper macro to declare accessor to VDB column
#define DECLARE_VDB_COLUMN_AS_4BITS(name)                               \
    CVDBValueFor4Bits name(uint64_t row) const {                        \
        return CVDBValueFor4Bits(m_Cursor, row, NCBI_NAME2(m_,name));   \
    }                                                                   \
    CVDBColumnBits<4> NCBI_NAME2(m_, name)

// INIT_VDB_COLUMN* are helper macros to initialize VDB column accessors
#define INIT_VDB_COLUMN(name)                   \
    NCBI_NAME2(m_, name)(m_Cursor, #name)
#define INIT_VDB_COLUMN_BACKUP(name, backup_name)       \
    NCBI_NAME2(m_, name)(m_Cursor, #name, #backup_name)
#define INIT_VDB_COLUMN_AS(name, type)                  \
    NCBI_NAME2(m_, name)(m_Cursor, "("#type")"#name)
#define INIT_OPTIONAL_VDB_COLUMN(name)                                  \
    NCBI_NAME2(m_, name)(m_Cursor, #name, CVDBColumn::eMissingAllow)
#define INIT_OPTIONAL_VDB_COLUMN_BACKUP(name, backup_name)              \
    NCBI_NAME2(m_, name)(m_Cursor, #name, #backup_name, CVDBColumn::eMissingAllow)
#define INIT_OPTIONAL_VDB_COLUMN_AS(name, type)                         \
    NCBI_NAME2(m_, name)(m_Cursor, "("#type")"#name, CVDBColumn::eMissingAllow)


class NCBI_SRAREAD_EXPORT CVDBValue
{
public:
    struct SValueIndex {
        SValueIndex(const VCursor* cursor, uint64_t row, uint32_t column)
            : cursor(cursor), row(row), column(column)
            {
            }
        SValueIndex(const CVDBCursor& cursor,
                    uint64_t row,
                    const CVDBColumn& column)
            : cursor(cursor), row(row), column(column.GetIndex())
            {
            }
        
        const VCursor* cursor;
        uint64_t row;
        uint32_t column;
    };
    CVDBValue(const CVDBCursor& cursor,
              const CVDBColumn& column)
        : m_Data(0),
          m_ElemCount(0)
        {
            x_Get(cursor, column.GetIndex());
        }
    CVDBValue(const CVDBCursor& cursor, uint64_t row,
              const CVDBColumn& column)
        : m_Data(0),
          m_ElemCount(0)
        {
            x_Get(cursor, row, column.GetIndex());
        }
    CVDBValue(const SValueIndex& value_index)
        : m_Data(0),
          m_ElemCount(0)
        {
            x_Get(value_index.cursor, value_index.row, value_index.column);
        }
    CVDBValue(const CVDBCursor& cursor,
              const char* param_name, const CTempString& param_value,
              const CVDBColumn& column)
        : m_Data(0),
          m_ElemCount(0)
        {
            cursor.SetParam(param_name, param_value);
            x_Get(cursor, column.GetIndex());
        }

    size_t size(void) const
        {
            return m_ElemCount;
        }

protected:
    void x_Get(const VCursor* cursor, uint32_t column);
    void x_Get(const VCursor* cursor, uint64_t row, uint32_t column);

    void x_ReportIndexOutOfBounds(size_t index) const;
    void x_CheckIndex(size_t index) const
        {
            if ( index >= size() ) {
                x_ReportIndexOutOfBounds(index);
            }
        }

    const void* m_Data;
    uint32_t m_ElemCount;
};


class NCBI_SRAREAD_EXPORT CVDBValueFor4Bits
{
public:
    typedef unsigned TValue;

    CVDBValueFor4Bits(const CVDBValue::SValueIndex& value_index)
        : m_RawData(0),
          m_RawElemCount(0),
          m_ElemOffset(0)
        {
            x_Get(value_index.cursor, value_index.row, value_index.column);
        }
    CVDBValueFor4Bits(const CVDBCursor& cursor, uint64_t row,
                      const CVDBColumn& column)
        : m_RawData(0),
          m_RawElemCount(0),
          m_ElemOffset(0)
        {
            x_Get(cursor, row, column.GetIndex());
        }

    uint32_t raw_size(void) const
        {
            return m_RawElemCount;
        }
    const char* raw_data(void) const
        {
            return m_RawData;
        }
    uint32_t offset(void) const
        {
            return m_ElemOffset;
        }

    uint32_t size(void) const
        {
            return raw_size()-offset();
        }

    static TValue sub_value(uint8_t v, size_t sub_index)
        {
            return sub_index? (v&0xf): (v>>4);
        }
    TValue ValueByRawIndex(size_t raw_index) const
        {
            x_CheckRawIndex(raw_index);
            return sub_value(raw_data()[raw_index/2], raw_index%2);
        }
    TValue Value(size_t index) const
        {
            return ValueByRawIndex(index+offset());
        }

protected:
    void x_Get(const VCursor* cursor, uint64_t row, uint32_t column);

    void x_ReportRawIndexOutOfBounds(size_t index) const;
    void x_CheckRawIndex(size_t raw_index) const
        {
            if ( raw_index >= raw_size() ) {
                x_ReportRawIndexOutOfBounds(raw_index);
            }
        }

    const char* m_RawData;
    uint32_t m_RawElemCount;
    uint32_t m_ElemOffset;
};


template<class V>
class CVDBValueFor : public CVDBValue
{
public:
    typedef V TValue;
    CVDBValueFor(const CVDBCursor& cursor,
                 const CVDBColumn& column)
        : CVDBValue(cursor, column)
        {
        }
    CVDBValueFor(const CVDBCursor& cursor, uint64_t row,
                 const CVDBColumn& column)
        : CVDBValue(cursor, row, column)
        {
        }
    CVDBValueFor(const CVDBValue::SValueIndex& value_index)
        : CVDBValue(value_index)
        {
        }
    CVDBValueFor(CVDBCursor& cursor,
                 const char* param_name, const CTempString& param_value,
                 const CVDBColumn& column)
        : CVDBValue(cursor, param_name, param_value, column)
        {
        }

    const TValue* data() const
        {
            return static_cast<const TValue*>(m_Data);
        }
    const TValue& operator[](size_t i) const
        {
            x_CheckIndex(i);
            return data()[i];
        }
    const TValue& Value(void) const
        {
            return (*this)[0];
        }
    operator const TValue&(void) const
        {
            return Value();
        }
    const TValue& operator*(void) const
        {
            return Value();
        }
    const TValue* operator->(void) const
        {
            return &Value();
        }
};


class CVDBStringValue : public CVDBValueFor<char>
{
public:
    CVDBStringValue(const CVDBCursor& cursor,
                    const CVDBColumn& column)
        : CVDBValueFor<char>(cursor, column)
        {
        }
    CVDBStringValue(const CVDBCursor& cursor, uint64_t row,
                    const CVDBColumn& column)
        : CVDBValueFor<char>(cursor, row, column)
        {
        }
    CVDBStringValue(const CVDBValue::SValueIndex& value_index)
        : CVDBValueFor<char>(value_index)
        {
        }
    CVDBStringValue(CVDBCursor& cursor,
                    const char* param_name, const CTempString& param_value,
                    const CVDBColumn& column)
        : CVDBValueFor<char>(cursor, param_name, param_value, column)
        {
        }

    const char* data(void) const
        {
            return static_cast<const char*>(m_Data);
        }

    operator CTempString(void) const
        {
            return CTempString(data(), size());
        }
    operator string(void) const
        {
            return Value();
        }
    string Value(void) const
        {
            return string(data(), size());
        }

    CTempString operator*(void) const
        {
            return *this;
        }
};


typedef CVDBValueFor<uint16_t> CVDBUInt16Value;
typedef CVDBValueFor<char> CVDBBytesValue;


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // SRA__READER__SRA__VDBREAD__HPP
