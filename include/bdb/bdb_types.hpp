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

#include <bdb/bdb_expt.hpp>
#include <corelib/ncbi_limits.hpp>

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

    typedef int (*BDB_CompareFunction)(DB*, const DBT*, const DBT*);



// Simple and fast comparison function for tables with 
// non-segmented "unsigned int" keys
int BDB_UintCompare(DB*, const DBT* val1, const DBT* val2);


// Simple and fast comparison function for tables with 
// non-segmented "int" keys
int BDB_IntCompare(DB*, const DBT* val1, const DBT* val2);

// Simple and fast comparison function for tables with 
// non-segmented "float" keys
int BDB_FloatCompare(DB*, const DBT* val1, const DBT* val2);

// Simple and fast comparison function for tables with 
// non-segmented "double" keys
int BDB_DoubleCompare(DB*, const DBT* val1, const DBT* val2);

// Simple and fast comparison function for tables with 
// non-segmented "C string" keys
int BDB_StringCompare(DB*, const DBT* val1, const DBT* val2);

// Simple and fast comparison function for tables with 
// non-segmented "case insensitive C string" keys
int BDB_StringCaseCompare(DB*, const DBT* val1, const DBT* val2);

// General purpose DBD comparison function
int BDB_Compare(DB* db, const DBT* val1, const DBT* val2);


}

BEGIN_NCBI_SCOPE


class CBDB_BufferManager;
class CBDB_Field;
class CBDB_File;
class CBDB_FileCursor;
class CBDB_FC_Condition;


//////////////////////////////////////////////////////////////////
//
// BDB Data Field interface definition.
//

class NCBI_BDB_EXPORT IBDB_Field
{
public:
    // Comparison function. p1 and p2 are void pointers on field buffers.
    // Positive if p1>p2, zero if p1==p2, negative if p1<p2.
    // NOTE:  both buffers can be unaligned. 
    virtual int         Compare(const void* p1, 
                                const void* p2) const = 0;

    // Return current effective size of the buffer.
    virtual size_t      GetDataLength(const void* buf) const = 0;

    // Set minimal possible value for the field type
	virtual void        SetMinVal() = 0;

    // Set maximum possible value for the field type
	virtual void        SetMaxVal() = 0;
};



//////////////////////////////////////////////////////////////////
//
// BDB Data Field conversion interface definition.
// All interface functions by default throw "bad conversion" exception.
//

class NCBI_BDB_EXPORT IBDB_FieldConvert
{
public:
    virtual void SetInt(int)
        { BDB_THROW(eType, "Bad conversion"); }
    virtual void SetUint(unsigned)  // ???
        { BDB_THROW(eType, "Bad conversion"); }
    virtual void SetString(const char*)
        { BDB_THROW(eType, "Bad conversion"); }
    virtual void SetFloat(float)
        { BDB_THROW(eType, "Bad conversion"); }
    virtual void SetDouble(double)
        { BDB_THROW(eType, "Bad conversion"); }
};



//////////////////////////////////////////////////////////////////
//
// Interface definition class for field construction.
// Used for interfaces' "access rights management".
// Some interfaces are declared non-public here (IFieldConvert).
//

class NCBI_BDB_EXPORT CBDB_FieldInterfaces : public    IBDB_Field,
                                             protected IBDB_FieldConvert
{
    friend class CBDB_FileCursor;
};



//////////////////////////////////////////////////////////////////
//
// Base class for constructing BDB fields. Implements field buffer pointer.
//
// All DBD field types do not own their buffers. 
// It works as a pointer on external memory kept by the record manager
// (CBDB_FieldBuf). Class cannot be used independently without record manager.
//

class NCBI_BDB_EXPORT CBDB_Field : public CBDB_FieldInterfaces
{
public:
    //
    enum ELengthType {
        eFixedLength,    // fixed-length (like int)
        eVariableLength  // variable-length (like string)
    };
    CBDB_Field(ELengthType length_type = eFixedLength);
    virtual ~CBDB_Field() {}
    
    // Virtual constructor - class factory for BDB fields.
    // Default (zero) value of 'buf-len' uses GetBufferSize().
    // For fixed length fields this buf_size parameter has no effect
    virtual CBDB_Field* Construct(size_t buf_size = 0) const = 0;

    // Return address to the type specific comparison function
    // By default it's universal BDB_Compare
    virtual BDB_CompareFunction GetCompareFunction() const; 

    // Return TRUE if field can be NULL
    bool IsNullable() const;

    // Assign field value to NULL
    void SetNull();

    // Return TRUE if field is NULL
    bool IsNull() const;

    // Return symbolic name for the field
    const string& GetName() const;

protected:
    // Return maximum possible buffer length
    size_t GetBufferSize() const;

    // Get length of the actual data
    size_t GetLength() const;

    // Field comparison function
    int CompareWith(const CBDB_Field& field) const;

    // Copies field value from another field.
    // The field type MUST be the same, or an exception will be thrown.
    void CopyFrom(const CBDB_Field& src);

    // Buffer management functions:

    // Return TRUE if it is a variable length variable (like string)
    bool IsVariableLength() const;

    // Return TRUE if external buffer has already been attached
    bool IsBufferAttached() const;

    // RTTI based check if fld is of the same type
    bool IsSameType(const CBDB_Field& field) const;

    // Mark field as "NULL" capable.
    void SetNullable();

    // Set "is NULL" flag to FALSE
    void SetNotNull();

    // Set symbolic name for the field
    void SetName(const char* name);

    // Set field position in the buffer manager
    void SetBufferIdx(unsigned int idx);


protected:
    // Unpack the buffer which contains this field (using CBDB_BufferManager).
    // Return new pointer to the field data -- located in the unpacked buffer.
    void*  Unpack();
    // Set external buffer pointer and length. 
    // Zero 'buf_size' means GetBufferSize() is used to obtain the buf. size.
    void  SetBuffer(void* buf, size_t buf_size = 0);
    // Set the buffer size.
    void  SetBufferSize(size_t size);
    // Set CBDB_BufferManager -- which works as a memory manager for BDB fields.
    void  SetBufferManager(CBDB_BufferManager* owner);

    // Get pointer to the data. NULL if not yet attached.
    const void* GetBuffer() const;
    void*       GetBuffer();

    // Copy buffer value from the external source
    void CopyFrom(const void* src_buf);

private:
    CBDB_Field& operator= (const CBDB_Field& data);
    CBDB_Field(const CBDB_Field& data);

private:
    CBDB_BufferManager*  m_BufferManager;
    struct {
        unsigned VariableLength : 1;
        unsigned Attached       : 1;
        unsigned Nullable       : 1;
    } m_Flags;
    void*      m_Buffer;       // Pointer to the field data (in buf. mgr.)
    size_t     m_BufferSize;   // Field data buffer capacity
    unsigned   m_BufferIdx;    // Fields' position in the managing buffer
    string     m_Name;         // Field's symbolic name

    // Friends
    friend class CBDB_BufferManager;
    friend class CBDB_File;
    friend class CBDB_BLobFile;
};



//////////////////////////////////////////////////////////////////
//
// Template class for building simple scalar data fields. 
// (int, float, double)
//

template<typename T>
class CBDB_FieldSimple : public CBDB_Field
{
public:
    CBDB_FieldSimple()
    : CBDB_Field(eFixedLength)
    {
        SetBufferSize(sizeof(T));
    }

    void Set(T val)
    {
        ::memcpy(GetBuffer(), &val, sizeof(T));
        SetNotNull();
    }

    void Set(const CBDB_FieldSimple& field)
    {
        if ( field.IsNull() ) {
            SetNull();
        } else {
            ::memcpy(GetBuffer(), field.GetBuffer(), sizeof(T));
            SetNotNull();
        }
    }

    T Get() const
    {
        T  v;
        ::memcpy(&v, GetBuffer(), sizeof(T));
        return v;
    }

    // Accessor operator
    operator T() const
    {
        return Get();
    }


    // IDBD_Field implementation

    virtual int Compare(const void* p1, const void* p2) const
    {
        T v1, v2;
        ::memcpy(&v1, p1, sizeof(v1));
        ::memcpy(&v2, p2, sizeof(v2));
        if (v1 < v2) return -1;
        if (v2 < v1) return 1;
        return 0;
    }

    virtual size_t GetDataLength(const void* /*buf*/) const
    {
        return sizeof(T);
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



//////////////////////////////////////////////////////////////////
//
// Template class for building simple scalar integer compatible data fields.
// (int, short, char)
//

template<typename T>
class CBDB_FieldSimpleInt : public CBDB_FieldSimple<T>
{
public:
    CBDB_FieldSimpleInt() : CBDB_FieldSimple<T>() {}

    virtual void SetInt    (int          val)  { Set((T) val); }
    virtual void SetUint   (unsigned int val)  { Set((T) val); }
    virtual void SetString (const char*  val)
    {
        long v = ::atol(val);
        Set((T) v);
    }
    
};



//////////////////////////////////////////////////////////////////
//
// Template class for building simple scalar floating point compatible fields.
// (float, double)
//

template<typename T>
class CBDB_FieldSimpleFloat : public CBDB_FieldSimple<T>
{
public:
    CBDB_FieldSimpleFloat() : CBDB_FieldSimple<T>() {}

    virtual void SetInt    (int          val) { Set((T) val); }
    virtual void SetUint   (unsigned int val) { Set((T) val); }
    virtual void SetString (const char*  val)
    {
        double v = ::atof(val);
        Set((T) v);
    }
    virtual void SetFloat (float  val) { Set((T) val); }
    virtual void SetDouble(double val) { Set((T) val); }
};



//////////////////////////////////////////////////////////////////
//
//  Int4 field type
//

class NCBI_BDB_EXPORT CBDB_FieldInt4 : public CBDB_FieldSimpleInt<Int4>
{
public:
    const CBDB_FieldInt4& operator= (Int4 val)
    {
        Set(val);
        return *this;
    }

    const CBDB_FieldInt4& operator= (const CBDB_FieldInt4& val)
    {
        Set(val);
        return *this;
    }

    virtual CBDB_Field* Construct(size_t /*buf_size*/) const
    {
        return new CBDB_FieldInt4();
    }

    operator Int4() const 
    { 
        return Get(); 
    }

    virtual BDB_CompareFunction GetCompareFunction() const
    {
        return BDB_IntCompare;
    } 

};



//////////////////////////////////////////////////////////////////
//
//  Uint4 field type
//

class NCBI_BDB_EXPORT CBDB_FieldUint4 : public CBDB_FieldSimpleInt<Uint4>
{
public:
    const CBDB_FieldUint4& operator= (Uint4 val)
    {
        Set(val);
        return *this;
    }

    const CBDB_FieldUint4& operator= (const CBDB_FieldUint4& val)
    {
        Set(val);
        return *this;
    }

    virtual CBDB_Field* Construct(size_t /*buf_size*/) const
    {
        return new CBDB_FieldUint4();
    }

    operator Uint4() const
    { 
        return Get(); 
    }

    virtual BDB_CompareFunction GetCompareFunction() const
    {
        return BDB_UintCompare;
    } 

};



//////////////////////////////////////////////////////////////////
//
//  Float field type
//

class NCBI_BDB_EXPORT CBDB_FieldFloat : public CBDB_FieldSimpleFloat<float>
{
public:
    const CBDB_FieldFloat& operator= (float val)
    {
        Set(val);
        return *this;
    }

    const CBDB_FieldFloat& operator= (const CBDB_FieldFloat& val)
    {
        Set(val);
        return *this;
    }

    virtual CBDB_Field* Construct(size_t /*buf_size*/) const
    {
        return new CBDB_FieldFloat();
    }

    operator float() const 
    { 
        return Get(); 
    }

    virtual BDB_CompareFunction GetCompareFunction() const
    {
        return BDB_FloatCompare;
    } 
};


//////////////////////////////////////////////////////////////////
//
//  Double precision floating point field type
//

class NCBI_BDB_EXPORT CBDB_FieldDouble : public CBDB_FieldSimpleFloat<double>
{
public:
    const CBDB_FieldDouble& operator= (double val)
    {
        Set(val);
        return *this;
    }

    const CBDB_FieldDouble& operator= (const CBDB_FieldDouble& val)
    {
        Set(val);
        return *this;
    }

    virtual CBDB_Field* Construct(size_t /*buf_size*/) const
    {
        return new CBDB_FieldDouble();
    }

    operator double() const 
    { 
        return Get(); 
    }

    virtual BDB_CompareFunction GetCompareFunction() const
    {
        return BDB_DoubleCompare;
    } 
};



//////////////////////////////////////////////////////////////////
//
//  String field type
//

class NCBI_BDB_EXPORT CBDB_FieldString : public CBDB_Field
{
public:
    CBDB_FieldString();

    // Class factory for string fields.
    // Default (zero) value of 'buf_size' uses GetBufferSize().
    virtual CBDB_Field* Construct(size_t buf_size) const
    {
        CBDB_FieldString* fld = new CBDB_FieldString();
        fld->SetBufferSize(buf_size ? buf_size : GetBufferSize());
        return fld;
    }

    operator const char* () const;
    const CBDB_FieldString& operator= (const CBDB_FieldString& str);
    const CBDB_FieldString& operator= (const char*             str);
    const CBDB_FieldString& operator= (const string&           str);

    // if mute == true function do not report overflow condition, but just
    // truncates the string value
    enum EOverflowAction {
        eThrowOnOverflow,
        eTruncateOnOverflow
    };
    void Set(const char* str, EOverflowAction if_overflow = eThrowOnOverflow);
    string Get() const { return string((const char*)GetBuffer()); }

    bool IsEmpty() const;
    bool IsBlank() const;


    // IField
    virtual int         Compare(const void* p1, const void* p2) const;
    virtual size_t      GetDataLength(const void* buf) const;
    virtual void        SetMinVal();
    virtual void        SetMaxVal();

    virtual void SetString(const char*);

    virtual BDB_CompareFunction GetCompareFunction() const
    {
        return BDB_StringCompare;
    } 
};



//////////////////////////////////////////////////////////////////
//
//  Case-insensitive string field type 
//

class NCBI_BDB_EXPORT CBDB_FieldStringCase : public CBDB_FieldString
{
public:
    typedef CBDB_FieldString CParent;

    explicit CBDB_FieldStringCase() : CBDB_FieldString() {}

    // Accessors
    operator const char* () const { return (const char*) GetBuffer(); }
	
    const CBDB_FieldStringCase& operator= (const CBDB_FieldString& str)
    {
        Set(str);
        return *this;
    }
    const CBDB_FieldStringCase& operator= (const CBDB_FieldStringCase& str)
    {
        Set(str);
        return *this;
    }
    const CBDB_FieldStringCase& operator= (const char* str) 
    { 
        Set(str);
        return *this;
    }
    const CBDB_FieldStringCase& operator= (const string& str) 
    { 
        Set(str.c_str());
        return *this;
    }

    virtual int Compare(const void* p1, const void* p2) const
    {
        _ASSERT(p1 && p2);
        return NStr::strcasecmp((const char*) p1, (const char*) p2);
    }

    virtual BDB_CompareFunction GetCompareFunction() const
    {
        return BDB_StringCaseCompare;
    } 

};



//////////////////////////////////////////////////////////////////
// 
// BDB Data Field Buffer manager class. 
// For internal use in BDB library.
//

class NCBI_BDB_EXPORT CBDB_BufferManager
{
public:
    // Return number of fields attached using function Bind
    unsigned int FieldCount() const;

    const CBDB_Field& GetField(unsigned int idx) const;
    CBDB_Field&       GetField(unsigned int idx);

protected:
    CBDB_BufferManager();

    // Create internal data buffer, assign places in this buffer to the fields
    void Construct();

    // Set minimum possible value to buffer fields from 'idx_from' to 'idx_to'
    void SetMinVal(unsigned int idx_from, unsigned int idx_to);

    // Set maximum possible value to buffer fields from 'idx_from' to 'idx_to'
    void SetMaxVal(unsigned int idx_from, unsigned int idx_to);

    // Attach 'field' to the buffer.
    // NOTE: buffer manager will not own the attached object, nor will it
    //       keep ref counters or do any other automagic memory management.
    void Bind(CBDB_Field* field, ENullable is_nullable = eNotNullable);

    // Duplicate (dynamic allocation is used) all fields from 'buf_mgr' and
    // bind them to the this buffer manager. Field values are not copied.
    // NOTE: CBDB_BufferManager does not own or deallocate fields, 
    //       caller is responsible for deallocation.
    void CopyFieldsFrom(const CBDB_BufferManager& buf_mgr);

    // Copy all field values from the 'buf_mgr'.
    void DuplicateStructureFrom(const CBDB_BufferManager& buf_mgr);

    // Compare fields of this buffer with those of 'buf_mgr' using
    // CBDB_Field::CompareWith().
    // Optional 'n_fields' parameter used when we want to compare only
    // several first fields instead of all.
    int Compare(const CBDB_BufferManager& buf_mgr,
                unsigned int              n_fields = 0) const;

    // Return TRUE if any field bound to this buffer manager has variable
    // length (i.e. packable)
    bool IsPackable() const;

    // Check if all NOT NULLABLE fields were assigned.
    // Throw an exception if not.
    void CheckNullConstraint() const;

    void ArrangePtrsUnpacked();
    void ArrangePtrsPacked();
    void Clear();
    unsigned Pack();
    unsigned Unpack();

    // Pack the buffer and initialize DBT structure for write operation
    void PrepareDBT_ForWrite(DBT* dbt);

    // Initialize DBT structure for read operation.
    void PrepareDBT_ForRead(DBT* dbt);

    // Calculate buffer size
    size_t ComputeBufferSize() const;

    // Return TRUE if buffer can carry NULL fields
    bool IsNullable() const;

    // Mark buffer as "NULL fields ready".
    // NOTE: Should be called before buffer construction.
    void SetNullable();

    void SetNull(unsigned int field_idx, bool value);
    bool IsNull (unsigned int field_idx) const;

    size_t ComputeNullSetSize() const;
    bool   TestNullBit(unsigned int idx) const;
    void   SetNullBit (unsigned int idx, bool value);
    void   SetAllNull();

    // Return buffer compare function
    BDB_CompareFunction GetCompareFunction() const;

private:
    CBDB_BufferManager(const CBDB_BufferManager&);
    CBDB_BufferManager& operator= (const CBDB_BufferManager&);

private:
    vector<CBDB_Field*>     m_Fields;
    vector<void*>           m_Ptrs;        // pointers on the fields' data
    auto_ptr<char>          m_Buffer;
    size_t                  m_BufferSize;
    size_t                  m_PackedSize;
    bool                    m_Packable;

    bool                    m_Nullable;    // TRUE if buffer can carry NULL fields
    size_t                  m_NullSetSize; // size of the 'is NULL' bitset in bytes

private:
    friend class CBDB_Field;
    friend class CBDB_BLobFile;
    friend class CBDB_File;
    friend class CBDB_FileCursor;
    friend class CBDB_FC_Condition;
};



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
    return Compare(GetBuffer(), field.GetBuffer());
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


/////////////////////////////////////////////////////////////////////////////
//  CBDB_FieldString::
//

inline CBDB_FieldString::CBDB_FieldString()
    : CBDB_Field(eVariableLength)
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
const CBDB_FieldString&
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
        if (if_overflow == eTruncateOnOverflow) {
            new_len = GetBufferSize();
        } else {
            string message("String field overflow."); 
            // TODO: add info what caused overflow, what length expected
            BDB_THROW(eOverflow, message);
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

inline int CBDB_FieldString::Compare(const void* p1, const void* p2) const
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
        if ( !isspace(*str) )
            return false;
    }
    return true;
}


inline size_t CBDB_FieldString::GetDataLength(const void* buf) const
{
    _ASSERT(buf);
    return ::strlen((const char*) buf) + 1;
}


inline const CBDB_FieldString& CBDB_FieldString::operator= (const char* str)
{ 
    Set(str, eThrowOnOverflow); 
    return *this;
}

inline const CBDB_FieldString& CBDB_FieldString::operator= (const string& str)
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
    return m_Fields.size();
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
    _ASSERT(m_Buffer.get() == 0);
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

    const unsigned char* buf  = (unsigned char*) m_Buffer.get();
    unsigned char        mask = (unsigned char) (1 << (n & 7));
    const unsigned char* offs = buf + (n >> 3);

    return ((*offs) & mask) != 0;
}


inline void CBDB_BufferManager::SetNullBit(unsigned int n, bool value)
{
    _ASSERT(IsNullable());

    unsigned char* buf  = (unsigned char*) m_Buffer.get();
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
    _ASSERT(IsNullable());
    unsigned char* buf = (unsigned char*) m_Buffer.get();
    for (size_t i = 0;  i < m_NullSetSize;  ++i) {
        *buf++ = (unsigned char) 0xFF;
    }
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
        CBDB_Field& df = GetField(i);
        df.SetBuffer(m_Ptrs[i]);
    }

    m_PackedSize = 0;  // not packed
}


inline void CBDB_BufferManager::Clear()
{
    ::memset(m_Buffer.get(), 0, m_BufferSize);
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


// Delete all field objects bound to field buffer. 
// Should be used with extreme caution. After calling this buf object should
// never be used again. All fields bound to the buffer must be dynamically
// allocated.
inline void DeleteFields(CBDB_BufferManager& buf)
{
    for (unsigned int i = 0;  i < buf.FieldCount();  ++i) {
        CBDB_Field* fld = &buf.GetField(i);
        delete fld;
    }
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.14  2003/07/25 15:46:47  kuznets
 * Added support for double field type
 *
 * Revision 1.13  2003/07/23 20:21:27  kuznets
 * Implemented new improved scheme for setting BerkeleyDB comparison function.
 * When table has non-segmented key the simplest(and fastest) possible function
 * is assigned (automatically without reloading CBDB_File::SetCmp function).
 *
 * Revision 1.12  2003/07/22 16:05:20  kuznets
 * Resolved operators ambiguity (GCC-MSVC conflict of interests)
 *
 * Revision 1.11  2003/07/22 15:48:34  kuznets
 * Fixed minor compilation issue with GCC
 *
 * Revision 1.10  2003/07/16 13:32:04  kuznets
 * Implemented CBDB_FieldString::SetString (part of IBDB_FieldConvert interface)
 *
 * Revision 1.9  2003/07/02 17:53:59  kuznets
 * Eliminated direct dependency from <db.h>
 *
 * Revision 1.8  2003/06/27 18:57:16  dicuccio
 * Uninlined strerror() adaptor.  Changed to use #include<> instead of #include ""
 *
 * Revision 1.7  2003/06/10 20:07:27  kuznets
 * Fixed header files not to repeat information from the README file
 *
 * Revision 1.6  2003/06/03 18:50:09  kuznets
 * Added dll export/import specifications
 *
 * Revision 1.5  2003/05/27 16:13:21  kuznets
 * Destructors of key classes declared virtual to make GCC happy
 *
 * Revision 1.4  2003/05/22 19:31:51  kuznets
 * +CBDB_FieldString::operator= (const string& str)
 *
 * Revision 1.3  2003/05/05 20:14:41  kuznets
 * Added CBDB_BLobFile, CBDB_File changed to support more flexible data record
 * management.
 *
 * Revision 1.2  2003/04/29 16:48:31  kuznets
 * Fixed minor warnings in Sun Workshop compiler
 *
 * Revision 1.1  2003/04/24 16:31:16  kuznets
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* BDB_TYPES__HPP */
