#ifndef CLEANUP___CLEANUP__HPP
#define CLEANUP___CLEANUP__HPP

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
 * Author:  Robert Smith
 *
 * File Description:
 *   Basic Cleanup of CSeq_entries.
 *   .......
 *
 */
#include <corelib/ncbistd.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;

class CCleanupChange;

class NCBI_CLEANUP_EXPORT CCleanupChangeItem : public CObject 
{
public:
    

    // destructor
    ~CCleanupChangeItem(void);

    // Change code
    const string&           GetChangeCode  (void) const;
    unsigned int            GetChangeIndex (void) const;
    static unsigned int     GetChangeCount(void);
    // Change message
    const string&           GetMsg      (void) const;
    // Verbose message
    const string&           GetVerbose  (void) const;
    // Offending object description
    const string&           GetObjDesc  (void) const;
    // Offending object
    const CSerialObject&    GetObject   (void) const;

    // Convert change code from enum to a string representation
    static const string& ConvertChangeCode(unsigned int);
    
private:
    friend class CCleanupChange;

    typedef CConstRef<CSerialObject>    TObject;

    // constructor
    CCleanupChangeItem(unsigned int         ec,        // change code
                  const string&        msg,       // message
                  const string&        obj_desc,  // object description
                  const CSerialObject& obj,       // offending object
                  CScope&              scope);    // scope
    
    CCleanupChangeItem(void);
    CCleanupChangeItem(const CCleanupChangeItem&);

    void x_SetAcc(void);
    void x_SetAcc(const CBioseq& seq);

    // member data values
    unsigned int  m_ChangeIndex; // error code index
    string        m_Message;    // specific error message
    string        m_Desc;       // changed object's description
    TObject       m_Object;     // changed object
    CRef<CScope>  m_Scope;      // scope

    // currently used for Seqdesc objects only
    CConstRef<CSeq_entry> m_Ctx;

    static const string sm_Terse[];
    static const string sm_Verbose[];
};


class NCBI_CLEANUP_EXPORT CCleanupChange : public CObject
{
public:
    // constructors
    CCleanupChange(const CSerialObject* obj = NULL);
    
    void AddChangedItem(unsigned int         ec,      // error code
                         const string&        msg,     // specific error message
                         const string&        desc,    // offending object's description
                         const CSerialObject& obj,     // offending object
                         CScope&              scope);  // scope

    // Statistics
    SIZE_TYPE Size(void)    const;

    // Get the cleaned object (Seq-entry, Seq-submit or Seq-align)
    const CSerialObject* GetCleaned(void) const;  // was GetValidated()

    // destructor
    ~CCleanupChange(void);

private:

    friend class CCleanupChange_CI;

    typedef vector < CConstRef < CCleanupChangeItem > > TChanges;
    typedef CConstRef<CSerialObject>               TValidated;
    typedef CConstRef<CSeq_entry>                  TContext;

    // Prohibit copy constructor & assignment operator
    CCleanupChange(const CCleanupChange&);
    CCleanupChange& operator= (const CCleanupChange&);

    // data
    TChanges    m_ChangeItems;  // Change list
    TValidated  m_Cleaned;      // the cleaned object
};


class NCBI_CLEANUP_EXPORT CCleanupChange_CI
{
public:
    CCleanupChange_CI(void);
    CCleanupChange_CI(const CCleanupChange& ve,
                   const string& errcode = kEmptyStr);
    CCleanupChange_CI(const CCleanupChange_CI& iter);
    virtual ~CCleanupChange_CI(void);

    CCleanupChange_CI& operator=(const CCleanupChange_CI& iter);
    CCleanupChange_CI& operator++(void);

    bool IsValid(void) const;
    DECLARE_OPERATOR_BOOL(IsValid());

    const CCleanupChangeItem& operator* (void) const;
    const CCleanupChangeItem* operator->(void) const;

private:
    bool Filter(const CCleanupChangeItem& item) const;
    bool AtEnd(void) const;
    void Next(void);

    CConstRef<CCleanupChange>                  m_Changes;
    CCleanupChange::TChanges::const_iterator   m_Current;

    // filters:
    string                               m_ChangeCodeFilter;
};


class NCBI_CLEANUP_EXPORT CCleanup : public CObject 
{
public:

    enum EValidOptions {};

    // Construtor / Destructor
    CCleanup();
    ~CCleanup();

    // Validate Seq-entry. 
    // If provding a scope the Seq-entry must be a 
    // top-level Seq-entry in that scope.
    CConstRef<CCleanupChange> BasicCleanup(CSeq_entry& se, Uint4 options = 0);

private:
    // Prohibit copy constructor & assignment operator
    CCleanup(const CCleanup&);
    CCleanup& operator= (const CCleanup&);

};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2005/10/18 13:34:09  rsmith
* add change reporting classes
*
* Revision 1.1  2005/10/17 12:21:07  rsmith
* initial checkin
*
*
* ===========================================================================
*/

#endif  /* CLEANUP___CLEANUP__HPP */
