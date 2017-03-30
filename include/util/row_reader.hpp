#ifndef UTIL___ROW_READER__HPP
#define UTIL___ROW_READER__HPP

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
* Authors: Design and advice - Denis Vakatov
*          Implementation and critique - Sergey Satskiy
*
* Special credits:  Wratko Hlavina, Alex Astashin, Mike Dicuccio
*
* File Description:
*   CRowReader - generic API to work with row streams
** ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbifile.hpp>


BEGIN_NCBI_SCOPE


/// Position in the data stream (zero based)
typedef Uint8 TStreamPos;

/// Line number (in the data stream, zero based)
typedef Uint8 TLineNo;

/// Field number (zero based)
typedef Uint4 TFieldNo;


/// Basic field data types
/// @note
///  The row reader does not perform any data conversion by itself.
///  It is up to the user to set a certain field type and how to use it
///  further. The row reader merely provides a storage for the user
///  provided field data types.
enum ERR_FieldType {
    eRR_String,    ///< string
    eRR_Boolean,   ///< bool
    eRR_Integer,   ///< int
    eRR_Double,    ///< double
    eRR_DateTime   ///< CTime
};


/// Row type
enum ERR_RowType {
    eRR_Data,      ///< row contains data
    eRR_Comment,   ///< row contains a comment
    eRR_Metadata,  ///< row contains metadata
    eRR_Invalid    ///< row is invalid
};


#define UTIL___ROW_READER_INCLUDE__INL
#include "row_reader.inl"
#undef  UTIL___ROW_READER_INCLUDE__INL


// Forward declarations
template <typename TTraits> class CRowReader;
template <typename TTraits> class CRR_Row;


/// A single field in a row abstraction.
template <typename TTraits>
class CRR_Field
{
public:
    /// Get the converted field value
    /// @return
    ///  Converted field value
    /// @note
    ///  Available specializations: string, bool, ints, floats, CTime
    /// @exception
    ///  Throws an exception if there is conversion problem or the field
    ///  value is NULL
    template <typename TValue>
    TValue Get(void) const;

    /// Get the converted field value
    /// @param default_value
    ///  Default value if the field was translated to NULL
    /// @return
    ///  Converted field value or a default value
    /// @note
    ///  Available specializations: string, bool, ints, floats, CTime
    /// @exception
    ///  Throws an exception if there is conversion problem
    template <typename TValue> TValue
    GetWithDefault(const TValue& default_value) const;

    /// Get the field value converted to CTime
    /// @param fmt
    ///  CTime format
    /// @return
    ///  Converted to CTime field value
    /// @exception
    ///  Throws an exception if there is conversion problem or the field
    ///  value was translated to NULL
    CTime Get(const CTimeFormat& fmt) const;

    /// Check if the field value is NULL
    /// @return
    ///  true if the field value is NULL
    bool IsNull(void) const;

    /// Get the field original data
    /// @return
    ///  The field original data
    const CTempString GetOriginalData(void) const;

    // Construct an empty field
    CRR_Field();

    // Moving coonstructor of a field
    CRR_Field(const CRR_Field&& other_field);

private:
    string x_GetStringValue(void) const;

private:
    friend class CRowReader<TTraits>;
    friend class CRR_Row<TTraits>;

    bool                m_IsNull;
    bool                m_Translated;
    CTempString         m_OriginalData;
    string              m_TranslatedValue;

    // Needs to provide a stream context in the exceptions if a row has not
    // been copied yet.
    CRowReader<TTraits>*   m_RowReader;

private:
    CRR_Field(const CRR_Field& field) = delete;
    CRR_Field& operator=(const CRR_Field& field) = delete;
};



/// Represents one row from an input stream
template <typename TTraits>
class CRR_Row
{
public:
    /// Construct an empty row
    CRR_Row();

    // Moving constructor of a row
    CRR_Row(const CRR_Row&& other_row);

    /// Create a copy of another row
    /// @param other_row
    ///  The row to be copied from
    void Copy(const CRR_Row& other_row);

    /// Get a row field
    /// @param field
    ///  0-based field number
    /// @return
    ///  A reference to the field
    /// @exception
    ///  Throws an exception in case if the field does not exist
    const CRR_Field<TTraits>& operator[](TFieldNo field) const;

    /// Get a row field
    /// @param field
    ///  Field name
    /// @return
    ///  A reference to the field
    /// @exception
    ///  Throws an exception in case if the field name is unknown or
    ///  if the field does not exist
    const CRR_Field<TTraits>& operator[](CTempString field) const;

    /// Get the row type
    /// @return
    ///  Row type
    ERR_RowType GetType(void) const;

    /// Get the number of fields in the row
    /// @return
    ///  The number of fields in the row
    TFieldNo GetNumberOfFields(void) const;

    /// Get the row original data
    /// @return
    ///  The row original data
    CTempString GetOriginalData(void) const;

    /// Get the field name.
    /// @param field
    ///  0-based field number
    /// @return
    ///  Field name
    /// @exception
    ///  Throws an exception if the field name has not been previously set.
    const string& GetFieldName(TFieldNo field) const;

    /// Get the (basic) data type of the field.
    /// @param field
    ///  0-based field number
    /// @return
    ///  The previously set field type
    /// @exception
    ///  Throws an exception if the field type has not been previously set.
    ERR_FieldType GetFieldType(TFieldNo field) const;

    /// Get the (trait specific) data type of the field.
    /// @param field
    ///  0-based field number
    /// @return
    ///  The previously set extended field type
    /// @exception
    ///  Throws an exception if the field type has not been previously set.
    typename TTraits::TExtendedFieldType
    GetExtendedFieldType(TFieldNo field) const;

private:
    friend class CRowReader<TTraits>;

    void x_OnFreshRead(void);
    void x_AdjustFieldsSize(size_t new_size);
    void x_SetFieldName(TFieldNo field, const string& name);
    void x_SetFieldType(TFieldNo field, ERR_FieldType type);
    void x_SetFieldTypeEx(TFieldNo                             field,
                          ERR_FieldType                        type,
                          typename TTraits::TExtendedFieldType extended_type);
    void x_ClearFieldsInfo(void);
    void x_DetachMetaInfo(void);
    TFieldNo x_GetFieldIndex(CTempString field) const;

private:
    /// Copy constructor (and operator=) are not for public usage to
    /// avoid accidential copying of rows. For example:
    /// for ( auto x: my_stream ) --> x would use copy constructor
    /// for ( auto &  x: my_stream ) --> x is a reference to CRR_Row
    /// The difference is a single character so it was decided (to prevent
    /// a hard-to-notice CRR_Row copying) to disable the copy constructor
    /// @{
    CRR_Row(const CRR_Row& other_row) = delete;
    CRR_Row& operator=(const CRR_Row&) = delete;
    /// @}


private:
    string                          m_RawData;
    ERR_RowType                     m_RowType;
    CRef<CRR_MetaInfo<TTraits>>     m_MetaInfo;
    mutable bool                    m_Copied;

    // The content of the vector is reused so there is a manual control
    // over what the current size/capacity is
    vector<CRR_Field<TTraits>>      m_Fields;
    size_t                          m_FieldsSize;
    size_t                          m_FieldsCapacity;

    // Needs to provide a stream context in the exceptions if a row has not
    // been copied yet.
    CRowReader<TTraits>*            m_RowReader;
};



/// Callback style template to iterate over a row stream.
/// The template provides a framework while the data source specifics are
/// implemented via stream traits. The stream traits are supplied in the
/// template parameter and must meet certain interface requirements.
/// The requirements are described in the CRowReaderStream_Base
/// class vanilla implementation of the traits (see row_reader_base.hpp)
template <typename TTraits>
class CRowReader
{
public:
    typedef CRR_Field<TTraits> CField;
    typedef CRR_Row<TTraits>   CRow;


    /// Construct a row reader
    /// @param is
    ///  Input stream to read rows from
    /// @param sourcename
    ///  Data source name
    /// @param ownership
    ///  If set to eTakeOwnership then the stream will be owned by
    ///  CRowReader -- and automatically deleted when necessary
    CRowReader(CNcbiIstream* is,
               const string& sourcename,
               EOwnership    ownership = eNoOwnership);

    /// Construct a row reader out of a file
    /// @param filename
    ///  File to read rows from
    /// @note
    ///  The file will be opened immediately (and, using TTraits-specific mode)
    /// @exception
    ///  Throws exceptions (in particular) if the file does not exist or there
    ///  are no read permissions
    CRowReader(const string& filename);

    virtual ~CRowReader();

    /// Switch to processing data from another stream
    /// @note
    ///  The stream's row iterator will still point to the current row in the
    ///  previous data source. Next call to CRowReader::begin() or
    ///  to iterator's operator++ will retrieve the data from the new stream.
    /// @param is
    ///  Input stream to read rows from
    /// @param sourcename
    ///  Data source name
    /// @param ownership
    ///  If set to eTakeOwnership then the stream will be owned by
    ///  CRowReader -- and automatically deleted when necessary
    void SetDataSource(CNcbiIstream* is,
                       const string& sourcename,
                       EOwnership    ownership = eNoOwnership);

    /// Switch to processing data from another file
    /// @note
    ///  The stream's row iterator will still point to the current row in the
    ///  previous data source. Next call to CRowReader::begin() or
    ///  to iterator's operator++ will retrieve the data from the new file.
    /// @param filename
    ///  File to read rows from
    /// @note
    ///  The file will be opened immediately (and, using TTraits-specific mode)
    /// @exception
    ///  Throws exceptions (in particular) if the file does not exist or there
    ///  are no read permissions
    void SetDataSource(const string& filename);

    /// Read and validate-only the stream, calling TTraits::Validate()
    /// on each row
    void Validate(void);

    /// Get the number of the line that is currently being processed
    /// @return
    ///  0-based number of the currently processed line (in the current
    ///  data source)
    /// @note
    ///  This method can also be safely used inside the TTraits methods
    /// @exception
    ///  Throws an exception if called before any line was read
    TLineNo GetCurrentLineNo(void) const;

    /// Get the absolute position (in the current data source) of the
    /// beginning of the row that is currently being processed
    /// @return
    ///  0-based absolute position of the beginning of the currently processed
    ///  row
    /// @note
    ///  This method can also be safely used in the TTraits methods
    /// @exception
    ///  Throws an exception if called before any line was read
    TStreamPos GetCurrentRowPos(void) const;

    /// Set the field name
    /// @note
    ///  If the names are set then the fields can be referred to by their names
    /// @param field
    ///  0-based field number. The new name overrides the previously set if so.
    /// @param name
    ///  The field name
    void SetFieldName(TFieldNo field, const string& name);

    /// Get the field name
    /// @param field
    ///  0-based field number
    /// @return
    ///  Field name
    /// @exception
    ///  Throws an exception if the field name has not been previously set
    const string& GetFieldName(TFieldNo field) const;

    /// Set the (basic) data type of the field
    /// @param field
    ///  0-based field number. The new type overrides the previously set if so.
    /// @param type
    ///  The field type
    void SetFieldType(TFieldNo field, ERR_FieldType type);

    /// Set the (trait specific) data type of the field
    /// @param field
    ///  0-based field number. The new types override the previously set if so.
    /// @param type
    ///  The field type
    /// @param extended_type
    ///  The field extended type
    /// @attention
    ///  This call resets the basic field type!
    void SetFieldTypeEx(TFieldNo                             field,
                        ERR_FieldType                        type,
                        typename TTraits::TExtendedFieldType extended_type);

    /// Get the (basic) data type of the field.
    /// @param field
    ///  0-based field number
    /// @return
    ///  The previously set field type
    /// @exception
    ///  Throws an exception if the field type has not been previously set
    ERR_FieldType GetFieldType(TFieldNo field) const;

    /// Get the (trait specific) data type of the field.
    /// @param field
    ///  0-based field number
    /// @return
    ///  The previously set extended field type
    /// @exception
    ///  Throws an exception if the field type has not been previously set
    typename TTraits::TExtendedFieldType
    GetExtendedFieldType(TFieldNo field) const;

    /// Clear the information about all field names, types and extended types
    /// @note
    ///  SetDataSource() calls by themselves don't clear the info
    ///  (although it indeed may be changed, as usual, while reading new
    ///  metainfo from the data source)
    void ClearFieldsInfo(void);

    /// Get the traits object
    /// @return
    ///  A reference to the traits instance currently used
    TTraits& GetTraits(void);

public:

    /// A (forward-only) iterator to iterate over the rows from a stream
    ///
    /// All iterators related to the same instance of CRowReader
    /// are in fact pointing to the one and the same row -- the (current) one
    /// that the CRowReader has advanced to so far.
    ///
    /// @attention
    ///  The iterator's data is destroyed after the iterator is advanced.
    class CRowIterator
    {
    public:
        /// Get a row field
        /// @param field
        ///  0-based field number
        /// @return
        ///  A reference to the field
        /// @exception
        ///  Throws an exception in case if:
        ///  - the iterator has reached the end
        ///  - the field does not exist
        const CRR_Field<TTraits>& operator[](TFieldNo field) const;

        /// Get a row field
        /// @param field
        ///  Field name
        /// @return
        ///  A reference to the field
        /// @exception
        ///  Throws an exception in case if:
        ///  - the iterator has reached the end
        ///  - the field name is unknown
        ///  - the field does not exist
        const CRR_Field<TTraits>& operator[](CTempString field) const;

        /// Get a reference to the current row
        /// @return
        ///  A reference to the current row
        /// @exception
        ///  Throws an exception if the iterator has reached the end
        CRR_Row<TTraits>& operator* (void) const;

        /// Get a pointer to the current row
        /// @return
        ///  A pointer to the current row
        /// @exception
        ///  Throws an exception if the iterator has reached the end
        CRR_Row<TTraits>* operator->(void) const;

        /// Compare this iterator with another
        /// @attention
        ///  This and/or "iter" iterator must be "end()" 
        /// @param iter
        ///  RHS iterator to compare
        /// @return
        ///  true if either of the following is true:
        ///  - this iterator points to the end of the same
        ///     underlying data source (SetDataSource() wise) as the
        ///     contemporary "end()"
        ///  - both iterators are "end()"
        ///  - both iterators are not at end
        /// @exception
        ///  Throw if the compared iterators belong to different streams
        bool operator==(const CRowIterator& iter) const;

        /// This is an exact negation of operator==
        /// @sa operator==
        bool operator!=(const CRowIterator& iter) const;

        // Iterator copy constructor
        CRowIterator(const CRowIterator& iter);

        ~CRowIterator();

        CRowIterator& operator=(const CRowIterator& iter);

        /// Advance the iterator to the next row
        /// @note
        ///  The next row can be in the next data source (if set using
        ///  SetDataSource()). In this case the ++ operator can
        ///  be legally applied to an iterator that points to the "end()" of
        ///  the previous data source.
        /// @return
        ///  A reference to self
        /// @exception
        ///  Throws an exception if any of the following happens:
        ///  - the iterator has already reached the end of its current(!)
        ///    data source
        ///  - the iterator belongs to another instance of CRowReader
        /// @attention
        ///  The data that was previously obtained through the iterator is
        ///  invalidated.
        ///  Because all iterators related to the same instance of
        ///  CRowReader are in fact pointing to the one and the same
        ///  row so advancing any iterator will advance all iterators that
        ///  belong to the same stream.
        CRowIterator& operator++(void);

    private:
        CRowReader<TTraits>* m_RowReader;
        bool                 m_IsEndIter;

        friend class CRowReader<TTraits>;

        CRowIterator(CRowReader<TTraits>* s, bool  is_end);
        CRowIterator(const CRowReader<TTraits>* s, bool  is_end);
        void x_CheckDereferencing(void) const;
        void x_CheckAdvancing(void) const;
        void x_CheckFieldAccess(void) const;
    };

    /// It works exactly(!) like CRowIterator::operator++, except it creates
    /// new iterator and therefore can also be used to create the very
    /// first iterator.
    CRowIterator begin(void);

    /// Get iterator pointing to the end of the current underlying data source
    CRowIterator end(void) const;

    typedef CRowIterator iterator;
    typedef CRowIterator const_iterator;

    /// Get basic context. It includes (if available) a data source name,
    /// a position in the stream, the current line and the current row data.
    /// @return
    ///  Basic context structure
    CRR_Context GetBasicContext(void) const;

    /// Get traits context
    /// @return
    ///  Traits current context
    /// @note
    ///  Traits may extend the basic context (by deriving from it) or use
    ///  CRR_Context as is
    typename TTraits::TRR_Context GetContext(void) const;

private:
    bool x_GetRowData(size_t* phys_lines_read);
    void x_ReadNextRow(void);
    void x_OpenFile(SRR_SourceInfo& stream_info);
    void x_Reset(void);

private:
    // Note: it was decided that when an underlying data stream is switched the
    // original one must stay intact till the first iteration over the next
    // stream. So two structures are introduced below.
    // Each of the structures holds an information about a stream, an ownership
    // on it and a file name if so.
    SRR_SourceInfo      m_DataSource;
    SRR_SourceInfo      m_NextDataSource;

    // End of the current stream flag
    bool                m_AtEnd;

    bool                m_LinesAlreadyRead;
    bool                m_RawDataAvailable;
    TLineNo             m_CurrentLineNo;
    size_t              m_PreviousPhysLinesRead;
    TStreamPos          m_CurrentRowPos;
    TTraits             m_Traits;
    CRR_Row<TTraits>    m_CurrentRow;

    // if true => within the validate() loop
    bool                m_Validation;

    // Note: this member is needed in a very limited scope within one method to
    // support the tokenization stage. So from the maintainability point of
    // view the variable had to be right there. However it was decided to do an
    // optimization (without having any profiling to see if this is a
    // performance bottleneck) and to reuse the tokens vector. Thus the
    // variable scope was extended to the class scope introducing a pollution.
    vector<CTempString> m_Tokens;

private:
    // prohibit assignment and copy-ctor
    CRowReader(const CRowReader&) = delete;
    CRowReader(CRowReader&&) = delete;
    CRowReader& operator=(const CRowReader&) = delete;
    CRowReader& operator=(CRowReader&&) = delete;
};



// Begin of the CRR_Field implementation

template <typename TTraits>
CTime CRR_Field<TTraits>::Get(const CTimeFormat& fmt) const
{
    if (m_RowReader == nullptr)
        return CTime(x_GetStringValue(), fmt);
    try {
        return CTime(x_GetStringValue(), fmt);
    } catch (CRowReaderException& exc) {
        exc.SetContext(m_RowReader->GetBasicContext().Clone());
        throw exc;
    } catch (const CException& exc) {
        NCBI_RETHROW2(exc, CRowReaderException, eFieldConvert,
                      "Cannot convert field value to CTime using "
                      "format " + fmt.GetString(),
                      m_RowReader->GetBasicContext().Clone());
    } catch (const exception& exc) {
        NCBI_THROW2(CRowReaderException, eFieldConvert,
                    exc.what(), m_RowReader->GetBasicContext().Clone());
    } catch (...) {
        NCBI_THROW2(CRowReaderException, eFieldConvert,
                    "Unknown error while converting field value to "
                    "CTime using format " + fmt.GetString(),
                    m_RowReader->GetBasicContext().Clone());
    }
}


template <typename TTraits>
template <typename TValue>
TValue CRR_Field<TTraits>::Get(void) const
{
    TValue    val;
    if (m_RowReader == nullptr) {
        CRR_Util::GetFieldValueConverted(x_GetStringValue(), val);
    } else {
        try {
            CRR_Util::GetFieldValueConverted(x_GetStringValue(), val);
        } catch (CRowReaderException& exc) {
            exc.SetContext(m_RowReader->GetBasicContext().Clone());
            throw exc;
        } catch (const CException& exc) {
            NCBI_RETHROW2(exc, CRowReaderException, eFieldConvert,
                          "Cannot convert field value to " +
                          string(typeid(TValue).name()),
                          m_RowReader->GetBasicContext().Clone());
        } catch (const exception& exc) {
            NCBI_THROW2(CRowReaderException, eFieldConvert,
                        exc.what(), m_RowReader->GetBasicContext().Clone());
        } catch (...) {
            NCBI_THROW2(CRowReaderException, eFieldConvert,
                        "Unknown error while converting field value to " +
                        string(typeid(TValue).name()),
                        m_RowReader->GetBasicContext().Clone());
        }
    }
    return val;
}


template <typename TTraits>
template <typename TValue> TValue
CRR_Field<TTraits>::GetWithDefault(const TValue& default_value) const
{
    if (m_IsNull)
        return default_value;
    return Get<TValue>();
}


template <typename TTraits>
bool CRR_Field<TTraits>::IsNull(void) const
{
    return m_IsNull;
}


template <typename TTraits>
const CTempString CRR_Field<TTraits>::GetOriginalData(void) const
{
    return m_OriginalData;
}


template <typename TTraits>
CRR_Field<TTraits>::CRR_Field() :
    m_IsNull(false), m_Translated(false),
    m_OriginalData(nullptr, 0), m_RowReader(nullptr)
{}


template <typename TTraits>
CRR_Field<TTraits>::CRR_Field(const CRR_Field&& other_field) :
    m_IsNull(other_field.m_IsNull),
    m_Translated(other_field.m_Translated),
    m_OriginalData(other_field.m_OriginalData),
    m_TranslatedValue(std::move(other_field.m_TranslatedValue)),
    m_RowReader(other_field.m_RowReader)
{}


template <typename TTraits>
string CRR_Field<TTraits>::x_GetStringValue(void) const
{
    if (m_IsNull)
        NCBI_THROW2(CRowReaderException, eNullField,
                    "The field value is translated to NULL", nullptr);
    if (m_Translated)
        return m_TranslatedValue;
    return string(m_OriginalData.data(), m_OriginalData.size());
}

// End of the CRR_Field implementation



// Begin of the CRR_Row implementation

template <typename TTraits>
CRR_Row<TTraits>::CRR_Row() :
    m_RowType(eRR_Invalid),
    m_MetaInfo(new CRR_MetaInfo<TTraits>()), m_Copied(false),
    m_FieldsSize(0), m_FieldsCapacity(0),
    m_RowReader(nullptr)
{}


template <typename TTraits>
void CRR_Row<TTraits>::Copy(const CRR_Row& other_row)
{
    m_RawData = other_row.m_RawData;
    m_RowType = other_row.m_RowType;
    m_MetaInfo = other_row.m_MetaInfo;
    m_Copied = false;
    m_RowReader = nullptr;
    other_row.m_Copied = true;

    // CRR_Field copy constructor is disabled and also CTempString
    // pointers adjustment is required
    ptrdiff_t raw_data_delta = m_RawData.data() - other_row.m_RawData.data();
    x_AdjustFieldsSize(other_row.m_FieldsSize);
    for (size_t index = 0; index < m_FieldsSize; ++index) {
        CRR_Field<TTraits>&       to_field = m_Fields[index];
        const CRR_Field<TTraits>& from_field = other_row.m_Fields[index];

        to_field.m_IsNull = from_field.m_IsNull;
        to_field.m_Translated = from_field.m_Translated;
        if (to_field.m_Translated)
            to_field.m_TranslatedValue = from_field.m_TranslatedValue;
        else
            to_field.m_TranslatedValue.clear();

        to_field.m_OriginalData = CTempString(
            from_field.m_OriginalData.data() + raw_data_delta,
            from_field.m_OriginalData.size());

        m_Fields[index].m_RowReader = nullptr;
    }
}


template <typename TTraits>
CRR_Row<TTraits>::CRR_Row(const CRR_Row&& other_row) :
    m_RawData(std::move(other_row.m_RawData)),
    m_RowType(other_row.m_RowType),
    m_MetaInfo(std::move(other_row.m_MetaInfo)),
    m_Copied(other_row.m_Copied),
    m_Fields(std::move(other_row.m_Fields)),
    m_RowReader(other_row.m_RowReader)
{}


template <typename TTraits>
const CRR_Field<TTraits>& CRR_Row<TTraits>::operator[](TFieldNo field) const
{
    if (field >= GetNumberOfFields()) {
        CRR_Context* ctxt = nullptr;
        if (m_RowReader != nullptr)
            ctxt = m_RowReader->GetBasicContext().Clone();

        NCBI_THROW2(CRowReaderException, eFieldNoOutOfRange,
                    "Field index " + NStr::NumericToString(field) +
                    " is out of range for the current row", ctxt);
    }
    return m_Fields[field];
}


template <typename TTraits>
const CRR_Field<TTraits>&
CRR_Row<TTraits>::operator[](CTempString field) const
{
    TFieldNo    index = x_GetFieldIndex(field);
    if (index >= GetNumberOfFields()) {
        CRR_Context* ctxt = nullptr;
        if (m_RowReader != nullptr)
            ctxt = m_RowReader->GetBasicContext().Clone();

        NCBI_THROW2(CRowReaderException, eFieldNoOutOfRange,
                    "Field index " + NStr::NumericToString(index) +
                    " provided for the field name '" + field +
                    "' is out of range for the current row", ctxt);
    }
    return m_Fields[index];
}


template <typename TTraits>
ERR_RowType CRR_Row<TTraits>::GetType(void) const
{
    return m_RowType;
}


template <typename TTraits>
TFieldNo CRR_Row<TTraits>::GetNumberOfFields(void) const
{
    return static_cast<TFieldNo>(m_FieldsSize);
}


template <typename TTraits>
CTempString CRR_Row<TTraits>::GetOriginalData(void) const
{
    return m_RawData;
}


template <typename TTraits>
const string& CRR_Row<TTraits>::GetFieldName(TFieldNo field) const
{
    if (m_RowReader == nullptr)
        return m_MetaInfo->GetFieldName(field);
    try {
        return m_MetaInfo->GetFieldName(field);
    } catch (CRowReaderException& exc) {
        exc.SetContext(m_RowReader->GetBasicContext().Clone());
        throw exc;
    } catch (const CException& exc) {
        NCBI_RETHROW2(exc, CRowReaderException, eFieldMetaInfoAccess,
                      "Cannot get field name for the field number " +
                      NStr::NumericToString(field),
                      m_RowReader->GetBasicContext().Clone());
    } catch (const exception& exc) {
        NCBI_THROW2(CRowReaderException, eFieldMetaInfoAccess,
                    exc.what(), m_RowReader->GetBasicContext().Clone());
    } catch (...) {
        NCBI_THROW2(CRowReaderException, eFieldMetaInfoAccess,
                    "Unknown error while getting field name for the field "
                    "number " + NStr::NumericToString(field),
                    m_RowReader->GetBasicContext().Clone());
    }
}


template <typename TTraits>
ERR_FieldType CRR_Row<TTraits>::GetFieldType(TFieldNo field) const
{
    if (m_RowReader == nullptr)
        return m_MetaInfo->GetFieldType(field);
    try {
        return m_MetaInfo->GetFieldType(field);
    } catch (CRowReaderException& exc) {
        exc.SetContext(m_RowReader->GetBasicContext().Clone());
        throw exc;
    } catch (const CException& exc) {
        NCBI_RETHROW2(exc, CRowReaderException, eFieldMetaInfoAccess,
                      "Cannot get field type for the field number " +
                      NStr::NumericToString(field),
                      m_RowReader->GetBasicContext().Clone());
    } catch (const exception& exc) {
        NCBI_THROW2(CRowReaderException, eFieldMetaInfoAccess,
                    exc.what(), m_RowReader->GetBasicContext().Clone());
    } catch (...) {
        NCBI_THROW2(CRowReaderException, eFieldMetaInfoAccess,
                    "Unknown error while getting field type for the field "
                    "number " + NStr::NumericToString(field),
                    m_RowReader->GetBasicContext().Clone());
    }
}


template <typename TTraits>
typename TTraits::TExtendedFieldType
CRR_Row<TTraits>::GetExtendedFieldType(TFieldNo field) const
{
    if (m_RowReader == nullptr)
        return m_MetaInfo->GetExtendedFieldType(field);
    try {
        return m_MetaInfo->GetExtendedFieldType(field);
    } catch (CRowReaderException& exc) {
        exc.SetContext(m_RowReader->GetBasicContext().Clone());
        throw exc;
    } catch (const CException& exc) {
        NCBI_RETHROW2(exc, CRowReaderException,
                      eFieldMetaInfoAccess,
                      "Cannot get field extended type for the field "
                      "number " + NStr::NumericToString(field),
                      m_RowReader->GetBasicContext().Clone());
    } catch (const exception& exc) {
        NCBI_THROW2(CRowReaderException, eFieldMetaInfoAccess,
                    exc.what(), m_RowReader->GetBasicContext().Clone());
    } catch (...) {
        NCBI_THROW2(CRowReaderException, eFieldMetaInfoAccess,
                    "Unknown error while getting field extended type for the "
                    "field number " + NStr::NumericToString(field),
                    m_RowReader->GetBasicContext().Clone());
    }
}


template <typename TTraits>
void CRR_Row<TTraits>::x_OnFreshRead(void)
{
    m_RawData.clear();
    m_RowType = eRR_Invalid;
    m_FieldsSize = 0;
}


template <typename TTraits>
void CRR_Row<TTraits>::x_AdjustFieldsSize(size_t new_size)
{
    m_FieldsSize = new_size;
    while (m_FieldsCapacity < new_size) {
        m_Fields.push_back(CRR_Field<TTraits>());
        ++m_FieldsCapacity;
    }
}


template <typename TTraits>
void CRR_Row<TTraits>::x_SetFieldName(TFieldNo field, const string& name)
{
    x_DetachMetaInfo();
    m_MetaInfo->SetFieldName(field, name);
}


template <typename TTraits>
void CRR_Row<TTraits>::x_SetFieldType(TFieldNo field, ERR_FieldType type)
{
    x_DetachMetaInfo();
    m_MetaInfo->SetFieldType(field, type);
}


template <typename TTraits>
void CRR_Row<TTraits>::x_SetFieldTypeEx(
            TFieldNo                             field,
            ERR_FieldType                        type,
            typename TTraits::TExtendedFieldType extended_type)
{
    x_DetachMetaInfo();
    m_MetaInfo->SetFieldTypeEx(field, type, extended_type);
}


template <typename TTraits>
void CRR_Row<TTraits>::x_ClearFieldsInfo(void)
{
    x_DetachMetaInfo();
    m_MetaInfo->Clear();
}


template <typename TTraits>
void CRR_Row<TTraits>::x_DetachMetaInfo(void)
{
    if (m_Copied) {
        CRR_MetaInfo<TTraits>* old_meta = m_MetaInfo.GetPointer();
        CRR_MetaInfo<TTraits>* new_meta = new CRR_MetaInfo<TTraits>(*old_meta);

        m_MetaInfo.Reset(new_meta);
        m_Copied = false;
    }
}


template <typename TTraits>
TFieldNo CRR_Row<TTraits>::x_GetFieldIndex(CTempString  field) const
{
    if (m_RowReader == nullptr)
        return m_MetaInfo->GetFieldIndexByName(field);
    try {
        return m_MetaInfo->GetFieldIndexByName(field);
    } catch (CRowReaderException& exc) {
        exc.SetContext(m_RowReader->GetBasicContext().Clone());
        throw exc;
    } catch (const CException& exc) {
        NCBI_RETHROW2(exc, CRowReaderException, eFieldMetaInfoAccess,
                      "Cannot get field number by name ('"
                      + string(field) + "')",
                      m_RowReader->GetBasicContext().Clone());
    } catch (const exception& exc) {
        NCBI_THROW2(CRowReaderException, eFieldMetaInfoAccess,
                    exc.what(), m_RowReader->GetBasicContext().Clone());
    } catch (...) {
        NCBI_THROW2(CRowReaderException, eFieldMetaInfoAccess,
                    "Unknown error while getting field number by name ('"
                    + string(field) + "')",
                    m_RowReader->GetBasicContext().Clone());
    }
}


// End of the CRR_Row implementation



// Begin of the CRowReader implementation

template <typename TTraits>
CRowReader<TTraits>::CRowReader(
            CNcbiIstream* is,
            const string& sourcename,
            EOwnership    ownership) :
    m_DataSource(is, sourcename, ownership == eTakeOwnership),
    m_AtEnd(false), m_LinesAlreadyRead(false), m_RawDataAvailable(false),
    m_CurrentLineNo(0), m_PreviousPhysLinesRead(0),
    m_Validation(false)
{
    m_CurrentRowPos = NcbiStreamposToInt8(m_DataSource.m_Stream->tellg());
    m_Traits.x_SetMyStream(this);
    m_CurrentRow.m_RowReader = this;
}


template <typename TTraits>
CRowReader<TTraits>::CRowReader(const string& filename) :
    m_DataSource(nullptr, filename, false),
    m_AtEnd(false), m_LinesAlreadyRead(false),
    m_RawDataAvailable(false), m_CurrentLineNo(0),
    m_PreviousPhysLinesRead(0),
    m_Validation(false)
{
    CRR_Util::CheckExistanceAndPermissions(filename);
    x_OpenFile(m_DataSource);
    m_CurrentRowPos = NcbiStreamposToInt8(m_DataSource.m_Stream->tellg());
    m_Traits.x_SetMyStream(this);
    m_CurrentRow.m_RowReader = this;
}


template <typename TTraits>
CRowReader<TTraits>::~CRowReader()
{}


template <typename TTraits>
void CRowReader<TTraits>::SetDataSource(
            CNcbiIstream* is,
            const string& sourcename,
            EOwnership    ownership)
{
    m_NextDataSource.Clear();
    m_NextDataSource.m_Stream = is;
    m_NextDataSource.m_Sourcename = sourcename;
    m_NextDataSource.m_StreamOwner = ownership == eTakeOwnership;
}


template <typename TTraits>
void CRowReader<TTraits>::SetDataSource(const string& filename)
{
    CRR_Util::CheckExistanceAndPermissions(filename);
    m_NextDataSource.Clear();
    m_NextDataSource.m_Sourcename = filename;

    x_OpenFile(m_NextDataSource);
}


template <typename TTraits>
void CRowReader<TTraits>::Validate(void)
{
    ERR_Action action = eRR_Interrupt;
    size_t     phys_lines_read;

    for (;;) {
        m_AtEnd = false;
        m_Validation = true;
        while (x_GetRowData(&phys_lines_read)) {
            if (m_LinesAlreadyRead)
                m_CurrentLineNo += m_PreviousPhysLinesRead;
            else
                m_LinesAlreadyRead = true;
            m_PreviousPhysLinesRead = phys_lines_read;

            try {
                action = m_Traits.Validate(CTempString(m_CurrentRow.m_RawData));
                if (action == eRR_Interrupt)
                    break;
            } catch (const CException& exc) {
                m_Validation = false;
                NCBI_RETHROW2(exc, CRowReaderException, eValidating,
                              "Validation error",
                              m_Traits.GetContext(GetBasicContext()).Clone());
            } catch (const exception& exc) {
                m_Validation = false;
                NCBI_THROW2(CRowReaderException, eValidating, exc.what(),
                            m_Traits.GetContext(GetBasicContext()).Clone());
            } catch (...) {
                m_Validation = false;
                NCBI_THROW2(CRowReaderException, eValidating,
                            "Unknown validating error",
                            m_Traits.GetContext(GetBasicContext()).Clone());
            }
        }
        m_Validation = false;

        // The end of the stream has been reached
        x_Reset();
        m_CurrentRow.x_OnFreshRead();
        m_AtEnd = true;

        switch (m_Traits.OnEvent(eRR_Event_SourceEOF)) {
            case eRR_EventAction_Continue:
                // Note: if traits return eRR_EventAction_Continue without
                // swithing the underlying data source then an infinite loop is
                // possible
                continue;
            case eRR_EventAction_Default:
            case eRR_EventAction_Stop:
            default:
                // The actual end of the loop
                return;
        }
    }
}


template <typename TTraits>
TLineNo CRowReader<TTraits>::GetCurrentLineNo(void) const
{
    if (!m_LinesAlreadyRead) {
        // Note: it was decided to make it a special case and return 0.
        // i.e. it is impossible to distinguish two cases:
        // - there ware no reads yet
        // - it was the very first read
        return 0;
    }
    return m_CurrentLineNo;
}


template <typename TTraits>
TStreamPos CRowReader<TTraits>::GetCurrentRowPos(void) const
{
    if (!m_LinesAlreadyRead) {
        // Note: it was decided to make it a special case and return 0.
        // i.e. it is impossible to distinguish two cases:
        // - there ware no reads yet
        // - it was the very first read
        return 0;
    }
    return m_CurrentRowPos;
}


template <typename TTraits>
void CRowReader<TTraits>::SetFieldName(TFieldNo      field,
                                       const string& name)
{
    m_CurrentRow.x_SetFieldName(field, name);
}


template <typename TTraits>
const string& CRowReader<TTraits>::GetFieldName(TFieldNo field) const
{
    return m_CurrentRow.GetFieldName(field);
}


template <typename TTraits>
void CRowReader<TTraits>::SetFieldType(TFieldNo      field,
                                       ERR_FieldType type)
{
    m_CurrentRow.x_SetFieldType(field, type);
}


template <typename TTraits>
void CRowReader<TTraits>::SetFieldTypeEx(
                    TFieldNo                             field,
                    ERR_FieldType                        type,
                    typename TTraits::TExtendedFieldType extended_type)
{
    m_CurrentRow.x_SetFieldTypeEx(field, type, extended_type);
}


template <typename TTraits>
ERR_FieldType CRowReader<TTraits>::GetFieldType(TFieldNo field) const
{
    return m_CurrentRow.GetFieldType(field);
}


template <typename TTraits>
typename TTraits::TExtendedFieldType
CRowReader<TTraits>::GetExtendedFieldType(TFieldNo field) const
{
    return m_CurrentRow.GetExtendedFieldType(field);
}


template <typename TTraits>
void CRowReader<TTraits>::ClearFieldsInfo(void)
{
    m_CurrentRow.x_ClearFieldsInfo();
}


template <typename TTraits>
TTraits& CRowReader<TTraits>::GetTraits(void)
{
    return m_Traits;
}


template <typename TTraits>
typename CRowReader<TTraits>::CRowIterator
CRowReader<TTraits>::begin(void)
{
    CRowIterator it(this, false);
    ++it;
    return it;
}


template <typename TTraits>
typename CRowReader<TTraits>::CRowIterator
CRowReader<TTraits>::end(void)  const
{
    if (m_Validation)
        NCBI_THROW2(CRowReaderException, eIteratorWhileValidating,
                    "It is prohibited to use iterators "
                    "during the stream validation", nullptr);
    return CRowIterator(this, true);
}


template <typename TTraits>
CRR_Context CRowReader<TTraits>::GetBasicContext(void) const
{
    return CRR_Context(m_DataSource.m_Sourcename, m_LinesAlreadyRead,
                       m_CurrentLineNo, m_CurrentRowPos,
                       m_RawDataAvailable, m_CurrentRow.m_RawData, m_AtEnd);
}


template <typename TTraits>
typename TTraits::TRR_Context
CRowReader<TTraits>::GetContext(void) const
{
    return m_Traits.GetContext(GetBasicContext());
}


template <typename TTraits>
bool CRowReader<TTraits>::x_GetRowData(size_t* phys_lines_read)
{
    for (;;) {
        // Switch the source stream if needed
        if (m_NextDataSource.m_Stream != nullptr) {
            m_DataSource.Clear();
            m_DataSource = m_NextDataSource;

            m_NextDataSource.m_Stream = nullptr;
            m_NextDataSource.m_Sourcename.clear();
            m_NextDataSource.m_StreamOwner = false;

            x_Reset();
            m_CurrentRowPos = NcbiStreamposToInt8(
                                        m_DataSource.m_Stream->tellg());
        }

        m_RawDataAvailable = false;
        m_CurrentRow.x_OnFreshRead();

        if (m_DataSource.m_Stream->bad() ||
            (m_DataSource.m_Stream->fail() && !m_DataSource.m_Stream->eof())) {
            switch ( m_Traits.OnEvent(eRR_Event_SourceError) ) {
                case eRR_EventAction_Continue:
                    // Note: if traits return eRR_EventAction_Continue without
                    // swithing the underlying data source or repairing the
                    // current stream then an infinite loop is possible
                    continue;
                case eRR_EventAction_Default:
                case eRR_EventAction_Stop:
                default:
                    NCBI_THROW2(CRowReaderException, eStreamFailure,
                                "Input stream failed before reaching the end",
                                GetBasicContext().Clone());
            }
        }

        m_CurrentRowPos = NcbiStreamposToInt8(m_DataSource.m_Stream->tellg());

        *phys_lines_read = m_Traits.ReadRowData(*m_DataSource.m_Stream,
                                                &m_CurrentRow.m_RawData);

        m_RawDataAvailable = true;
        return bool(*m_DataSource.m_Stream);
    }
}


// true: the line is ready; a non-end iterator needs to be provided
// false: the stream is over; an end iterator needs to be provided
template <typename TTraits>
void CRowReader<TTraits>::x_ReadNextRow(void)
{
    ERR_Action action = eRR_Interrupt;
    size_t     phys_lines_read;

    for (;;) {
        m_AtEnd = false;
        while (x_GetRowData(&phys_lines_read)) {
            if (m_LinesAlreadyRead)
                m_CurrentLineNo += m_PreviousPhysLinesRead;
            else
                m_LinesAlreadyRead = true;
            m_PreviousPhysLinesRead = phys_lines_read;

            // Call the user OnNextLine(). The line may need to be skipped
            // or the stream processing needs to be interrupted.
            try {
                action = m_Traits.OnNextLine(m_CurrentRow.m_RawData);

                if (action == eRR_Skip)
                    continue;
                if (action == eRR_Interrupt) {
                    m_CurrentRow.x_OnFreshRead();

                    // Note: break leads to a 'soft end' return
                    break;
                }

                m_CurrentRow.m_RowType = CRR_Util::ActionToRowType(action);

                if (action == eRR_Continue_Data) {
                    // Need to do tokenize
                    m_Tokens.clear();
                    action = m_Traits.Tokenize(m_CurrentRow.m_RawData,
                                               m_Tokens);
                    if (action == eRR_Skip)
                        continue;
                    if (action == eRR_Interrupt) {
                        m_CurrentRow.x_OnFreshRead();

                        // Note: break leads to a 'soft end' return
                        break;
                    }
                    if (action != eRR_Continue_Data) {
                        NCBI_THROW2(CRowReaderException,
                                    eInvalidAction, "Invalid action " +
                                    CRR_Util::ERR_ActionToString(action) +
                                    " in response to the Tokenize() call.",
                                    nullptr);
                    }

                    // The current row fields vector content is reused so
                    // adjust it to a new size
                    m_CurrentRow.x_AdjustFieldsSize(m_Tokens.size());

                    // Need to translate
                    for (TFieldNo field_index = 0;
                         field_index < m_Tokens.size(); ++field_index) {
                        CRR_Field<TTraits>& current_field =
                                            m_CurrentRow.m_Fields[field_index];

                        switch (m_Traits.Translate(
                                    field_index, m_Tokens[field_index],
                                    current_field.m_TranslatedValue)) {
                            case eRR_UseOriginal:
                                current_field.m_IsNull = false;
                                current_field.m_Translated = false;
                                break;
                            case eRR_Translated:
                                current_field.m_IsNull = false;
                                current_field.m_Translated = true;
                                break;
                            case eRR_Null:
                                current_field.m_IsNull = true;
                                current_field.m_Translated = false;
                        }
                        current_field.m_OriginalData = m_Tokens[field_index];
                        current_field.m_RowReader = this;
                    }
                }
            } catch (CRowReaderException& exc) {
                exc.SetContext(m_Traits.GetContext(GetBasicContext()).Clone());
                throw exc;
            } catch (const CException& exc) {
                NCBI_RETHROW2(exc, CRowReaderException,
                              eLineProcessing, "Error processing line",
                              m_Traits.GetContext(GetBasicContext()).Clone());
            } catch (const exception& exc) {
                NCBI_THROW2(CRowReaderException, eLineProcessing, exc.what(),
                            m_Traits.GetContext(GetBasicContext()).Clone());
            } catch (...) {
                NCBI_THROW2(CRowReaderException, eLineProcessing,
                            "Unknown line processing error",
                            m_Traits.GetContext(GetBasicContext()).Clone());
            }

            // Data for an iterator has been prepared
            return;
        }

        // The end of the stream has been reached
        x_Reset();
        m_CurrentRow.x_OnFreshRead();
        m_AtEnd = true;

        switch (m_Traits.OnEvent(eRR_Event_SourceEOF)) {
            case eRR_EventAction_Continue:
                // Note: if traits return eRR_EventAction_Continue without
                // swithing the underlying data source then an infinite loop is
                // possible
                continue;
            case eRR_EventAction_Default:
            case eRR_EventAction_Stop:
            default:
                // The actual return from the loop with the 'soft end' iterator
                return;
        }
    }
}


template <typename TTraits>
void CRowReader<TTraits>::x_OpenFile(SRR_SourceInfo& stream_info)
{
    ios_base::openmode open_mode = ios_base::in;
    if (m_Traits.GetFlags() | fRR_OpenFile_AsBinary)
         open_mode |= ios_base::binary;
    stream_info.m_Stream = new CNcbiIfstream(stream_info.m_Sourcename.c_str(),
                                             open_mode);
    stream_info.m_StreamOwner = true;
}


template <typename TTraits>
void CRowReader<TTraits>::x_Reset(void)
{
    m_LinesAlreadyRead = false;
    m_RawDataAvailable = false;
    m_CurrentLineNo = 0;
    m_PreviousPhysLinesRead = 0;
    m_CurrentRowPos = 0;
}

// End of the CRowReader implementation

// Begin of the CRowReader::CRowIterator implementation

template <typename TTraits>
const CRR_Field<TTraits>&
CRowReader<TTraits>::CRowIterator::operator[](TFieldNo field) const
{
    x_CheckFieldAccess();
    return m_RowReader->m_CurrentRow[field];
}


template <typename TTraits>
const CRR_Field<TTraits>&
CRowReader<TTraits>::CRowIterator::operator[](CTempString field) const
{
    x_CheckFieldAccess();
    return m_RowReader->m_CurrentRow[field];
}


template <typename TTraits>
CRR_Row<TTraits>&
CRowReader<TTraits>::CRowIterator::operator* (void) const
{
    x_CheckDereferencing();
    return m_RowReader->m_CurrentRow;
}


template <typename TTraits>
CRR_Row<TTraits>*
CRowReader<TTraits>::CRowIterator::operator->(void) const
{
    x_CheckDereferencing();
    return &m_RowReader->m_CurrentRow;
}


template <typename TTraits>
bool
CRowReader<TTraits>::CRowIterator::operator==
(const CRowIterator& i) const
{
    _ASSERT(m_RowReader == i.m_RowReader);
    _ASSERT(m_IsEndIter  ||  i.m_IsEndIter);

    if (m_RowReader != i.m_RowReader)
        return false;

    if (!m_IsEndIter  &&  !i.m_IsEndIter)
        NCBI_THROW2(CRowReaderException, eNonEndIteratorCompare,
                    "Comparing two non end iterators is prohibited", nullptr);

    if (m_IsEndIter  &&  i.m_IsEndIter)
        return true;

    if ( m_IsEndIter )
        return i.m_RowReader->m_AtEnd
            && (i.m_RowReader->m_NextDataSource.m_Stream == nullptr);

    return m_RowReader->m_AtEnd
            && (m_RowReader->m_NextDataSource.m_Stream == nullptr);
}


template <typename TTraits>
bool
CRowReader<TTraits>::CRowIterator::operator!=
(const CRowIterator& end_iterator) const
{
    return !operator==(end_iterator);
}


template <typename TTraits>
CRowReader<TTraits>::CRowIterator::CRowIterator(const CRowIterator& i)
    : m_RowReader(i.m_RowReader), m_IsEndIter(i.m_IsEndIter)
{}


template <typename TTraits>
CRowReader<TTraits>::CRowIterator::~CRowIterator()
{}


template <typename TTraits>
typename CRowReader<TTraits>::CRowIterator&
CRowReader<TTraits>::CRowIterator::operator=(const CRowIterator& i)
{
    m_RowReader = i.m_RowReader;
    m_IsEndIter = i.m_IsEndIter;
    return *this;
}


template <typename TTraits>
typename CRowReader<TTraits>::CRowIterator&
CRowReader<TTraits>::CRowIterator::operator++(void)
{
    x_CheckAdvancing();
    m_RowReader->x_ReadNextRow();
    return *this;
}


template <typename TTraits>
CRowReader<TTraits>::CRowIterator::CRowIterator(
        CRowReader<TTraits>* s,
        bool                 is_end) :
    m_RowReader(s), m_IsEndIter(is_end)
{}


// Needed for the constness of the CRowReader::end() call
template <typename TTraits>
CRowReader<TTraits>::CRowIterator::CRowIterator(
        const CRowReader<TTraits>* s,
        bool                       is_end) :
    m_RowReader(const_cast<CRowReader<TTraits>*>(s)),
    m_IsEndIter(is_end)
{}


template <typename TTraits>
void CRowReader<TTraits>::CRowIterator::x_CheckDereferencing(void) const
{
    if (m_RowReader->m_Validation)
        NCBI_THROW2(CRowReaderException, eIteratorWhileValidating,
                    "It is prohibited to use iterators "
                    "during the stream validation", nullptr);
    if (m_IsEndIter || m_RowReader->m_AtEnd)
        NCBI_THROW2(CRowReaderException, eDereferencingEndIterator,
                    "Dereferencing end iterator is prohibited", nullptr);
}


template <typename TTraits>
void CRowReader<TTraits>::CRowIterator::x_CheckAdvancing(void) const
{
    if (m_RowReader->m_Validation)
        NCBI_THROW2(CRowReaderException, eIteratorWhileValidating,
                    "It is prohibited to use iterators "
                    "during the stream validation", nullptr);
    if (m_IsEndIter ||
        (m_RowReader->m_AtEnd && m_RowReader->m_NextDataSource.m_Stream == nullptr))
        NCBI_THROW2(CRowReaderException, eAdvancingEndIterator,
                    "Advancing end iterator is prohibited", nullptr);
}


template <typename TTraits>
void CRowReader<TTraits>::CRowIterator::x_CheckFieldAccess(void) const
{
    if (m_RowReader->m_Validation)
        NCBI_THROW2(CRowReaderException, eIteratorWhileValidating,
                    "It is prohibited to use iterators "
                    "during the stream validation", nullptr);
    if (m_IsEndIter || m_RowReader->m_AtEnd)
        NCBI_THROW2(CRowReaderException, eEndIteratorRowAccess,
                    "Accessing row field via end iterator", nullptr);
}

// End of the CRowReader::CRowIterator implementation


END_NCBI_SCOPE

#endif  /* UTIL___ROW_READER__HPP */
