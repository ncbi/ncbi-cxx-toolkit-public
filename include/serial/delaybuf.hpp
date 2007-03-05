#ifndef DELAYBUF__HPP
#define DELAYBUF__HPP

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
*   Memory buffer to hold unparsed input data
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <serial/serialdef.hpp>
#include <memory>


/** @addtogroup ObjStreamSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CByteSource;
class CItemInfo;

/////////////////////////////////////////////////////////////////////////////
///
///  CDelayBuffer
///
///  Memory buffer to hold unparsed input data
class NCBI_XSERIAL_EXPORT CDelayBuffer
{
public:
    CDelayBuffer(void)
        {
        }
    ~CDelayBuffer(void);

    /// Check if there is input data in the buffer
    ///
    /// @return
    ///   TRUE is the buffer is not empty
    bool Delayed(void) const
        {
            return m_Info.get() != 0;
        }

    DECLARE_OPERATOR_BOOL_PTR(m_Info.get());

    /// Forget the stored data
    void Forget(void);
    
    /// Parse stored data
    void Update(void)
        {
            if ( Delayed() )
                DoUpdate();
        }

    /// Check stored data format
    ///
    /// @param format
    ///   Data format
    /// @return
    ///   TRUE is stored data is in this format
    bool HaveFormat(ESerialDataFormat format) const
        {
            const SInfo* info = m_Info.get();
            return info && info->m_DataFormat == format;
        }
    
    /// Get data source
    CByteSource& GetSource(void) const
        {
            return *m_Info->m_Source;
        }

    /// Get member index
    TMemberIndex GetIndex(void) const;

    /// Reset the buffer with a new data
    void SetData(const CItemInfo* itemInfo, TObjectPtr object,
                 ESerialDataFormat dataFormat, CByteSource& data);

private:
    struct SInfo
    {
    public:
        SInfo(const CItemInfo* itemInfo, TObjectPtr object,
              ESerialDataFormat dataFormat, CByteSource& source);
        ~SInfo(void);

        // member info
        const CItemInfo* m_ItemInfo;
        // main object
        TObjectPtr m_Object;
        // data format
        ESerialDataFormat m_DataFormat;
        // data source
        mutable CRef<CByteSource> m_Source;
    };

    // private method declarations to prevent implicit generation by compiler
    CDelayBuffer(const CDelayBuffer&);
    CDelayBuffer& operator==(const CDelayBuffer&);
    static void* operator new(size_t);

    void DoUpdate(void);

    auto_ptr<SInfo> m_Info;
};

/* @} */


END_NCBI_SCOPE

#endif  /* DELAYBUF__HPP */
