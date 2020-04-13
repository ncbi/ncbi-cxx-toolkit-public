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
 * Author: Justin Foley
 *
 * File Description:
 *  Basic objtools message interface
 *
 */

#ifndef _OBJTOOLS_MESSAGE_HPP_
#define _OBJTOOLS_MESSAGE_HPP_

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects) 

//  ============================================================================
class NCBI_XOBJUTIL_EXPORT IObjtoolsMessage 
//  ============================================================================
{
public:
    virtual ~IObjtoolsMessage(void) = default;

    virtual IObjtoolsMessage *Clone(void) const = 0;

    virtual void Write(CNcbiOstream& out) const = 0;

    virtual void Dump(CNcbiOstream& out) const = 0;

    virtual void WriteAsXML(CNcbiOstream& out) const = 0;

    virtual void DumpAsXML(CNcbiOstream& out) const = 0;

    virtual string GetText(void) const = 0;
    virtual EDiagSev GetSeverity(void) const = 0;
    virtual int GetCode(void) const = 0;
    virtual int GetSubCode(void) const = 0;
};


//  ============================================================================
class NCBI_XOBJUTIL_EXPORT CObjtoolsMessage : public IObjtoolsMessage 
//  ============================================================================
{
public:
    CObjtoolsMessage(const string& text,
                     EDiagSev severity);

    virtual CObjtoolsMessage *Clone(void) const;

    NCBI_DEPRECATED virtual string Compose(void) const;

    virtual void Write(CNcbiOstream& out) const;

    virtual void Dump(CNcbiOstream& out) const;

    virtual void WriteAsXML(CNcbiOstream& out) const;

    virtual void DumpAsXML(CNcbiOstream& out) const;

    virtual string GetText(void) const;
    virtual EDiagSev GetSeverity(void) const;
    virtual int GetCode(void) const;
    virtual int GetSubCode(void) const;

protected:
    string m_Text;
    EDiagSev m_Severity;
};


//  ============================================================================
class NCBI_XOBJUTIL_EXPORT CProgressMessage : public CObjtoolsMessage 
//  ============================================================================
{
public:
    CProgressMessage(
        int numDone, int numTotal);

    virtual CProgressMessage *Clone() const override;
    virtual void Write(CNcbiOstream& out) const override;
  
    virtual int Done() const { return mDone; };
    virtual int Total() const { return mTotal; };

protected:
    int mDone;
    int mTotal;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _OBJTOOLS_MESSAGE_HPP_
