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
#include <sra/readers/sra/exception.hpp>
#include <sra/readers/sra/sdk.hpp>
#include <vector>

// for String
#include <klib/text.h>

// SRA SDK structures
struct KConfig;
struct KDBManager;
struct KNSManager;
struct VFSManager;
struct VDBManager;
struct VPath;
struct VResolver;
struct VDatabase;
struct VTable;
struct VCursor;
struct KIndex;

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

DECLARE_SRA_REF_TRAITS(VDBManager, const);
DECLARE_SRA_REF_TRAITS(VDatabase, const);
DECLARE_SRA_REF_TRAITS(VTable, const);
DECLARE_SRA_REF_TRAITS(VCursor, const);
DECLARE_SRA_REF_TRAITS(KIndex, const);
DECLARE_SRA_REF_TRAITS(KConfig, const);
DECLARE_SRA_REF_TRAITS(KDBManager, const);
DECLARE_SRA_REF_TRAITS(KNSManager, );
DECLARE_SRA_REF_TRAITS(VFSManager, );
DECLARE_SRA_REF_TRAITS(VPath, );
DECLARE_SRA_REF_TRAITS(VPath, const); // for constant path in CVPathRet
DECLARE_SRA_REF_TRAITS(VResolver, );


class CVFSManager;
class CVPath;
class CVResolver;

class CVDBMgr;
class CVDB;
class CVDBTable;
class CVDBColumn;
class CVDBCursor;
class CVDBStringValue;

typedef int64_t TVDBRowId;
typedef uint64_t TVDBRowCount;
typedef pair<TVDBRowId, TVDBRowCount> TVDBRowIdRange;
typedef uint32_t TVDBColumnIdx;

class NCBI_SRAREAD_EXPORT CKConfig
    : public CSraRef<const KConfig>
{
public:
    NCBI_DEPRECATED_CTOR(CKConfig(void));
    CKConfig(const CVDBMgr& mgr);
    explicit CKConfig(ENull /*null*/)
        {
        }
};


class NCBI_SRAREAD_EXPORT CKDBManager
    : public CSraRef<const KDBManager>
{
public:
    CKDBManager(const CVDBMgr& mgr);
    explicit CKDBManager(ENull /*null*/)
        {
        }
};


class NCBI_SRAREAD_EXPORT CKNSManager
    : public CSraRef<KNSManager>
{
public:
    CKNSManager(const CVFSManager& mgr);
    explicit CKNSManager(ENull /*null*/)
        {
        }
};


class NCBI_SRAREAD_EXPORT CVFSManager
    : public CSraRef<VFSManager>
{
public:
    NCBI_DEPRECATED_CTOR(CVFSManager(void));
    CVFSManager(const CVDBMgr& mgr);
    enum ECreateNew {
        eCreateNew
    };
    CVFSManager(ECreateNew);
    explicit CVFSManager(ENull /*null*/)
        {
        }

private:
    void x_InitNew(void);
};


class NCBI_SRAREAD_EXPORT CVPath
    : public CSraRef<VPath>
{
public:
    CVPath(void)
        {
            m_Str.addr = 0;
            m_Str.size = 0;
        }
    enum EType {
        eSys,
        eAcc
    };
    CVPath(const CVFSManager& mgr, const string& path, EType type = eSys);
    NCBI_DEPRECATED_CTOR(explicit CVPath(const string& path, EType type = eSys));

    const char* data(void) const
        {
            return m_Str.addr;
        }
    size_t size(void) const
        {
            return m_Str.size;
        }
    operator string(void) const
        {
            return string(data(), size());
        }

    // Check if argument string is plain VDB accession,
    // otherwise it's a system path.
    static bool IsPlainAccession(const string& acc_or_path);

    // Convert system path to a POSIX path, accepted by VDB.
    // Note that only MS Windows requires this conversion.
    static string ConvertSysPathToPOSIX(const string& sys_path);

    // Convert system path to a POSIX path, accepted by VDB.
    // Note that only MS Windows requires this conversion.
    // Keep plain VDB accession unchanged.
    static string ConvertAccOrSysPathToPOSIX(const string& acc_or_path);

private:
    void x_Init(const CVFSManager& mgr, const string& path, EType type);

    String m_Str;
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
    CVResolver(const CVFSManager& mgr);
    NCBI_DEPRECATED_CTOR(CVResolver(const CVFSManager& mgr, const CKConfig& cfg));
    explicit CVResolver(ENull /*null*/)
        : m_Mgr(null)
        {
        }

    // Resolve VDB accession to POSIX path.
    // Keep non plain accession string unchanged.
    string Resolve(const string& acc) const;

private:
    CVFSManager m_Mgr;
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
    CVDB(const CVDBMgr& mgr, const string& acc_or_path);

    const string& GetName(void) const
        {
            return m_Name;
        }
    const string& GetFullName(void) const
        {
            return m_Name;
        }
    CNcbiOstream& PrintFullName(CNcbiOstream& out) const;

private:
    string m_Name;
};


class NCBI_SRAREAD_EXPORT CVDBTable
    : public CSraRef<const VTable>
{
public:
    enum EMissing {
        eMissing_Throw,
        eMissing_Allow
    };

    CVDBTable(void)
        {
        }
    CVDBTable(const CVDB& db,
              const char* table_name,
              EMissing missing = eMissing_Throw);
    CVDBTable(const CVDBMgr& mgr,
              const string& acc_or_path,
              EMissing missing = eMissing_Throw);

    const CVDB& GetDb(void) const
        {
            return m_Db;
        }
    const string& GetName(void) const
        {
            return m_Name;
        }
    string GetFullName(void) const;
    CNcbiOstream& PrintFullName(CNcbiOstream& out) const;

public:
    CVDB m_Db;
    string m_Name;
};


class NCBI_SRAREAD_EXPORT CVDBTableIndex
    : public CSraRef<const KIndex>
{
public:
    enum EMissing {
        eMissing_Throw,
        eMissing_Allow
    };

    CVDBTableIndex(void)
        {
        }
    CVDBTableIndex(const CVDBTable& table,
                   const char* index_name,
                   EMissing missing = eMissing_Throw);
    
    const CVDBTable& GetTable(void) const
        {
            return m_Table;
        }
    const char* GetName(void) const
        {
            return m_Name;
        }
    string GetFullName(void) const;
    CNcbiOstream& PrintFullName(CNcbiOstream& out) const;

    TVDBRowIdRange Find(const string& value) const;

private:
    CVDBTable m_Table;
    const char* m_Name;
};


class NCBI_SRAREAD_EXPORT CVDBCursor
    : public CSraRef<const VCursor>
{
public:
    explicit CVDBCursor(const CVDBTable& table)
        : m_RowOpened(false)
        {
            Init(table);
        }

    const CVDBTable& GetTable(void) const
        {
            return m_Table;
        }

    bool RowIsOpened(void) const
        {
            return m_RowOpened;
        }
    rc_t OpenRowRc(TVDBRowId row_id);
    void OpenRow(TVDBRowId row_id);
    bool TryOpenRow(TVDBRowId row_id)
        {
            return OpenRowRc(row_id) == 0;
        }
    void CloseRow(void);

    // returns first id, and count of ids in the range
    TVDBRowIdRange GetRowIdRange(TVDBColumnIdx column = 0) const;

    TVDBRowId GetMaxRowId(void) const;

    void SetParam(const char* name, const CTempString& value) const;

    void ReadElements(TVDBRowId row, const CVDBColumn& column,
                      uint32_t elem_bits,
                      uint32_t start, uint32_t count,
                      void* buffer) const;
    
protected:
    void Init(const CVDBTable& table);

private:
    CVDBTable m_Table;
    bool m_RowOpened;
};


class NCBI_SRAREAD_EXPORT CVDBObjectCacheBase
{
public:
    CVDBObjectCacheBase(void);
    ~CVDBObjectCacheBase(void);

protected:
    CObject* Get(TVDBRowId row);
    void Put(CObject* curs, TVDBRowId row);

private:
    typedef pair<TVDBRowId, CRef<CObject> > TSlot;
    typedef vector<TSlot> TObjects;
    TObjects m_Objects;

private:
    CVDBObjectCacheBase(const CVDBObjectCacheBase&);
    void operator=(const CVDBObjectCacheBase&);
};


template<class Object>
class CVDBObjectCache : public CVDBObjectCacheBase
{
public:
    CRef<Object> Get(TVDBRowId row = 0) {
        Object* obj = static_cast<Object*>(CVDBObjectCacheBase::Get(row));
        return CRef<Object>(obj);
    }
    void Put(CRef<Object>& ref, TVDBRowId row = 0) {
        if ( Object* obj = ref.ReleaseOrNull() ) {
            CVDBObjectCacheBase::Put(obj, row);
        }
    }
};


class NCBI_SRAREAD_EXPORT CVDBColumn
{
public:
    enum EMissing {
        eMissing_Throw,
        eMissing_Allow
    };

    CVDBColumn(const CVDBCursor& cursor,
               const char* name,
               EMissing missing)
        {
            Init(cursor, 0, name, NULL, missing);
        }
    CVDBColumn(const CVDBCursor& cursor,
               const char* name,
               const char* backup_name = NULL,
               EMissing missing = eMissing_Throw)
        {
            Init(cursor, 0, name, backup_name, missing);
        }
    CVDBColumn(const CVDBCursor& cursor,
               size_t element_bit_size,
               const char* name,
               const char* backup_name = NULL,
               EMissing missing = eMissing_Throw)
        {
            Init(cursor, element_bit_size, name, backup_name, missing);
        }

    const char* GetName(void) const
        {
            return m_Name;
        }
    
    enum {
        kInvalidIndex = TVDBColumnIdx(~0)
    };
    DECLARE_OPERATOR_BOOL(m_Index != kInvalidIndex);

    TVDBColumnIdx GetIndex(void) const
        {
            return m_Index;
        }

    // returns first id, and count of ids in the range
    TVDBRowIdRange GetRowIdRange(CVDBCursor& cursor) const
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
    const char* m_Name;
    TVDBColumnIdx m_Index;
};


template<size_t ElementBitSize>
class CVDBColumnBits : public CVDBColumn
{
public:
    CVDBColumnBits(const CVDBCursor& cursor,
                   const char* name,
                   const char* backup_name = NULL,
                   EMissing missing = eMissing_Throw)
        : CVDBColumn(cursor, ElementBitSize, name, backup_name, missing)
        {
        }
};


// DECLARE_VDB_COLUMN is helper macro to declare accessor to VDB column
#define DECLARE_VDB_COLUMN(name)                                        \
    CVDBValue::SRef name(TVDBRowId row) const {                         \
        return CVDBValue::SRef(m_Cursor, row, NCBI_NAME2(m_,name));     \
    }                                                                   \
    CVDBColumn NCBI_NAME2(m_, name)

// DECLARE_VDB_COLUMN is helper macro to declare accessor to VDB column
#define DECLARE_VDB_COLUMN_AS(type, name)                               \
    CVDBValueFor<type> name(TVDBRowId row, CVDBValue::EMissing missing = CVDBValue::eMissing_Throw) const { \
        return CVDBValueFor<type>(m_Cursor, row, NCBI_NAME2(m_,name), missing); \
    }                                                                   \
    CVDBColumnBits<sizeof(type)*8> NCBI_NAME2(m_, name)

// DECLARE_VDB_COLUMN is helper macro to declare accessor to VDB column
#define DECLARE_VDB_COLUMN_AS_STRING(name)                              \
    CVDBStringValue name(TVDBRowId row) const {                         \
        return CVDBStringValue(m_Cursor, row, NCBI_NAME2(m_,name));     \
    }                                                                   \
    CVDBColumnBits<8> NCBI_NAME2(m_, name)

// DECLARE_VDB_COLUMN is helper macro to declare accessor to VDB column
#define DECLARE_VDB_COLUMN_AS_4BITS(name)                               \
    CVDBValueFor4Bits name(TVDBRowId row) const {                       \
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
    NCBI_NAME2(m_, name)(m_Cursor, #name, NULL, CVDBColumn::eMissing_Allow)
#define INIT_OPTIONAL_VDB_COLUMN_BACKUP(name, backup_name)              \
    NCBI_NAME2(m_, name)(m_Cursor, #name, #backup_name, CVDBColumn::eMissing_Allow)
#define INIT_OPTIONAL_VDB_COLUMN_AS(name, type)                         \
    NCBI_NAME2(m_, name)(m_Cursor, "("#type")"#name, NULL, CVDBColumn::eMissing_Allow)


class NCBI_SRAREAD_EXPORT CVDBValue
{
public:
    enum EMissing {
        eMissing_Throw,
        eMissing_Allow
    };

    struct SRef {
        SRef(const CVDBCursor& cursor,
             TVDBRowId row,
             const CVDBColumn& column)
            : cursor(cursor), row(row), column(column)
            {
            }
        
        const CVDBCursor& cursor;
        TVDBRowId row;
        const CVDBColumn& column;
    };
    CVDBValue(void)
        : m_Data(0),
          m_ElemCount(0)
        {
        }
    CVDBValue(const CVDBCursor& cursor, const CVDBColumn& column)
        : m_Data(0),
          m_ElemCount(0)
        {
            x_Get(cursor, column);
        }
    CVDBValue(const CVDBCursor& cursor, TVDBRowId row,
              const CVDBColumn& column, EMissing missing = eMissing_Throw)
        : m_Data(0),
          m_ElemCount(0)
        {
            x_Get(cursor, row, column, missing);
        }
    explicit CVDBValue(const SRef& ref)
        : m_Data(0),
          m_ElemCount(0)
        {
            x_Get(ref.cursor, ref.row, ref.column);
        }
    CVDBValue(const CVDBCursor& cursor,
              const char* param_name, const CTempString& param_value,
              const CVDBColumn& column)
        : m_Data(0),
          m_ElemCount(0)
        {
            cursor.SetParam(param_name, param_value);
            x_Get(cursor, column);
        }

    bool empty(void) const
        {
            return !m_ElemCount;
        }
    size_t size(void) const
        {
            return m_ElemCount;
        }

protected:
    CVDBValue(const void* data, uint32_t size)
        : m_Data(data), m_ElemCount(size)
        {
        }

    void x_Get(const CVDBCursor& cursor,
               const CVDBColumn& column);
    void x_Get(const CVDBCursor& cursor,
               TVDBRowId row,
               const CVDBColumn& column,
               EMissing missing = eMissing_Throw);

    void x_ReportIndexOutOfBounds(size_t index) const;
    void x_ReportNotOneValue(void) const;
    void x_CheckIndex(size_t index) const
        {
            if ( index >= size() ) {
                x_ReportIndexOutOfBounds(index);
            }
        }
    void x_CheckRange(size_t pos, size_t len) const;
    void x_CheckOneValue(void) const
        {
            if ( size() != 1 ) {
                x_ReportNotOneValue();
            }
        }

    const void* m_Data;
    uint32_t m_ElemCount;
};


class NCBI_SRAREAD_EXPORT CVDBValueFor4Bits
{
public:
    typedef unsigned TValue;

    CVDBValueFor4Bits(void)
        : m_RawData(0),
          m_ElemOffset(0),
          m_ElemCount(0)
        {
        }
    explicit CVDBValueFor4Bits(const CVDBValue::SRef& ref)
        : m_RawData(0),
          m_ElemOffset(0),
          m_ElemCount(0)
        {
            x_Get(ref.cursor, ref.row, ref.column);
        }
    CVDBValueFor4Bits(const CVDBCursor& cursor, TVDBRowId row,
                      const CVDBColumn& column)
        : m_RawData(0),
          m_ElemOffset(0),
          m_ElemCount(0)
        {
            x_Get(cursor, row, column);
        }

    const char* raw_data(void) const
        {
            return m_RawData;
        }
    uint32_t raw_offset(void) const
        {
            return m_ElemOffset;
        }
    uint32_t size(void) const
        {
            return m_ElemCount;
        }
    bool empty(void) const
        {
            return !size();
        }

    TValue Value(size_t index) const
        {
            x_CheckIndex(index);
            return x_ValueByRawIndex(index+raw_offset());
        }
    TValue operator[](size_t index) const
        {
            return Value(index);
        }

    CVDBValueFor4Bits substr(size_t pos, size_t len) const;

protected:
    void x_Get(const CVDBCursor& cursor,
               TVDBRowId row,
               const CVDBColumn& column);
    static TValue sub_value(uint8_t v, size_t sub_index)
        {
            return sub_index? (v&0xf): (v>>4);
        }
    TValue x_ValueByRawIndex(size_t raw_index) const
        {
            return sub_value(raw_data()[raw_index/2], raw_index%2);
        }

    void x_ReportIndexOutOfBounds(size_t index) const;
    void x_CheckIndex(size_t index) const
        {
            if ( index >= size() ) {
                x_ReportIndexOutOfBounds(index);
            }
        }
    void x_CheckRange(size_t pos, size_t len) const;

    CVDBValueFor4Bits(const char* raw_data, uint32_t offset, uint32_t size)
        : m_RawData(raw_data),
          m_ElemOffset(offset),
          m_ElemCount(size)
        {
        }

    const char* m_RawData;
    uint32_t m_ElemOffset;
    uint32_t m_ElemCount;
};


class NCBI_SRAREAD_EXPORT CVDBValueFor2Bits
{
public:
    typedef unsigned TValue;

    CVDBValueFor2Bits(void)
        : m_RawData(0),
          m_ElemOffset(0),
          m_ElemCount(0)
        {
        }
    explicit CVDBValueFor2Bits(const CVDBValue::SRef& ref)
        : m_RawData(0),
          m_ElemOffset(0),
          m_ElemCount(0)
        {
            x_Get(ref.cursor, ref.row, ref.column);
        }
    CVDBValueFor2Bits(const CVDBCursor& cursor, TVDBRowId row,
                      const CVDBColumn& column)
        : m_RawData(0),
          m_ElemOffset(0),
          m_ElemCount(0)
        {
            x_Get(cursor, row, column);
        }

    const char* raw_data(void) const
        {
            return m_RawData;
        }
    uint32_t raw_offset(void) const
        {
            return m_ElemOffset;
        }
    uint32_t size(void) const
        {
            return m_ElemCount;
        }
    bool empty(void) const
        {
            return !size();
        }

    TValue Value(size_t index) const
        {
            x_CheckIndex(index);
            return x_ValueByRawIndex(index+raw_offset());
        }
    TValue operator[](size_t index) const
        {
            return Value(index);
        }

    CVDBValueFor2Bits substr(size_t pos, size_t len) const;

protected:
    void x_Get(const CVDBCursor& cursor,
               TVDBRowId row,
               const CVDBColumn& column);
    static TValue sub_value(uint8_t v, size_t sub_index)
        {
            return (v>>(6-2*sub_index))&3;
        }
    TValue x_ValueByRawIndex(size_t raw_index) const
        {
            return sub_value(raw_data()[raw_index/4], raw_index%4);
        }

    void x_ReportIndexOutOfBounds(size_t index) const;
    void x_CheckIndex(size_t index) const
        {
            if ( index >= size() ) {
                x_ReportIndexOutOfBounds(index);
            }
        }
    void x_CheckRange(size_t pos, size_t len) const;

    CVDBValueFor2Bits(const char* raw_data, uint32_t offset, uint32_t size)
        : m_RawData(raw_data),
          m_ElemOffset(offset),
          m_ElemCount(size)
        {
        }

    const char* m_RawData;
    uint32_t m_ElemOffset;
    uint32_t m_ElemCount;
};


template<class V>
class CVDBValueFor : public CVDBValue
{
public:
    typedef V TValue;
    CVDBValueFor(void)
        {
        }
    CVDBValueFor(const CVDBCursor& cursor, const CVDBColumn& column)
        : CVDBValue(cursor, column)
        {
        }
    CVDBValueFor(const CVDBCursor& cursor, TVDBRowId row,
                 const CVDBColumn& column, EMissing missing = eMissing_Throw)
        : CVDBValue(cursor, row, column, missing)
        {
        }
    explicit CVDBValueFor(const CVDBValue::SRef& ref)
        : CVDBValue(ref)
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
            x_CheckOneValue();
            return *data();
        }
    const TValue& operator*(void) const
        {
            return Value();
        }
    const TValue* operator->(void) const
        {
            return &Value();
        }

    operator const TValue&(void) const
        {
            return Value();
        }

    CVDBValueFor<TValue> substr(size_t pos, size_t len) const
        {
            x_CheckRange(pos, len);
            return CVDBValueFor<TValue>(data()+pos, uint32_t(len));
        }

protected:
    CVDBValueFor(const TValue* data, uint32_t len)
        : CVDBValue(data, len)
        {
        }

private:
    // to cause ambiguity if assigned to a wrong type
    operator double(void) const;
};


class CVDBStringValue : public CVDBValueFor<char>
{
public:
    CVDBStringValue(void)
        {
        }
    CVDBStringValue(const CVDBCursor& cursor,
                    const CVDBColumn& column)
        : CVDBValueFor<char>(cursor, column)
        {
        }
    CVDBStringValue(const CVDBCursor& cursor, TVDBRowId row,
                    const CVDBColumn& column)
        : CVDBValueFor<char>(cursor, row, column)
        {
        }
    explicit CVDBStringValue(const CVDBValue::SRef& ref)
        : CVDBValueFor<char>(ref)
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


#ifndef NCBI_OS_MSWIN
// Non-Windows paths are already POSIX
inline
string CVPath::ConvertSysPathToPOSIX(const string& sys_path)
{
    return sys_path;
}


// Non-Windows paths are already POSIX
inline
string CVPath::ConvertAccOrSysPathToPOSIX(const string& acc_or_path)
{
    return acc_or_path;
}
#endif


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // SRA__READER__SRA__VDBREAD__HPP
