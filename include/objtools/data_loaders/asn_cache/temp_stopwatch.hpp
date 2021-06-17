#ifndef TEMP_STOPWATCH_HPP__
#define TEMP_STOPWATCH_HPP__

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
 * Authors:  Cheinan Marks
 *
 * File Description:
 * A temporary stopwatch class that can be compiled out by changing its
 * template parameter.
 */

BEGIN_NCBI_SCOPE

template<bool FIsInstantiated> class CTempStopWatch
{
    // This class should never be instantiated.  Use one of the
    // specializations below.
};

template<> class CTempStopWatch<false>
{
public:
    void    Start() {}
    double  Elapsed() { return 0.0; }
    void    Stop() {}
    double  Restart() { return 0.0; }

    enum    { EIsInstantiated = false };
};

template<> class CTempStopWatch<true>
{
public:
    void    Start() { m_StopWatch.Start(); }
    double  Elapsed() { return m_StopWatch.Elapsed(); }
    void    Stop() { m_StopWatch.Stop(); }
    double  Restart() { return m_StopWatch.Restart(); }

    enum    { EIsInstantiated = true };

private:
    CStopWatch  m_StopWatch;
};

END_NCBI_SCOPE

#endif  // TEMP_STOPWATCH_HPP__

