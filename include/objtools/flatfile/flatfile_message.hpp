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
 * Author: Justin Foley
 *
 * File Description:
 *
 */

#ifndef _FLATFILE_MESSAGE_HPP_
#define _FLATFILE_MESSAGE_HPP_

#include <corelib/ncbistd.hpp>
#include <objtools/logging/message.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CFlatFileMessage : public CObjtoolsMessage
{
public:
    CFlatFileMessage(const string& module,
                     EDiagSev      severity,
                     int           code,
                     int           subcode,
                     const string& text,
                     int           lineNum = -1);

    virtual ~CFlatFileMessage();

    CFlatFileMessage* Clone(void) const override;
    void              Dump(CNcbiOstream& out) const override;
    void              DumpAsXML(CNcbiOstream& out) const override;
    void              Write(CNcbiOstream& out) const override;
    void              WriteAsXML(CNcbiOstream& out) const override;
    const string&     GetModule() const;
    int               GetCode(void) const override;
    int               GetSubCode(void) const override;
    int               GetLineNum(void) const;

private:
    string m_Module;
    int    m_Code;
    int    m_Subcode;
    int    m_LineNum;
};

END_SCOPE(objects)
END_NCBI_SCOPE


#endif
