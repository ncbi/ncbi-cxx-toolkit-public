#ifndef BDB_TYPES__HPP
#define BDB_TYPES__HPP

/* $Id$
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
 * Author:  Anatoliy Kuznetsov
 *
 * File Description: Field types classes.
 *
 */

#include <db/bdb/bdb_expt.hpp>
#include <corelib/ncbi_limits.hpp>
#include <corelib/ncbi_bswap.hpp>
#include <corelib/ncbistr.hpp>
#include <util/simple_buffer.hpp>

#include <string>
#include <vector>
#include <memory>


extern "C" {
    //
    // Forward structure declarations, so we can declare pointers and
    // applications can get type checking.
    // Taken from <db.h>
    struct __db_dbt; typedef struct __db_dbt DBT;
    struct __db;     typedef struct __db     DB;
    struct __dbc;    typedef struct __dbc    DBC;
    struct __db_env; typedef struct __db_env DB_ENV;
    struct __db_txn; typedef struct __db_txn DB_TXN;

    typedef int (*BDB_CompareFunction)(DB*, const DBT*, const DBT*);
    typedef unsigned int (*BDB_HashFunction)
                            (DB *, const void *bytes, unsigned length);
}

BEGIN_NCBI_SCOPE

/** @addtogroup BDB_Types
 *
 * @{
 */


extern "C" {


/// Simple and fast comparison function for tables with
/// non-segmented "unsigned int" keys
NCBI_DEPRECATED
NCBI_BDB_EXPORT int BDB_UintCompare(DB*, const DBT* val1, const DBT* val2);
NCBI_BDB_EXPORT int BDB_Uint4Compare(DB*, const DBT* val1, const DBT* val2);

/// Simple and fast comparison function for tables with
/// non-segmented "int" keys
NCBI_DEPRECATED
NCBI_BDB_EXPORT int BDB_IntCompare(DB*, const DBT* val1, const DBT* val2);
NCBI_BDB_EXPORT int BDB_Int4Compare(DB*, const DBT* val1, const DBT* val2);

/// Simple and fast comparison function for tables with
/// non-segmented "Int8" keys
NCBI_BDB_EXPORT int BDB_Int8Compare(DB*, const DBT* val1, const DBT* val2);

/// Simple and fast comparison function for tables with
/// non-segmented "Uint8" keys
NCBI_BDB_EXPORT int BDB_Uint8Compare(DB*, const DBT* val1, const DBT* val2);

/// Simple and fast comparison function for tables with
/// non-segmented "short int" keys
NCBI_BDB_EXPORT int BDB_Int2Compare(DB*, const DBT* val1, const DBT* val2);

/// Simple and fast comparison function for tables with
/// non-segmented "unsigned short int" keys
NCBI_BDB_EXPORT int BDB_Uint2Compare(DB*, const DBT* val1, const DBT* val2);

/// Simple and fast comparison function for tables with
/// non-segmented "char" keys
NCBI_BDB_EXPORT int BDB_CharCompare(DB*, const DBT* val1, const DBT* val2);

/// Simple and fast comparison function for tables with
/// non-segmented "unsigned char" keys
NCBI_BDB_EXPORT int BDB_UCharCompare(DB*, const DBT* val1, const DBT* val2);

/// Simple and fast comparison function for tables with
/// non-segmented "float" keys
NCBI_BDB_EXPORT int BDB_FloatCompare(DB*, const DBT* val1, const DBT* val2);

/// Simple and fast comparison function for tables with
/// non-segmented "double" keys
NCBI_BDB_EXPORT int BDB_DoubleCompare(DB*, const DBT* val1, const DBT* val2);

/// Simple and fast comparison function for tables with
/// non-segmented "C string" keys
NCBI_BDB_EXPORT int BDB_StringCompare(DB*, const DBT* val1, const DBT* val2);

/// Simple and fast comparison function for tables with
/// non-segmented length prefixed string keys
NCBI_BDB_EXPORT int BDB_LStringCompare(DB*, const DBT* val1, const DBT* val2);

/// Simple and fast comparison function for tables with
/// non-segmented fixed length string keys
NCBI_BDB_EXPORT int BDB_FixedByteStringCompare
                                    (DB*, const DBT* val1, const DBT* val2);


/// Simple and fast comparison function for tables with
/// non-segmented "case insensitive C string" keys
NCBI_BDB_EXPORT int
BDB_StringCaseCompare(DB*, const DBT* val1, const DBT* val2);

/// General purpose DBD comparison function
int NCBI_BDB_EXPORT BDB_Compare(DB* db, const DBT* val1, const DBT* val2);



/// Simple and fast comparison function for tables with
/// non-segmented "unsigned int" keys.
/// Used when the data file is in a different byte order architecture.
NCBI_DEPRECATED
NCBI_BDB_EXPORT int
BDB_ByteSwap_UintCompare(DB*, const DBT* val1, const DBT* val2);
NCBI_BDB_EXPORT int
BDB_ByteSwap_Uint4Compare(DB*, const DBT* val1, const DBT* val2);

/// Simple and fast comparison function for tables with
/// non-segmented "Int8" keys
/// Used when the data file is in a different byte order architecture.
NCBI_BDB_EXPORT int
BDB_ByteSwap_Int8Compare(DB*, const DBT* val1, const DBT* val2);

/// Simple and fast comparison function for tables with
/// non-segmented "Uint8" keys
/// Used when the data file is in a different byte order architecture.
NCBI_BDB_EXPORT int
BDB_ByteSwap_Uint8Compare(DB*, const DBT* val1, const DBT* val2);

/// Simple and fast comparison function for tables with
/// non-segmented "int" keys
/// Used when the data file is in a different byte order architecture.
NCBI_DEPRECATED
NCBI_BDB_EXPORT int
BDB_ByteSwap_IntCompare(DB*, const DBT* val1, const DBT* val2);
NCBI_BDB_EXPORT int
BDB_ByteSwap_Int4Compare(DB*, const DBT* val1, const DBT* val2);

/// Simple and fast comparison function for tables with
/// non-segmented "short int" keys
/// Used when the data file is in a different byte order architecture.
NCBI_BDB_EXPORT int
BDB_ByteSwap_Int2Compare(DB*, const DBT* val1, const DBT* val2);

/// Simple and fast comparison function for tables with
/// non-segmented "unsigned short int" keys
/// Used when the data file is in a different byte order architecture.
NCBI_BDB_EXPORT int
BDB_ByteSwap_Uint2Compare(DB*, const DBT* val1, const DBT* val2);

/// Simple and fast comparison function for tables with
/// non-segmented "float" keys
/// Used when the data file is in a different byte order architecture.
NCBI_BDB_EXPORT int
BDB_ByteSwap_FloatCompare(DB*, const DBT* val1, const DBT* val2);

/// Simple and fast comparison function for tables with
/// non-segmented "double" keys
/// Used when the data file is in a different byte order architecture.
NCBI_BDB_EXPORT int
BDB_ByteSwap_DoubleCompare(DB*, const DBT* val1, const DBT* val2);


/// General purpose hash function
unsigned int
NCBI_BDB_EXPORT BDB_Hash(DB *, const void *bytes, unsigned length);

/// Hash method for databases with a single (unique) UInt4 key
unsigned int
NCBI_BDB_EXPORT BDB_Uint4Hash(DB *, const void *bytes, unsigned length);


}


class CBDB_BufferManager;
class CBDB_Field;
class CBDB_File;
class CBDB_FileCursor;
class CBDB_FC_Condition;




/// BDB Data Field interface definition.
///
/// Every relational table in BDB library consists of fields, supporting
/// basic IBDB_Field interface
class NCBI_BDB_EXPORT IBDB_Field
{
public:
    virtual ~IBDB_Field();

    /// Comparison function. p1 and p2 are void pointers on field buffers.
    /// Positive if p1>p2, zero if p1==p2, negative if p1<p2.
    /// NOTE:  both buffers can be unaligned.
    /// byte_swapped TRUE indicates that buffers values are in
    /// a different byte order architecture
    virtual int         Compare(const void* p1,
                                const void* p2,
                                bool byte_swapped) const = 0;

    /// Return current effective size of the buffer.
    virtual size_t      GetDataLength(const void* buf) const = 0;

    /// Set minimal possible value for the field type
    virtual void        SetMinVal() = 0;

    /// Set maximum possible value for the field type
    virtual void        SetMaxVal() = 0;
};



/// BDB Data Field conversion interface definition.
/// All interface functions by default throw "bad conversion" exception.

class NCBI_BDB_EXPORT IBDB_FieldConvert
{
public:
    virtual ~IBDB_FieldConvert() {}

    virtual void SetInt(int)
        { BDB_THROW(eType, "Bad conversion"); }
    virtual void SetUint(unsigned)
        { BDB_THROW(eType, "Bad conversion"); }
    virtual int GetInt() const
        { BDB_THROW(eType, "Bad conversion"); }
    virtual unsigned GetUint() const
        { BDB_THROW(eType, "Bad conversion"); }
    virtual void SetString(const char*)
        { BDB_THROW(eType, "Bad conversion"); }
    virtual void SetStdString(const string&)
        { BDB_THROW(eType, "Bad conversion"); }
    virtual void SetFloat(float)
        { BDB_THROW(eType, "Bad conversion"); }
    virtual void SetDouble(double)
        { BDB_THROW(eType, "Bad conversion"); }

    virtual string GetString() const = 0;

    virtual void ToString(string& str) const = 0;
};



/// Interface definition class for field construction.
/// Used for interfaces' "access rights management".

class CBDB_FieldInterfaces : public  IBDB_Field,
                             public  IBDB_FieldConvert
{
    friend class CBDB_FileCursor;
};



/// Base class for constructing BDB fields. Implements field buffer pointer.
///
/// All DBD field types do not own their buffers.
/// It works as a pointer on external memory kept by the record manager
/// (CBDB_FieldBuf). Class cannot be used independently without record manager.

class NCBI_BDB_EXPORT CBDB_Field : public CBDB_FieldInterfaces
{
public:
    /// Length based classificator for fields (fixed-variable)
    enum ELengthType {
        eFixedLength,    //!< fixed-length (like int)
        eVariableLength  //!< variable-length (like string)
    };
    CBDB_Field(ELengthType length_type = eFixedLength);
    virtual ~CBDB_Field() {}

    /// Virtual constructor - class factory for BDB fields.
    /// Default (zero) value of 'buf-len' uses GetBufferSize().
    /// For fixed length fields this buf_size parameter has no effect
    virtual CBDB_Field* Construct(size_t buf_size = 0) const = 0;

    /// Return address to the type specific comparison function
    /// By default it's universal BDB_Compare
    virtual BDB_CompareFunction GetCompareFunction(bool byte_swapped) const;

    /// Return TRUE if field can be NULL
    bool IsNullable() const;

    /// Assign field value to NULL
    void SetNull();

    /// Return TRUE if field is NULL
    bool IsNull() const;

    /// Return symbolic name for the field
    const string& GetName() const;

    /// Get pointer to the data. NULL if not yet attached.
    const void* GetBuffer() const;
    /// Get pointer to the data. NULL if not yet attached.
    void*       GetBuffer();

    /// Return maximum possible buffer length
    size_t GetBufferSize() const;

    /// Get length of the actual data
    size_t GetLength() const;

protected:

    /// Field comparison function
    int CompareWith(const CBDB_Field& field) const;

    /// Copies field value from another field.
    /// The field type MUST be the same, or an exception will be thrown.
    void CopyFrom(const CBDB_Field& src);

    // Buffer management functions:

    /// Return TRUE if it is a variable length variable (like string)
    bool IsVariableLength() const;

    /// Return TRUE if external buffer has already been attached
    bool IsBufferAttached() const;

    /// RTTI based check if fld is of the same type
    bool IsSameType(const CBDB_Field& field) const;

    /// Return TRUE if field belongs to a file with an alternative
    /// byte order
    bool IsByteSwapped() const;


    /// Mark field as "NULL" capable.
    void SetNullable();

    /// Set "is NULL" flag to FALSE
    void SetNotNull();

    /// Set symbolic name for the field
    void SetName(const char* name);

    /// Set field position in the buffer manager
    void SetBufferIdx(unsigned int idx);


protected:
    /// Unpack the buffer which contains this field (using CBDB_BufferManager).
    /// Return new pointer to the field data -- located in the unpacked buffer.
    void*  Unpack();
    /// Set external buffer pointer and length.
    /// Zero 'buf_size' means GetBufferSize() is used to obtain the buf. size.
    void  SetBuffer(void* buf, size_t buf_size = 0);
    /// Set the buffer size.
    void  SetBufferSize(size_t size);
    /// Set data size, taking into account possible extra data for some fields
    void SetDataSize(size_t size);
    /// Hook for defining extra data length
    virtual size_t GetExtraDataLength();
    /// Set CBDB_BufferManager -- which works as a memory manager for BDB fields.
    void  SetBufferManager(CBDB_BufferManager* owner);

    /// Copy buffer value from the external source
    void CopyFrom(const void* src_buf);

private:
    CBDB_Field& operator= (const CBDB_Field& data);
    CBDB_Field(const CBDB_Field& data);

protected:
    CBDB_BufferManager*  m_BufferManager;
    struct {
        unsigned VariableLength : 1;
        unsigned Attached       : 1;
        unsigned Nullable       : 1;
    } m_Flags;
private:
    void*      m_Buffer;       // Pointer to the field data (in buf. mgr.)
    size_t     m_BufferSize;   // Field data buffer capacity
    unsigned   m_BufferIdx;    // Fields' position in the managing buffer
    string     m_Name;         // Field's symbolic name

    // Friends
    friend class CBDB_BufferManager;
    friend class CBDB_File;
    friend class CBDB_BLobFile;
};



/// Template class for building simple scalar data fields.
/// (int, float, double)

template<typename T>
class CBDB_FieldSimple : public CBDB_Field
{
public:
    typedef  T  TFieldType;
public:
    CBDB_FieldSimple()
    : CBDB_Field(eFixedLength)
    {
        SetBufferSize(sizeof(T));
    }

    void SetField(const CBDB_FieldSimple& field)
    {
        if ( field.IsNull() ) {
            SetNull();
        } else {
            if (IsByteSwapped() != field.IsByteSwapped()) {
                BDB_THROW(eInvalidValue, "Byte order incompatibility");
            }
#ifdef HAVE_UNALIGNED_READS
            TFieldType* b = (TFieldType*) GetBuffer();
            TFieldType* f = (TFieldType*) field.GetBuffer();
            *b = *f;
#else
            ::memcpy(GetBuffer(), field.GetBuffer(), sizeof(T));
#endif
            SetNotNull();
        }
    }

    // IDBD_Field implementation

    virtual int Compare(const void* p1,
                        const void* p2,
                        bool/* byte_swapped*/) const
    {
        // Default implementation ignores byte swapping
        T v1, v2;

#ifdef HAVE_UNALIGNED_READS
        v1 = *((TFieldType*)p1);
        v2 = *((TFieldType*)p2);
#else
        ::memcpy(&v1, p1, sizeof(v1));
        ::memcpy(&v2, p2, sizeof(v2));
#endif
        if (v1 < v2) return -1;
        if (v2 < v1) return 1;
        return 0;
    }

    virtual size_t GetDataLength(const void* /*buf*/) const
    {
        return sizeof(T);
    }
};



/// Template class for building simple scalar integer compatible data fields.
/// (int, short, char)

template<typename T>
class CBDB_FieldSimpleInt : public CBDB_FieldSimple<T>
{
public:
    CBDB_FieldSimpleInt() : CBDB_FieldSimple<T>() {}

    void Set(T val)
    {
        if (this->IsByteSwapped()) {

            if (sizeof(T) == 2) {
                CByteSwap::PutInt2((unsigned char*)this->GetBuffer(),
                                   (Int2) val);
            } else
            if (sizeof(T) == 4) {
                CByteSwap::PutInt4((unsigned char*)this->GetBuffer(),
                                   (Int4) val);
            } else
            if (sizeof(T) == 8) {
                CByteSwap::PutInt8((unsigned char*)this->GetBuffer(),
                                   (Int8) val);
            }
            else {
                _ASSERT(0);
            }

        } else {
#ifdef HAVE_UNALIGNED_READS
            typename CBDB_FieldSimple<T>::TFieldType* b =
                (typename CBDB_FieldSimple<T>::TFieldType*) this->GetBuffer();
            *b = val;
#else
            ::memcpy(this->GetBuffer(), &val, sizeof(T));
#endif
        }
        this->SetNotNull();
    }

    virtual void SetInt    (int          val)  { Set((T) val); }
    virtual void SetUint   (unsigned int val)  { Set((T) val); }
    virtual void SetString (const char*  val)
    {
        long v = ::atol(val);
        Set((T) v);
    }

    virtual int GetInt() const
    {
        int value;
#ifdef HAVE_UNALIGNED_READS
        typename CBDB_FieldSimple<T>::TFieldType* b =
            (typename CBDB_FieldSimple<T>::TFieldType*) this->GetBuffer();
        value = (int)*b;
#else
        ::memcpy(&value, this->GetBuffer(), sizeof(T));
#endif
        return value;
    }

    virtual unsigned GetUint() const
    {
        unsigned value;
#ifdef HAVE_UNALIGNED_READS
        typename CBDB_FieldSimple<T>::TFieldType* b =
            (typename CBDB_FieldSimple<T>::TFieldType*) this->GetBuffer();
        value = (unsigned)*b;
#else
        ::memcpy(&value, this->GetBuffer(), sizeof(T));
#endif
        return value;
    }

    virtual void SetStdString(const string& str)
    {
        SetString(str.c_str());
    }

    virtual int Compare(const void* p1,
                        const void* p2,
                        bool byte_swapped) const
    {
        if (!byte_swapped)
            return CBDB_FieldSimple<T>::Compare(p1, p2, byte_swapped);

        T v1, v2;
        v1 = (T) CByteSwap::GetInt4((unsigned char*)p1);
        v2 = (T) CByteSwap::GetInt4((unsigned char*)p2);
        if (v1 < v2) return -1;
        if (v2 < v1) return 1;
        return 0;
    }

    virtual void SetMinVal()
    {
        Set(numeric_limits<T>::min());
    }

    virtual void SetMaxVal()
    {
        Set(numeric_limits<T>::max());
    }
};



/// Template class for building simple scalar floating point compatible fields.
/// (float, double)
///

template<typename T>
class CBDB_FieldSimpleFloat : public CBDB_FieldSimple<T>
{
public:
    CBDB_FieldSimpleFloat() : CBDB_FieldSimple<T>() {}

    void Set(T val)
    {
        if (this->IsByteSwapped()) {

            if (sizeof(T) == 4) {
                CByteSwap::PutFloat((unsigned char*)this->GetBuffer(),
                                    (float)val);
            } else
            if (sizeof(T) == 8) {
                CByteSwap::PutDouble((unsigned char*)this->GetBuffer(), val);
            }
            else {
                _ASSERT(0);
            }
        } else {
#ifdef HAVE_UNALIGNED_READS
            typename CBDB_FieldSimple<T>::TFieldType* b =
                (typename CBDB_FieldSimple<T>::TFieldType*) this->GetBuffer();
            *b = val;
#else
            ::memcpy(this->GetBuffer(), &val, sizeof(T));
#endif
        }
        this->SetNotNull();
    }

    virtual void SetInt    (int          val) { Set((T) val); }
    virtual void SetUint   (unsigned int val) { Set((T) val); }
    virtual void SetString (const char*  val)
    {
        double v = ::atof(val);
        Set((T) v);
    }
    virtual void SetStdString(const string& str)
    {
        SetString(str.c_str());
    }

    virtual void SetFloat (float  val) { Set((T) val); }
    virtual void SetDouble(double val) { Set((T) val); }

    virtual void SetMinVal()
    {
        Set(numeric_limits<T>::min());
    }

    virtual void SetMaxVal()
    {
        Set(numeric_limits<T>::max());
    }
};


///  Int8 field type
///

class NCBI_BDB_EXPORT CBDB_FieldInt8 : public CBDB_FieldSimpleInt<Int8>
{
public:
    CBDB_FieldInt8& operator= (Int8 val)
    {
        Set(val);
        return *this;
    }

    CBDB_FieldInt8& operator= (const CBDB_FieldInt8& val)
    {
        Set(val);
        return *this;
    }

    virtual CBDB_Field* Construct(size_t /*buf_size*/) const
    {
        return new CBDB_FieldInt8();
    }

    Int8 Get() const
    {
        Int8  v;

        _ASSERT(!IsNull());

        if (IsByteSwapped()) {
            v = CByteSwap::GetInt8((unsigned char*)GetBuffer());
        } else {
#ifdef HAVE_UNALIGNED_READS
            TFieldType* b = (TFieldType*) this->GetBuffer();
            v = *b;
#else
            ::memcpy(&v, GetBuffer(), sizeof(Int8));
#endif
        }
        return v;
    }

    virtual string GetString() const
    {
        Int8  v = Get();
        return NStr::Int8ToString(v);
    }

    virtual void ToString(string& str) const
    {
        Int8 v = Get();
        NStr::Int8ToString(str, v);
    }

    operator Int8() const
    {
        return Get();
    }

    virtual BDB_CompareFunction GetCompareFunction(bool byte_swapped) const
    {
        if (byte_swapped)
            return BDB_ByteSwap_Int8Compare;
        return BDB_Int8Compare;
    }

    virtual int Compare(const void* p1,
                        const void* p2,
                        bool byte_swapped) const
    {
        if (!byte_swapped)
            return CBDB_FieldSimpleInt<Int8>::Compare(p1, p2, byte_swapped);

        Int8 v1, v2;
        v1 = CByteSwap::GetInt8((unsigned char*)p1);
        v2 = CByteSwap::GetInt8((unsigned char*)p2);
        if (v1 < v2) return -1;
        if (v2 < v1) return 1;
        return 0;
    }

};


///  Uint8 field type
///

class NCBI_BDB_EXPORT CBDB_FieldUint8 : public CBDB_FieldSimpleInt<Uint8>
{
public:
    CBDB_FieldUint8& operator= (Uint8 val)
    {
        Set(val);
        return *this;
    }

    CBDB_FieldUint8& operator= (const CBDB_FieldUint8& val)
    {
        Set(val);
        return *this;
    }

    virtual CBDB_Field* Construct(size_t /*buf_size*/) const
    {
        return new CBDB_FieldUint8();
    }

    Uint8 Get() const
    {
        Uint8  v;

        _ASSERT(!IsNull());

        if (IsByteSwapped()) {
            v = CByteSwap::GetInt8((unsigned char*)GetBuffer());
        } else {
#ifdef HAVE_UNALIGNED_READS
            TFieldType* b = (TFieldType*) GetBuffer();
            v = *b;
#else
            ::memcpy(&v, GetBuffer(), sizeof(Uint8));
#endif
        }
        return v;
    }

    virtual string GetString() const
    {
        Uint8  v = Get();
        return NStr::UInt8ToString(v);
    }

    virtual void ToString(string& str) const
    {
        Uint8 v = Get();
        NStr::UInt8ToString(str, v);
    }

    operator Uint8() const
    {
        return Get();
    }

    virtual BDB_CompareFunction GetCompareFunction(bool byte_swapped) const
    {
        if (byte_swapped)
            return BDB_ByteSwap_Uint8Compare;
        return BDB_Uint8Compare;
    }

    virtual int Compare(const void* p1,
                        const void* p2,
                        bool byte_swapped) const
    {
        if (!byte_swapped)
            return CBDB_FieldSimpleInt<Uint8>::Compare(p1, p2, byte_swapped);

        Uint8 v1, v2;
        v1 = CByteSwap::GetInt8((unsigned char*)p1);
        v2 = CByteSwap::GetInt8((unsigned char*)p2);
        if (v1 < v2) return -1;
        if (v2 < v1) return 1;
        return 0;
    }

};


///  Int4 field type
///

class NCBI_BDB_EXPORT CBDB_FieldInt4 : public CBDB_FieldSimpleInt<Int4>
{
public:
    CBDB_FieldInt4& operator= (Int4 val)
    {
        Set(val);
        return *this;
    }

    CBDB_FieldInt4& operator= (const CBDB_FieldInt4& val)
    {
        Set(val);
        return *this;
    }

    virtual CBDB_Field* Construct(size_t /*buf_size*/) const
    {
        return new CBDB_FieldInt4();
    }

    Int4 Get() const
    {
        Int4  v;

        _ASSERT(!IsNull());

        if (IsByteSwapped()) {
            v = CByteSwap::GetInt4((unsigned char*)GetBuffer());
        } else {
#ifdef HAVE_UNALIGNED_READS
            TFieldType* b = (TFieldType*)GetBuffer();
            v = *b;
#else
            ::memcpy(&v, GetBuffer(), sizeof(Int4));
#endif
        }
        return v;
    }

    virtual string GetString() const
    {
        Int4  v = Get();
        return NStr::IntToString(v);
    }

    virtual void ToString(string& str) const
    {
        Int4 v = Get();
        NStr::IntToString(str, v);
    }

    operator Int4() const
    {
        return Get();
    }

    virtual BDB_CompareFunction GetCompareFunction(bool byte_swapped) const
    {
        if (byte_swapped)
            return BDB_ByteSwap_Int4Compare;
        return BDB_Int4Compare;
    }

    virtual int Compare(const void* p1,
                        const void* p2,
                        bool byte_swapped) const
    {
        if (!byte_swapped)
            return CBDB_FieldSimpleInt<Int4>::Compare(p1, p2, byte_swapped);

        Int4 v1, v2;
        v1 = CByteSwap::GetInt4((unsigned char*)p1);
        v2 = CByteSwap::GetInt4((unsigned char*)p2);
        if (v1 < v2) return -1;
        if (v2 < v1) return 1;
        return 0;
    }

};


///  Int2 field type
///

class NCBI_BDB_EXPORT CBDB_FieldInt2 : public CBDB_FieldSimpleInt<Int2>
{
public:
    CBDB_FieldInt2& operator= (Int2 val)
    {
        Set(val);
        return *this;
    }

    CBDB_FieldInt2& operator= (const CBDB_FieldInt2& val)
    {
        Set(val);
        return *this;
    }

    virtual CBDB_Field* Construct(size_t /*buf_size*/) const
    {
        return new CBDB_FieldInt2();
    }

    Int2 Get() const
    {
        Int2  v;

        _ASSERT(!IsNull());

        if (IsByteSwapped()) {
            v = CByteSwap::GetInt2((unsigned char*)GetBuffer());
        } else {
#ifdef HAVE_UNALIGNED_READS
            TFieldType* b = (TFieldType*) GetBuffer();
            v = *b;
#else
            ::memcpy(&v, GetBuffer(), sizeof(Int2));
#endif
        }
        return v;
    }

    virtual string GetString() const
    {
        Int4  v = Get();
        return NStr::IntToString(v);
    }

    virtual void ToString(string& str) const
    {
        Int4 v = Get();
        NStr::IntToString(str, v);
    }

    operator Int2() const
    {
        return Get();
    }

    virtual BDB_CompareFunction GetCompareFunction(bool byte_swapped) const
    {
        if (byte_swapped)
            return BDB_ByteSwap_Int2Compare;
        return BDB_Int2Compare;
    }

    virtual int Compare(const void* p1,
                        const void* p2,
                        bool byte_swapped) const
    {
        if (!byte_swapped)
            return CBDB_FieldSimpleInt<Int2>::Compare(p1, p2, byte_swapped);

        Int2 v1, v2;
        v1 = CByteSwap::GetInt2((unsigned char*)p1);
        v2 = CByteSwap::GetInt2((unsigned char*)p2);
        if (v1 < v2) return -1;
        if (v2 < v1) return 1;
        return 0;
    }

};


/// Uint2 field type
///

class NCBI_BDB_EXPORT CBDB_FieldUint2 : public CBDB_FieldSimpleInt<Uint2>
{
public:
    CBDB_FieldUint2& operator= (Uint2 val)
    {
        Set(val);
        return *this;
    }

    CBDB_FieldUint2& operator= (const CBDB_FieldUint2& val)
    {
        Set(val);
        return *this;
    }

    virtual CBDB_Field* Construct(size_t /*buf_size*/) const
    {
        return new CBDB_FieldUint2();
    }

    Uint2 Get() const
    {
        Uint2  v;

        _ASSERT(!IsNull());

        if (IsByteSwapped()) {
            v = CByteSwap::GetInt2((unsigned char*)GetBuffer());
        } else {
#ifdef HAVE_UNALIGNED_READS
            TFieldType* b = (TFieldType*) GetBuffer();
            v = *b;
#else
            ::memcpy(&v, GetBuffer(), sizeof(Uint2));
#endif
        }
        return v;
    }

    virtual string GetString() const
    {
        Int4  v = Get();
        return NStr::IntToString(v);
    }

    virtual void ToString(string& str) const
    {
        Int4 v = Get();
        NStr::IntToString(str, v);
    }

    operator Uint2() const
    {
        return Get();
    }

    virtual BDB_CompareFunction GetCompareFunction(bool byte_swapped) const
    {
        if (byte_swapped)
            return BDB_ByteSwap_Uint2Compare;
        return BDB_Uint2Compare;
    }

    virtual int Compare(const void* p1,
                        const void* p2,
                        bool byte_swapped) const
    {
        if (!byte_swapped)
            return CBDB_FieldSimpleInt<Uint2>::Compare(p1, p2, byte_swapped);

        Uint2 v1, v2;
        v1 = CByteSwap::GetInt2((unsigned char*)p1);
        v2 = CByteSwap::GetInt2((unsigned char*)p2);
        if (v1 < v2) return -1;
        if (v2 < v1) return 1;
        return 0;
    }

};

/// Char field type
///

class CBDB_FieldUChar : public CBDB_FieldSimpleInt<unsigned char>
{
public:
    CBDB_FieldUChar& operator = (unsigned char val)
    {
        Set(val);
        return *this;
    }

    CBDB_FieldUChar& operator = (const CBDB_FieldUChar& val)
    {
        Set(val);
        return *this;
    }

    virtual CBDB_Field * Construct(size_t) const
    {
        return new CBDB_FieldUChar();
    }

    unsigned char Get() const
    {
        _ASSERT(!IsNull());

        return *(const unsigned char*)GetBuffer();
    }

    operator char () const
    {
        return Get();
    }

    virtual string GetString() const
    {
        return string(1, Get());
    }

    virtual void ToString(string& s) const
    {
        s.assign(1, Get());
    }

    virtual BDB_CompareFunction GetCompareFunction(bool) const
    {
        return BDB_UCharCompare;
    }

    virtual int Compare(const void * p1,
                        const void * p2,
                        bool) const
    {
        const unsigned char& c1 = *(const unsigned char *)p1;
        const unsigned char& c2 = *(const unsigned char *)p2;

        return (c1 < c2) ? -1 : (c1 > c2) ? 1 : 0;
    }
};

class CBDB_FieldChar : public CBDB_FieldSimpleInt<char>
{
public:
    CBDB_FieldChar& operator = (char val)
    {
        Set(val);
        return *this;
    }

    CBDB_FieldChar& operator = (const CBDB_FieldChar& val)
    {
        Set(val);
        return *this;
    }

    virtual CBDB_Field * Construct(size_t) const
    {
        return new CBDB_FieldChar();
    }

    char Get() const
    {
        _ASSERT(!IsNull());

        return *(const char*)GetBuffer();
    }

    operator char () const
    {
        return Get();
    }

    virtual string GetString() const
    {
        return string(1, Get());
    }

    virtual void ToString(string& s) const
    {
        s.assign(1, Get());
    }

    virtual BDB_CompareFunction GetCompareFunction(bool) const
    {
        return BDB_CharCompare;
    }

    virtual int Compare(const void * p1,
                        const void * p2,
                        bool) const
    {
        const char& c1 = *(const char *)p1;
        const char& c2 = *(const char *)p2;

        return (c1 < c2) ? -1 : (c1 > c2) ? 1 : 0;
    }
};

/// Int1 field type
///

class CBDB_FieldInt1 : public CBDB_FieldChar
{
public:
    CBDB_FieldInt1& operator=(Int1 val)
    {
        Set(val);
        return *this;
    }

    CBDB_FieldInt1& operator=(const CBDB_FieldChar& val)
    {
        Set(val);
        return *this;
    }

    virtual CBDB_Field * Construct(size_t) const
    {
        return new CBDB_FieldInt1();
    }

    virtual string GetString() const
    {
        return NStr::UIntToString(Get());
    }

    virtual void ToString(string& s) const
    {
        s = GetString();
    }
};


/// Uint1 field type
///

class CBDB_FieldUint1 : public CBDB_FieldUChar
{
public:
    CBDB_FieldUint1& operator= (unsigned char val)
    {
        Set(val);
        return *this;
    }

    CBDB_FieldUint1& operator= (const CBDB_FieldUChar& val)
    {
        Set(val);
        return *this;
    }

    virtual CBDB_Field * Construct(size_t) const
    {
        return new CBDB_FieldUint1();
    }

    virtual string GetString() const
    {
        return NStr::UIntToString(Get());
    }

    virtual void ToString(string& s) const
    {
        s = GetString();
    }
};


///  Uint4 field type
///

class NCBI_BDB_EXPORT CBDB_FieldUint4 : public CBDB_FieldSimpleInt<Uint4>
{
public:
    CBDB_FieldUint4& operator= (Uint4 val)
    {
        Set(val);
        return *this;
    }

    CBDB_FieldUint4& operator= (const CBDB_FieldUint4& val)
    {
        Set(val);
        return *this;
    }

    virtual CBDB_Field* Construct(size_t /*buf_size*/) const
    {
        return new CBDB_FieldUint4();
    }

    Uint4 Get() const
    {
        Uint4  v;

        _ASSERT(!IsNull());

        if (IsByteSwapped()) {
            v = (Uint4)CByteSwap::GetInt4((unsigned char*)GetBuffer());
        } else {
#ifdef HAVE_UNALIGNED_READS
            TFieldType* b = (TFieldType*) GetBuffer();
            v = *b;
#else
            ::memcpy(&v, GetBuffer(), sizeof(Uint4));
#endif
        }
        return v;
    }

    virtual string GetString() const
    {
        Uint4  v = Get();
        return NStr::UIntToString(v);
    }

    virtual void ToString(string& str) const
    {
        Uint4 v = Get();
        NStr::UIntToString(str, v);
    }

    operator Uint4() const
    {
        return Get();
    }

    virtual BDB_CompareFunction GetCompareFunction(bool byte_swapped) const
    {
        if (byte_swapped)
            return BDB_ByteSwap_Uint4Compare;
        return BDB_Uint4Compare;
    }

    virtual int Compare(const void* p1,
                        const void* p2,
                        bool byte_swapped) const
    {
        if (!byte_swapped)
            return CBDB_FieldSimpleInt<Uint4>::Compare(p1, p2, byte_swapped);

        Uint4 v1, v2;
        v1 = (Uint4)CByteSwap::GetInt4((unsigned char*)p1);
        v2 = (Uint4)CByteSwap::GetInt4((unsigned char*)p2);
        if (v1 < v2) return -1;
        if (v2 < v1) return 1;
        return 0;
    }

};



///  Float field type
///

class NCBI_BDB_EXPORT CBDB_FieldFloat : public CBDB_FieldSimpleFloat<float>
{
public:
    CBDB_FieldFloat& operator= (float val)
    {
        Set(val);
        return *this;
    }

    CBDB_FieldFloat& operator= (const CBDB_FieldFloat& val)
    {
        Set(val);
        return *this;
    }

    virtual CBDB_Field* Construct(size_t /*buf_size*/) const
    {
        return new CBDB_FieldFloat();
    }

    float Get() const
    {
        float  v;

        _ASSERT(!IsNull());

        if (IsByteSwapped()) {
            v = CByteSwap::GetFloat((unsigned char*)GetBuffer());
        } else {
#ifdef HAVE_UNALIGNED_READS
            TFieldType* b = (TFieldType*) this->GetBuffer();
            v = *b;
#else
            ::memcpy(&v, GetBuffer(), sizeof(float));
#endif
        }
        return v;
    }

    virtual string GetString() const
    {
        double v = Get();
        return NStr::DoubleToString(v);
    }

    virtual void ToString(string& str) const
    {
        double v = Get();
        NStr::DoubleToString(str, v);
    }

    operator float() const
    {
        return Get();
    }

    virtual BDB_CompareFunction GetCompareFunction(bool byte_swapped) const
    {
        if (byte_swapped)
            return BDB_ByteSwap_FloatCompare;

        return BDB_FloatCompare;
    }

    virtual int Compare(const void* p1,
                        const void* p2,
                        bool byte_swapped) const
    {
        if (!byte_swapped)
            return CBDB_FieldSimpleFloat<float>::Compare(p1, p2, byte_swapped);

        float v1, v2;
        v1 = CByteSwap::GetFloat((unsigned char*)p1);
        v2 = CByteSwap::GetFloat((unsigned char*)p2);
        if (v1 < v2) return -1;
        if (v2 < v1) return 1;
        return 0;
    }
};


///  Double precision floating point field type
///

class NCBI_BDB_EXPORT CBDB_FieldDouble : public CBDB_FieldSimpleFloat<double>
{
public:
    CBDB_FieldDouble& operator= (double val)
    {
        Set(val);
        return *this;
    }

    CBDB_FieldDouble& operator= (const CBDB_FieldDouble& val)
    {
        Set(val);
        return *this;
    }

    virtual CBDB_Field* Construct(size_t /*buf_size*/) const
    {
        return new CBDB_FieldDouble();
    }

    double Get() const
    {
        double  v;

        _ASSERT(!IsNull());

        if (IsByteSwapped()) {
            v = CByteSwap::GetDouble((unsigned char*)GetBuffer());
        } else {
#ifdef HAVE_UNALIGNED_READS
            TFieldType* b = (TFieldType*) GetBuffer();
            v = *b;
#else
            ::memcpy(&v, GetBuffer(), sizeof(double));
#endif
        }
        return v;
    }

    virtual string GetString() const
    {
        double v = Get();
        return NStr::DoubleToString(v);
    }

    virtual void ToString(string& str) const
    {
        double v = Get();
        NStr::DoubleToString(str, v);
    }

    operator double() const
    {
        return Get();
    }

    virtual BDB_CompareFunction GetCompareFunction(bool byte_swapped) const
    {
        if (byte_swapped)
            return BDB_ByteSwap_DoubleCompare;

        return BDB_DoubleCompare;
    }

    virtual int Compare(const void* p1,
                        const void* p2,
                        bool byte_swapped) const
    {
        if (!byte_swapped)
            return CBDB_FieldSimpleFloat<double>::Compare(p1, p2, byte_swapped);

        double v1, v2;
        v1 = CByteSwap::GetDouble((unsigned char*)p1);
        v2 = CByteSwap::GetDouble((unsigned char*)p2);
        if (v1 < v2) return -1;
        if (v2 < v1) return 1;
        return 0;
    }

};

/// Fixed length bytestring. (Can hold 0 chars).
///
class NCBI_BDB_EXPORT CBDB_FieldFixedByteString : public CBDB_Field
{
public:
    CBDB_FieldFixedByteString();

    virtual CBDB_Field* Construct(size_t buf_size) const
    {
        CBDB_FieldFixedByteString* fld = new CBDB_FieldFixedByteString();
        fld->SetBufferSize(buf_size ? buf_size : GetBufferSize());
        return fld;
    }
    operator const char* () const;
    CBDB_FieldFixedByteString& operator=
                    (const CBDB_FieldFixedByteString& str);

    void Set(const char* str);
    virtual void SetString (const char*  val) { Set(val); }
    string Get() const;
    virtual string GetString() const { return Get(); }

    bool IsEmpty() const { return false; }
    bool IsBlank() const { return false; }


    // IField
    virtual int         Compare(const void* p1,
                                const void* p2,
                                bool /* byte_swapped */) const;
    virtual size_t      GetDataLength(const void* buf) const;
    virtual void        SetMinVal();
    virtual void        SetMaxVal();

    virtual void SetStdString(const string& str)
    {
        _ASSERT(str.length() == GetBufferSize());
        SetString(str.c_str());
    }
    virtual void ToString(string& str) const
    {
        str.assign((const char*) GetBuffer(), GetBufferSize());
    }

    virtual BDB_CompareFunction GetCompareFunction(bool) const
    {
        return BDB_FixedByteStringCompare;
    }

};


///
///  String field type
///
class NCBI_BDB_EXPORT CBDB_FieldStringBase : public CBDB_Field
{
public:
    enum EOverflowAction {
        eThrowOnOverflow,
        eTruncateOnOverflow,
        eTruncateOnOverflowLogError
    };

protected:
    CBDB_FieldStringBase() : CBDB_Field(eVariableLength) {}
};


///
///  String field type designed to work with C-strings (ASCIIZ)
///

class NCBI_BDB_EXPORT CBDB_FieldString : public CBDB_FieldStringBase
{
public:
    CBDB_FieldString();

    /// Class factory for string fields.
    /// Default (zero) value of 'buf_size' uses GetBufferSize().
    virtual CBDB_Field* Construct(size_t buf_size) const
    {
        CBDB_FieldString* fld = new CBDB_FieldString();
        fld->SetBufferSize(buf_size ? buf_size : GetBufferSize());
        return fld;
    }

    operator const char* () const;
    CBDB_FieldString& operator= (const CBDB_FieldString& str);
    CBDB_FieldString& operator= (const char*             str);
    CBDB_FieldString& operator= (const string&           str);

    void Set(const char* str, EOverflowAction if_overflow = eThrowOnOverflow);
    string Get() const { return string((const char*)GetBuffer()); }

    virtual string GetString() const
    {
        return Get();
    }

    bool IsEmpty() const;
    bool IsBlank() const;


    // IField
    virtual int         Compare(const void* p1,
                                const void* p2,
                                bool /* byte_swapped */) const;
    virtual size_t      GetDataLength(const void* buf) const;
    virtual void        SetMinVal();
    virtual void        SetMaxVal();

    virtual void SetString(const char*);
    virtual void SetStdString(const string& str)
    {
        SetString(str.c_str());
    }
    virtual void ToString(string& str) const
    {
        str = (const char*) GetBuffer();
    }

    virtual BDB_CompareFunction GetCompareFunction(bool) const
    {
        return BDB_StringCompare;
    }
};



///  Case-insensitive (but case preserving) string field type
///

class NCBI_BDB_EXPORT CBDB_FieldStringCase : public CBDB_FieldString
{
public:
    typedef CBDB_FieldString CParent;

    explicit CBDB_FieldStringCase() : CBDB_FieldString() {}

    virtual CBDB_Field* Construct(size_t buf_size) const
    {
        CBDB_FieldStringCase* fld = new CBDB_FieldStringCase();
        fld->SetBufferSize(buf_size ? buf_size : GetBufferSize());
        return fld;
    }

    // Accessors
    operator const char* () const { return (const char*) GetBuffer(); }

    CBDB_FieldStringCase& operator= (const CBDB_FieldString& str)
    {
        Set(str);
        return *this;
    }
    CBDB_FieldStringCase& operator= (const CBDB_FieldStringCase& str)
    {
        Set(str);
        return *this;
    }
    CBDB_FieldStringCase& operator= (const char* str)
    {
        Set(str);
        return *this;
    }
    CBDB_FieldStringCase& operator= (const string& str)
    {
        Set(str.c_str());
        return *this;
    }

    virtual int Compare(const void* p1,
                        const void* p2,
                        bool/* byte_swapped */) const
    {
        _ASSERT(p1 && p2);
        return NStr::CompareNocase((const char*) p1, (const char*) p2);
    }

    virtual
    BDB_CompareFunction GetCompareFunction(bool /* byte_swapped */) const
    {
        return BDB_StringCaseCompare;
    }

};


///
///  Length prefised string field type
///

class NCBI_BDB_EXPORT CBDB_FieldLString : public CBDB_FieldStringBase
{
public:
    CBDB_FieldLString();

    // Class factory for string fields.
    // Default (zero) value of 'buf_size' uses GetBufferSize().
    virtual CBDB_Field* Construct(size_t buf_size) const;

    operator string() const { return GetString(); }
    CBDB_FieldLString& operator= (const CBDB_FieldLString& str);
    CBDB_FieldLString& operator= (const char*             str);
    CBDB_FieldLString& operator= (const string&           str);

    void Set(const char* str, EOverflowAction if_overflow = eThrowOnOverflow);
    void Set(const char* str, size_t size,
             EOverflowAction if_overflow = eThrowOnOverflow);
    string Get() const;

    virtual string GetString() const
    {
        return Get();
    }

    bool IsEmpty() const;
    bool IsBlank() const;


    // IField
    virtual int         Compare(const void* p1,
                                const void* p2,
                                bool /* byte_swapped */) const;
    virtual size_t      GetDataLength(const void* buf) const;
    virtual void        SetMinVal();
    virtual void        SetMaxVal();

    virtual void SetString(const char*);
    virtual void SetStdString(const string& str);

    virtual BDB_CompareFunction GetCompareFunction(bool) const
    {
        return BDB_LStringCompare;
    }
    virtual void ToString(string& str) const;
protected:
    const unsigned char* GetLString(const unsigned char* str,
                                    bool                 check_legacy,
                                    int*                 str_len) const;
    virtual size_t GetExtraDataLength();
};




/// BDB Data Field Buffer manager class.
/// For internal use in BDB library.
///
/// @internal

class NCBI_BDB_EXPORT CBDB_BufferManager
{
public:
    /// Return number of fields attached using function Bind
    unsigned int FieldCount() const;

    const CBDB_Field& GetField(unsigned int idx) const;
    CBDB_Field&       GetField(unsigned int idx);

    /// Find the field with the specified name. Name is case insensitive.
    /// @return -1 if field cannot be found
    int GetFieldIndex(const string& name) const;

    /// Return TRUE if buffer is in a non-native byte order
    bool IsByteSwapped() const { return m_ByteSwapped; }

    /// Sets maximum number of fields participating in comparison
    /// Should be less than total number of fields in the buffer
    void SetFieldCompareLimit(unsigned int n_fields);

    /// Get number of fields in comparison.
    /// 0 - means no forced limit
    unsigned int GetFieldCompareLimit() const;

    /// Return TRUE if buffer l-strings should check about legacy
    /// c-str compatibility
    bool IsLegacyStrings() const { return m_LegacyString; }

    /// Get DBT.size of the parent file (key or data area)
    /// (Set by CBDB_File after read)
    size_t GetDBT_Size() const { return m_DBT_Size; }

    ~CBDB_BufferManager();

    /// Check if any field is NULL
    bool HasNull() const;

    /// Copy all fields from another manager with the same(a must!) structure
    void CopyFrom(const CBDB_BufferManager& bman);

    /// Copy packed data (buffer) from an external source
    void CopyPackedFrom(void* data, size_t data_size);

protected:
    CBDB_BufferManager();

    /// Create internal data buffer, assign places in this buffer to the fields
    void Construct();

    /// Set minimum possible value to buffer fields from 'idx_from' to 'idx_to'
    void SetMinVal(unsigned int idx_from, unsigned int idx_to);

    /// Set maximum possible value to buffer fields from 'idx_from' to 'idx_to'
    void SetMaxVal(unsigned int idx_from, unsigned int idx_to);

    /// Attach 'field' to the buffer.
    /// NOTE: buffer manager will not own the attached object, nor will it
    ///       keep ref counters or do any other automagic memory management,
    ///       unless field ownership is set to TRUE
    void Bind(CBDB_Field* field, ENullable is_nullable = eNotNullable);

    /// Copy all field values from the 'buf_mgr'.
    void CopyFieldsFrom(const CBDB_BufferManager& buf_mgr);

    /// Duplicate (dynamic allocation is used) all fields from 'buf_mgr' and
    /// bind them to the this buffer manager. Field values are not copied.
    /// NOTE: CBDB_BufferManager does not own or deallocate fields,
    ///       caller is responsible for deallocation,
    ///       unless field ownership is set to TRUE
    void DuplicateStructureFrom(const CBDB_BufferManager& buf_mgr);

    /// Compare fields of this buffer with those of 'buf_mgr' using
    /// CBDB_Field::CompareWith().
    /// Optional 'n_fields' parameter used when we want to compare only
    /// several first fields instead of all.
    int Compare(const CBDB_BufferManager& buf_mgr,
                unsigned int              n_fields = 0) const;

    /// Return TRUE if any field bound to this buffer manager has variable
    /// length (i.e. packable)
    bool IsPackable() const;

    /// Check if all NOT NULLABLE fields were assigned.
    /// Throw an exception if not.
    void CheckNullConstraint() const;

    void ArrangePtrsUnpacked();
    void ArrangePtrsPacked();
    void Clear();
    unsigned Pack();
    unsigned Unpack();

    /// Pack the buffer and initialize DBT structure for write operation
    void PrepareDBT_ForWrite(DBT* dbt);

    /// Initialize DBT structure for read operation.
    void PrepareDBT_ForRead(DBT* dbt);

    /// Calculate buffer size
    size_t ComputeBufferSize() const;

    /// Return TRUE if buffer can carry NULL fields
    bool IsNullable() const;

    /// Set byte swapping flag for the buffer
    void SetByteSwapped(bool byte_swapped) { m_ByteSwapped = byte_swapped; }

    /// Mark buffer as "NULL fields ready".
    /// NOTE: Should be called before buffer construction.
    void SetNullable();

    void SetNull(unsigned int field_idx, bool value);
    bool IsNull (unsigned int field_idx) const;

    size_t ComputeNullSetSize() const;
    bool   TestNullBit(unsigned int idx) const;
    void   SetNullBit (unsigned int idx, bool value);
    void   SetAllNull();

    /// Return buffer compare function
    BDB_CompareFunction GetCompareFunction() const;

    /// Return buffer hash function
    BDB_HashFunction GetHashFunction() const;

    /// Set C-str detection
    void SetLegacyStringsCheck(bool value) { m_LegacyString = value; }

    void SetDBT_Size(size_t size) { m_DBT_Size = size; }

    /// Fields deletion is managed by the class when own_fields is TRUE
    void SetFieldOwnership(bool own_fields) { m_OwnFields = own_fields; }

    /// Return fields ownership flag
    bool IsOwnFields() const { return m_OwnFields; }

    /// Disable-enable packing
    void SetPackable(bool packable) { m_Packable = packable; }

private:
    void x_ComputePackOpt();
private:
    CBDB_BufferManager(const CBDB_BufferManager&);
    CBDB_BufferManager& operator= (const CBDB_BufferManager&);

private:
    typedef vector<CBDB_Field*>  TFieldVector;

private:
    TFieldVector            m_Fields;
    /// Array of pointers to the fields' data
    vector<void*>           m_Ptrs;
    char*                   m_Buffer;
    size_t                  m_BufferSize;
    size_t                  m_PackedSize;
    size_t                  m_DBT_Size;
    bool                    m_Packable;
    /// TRUE if buffer is in a non-native arch.
    bool                    m_ByteSwapped;

    /// TRUE if buffer can carry NULL fields
    bool                    m_Nullable;
    /// size of the 'is NULL' bitset in bytes
    size_t                  m_NullSetSize;

    /// Number of fields in key comparison
    unsigned int            m_CompareLimit;

    /// Flag to check for legacy string compatibility
    bool                    m_LegacyString;

    /// Field ownership flag
    bool                    m_OwnFields;


    // pack optimization fields

    /// Pack optimization flag (turns TRUE on first Pack call)
    bool                    m_PackOptComputed;
    /// Index of first variable length field
    unsigned int            m_FirstVarFieldIdx;
    /// Buffer offset of first variable length field
    unsigned int            m_FirstVarFieldIdxOffs;

private:
    friend class CBDB_Field;
    friend class CBDB_BLobFile;
    friend class CBDB_File;
    friend class CBDB_FileCursor;
    friend class CBDB_FC_Condition;
};

/// Class factory for BDB field types
class NCBI_BDB_EXPORT CBDB_FieldFactory
{
public:
    enum EType
    {
        eUnknown,

        eString,
        eLString,
        eInt8,
        eInt4,
        eUint4,
        eInt2,
        eUint1,
        eFloat,
        eDouble,
        eUChar,
        eBlob
    };

public:
    CBDB_FieldFactory();

    /// Return type enumerator by string type (case insensitive)
    EType GetType(const string& type) const;


    CBDB_Field* Create(EType type) const;

    /// Create field type by string. Caller is responsible for deletion.
    ///
    /// @param type
    ///    string type ("string", "int4", "double"). Case insensitive.

    CBDB_Field* Create(const string& type) const;
};


/////////////////////////////////////////////////////////////////////////////
///
/// Type trait classes for finding the correct BDB field type automagically
///

///
/// SBDB_TypeTraits defines via template parameters a policy for mapping a
/// built-in type to a BDB field type.
///
template <class Type>
struct SBDB_TypeTraits
{
    typedef Type TType;
    typedef void* TFieldType;
};


///
/// 8-bit signed and unsigned integral types
///
template<>
struct SBDB_TypeTraits<Int1>
{
    typedef Int4 TType;
    typedef CBDB_FieldInt1 TFieldType;
};

template<>
struct SBDB_TypeTraits<Uint1>
{
    typedef Uint4 TType;
    typedef CBDB_FieldUint1 TFieldType;
};


///
/// 16-bit signed and unsigned integral types
///
template<>
struct SBDB_TypeTraits<Int2>
{
    typedef Int4 TType;
    typedef CBDB_FieldInt2 TFieldType;
};

template<>
struct SBDB_TypeTraits<Uint2>
{
    typedef Uint4 TType;
    typedef CBDB_FieldUint2 TFieldType;
};


///
/// 32-bit signed and unsigned integral types
///
template<>
struct SBDB_TypeTraits<Int4>
{
    typedef Int4 TType;
    typedef CBDB_FieldInt4 TFieldType;
};

template<>
struct SBDB_TypeTraits<Uint4>
{
    typedef Uint4 TType;
    typedef CBDB_FieldUint4 TFieldType;
};


///
/// 64-bit signed and unsigned integral types
///
template<>
struct SBDB_TypeTraits<Int8>
{
    typedef Int8 TType;
    typedef CBDB_FieldInt8 TFieldType;
};

template<>
struct SBDB_TypeTraits<Uint8>
{
    typedef Uint8 TType;
    typedef CBDB_FieldUint8 TFieldType;
};


///
/// string types
///
template<>
struct SBDB_TypeTraits<string>
{
    typedef string TType;
    typedef CBDB_FieldString TFieldType;
};


///
/// 32- and 64- bit floating point types
///
template<>
struct SBDB_TypeTraits<float>
{
    typedef float TType;
    typedef CBDB_FieldFloat TFieldType;
};

template<>
struct SBDB_TypeTraits<double>
{
    typedef double TType;
    typedef CBDB_FieldDouble TFieldType;
};


/* @} */

/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
//  CBDB_Field::
//


inline bool CBDB_Field::IsVariableLength() const
{
    return m_Flags.VariableLength == 1;
}


inline size_t CBDB_Field::GetBufferSize() const
{
    return m_BufferSize;
}


inline bool CBDB_Field::IsBufferAttached() const
{
    return m_Flags.Attached == 1;
}


inline bool CBDB_Field::IsNullable() const
{
    return m_Flags.Nullable == 1;
}


inline void CBDB_Field::SetNullable()
{
    m_Flags.Nullable = 1;
}


inline void CBDB_Field::SetNotNull()
{
    m_BufferManager->SetNull(m_BufferIdx, false);
}


inline void CBDB_Field::SetNull()
{
    _ASSERT(m_BufferManager->IsNullable());
    m_BufferManager->SetNull(m_BufferIdx, true);
}


inline bool CBDB_Field::IsNull() const
{
    return m_BufferManager->IsNull(m_BufferIdx);
}


inline void* CBDB_Field::Unpack()
{
    _ASSERT(m_BufferManager);
    m_BufferManager->Unpack();
    return GetBuffer();
}


inline void CBDB_Field::SetBuffer(void* buf, size_t buf_size)
{
    m_Buffer = buf;
    if ( buf_size )
        m_BufferSize = buf_size;
    m_Flags.Attached = 1;
}


inline void CBDB_Field::SetBufferSize(size_t buf_size)
{
    _ASSERT(buf_size != 0);
    m_BufferSize = buf_size;
}


inline void CBDB_Field::SetDataSize(size_t size)
{
    m_BufferSize = size + GetExtraDataLength();
}


inline void CBDB_Field::SetBufferIdx(unsigned int idx)
{
    m_BufferIdx = idx;
}


inline const void* CBDB_Field::GetBuffer() const
{
    return m_Buffer;
}


inline void* CBDB_Field::GetBuffer()
{
    return m_Buffer;
}


inline void CBDB_Field::SetBufferManager(CBDB_BufferManager* owner)
{
    _ASSERT(owner);
    m_BufferManager = owner;
}


inline size_t CBDB_Field::GetLength() const
{
    return GetDataLength(m_Buffer);
}


inline int CBDB_Field::CompareWith(const CBDB_Field& field) const
{
    bool byte_swapped = m_BufferManager->IsByteSwapped();
    return Compare(GetBuffer(), field.GetBuffer(), byte_swapped);
}


inline bool CBDB_Field::IsSameType(const CBDB_Field& field) const
{
    try {
        const type_info& t1 = typeid(*this);
        const type_info& t2 = typeid(field);
        return (t1 == t2) != 0;
    } catch (...) {
    }
    return false;
}



inline void CBDB_Field::CopyFrom(const CBDB_Field& src)
{
    if (this == &src)
        return;

    if ( !IsSameType(src) ) {
        BDB_THROW(eType, "Wrong field type");
    }

    CopyFrom(src.GetBuffer());
}


inline void CBDB_Field::CopyFrom(const void* src_buf)
{
    _ASSERT(src_buf);

    void* dst_ptr = Unpack();
    _ASSERT(dst_ptr);

    size_t max_len  = GetBufferSize();
    size_t copy_len = GetDataLength(src_buf);

    if (copy_len > max_len) {
        BDB_THROW(eOverflow, "Cannot copy. Data length exceeds max value");
    }
    ::memcpy(dst_ptr, src_buf, copy_len);
    SetNotNull();
}


inline const string& CBDB_Field::GetName() const
{
    return m_Name;
}


inline void CBDB_Field::SetName(const char* name)
{
    _ASSERT(name);
    m_Name = name;
}

inline bool CBDB_Field::IsByteSwapped() const
{
    return m_BufferManager->IsByteSwapped();
}

/////////////////////////////////////////////////////////////////////////////
//  CBDB_FieldString::
//

inline CBDB_FieldFixedByteString::operator const char* () const
{
    const char* str = (const char*) GetBuffer();
    _ASSERT(str);
    return str;
}


inline
void CBDB_FieldFixedByteString::Set(const char* str)
{
    ::memcpy((char*)GetBuffer(), str, GetBufferSize());
}

inline
string CBDB_FieldFixedByteString::Get() const
{
    _ASSERT(!IsNull());

    const char* buf = (const char*) GetBuffer();
    return string(buf, GetBufferSize());
}

inline
int CBDB_FieldFixedByteString::Compare(const void* p1,
                                       const void* p2,
                                       bool /*byte_swapped*/) const
{
    return ::memcmp(p1, p2, GetBufferSize());
}

inline
size_t CBDB_FieldFixedByteString::GetDataLength(const void* buf) const
{
    return GetBufferSize();
}

inline
void CBDB_FieldFixedByteString::SetMinVal()
{
    void* buf = Unpack();
    ::memset(buf, 0, GetBufferSize());
}

inline
void CBDB_FieldFixedByteString::SetMaxVal()
{
    void* buf = Unpack();
    ::memset(buf, 0xFF, GetBufferSize());
}

/////////////////////////////////////////////////////////////////////////////
//  CBDB_FieldString::
//

inline CBDB_FieldString::CBDB_FieldString()
    : CBDB_FieldStringBase()
{
    SetBufferSize(256);
}


inline CBDB_FieldString::operator const char* () const
{
    const char* str = (const char*) GetBuffer();
    _ASSERT(str);
    return str;
}


inline
CBDB_FieldString&
CBDB_FieldString::operator= (const CBDB_FieldString& str)
{
    if (this == &str)
        return *this;

    size_t len = str.GetLength();
    if (len > GetBufferSize()) {
        // TODO: allow partial string assignment?
        BDB_THROW(eOverflow, "String field overflow.");
    }
    Unpack();
    ::strcpy((char*)GetBuffer(), (const char*) str);

    if ( str.IsNull() ) {
        SetNull();
    } else {
        SetNotNull();
    }

    return *this;
}


inline
void CBDB_FieldString::Set(const char* str, EOverflowAction if_overflow)
{
    if ( !str )
        str = kEmptyCStr;

    size_t new_len = ::strlen(str) + 1;

    // check overflow
    if (new_len > GetBufferSize()) {
        switch (if_overflow) {
        case eTruncateOnOverflowLogError:
            LOG_POST(Warning << "Value truncated for field '"
                    << GetName() << "'");
            /* FALLTHROUGH */
        case eTruncateOnOverflow:
            new_len = GetBufferSize();
            break;
        case eThrowOnOverflow:
            string msg("String field '");
            msg += GetName();
            msg += "' overflow: max size = ";
            msg += NStr::SizetToString(GetBufferSize());
            msg += ", assignee size = ";
            msg += NStr::SizetToString(new_len);
            BDB_THROW(eOverflow, msg);
            break;
        }
    }
    Unpack();
    ::memcpy(GetBuffer(), str, new_len);
    SetNotNull();
}

inline
void CBDB_FieldString::SetString(const char* str)
{
    operator=(str);
}

inline int CBDB_FieldString::Compare(const void* p1,
                                     const void* p2,
                                     bool /*byte_swapped*/) const
{
    _ASSERT(p1 && p2);
    return ::strcmp((const char*) p1, (const char*) p2);
}


inline void CBDB_FieldString::SetMinVal()
{
    ((char*) Unpack())[0] = '\0';
}


inline void CBDB_FieldString::SetMaxVal()
{
    void* buf = Unpack();
    // TODO:  find out whether xFF or 0x7F should be used, and how
    //        (because of possible 'signed char' comparison in strcmp()).
    ::memset(buf, 0x7F, GetBufferSize()); // 0xFF for international
    ((char*) buf)[GetBufferSize() - 1] = '\0';

    SetNotNull();
}


inline bool CBDB_FieldString::IsEmpty() const
{
    const char* str = (const char*) GetBuffer();
    _ASSERT(str);
    return !*str;
}


inline bool CBDB_FieldString::IsBlank() const
{
    const char* str = (const char*) GetBuffer();
    _ASSERT(str);
    for (;  *str;  ++str) {
        if ( !isspace((unsigned char)(*str)) )
            return false;
    }
    return true;
}


inline size_t CBDB_FieldString::GetDataLength(const void* buf) const
{
    _ASSERT(buf);
    return ::strlen((const char*) buf) + 1;
}


inline CBDB_FieldString& CBDB_FieldString::operator= (const char* str)
{
    Set(str, eThrowOnOverflow);
    return *this;
}

inline CBDB_FieldString& CBDB_FieldString::operator= (const string& str)
{
    return this->operator=(str.c_str());
}



/////////////////////////////////////////////////////////////////////////////
//  CBDB_BufferManager::
//


inline const CBDB_Field& CBDB_BufferManager::GetField(unsigned int n) const
{
    return *m_Fields[n];
}


inline CBDB_Field& CBDB_BufferManager::GetField(unsigned int n)
{
    return *m_Fields[n];
}


inline unsigned int CBDB_BufferManager::FieldCount() const
{
    return (unsigned int)m_Fields.size();
}


inline bool CBDB_BufferManager::IsPackable() const
{
    return m_Packable;
}


inline bool CBDB_BufferManager::IsNullable() const
{
    return m_Nullable;
}


inline void CBDB_BufferManager::SetNullable()
{
    _ASSERT(m_Buffer == 0);
    m_Nullable = true;
}


inline size_t CBDB_BufferManager::ComputeNullSetSize() const
{
    if ( !IsNullable() )
        return 0;

    return (FieldCount() + 7) / 8;
}


inline bool CBDB_BufferManager::TestNullBit(unsigned int n) const
{
    _ASSERT(IsNullable());

    const unsigned char* buf  = (unsigned char*) m_Buffer;
    unsigned char        mask = (unsigned char) (1 << (n & 7));
    const unsigned char* offs = buf + (n >> 3);

    return ((*offs) & mask) != 0;
}


inline void CBDB_BufferManager::SetNullBit(unsigned int n, bool value)
{
    _ASSERT(IsNullable());

    unsigned char* buf  = (unsigned char*) m_Buffer;
    unsigned char  mask = (unsigned char) (1 << (n & 7));
    unsigned char* offs = buf + (n >> 3);

    (void) (value ? (*offs |= mask) : (*offs &= ~mask));
}


inline void CBDB_BufferManager::SetNull(unsigned int field_idx, bool value)
{
    if ( !IsNullable() )
        return;
    _ASSERT(field_idx < m_Fields.size());
    SetNullBit(field_idx, value);
}


inline void CBDB_BufferManager::SetAllNull()
{
    if ( !IsNullable() )
        return;
    unsigned char* buf = (unsigned char*) m_Buffer;
    for (size_t i = 0;  i < m_NullSetSize;  ++i) {
        *buf++ = (unsigned char) 0xFF;
    }
}


inline bool CBDB_BufferManager::HasNull() const
{
    if (IsNullable()) {
        for (size_t i = 0;  i < m_NullSetSize;  ++i) {
            if (m_Buffer[i])
                return true;
        }
    }
    return false;
}


inline bool CBDB_BufferManager::IsNull(unsigned field_idx) const
{
    if ( !IsNullable() )
        return false;

    _ASSERT(field_idx < m_Fields.size());

    return TestNullBit(field_idx);
}


inline void CBDB_BufferManager::ArrangePtrsUnpacked()
{
    if ( !m_PackedSize )
        return;

    if ( !IsPackable() ) {
        m_PackedSize = 0;
        return;
    }

    for (size_t i = 0;  i < m_Fields.size();  ++i) {
        CBDB_Field& df = GetField((unsigned)i);
        df.SetBuffer(m_Ptrs[i]);
    }

    m_PackedSize = 0;  // not packed
}


inline void CBDB_BufferManager::Clear()
{
    ::memset(m_Buffer, 0, m_BufferSize);
    ArrangePtrsUnpacked();
}


inline
void CBDB_BufferManager::CopyFieldsFrom(const CBDB_BufferManager& buf_mgr)
{
    unsigned int field_count = min(FieldCount(), buf_mgr.FieldCount());

    for (unsigned int i = 0;  i < field_count;  ++i) {
        CBDB_Field* fld = m_Fields[i];
        fld->CopyFrom(buf_mgr.GetField(i));
    }
}


inline
void CBDB_BufferManager::SetMinVal(unsigned int idx_from, unsigned int idx_to)
{
    _ASSERT(idx_to <= FieldCount());

    for ( ;  idx_from < idx_to;  ++idx_from) {
        CBDB_Field& fld = GetField(idx_from);
        fld.SetMinVal();
    }
}


inline
void CBDB_BufferManager::SetMaxVal(unsigned int idx_from, unsigned int idx_to)
{
    _ASSERT(idx_to <= FieldCount());

    for ( ;  idx_from < idx_to;  ++idx_from) {
        CBDB_Field& fld = GetField(idx_from);
        fld.SetMaxVal();
    }
}

inline
void CBDB_BufferManager::SetFieldCompareLimit(unsigned int n_fields)
{
    m_CompareLimit = n_fields;
}

inline
unsigned int CBDB_BufferManager::GetFieldCompareLimit() const
{
    return m_CompareLimit;
}

// Delete all field objects bound to field buffer.
// Should be used with extreme caution. After calling this buf object should
// never be used again. All fields bound to the buffer must be dynamically
// allocated.
inline
void DeleteFields(CBDB_BufferManager& buf)
{
    for (unsigned int i = 0;  i < buf.FieldCount();  ++i) {
        CBDB_Field* fld = &buf.GetField(i);
        delete fld;
    }
}


END_NCBI_SCOPE



#endif  /* BDB_TYPES__HPP */
