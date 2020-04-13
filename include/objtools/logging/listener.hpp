
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
 *  Objtools message listener classes - based on ILineErrorListener
 *
 */

#ifndef _OBJTOOLS_LISTENER_HPP_
#define _OBJTOOLS_LISTENER_HPP_

#include <corelib/ncbistd.hpp>
#include <objtools/logging/message.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


//  ============================================================================
class NCBI_XOBJUTIL_EXPORT IObjtoolsListener
//  ============================================================================
{
public:
    virtual ~IObjtoolsListener(void) = default;

    virtual bool SevEnabled(EDiagSev severity) const;

    virtual bool PutMessage(const IObjtoolsMessage& message) = 0;
};


//  ============================================================================
class NCBI_XOBJUTIL_EXPORT CObjtoolsListener : public IObjtoolsListener
//  ============================================================================
{
protected:
    using TMessages = vector<unique_ptr<IObjtoolsMessage>>;
private:
    using TBaseIterator = TMessages::const_iterator;

public:
    CObjtoolsListener() = default;

    CObjtoolsListener(const CObjtoolsListener&) = delete;

    CObjtoolsListener& operator=(const CObjtoolsListener&) = delete;

    virtual ~CObjtoolsListener(void);

    virtual bool PutMessage(const IObjtoolsMessage& message);

    virtual void PutProgress(const string& message,
        const Uint8 iNumDone,
        const Uint8 iNumTotal);

    virtual const IObjtoolsMessage& GetMessage(size_t index) const;

    virtual size_t Count(void) const;

    virtual void ClearAll(void);

    virtual size_t LevelCount(EDiagSev severity) const;

    virtual void Dump(CNcbiOstream& ostr) const;

    virtual void DumpAsXML(CNcbiOstream& ostr) const;

    virtual void SetProgressOstream(CNcbiOstream* pProgressOstream);

    class CConstIterator : public TBaseIterator {
    public:
        using value_type = TBaseIterator::value_type::element_type;
        using pointer = value_type*;
        using reference = value_type&;

        CConstIterator(const TBaseIterator& base_it) : TBaseIterator(base_it) {}

        reference operator*() const { return *(this->TBaseIterator::operator*()); }
        pointer operator->() const { return this->TBaseIterator::operator*().get(); }
    };    

    using TConstIterator = CConstIterator;
    TConstIterator begin(void) const;
    TConstIterator end(void) const;
protected:
    TMessages m_Messages;
    CNcbiOstream* m_pProgressOstrm = nullptr;
};


//  ============================================================================
class NCBI_XOBJUTIL_EXPORT CObjtoolsListenerLevel : public CObjtoolsListener
//  ============================================================================
{
public:
    CObjtoolsListenerLevel(int accept_level);
    virtual ~CObjtoolsListenerLevel(void);

    bool PutMessage(const IObjtoolsMessage& message) override;
private:
    int m_AcceptLevel;
};


//  =================================================================================
class NCBI_XOBJUTIL_EXPORT CObjtoolsListenerLenient final : public CObjtoolsListenerLevel
//  =================================================================================
{
public:
    CObjtoolsListenerLenient(void);
};


//  =================================================================================
class NCBI_XOBJUTIL_EXPORT CObjtoolsListenerStrict final : public CObjtoolsListenerLevel
//  =================================================================================
{
public:
    CObjtoolsListenerStrict(void);
};

END_SCOPE(objects)
END_NCBI_SCOPE


#endif // _OBJTOOLS_LISTENER_HPP_

