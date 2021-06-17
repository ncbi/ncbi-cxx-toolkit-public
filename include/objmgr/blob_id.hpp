#ifndef OBJECTS_OBJMGR___BLOB_ID__HPP
#define OBJECTS_OBJMGR___BLOB_ID__HPP

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
*   Class used for identification of top level Seq-entries,
*   also known as TSEs and blobs.
*
*/

#include <corelib/ncbiobj.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <functional>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/** @addtogroup ObjectManagerCore
 *
 * @{
 */

template<typename Value> struct PConvertToString;

template<>
struct PConvertToString<int>
{
    string operator()(int v) const
        {
            return NStr::IntToString(v);
        }
};


template<>
struct PConvertToString<const void*>
{
    string operator()(const void* v) const
        {
            return NStr::PtrToString(v);
        }
};


template<>
struct PConvertToString<string>
{
    const string& operator()(const string& v) const
        {
            return v;
        }
};


template<>
struct PConvertToString<CSeq_id_Handle>
{
    string operator()(const CSeq_id_Handle& v) const
        {
            return v.AsString();
        }
};


class NCBI_XOBJMGR_EXPORT CBlobId : public CObject
{
public:
    virtual ~CBlobId(void);

    /// Get string representation of blob id.
    /// Should be unique for all blob ids from the same data loader.
    virtual string ToString(void) const = 0;

    // Comparison methods
    virtual bool operator<(const CBlobId& id) const = 0;
    virtual bool operator==(const CBlobId& id) const;

protected:
    bool LessByTypeId(const CBlobId& id2) const;
};


template<typename Value, class Converter = PConvertToString<Value> >
class CBlobIdFor : public CBlobId
{
public:
    typedef Value value_type;
    typedef Converter convert_to_string_type;
    typedef CBlobIdFor<Value, Converter> TThisType; // for self reference

    CBlobIdFor(const value_type& v)
        : m_Value(v)
        {
        }

    const value_type& GetValue(void) const
        {
            return m_Value.second();
        }

    string ToString(void) const
        {
            return m_Value.first()(m_Value.second());
        }

    bool operator==(const CBlobId& id_ref) const
        {
            const TThisType* id_ptr = dynamic_cast<const TThisType*>(&id_ref);
            return id_ptr && GetValue() == id_ptr->GetValue();
        }
    bool operator<(const CBlobId& id_ref) const
        {
            const TThisType* id_ptr = dynamic_cast<const TThisType*>(&id_ref);
            if ( !id_ptr ) {
                return this->LessByTypeId(id_ref);
            }
            return GetValue() < id_ptr->GetValue();
        }

private:
    pair_base_member<convert_to_string_type, value_type> m_Value;
};


typedef CBlobIdFor<int> CBlobIdInt;
typedef CBlobIdFor<const void*> CBlobIdPtr;
typedef CBlobIdFor<string> CBlobIdString;
typedef CBlobIdFor<CSeq_id_Handle> CBlobIdSeq_id;


class CBlobIdKey
{
public:
    CBlobIdKey(const CBlobId* id = 0)
        : m_Id(id)
        {
        }

    DECLARE_OPERATOR_BOOL_REF(m_Id);

    const CBlobId& operator*(void) const
        {
            return *m_Id;
        }
    const CBlobId* operator->(void) const
        {
            return m_Id;
        }

    string ToString(void) const
        {
            return m_Id->ToString();
        }

    bool operator<(const CBlobIdKey& id) const
        {
            return *m_Id < *id;
        }

    bool operator==(const CBlobIdKey& id) const
        {
            return *m_Id == *id;
        }
    bool operator!=(const CBlobIdKey& id) const
        {
            return !(*m_Id == *id);
        }

private:
    CConstRef<CBlobId>  m_Id;
};


inline CNcbiOstream& operator<<(CNcbiOstream& out, const CBlobId& id)
{
    return out << id.ToString();
}


inline CNcbiOstream& operator<<(CNcbiOstream& out, const CBlobIdKey& id)
{
    return out << *id;
}


/* @} */

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // OBJECTS_OBJMGR___BLOB_ID__HPP
