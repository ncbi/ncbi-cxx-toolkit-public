#ifndef GENBANK__STATISTICS__HPP_INCLUDED
#define GENBANK__STATISTICS__HPP_INCLUDED

/*  $Id$
* ===========================================================================
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
* ===========================================================================
*
*  Author:  Eugene Vasilchenko
*
*  File Description: Classes for gathering timing statistics.
*
*/

#include <corelib/ncbitime.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class NCBI_XREADER_EXPORT CGBRequestStatistics
{
public:
    enum EStatType {
        eStat_First,

        eStat_StringSeq_ids = 0,
        eStat_Seq_idSeq_ids,
        eStat_Seq_idGi,
        eStat_Seq_idAcc,
        eStat_Seq_idLabel,
        eStat_Seq_idTaxId,
        eStat_Seq_idBlob_ids,
        eStat_BlobState,
        eStat_BlobVersion,
        eStat_LoadBlob,
        eStat_LoadSNPBlob,
        eStat_LoadSplit,
        eStat_LoadChunk,
        eStat_ParseBlob,
        eStat_ParseSNPBlob,
        eStat_ParseSplit,
        eStat_ParseChunk,

        eStats_Count,
        eStat_Last = eStats_Count-1
    };

    CGBRequestStatistics(const char* action, const char* entity);

    const char* GetAction(void) const {
        return m_Action;
    }
    const char* GetEntity(void) const {
        return m_Entity;
    }
    size_t GetCount(void) const {
        return m_Count;
    }
    double GetTime(void) const {
        return m_Time;
    }
    double GetSize(void) const {
        return m_Size;
    }

    static const CGBRequestStatistics& GetStatistics(EStatType type);

    void PrintStat(void) const;
    static void PrintStatistics(void);
    
    void AddTime(double time) {
        m_Count += 1;
        m_Time += time;
    }

    void AddTimeSize(double time, double size) {
        m_Count += 1;
        m_Time += time;
        m_Size += size;
    }

private:
    const char* m_Action;
    const char* m_Entity;
    size_t m_Count;
    double m_Time;
    double m_Size;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif //GENBANK__STATISTICS__HPP_INCLUDED
