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
#include <objtools/readers/line_error.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ============================================================================
class IErrorContainer
//  ============================================================================
{
public:
    virtual ~IErrorContainer() {}
    //
    //  return true if the error was added to the container, false if not. In the
    //  second case, the caller should terminate all further processing
    //
    virtual bool
    PutError(
        const ILineError& ) =0;
    
    virtual const ILineError&
    GetError(
        size_t ) =0;
        
    virtual size_t
    Count() const =0;
    
    virtual size_t
    LevelCount(
        EDiagSev ) =0;
            
    virtual void
    ClearAll() =0;
};
            
//  ============================================================================
class CErrorContainerBase:
//  ============================================================================
    public IErrorContainer, public CObject
{
public:
    CErrorContainerBase() {};
    virtual ~CErrorContainerBase() {};
    
public:
    size_t
    Count() const { return m_Errors.size(); };
    
    virtual size_t
    LevelCount(
        EDiagSev eSev ) {
        
        size_t uCount( 0 );
        for ( size_t u=0; u < Count(); ++u ) {
            if ( m_Errors[u].Severity() == eSev ) ++uCount;
        }
        return uCount;
    };
    
    void
    ClearAll() { m_Errors.clear(); };
    
    const ILineError&
    GetError(
        size_t uPos ) { return m_Errors[ uPos ]; };
    
    virtual void Dump(
        std::ostream& out )
    {
        if ( m_Errors.size() ) {
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

//  ============================================================================
class CErrorContainerLenient:
//
//  Accept everything.
//  ============================================================================
    public CErrorContainerBase
{
public:
    CErrorContainerLenient() {};
    ~CErrorContainerLenient() {};
    
    bool
    PutError(
        const ILineError& err ) 
    {
        m_Errors.push_back( 
            CLineError( err.Problem(), err.Severity(), err.SeqId(), err.Line(), 
                err.FeatureName(), err.QualifierName(), err.QualifierValue() ) );
        return true;
    };
};        

//  ============================================================================
class CErrorContainerStrict:
//
//  Don't accept any errors, at all.
//  ============================================================================
    public CErrorContainerBase
{
public:
    CErrorContainerStrict() {};
    ~CErrorContainerStrict() {};
    
    bool
    PutError(
        const ILineError& err ) 
    {
        m_Errors.push_back( 
            CLineError( err.Problem(), err.Severity(), err.SeqId(), err.Line(), 
                err.FeatureName(), err.QualifierName(), err.QualifierValue() ) );
        return false;
    };
};        

//  ===========================================================================
class CErrorContainerCount:
//
//  Accept up to <<count>> errors, any level.
//  ===========================================================================
    public CErrorContainerBase
{
public:
    CErrorContainerCount(
        size_t uMaxCount ): m_uMaxCount( uMaxCount ) {};
    ~CErrorContainerCount() {};
    
    bool
    PutError(
        const ILineError& err ) 
    {
        m_Errors.push_back( 
            CLineError( err.Problem(), err.Severity(), err.SeqId(), err.Line(), 
                err.FeatureName(), err.QualifierName(), err.QualifierValue() ) );
        return (Count() < m_uMaxCount);
    };    
protected:
    size_t m_uMaxCount;
};

//  ===========================================================================
class CErrorContainerLevel:
//
//  Accept evrything up to a certain level.
//  ===========================================================================
    public CErrorContainerBase
{
public:
    CErrorContainerLevel(
        int iLevel ): m_iAcceptLevel( iLevel ) {};
    ~CErrorContainerLevel() {};
    
    bool
    PutError(
        const ILineError& err ) 
    {
        m_Errors.push_back( 
            CLineError( err.Problem(), err.Severity(), err.SeqId(), err.Line(), 
                err.FeatureName(), err.QualifierName(), err.QualifierValue() ) );
        return (err.Severity() <= m_iAcceptLevel);
    };    
protected:
    int m_iAcceptLevel;
};

//  ===========================================================================
class CErrorContainerWithLog:
//
//  Accept everything, and besides storing all errors, post them.
//  ===========================================================================
    public CErrorContainerBase
{
public:
    CErrorContainerWithLog(const CDiagCompileInfo& info)
        : m_Info(info) {};
    ~CErrorContainerWithLog() {};

    bool
    PutError(
        const ILineError& err )
    {
        CNcbiDiag(m_Info, err.Severity(),
                  eDPF_Log | eDPF_IsMessage).GetRef()
           << err.Message() << Endm;

        m_Errors.push_back(
            CLineError( err.Problem(), err.Severity(), err.SeqId(), err.Line(),
                err.FeatureName(), err.QualifierName(), err.QualifierValue() ) );
        return true;
    };

private:
    const CDiagCompileInfo m_Info;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___ERRORCONTAINER__HPP
