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
 *   data structures of interest to multiple readers
 *
 */

#ifndef OBJTOOLS_READERS___TRACKDATA__HPP
#define OBJTOOLS_READERS___TRACKDATA__HPP

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects) // namespace ncbi::objects::

//  ============================================================================
class NCBI_XOBJREAD_EXPORT CTrackData
//  ============================================================================
{
public:
    typedef std::vector<std::string> LineData;
    typedef std::map<const std::string, std::string > TrackData;
public:
    CTrackData();
    ~CTrackData() {};
    bool ParseLine(
        const LineData& );
    static bool IsTrackData(
        const LineData& );
    const TrackData& Values() const {return mData;};
    bool WriteToAnnot(
        CSeq_annot&);
    bool ContainsData() const {return !mData.empty();};

    //convenience accessors
    string Type() const {return ValueOf("type");};
    string Description() const {return ValueOf("description");};
    string Name() const {return ValueOf("name");};
    int Offset() const;

    string ValueOf(
        const std::string&) const;

protected:
    TrackData mData;
    //string m_strType;
    //string m_strDescription;
    //string m_strName;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___TRACKDATA__HPP
