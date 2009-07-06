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
 * Author: Frank Ludwig
 *
 * File Description:
 *   Basic reader interface
 *
 */

#ifndef OBJTOOLS_READERS___ERRORCONTAINER__HPP
#define OBJTOOLS_READERS___ERRORCONTAINER__HPP

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ============================================================================
class CLineError
//  ============================================================================
{
public:
    CLineError(
        EDiagSev eSeverity = eDiag_Error,
        unsigned int uLine = 0,
        const std::string& strMessage = std::string( "" ) )
    : m_eSeverity( eSeverity ), m_uLine( uLine ), m_strMessage( strMessage ) {};
        
    EDiagSev
    Severity() const { return m_eSeverity; };
    
    unsigned int
    Line() const { return m_uLine; };
    
    std::string
    Message() const { return m_strMessage; };
    
    std::string
    SeverityStr() const 
    {
        switch ( m_eSeverity ) {
        default:
            return "Unknown";
        case eDiag_Info:
            return "Info";
        case eDiag_Warning:
            return "Warning";
        case eDiag_Error:
            return "Error";
        case eDiag_Critical:
            return "Critical";
        case eDiag_Fatal:
            return "Fatal";
        }
    };
    
    void Dump( 
        std::ostream& out )
    {
        out << "            " << SeverityStr() << ":" << endl;
        out << "Line:       " << Line() << endl;
        out << "Message:    " << Message() << endl;
        out << endl;
    };
        
protected:
    EDiagSev m_eSeverity;
    unsigned int m_uLine;
    std::string m_strMessage;
};

//  ============================================================================
class CObjReaderLineException
//  ============================================================================
    : public CObjReaderParseException
{
public:
    CObjReaderLineException(
        EDiagSev eSeverity,
        unsigned int uLine,
        const std::string& strMessage )
    : CObjReaderParseException( DIAG_COMPILE_INFO, 0, eFormat, strMessage, uLine, 
        eDiag_Info ) 
    {
        SetSeverity( eSeverity );
    };
    
    EDiagSev Severity() const { return GetSeverity(); };
    unsigned int LineNumber() const { return m_uLineNumber; };
    const std::string& Message() const { return GetMsg(); };
    
    //
    //  Cludge alert: The line number may not be known at the time the exception
    //  is generated. In that case, the exception will be fixed up before being
    //  rethrown.
    //
    void 
    SetLineNumber(
        unsigned int uLineNumber ) { m_uLineNumber = uLineNumber; };
        
protected:
    unsigned int m_uLineNumber;
};
        
//  ============================================================================
class CErrorContainer
//  ============================================================================
{
public:
    CErrorContainer() {};
    
public:
    bool
    PostError(
        const CObjReaderLineException& err ) 
    { 
        m_Errors.push_back( 
            CLineError( err.Severity(), err.LineNumber(), err.Message() ) );
        return ( err.Severity() < eDiag_Critical );
    };
        
    size_t
    Count() const { return m_Errors.size(); };
    
    void
    ClearAll() { m_Errors.clear(); };
    
    const CLineError&
    GetError(
        size_t uPos ) { return m_Errors[ uPos ]; };
    
    void Dump(
        std::ostream& out )
    {
        if ( m_Errors.size() ) {
//            out << "ErrorContainer with " << m_Errors.size() << "errors:" << endl;
            std::vector<CLineError>::iterator it;
            for ( it= m_Errors.begin(); it != m_Errors.end(); ++it ) {
                it->Dump( out );
                out << endl;
            }
        }
        else {
            out << "(( no errors ))" << endl;
        }
    };
            
protected:
    std::vector< CLineError > m_Errors;
        
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___ERRORCONTAINER__HPP
