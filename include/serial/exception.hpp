#ifndef EXCEPTION__HPP
#define EXCEPTION__HPP
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
* Author: Eugene Vasilchenko
*
* File Description:
*   Standard exception classes used in serial package
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiexpt.hpp>


/** @addtogroup ObjStreamSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CSerialObject;

/// Root class for all serialization exceptions
class NCBI_XSERIAL_EXPORT CSerialException : public CException
{
public:
    /// Error codes
    enum EErrCode {
        eNotImplemented,  ///< Attempt to use unimplemented funtionality
        eEOF,             ///< Unexpected end-of-file
        eIoError,         ///< An unknown error during serialization
        eFormatError,     ///< Malformed input data
        eOverflow,        ///< Data is beyond the allowed limits
        eInvalidData,     ///< Data is incorrect
        eIllegalCall,     ///< Illegal in a given context function call
        eFail,            ///< Internal error, the real reason is unclear
        eNotOpen,         ///< No input or output file
        eMissingValue     ///< Mandatory value was missing in the input.
    };
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CSerialException,CException);

public:
    // Combine steram frames info into single message
    void AddFrameInfo(string frame_info);
    virtual void ReportExtra(ostream& out) const;
private:
    string m_FrameStack;
};


/// Thrown on an attempt to write unassigned data member
class NCBI_XSERIAL_EXPORT CUnassignedMember : public CSerialException
{
public:
    enum EErrCode {
        eGet,
        eWrite,
        eUnknownMember
    };
    virtual const char* GetErrCodeString(void) const;

    NCBI_EXCEPTION_DEFAULT(CUnassignedMember,CSerialException);
};


/// Thrown on an attempt to access wrong choice variant
///
/// For example, if in a choice (a|b), the variant 'a' is selected;
/// this exception will be thrown on an attempt to access variant 'b' data
class NCBI_XSERIAL_EXPORT CInvalidChoiceSelection : public CSerialException
{
public:
    enum EErrCode {
        eFail
    };
    virtual const char* GetErrCodeString(void) const;
    static const char* GetName(size_t index,
                               const char* const names[], 
                               size_t namesCount);

    CInvalidChoiceSelection(const CDiagCompileInfo& diag_info,
                            const CSerialObject* object,
                            size_t currentIndex, size_t mustBeIndex,
                            const char* const names[], size_t namesCount, 
                            EDiagSev severity = eDiag_Error);
    // for backward compatibility
    CInvalidChoiceSelection(const CDiagCompileInfo& diag_info,
                            size_t currentIndex, size_t mustBeIndex,
                            const char* const names[], size_t namesCount, 
                            EDiagSev severity = eDiag_Error);
    // for backward compatibility
    CInvalidChoiceSelection(const char* file, int line,
                            size_t currentIndex, size_t mustBeIndex,
                            const char* const names[], size_t namesCount, 
                            EDiagSev severity = eDiag_Error);
    // for backward compatibility
    CInvalidChoiceSelection(size_t currentIndex, size_t mustBeIndex,
                            const char* const names[], size_t namesCount,
                            EDiagSev severity = eDiag_Error);

    CInvalidChoiceSelection(const CInvalidChoiceSelection& other);
    virtual ~CInvalidChoiceSelection(void) throw();

    virtual const char* GetType(void) const;
    typedef int TErrCode;
    TErrCode GetErrCode(void) const;

protected:
    CInvalidChoiceSelection(void);
    virtual const CException* x_Clone(void) const;
};

END_NCBI_SCOPE


/* @} */

#endif /* EXCEPTION__HPP */
