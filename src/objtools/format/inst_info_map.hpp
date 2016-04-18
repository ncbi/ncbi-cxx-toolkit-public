/* $Id$
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
 * Author:  Michael Kornbluh
 *
 * File Description:
 *   This just wraps a hash-table that converts institution abbrev to
 *   institution name and other info.  It's in its own file because 
 *   the data itself is so large.
 *
 * ===========================================================================
 */
#ifndef OBJTOOLS_FORMAT___INST_INFO_MAP_HPP
#define OBJTOOLS_FORMAT___INST_INFO_MAP_HPP

#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CInstInfoMap {
public:
    struct SVoucherInfo : public CObject {
        SVoucherInfo( 
            const string *links,
            bool          prependInstitute,
            int           pad_to,
            const string *pad_with,
            const string *prefix,
            const string *suffix,
            const char   *inst_full_name ): 
            m_Links(links),
            m_PrependInstitute(prependInstitute),
            m_PadTo(pad_to),
            m_PadWith(pad_with),
            m_Prefix(prefix),
            m_Suffix(suffix),
            m_InstFullName(inst_full_name) { }

        const string *m_Links;
        bool          m_PrependInstitute;
        int           m_PadTo;
        const string *m_PadWith;
        const string *m_Prefix;
        const string *m_Suffix;
        const char   *m_InstFullName;
    };

    // Returns unset CConstRef if can't find it
    typedef CConstRef<SVoucherInfo> TVoucherInfoRef;
    static TVoucherInfoRef GetInstitutionVoucherInfo(
        const string &inst_abbrev );

private:

    // forbid construction, etc.
    CInstInfoMap(void);
    CInstInfoMap( const CInstInfoMap & );
    void operator = (const CInstInfoMap &);
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_FORMAT___INST_INFO_MAP_HPP */
