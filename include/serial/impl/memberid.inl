#if defined(MEMBERID__HPP)  &&  !defined(MEMBERID__INL)
#define MEMBERID__INL

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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*/

inline
const string& CMemberId::GetName(void) const
{
    return m_Name;
}

inline
CMemberId::TTag CMemberId::GetTag(void) const
{
    return m_Tag;
}

inline
bool CMemberId::HaveExplicitTag(void) const
{
    return m_ExplicitTag;
}

inline
void CMemberId::SetName(const string& name)
{
    m_Name = name;
}

inline
void CMemberId::SetTag(TTag tag, bool explicitTag)
{
    m_Tag = tag;
    m_ExplicitTag = explicitTag;
}

#endif /* def MEMBERID__HPP  &&  ndef MEMBERID__INL */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2006/10/05 19:23:37  gouriano
* Moved from parent folder
*
* Revision 1.10  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.9  2000/10/03 17:22:33  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.8  2000/09/01 13:15:59  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.7  2000/07/03 18:42:34  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.6  2000/06/16 16:31:05  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.5  2000/05/24 20:08:12  vasilche
* Implemented XML dump.
*
* Revision 1.4  1999/12/17 19:04:53  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.3  1999/07/13 20:18:06  vasilche
* Changed types naming.
*
* Revision 1.2  1999/07/01 17:55:18  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.1  1999/06/30 16:04:25  vasilche
* Added support for old ASN.1 structures.
*
* ===========================================================================
*/
