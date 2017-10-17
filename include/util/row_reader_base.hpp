#ifndef UTIL___ROW_READER_BASE__HPP
#define UTIL___ROW_READER_BASE__HPP

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
* Authors: Denis Vakatov, Sergey Satskiy
*
* Credits: Alex Astashyn
*          (the optimized version of NcbiGetlineEOL)
*
* File Description:
*   Implementation of the CRowReaderStream_Base traits. This class can
*   be used as a base class for the other traits implementations.
*
* ===========================================================================
*/

#include <util/row_reader.hpp>


BEGIN_NCBI_SCOPE

/// General traits implementation recommendations:
///
///  - Use IMessage API to communicate processing messages, errors and progress
///    to the upper-level code


/// Traits base class:
///  - Illustrates and explains the trait's API
///  - Can be used to derive more complex user-defined traits
///  - Does not split a row into fields. Treats the row content as one field.
///    (no header, comments, empty lines, etc.)
class CRowReaderStream_Base
{
public:
    // Can be extended and used in the derived (or alternative) implementations
    typedef ERR_FieldType TExtendedFieldType;

    // Can be extended and used in the derived (or alternative) implementations
    // Requirement: the TRR_Context must derive from CRR_Context if extended
    typedef CRR_Context TRR_Context;

    /// Validation mode
    /// Derived traits can override and extend this enum type.
    /// @attention  The overrides must have eRR_ValidationMode_Default variant!
    enum ERR_ValidationMode {
        eRR_ValidationMode_Default
    };

    /// Set the validation mode
    /// @param validation_mode
    ///  validation mode
    void SetValidationMode(ERR_ValidationMode /* validation_mode */)
    {}

    /// Called by CRowReader<>::Validate() for each line
    /// @param raw_line
    ///  current stream line
    /// @return
    ///  Instructions of what to do next e.g. interrupt validation or continue.
    ///  See ERR_Action in row_reader.inl
    ERR_Action Validate(CTempString /* raw_line */,
                        ERR_FieldValidationMode /* field_validation_mode */)
    { return eRR_Skip; }

    /// Called before any other processing of the next read line
    /// @param raw_line
    ///  current stream line
    /// @return
    ///  Instructions of what to do next e.g. interrupt looping or continue.
    ///  See ERR_Action in row_reader.inl
    /// @example
    ///  return NStr::IsBlank(raw_line) || NStr::StartsWith(raw_line, "#")
    ///         ? eRR_Skip : eRR_Continue_Data;
    ERR_Action OnNextLine(CTempString /* raw_line */)
    { return eRR_Continue_Data; }

    /// Tokenize the raw line and put the tokens into the tokens vector
    /// @param raw_line
    ///  A raw line from an input stream
    /// @param tokens
    ///  The storage where tokens should be stored
    /// @return
    ///  Instructions of what to do next e.g. interrupt looping or continue.
    ///  See ERR_Action in row_reader.inl
    ERR_Action Tokenize(const CTempString    raw_line,
                        vector<CTempString>& tokens)
    {
        tokens.push_back(raw_line);
        return eRR_Continue_Data;
    }

    /// Translate the column value if necessary
    /// usually used for special values such as null, empty, etc
    /// @param field_no
    ///  Field index (zero based)
    /// @param raw_value
    ///  Field raw value
    /// @param translated_value
    ///  The translated value. Used only if "eRR_Translated" is returned.
    /// @return
    ///  Translation action taken
    ERR_TranslationResult Translate(TFieldNo          /* field_no */,
                                    const CTempString /* raw_value */,
                                    string&           /* translated_value */)
    { return eRR_UseOriginal; }


    /// Get trait-specific flags
    /// @return
    ///  Flags which tell how a file should be opened
    TTraitsFlags GetFlags(void)  const { return fRR_Default; }

    /// Traits can extend the stream CRR_Context the way they need and provide
    /// it in this call. The context will be used to throw exceptions.
    /// @param stream_ctx
    ///  Stream basic context. It contains information about a position in the
    ///  stream, the current line and the current data if so.
    /// @return
    ///  Traits context constructed on the basic stream context.
    /// @attention
    ///  The TRR_Context must derive from CRR_Context and must reimplement
    ///  x_Assign() member function. If these conditions are not met then the
    ///  exceptions in the client code may miss some information or the code is
    ///  not compiled at all.
    TRR_Context GetContext(const CRR_Context& stream_ctx) const
    { return stream_ctx; }


    /// Handle potentially disruptive events.
    /// Traits e.g. can repair (including resetting the underlying data source
    /// using SetDataSource()) the situation described by the event
    /// and then return "eRR_EventAction_Continue"
    /// @param event
    ///  An event in the in input stream
    /// @param event_mode
    ///  An event mode (to distinguish if it is a validiation or not)
    /// @return
    ///  Instructions what to do next e.g. stop processing the stream. See
    ///  ERR_EventAction in row_reader.inl
    ERR_EventAction OnEvent(ERR_Event /*event*/,
                            ERR_EventMode /*event_mode*/)
    { return eRR_EventAction_Default; }

    /// Read data for one row.
    /// @note
    ///  One row may consists of several raw lines
    /// @param is
    ///  Input stream to read from
    /// @param data
    ///  String to read to
    /// @return
    ///  Number of raw lines read from the stream
    size_t ReadRowData(CNcbiIstream& is, string* data)
    {
        data->clear();

        // Optimized version of NcbiGetlineEOL
        // This works for \n and \r\n but not for \r, but the latter was used
        // in old MacOS which has been obsolete for 15 years.
        std::getline(is, *data);
        if(!data->empty()  &&  data->back() == '\r')
            data->pop_back();
        return 1;
    }

    // Standard typedefs and methods to bind to the "parent stream"
    RR_TRAITS_PARENT_STREAM(CRowReaderStream_Base);
};




/////////////////////////////////////////////////////////
//  USAGE
//
//
//
// typedef CRowReader<CRowReaderStream_Base> TMyDelimitedStream;
//
//
// CNcbiIfstream       my_file("file.tab");
// TMyDelimitedStream  delim_stream(my_file);
//
//
// delim_stream.SetFieldName(1, "one");
//
//
// for (auto & row : delim_stream) {
//     cout << "Value of the first field: " << row["one"] << endl;
//
//     // Exception will be generated
//     row[1];
// }


END_NCBI_SCOPE

#endif  /* UTIL___ROW_READER_BASE__HPP */
