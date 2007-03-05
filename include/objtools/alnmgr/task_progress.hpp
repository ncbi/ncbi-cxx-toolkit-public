#ifndef OBJECTS_ALNMGR___TASK_PROGRESS__HPP
#define OBJECTS_ALNMGR___TASK_PROGRESS__HPP

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
* Author:  Kamen Todorov, NCBI
*
* File Description:
*   Callback interface for feedback on task progress
*
*/


BEGIN_NCBI_SCOPE


class NCBI_XALNMGR_EXPORT ITaskProgressCallback
/// Task clients implement this callback interface
{
public:
    virtual void SetTaskName      (const string& name) = 0;
    virtual void SetTaskCompleted (int completed)      = 0;
    virtual void SetTaskTotal     (int total)          = 0;
    virtual bool InterruptTask    ()                   = 0;
    virtual ~ITaskProgressCallback() {}
};



class NCBI_XALNMGR_EXPORT CTaskProgressReporter
/// Tasks that report progress derive from this class
{

public:
    /// Constructor
    CTaskProgressReporter() : m_Callback(0) {};

    /// Hook a callback to a task
    void SetTaskProgressCallback(ITaskProgressCallback* callback) {
        m_Callback = callback;
    }

protected:
    /// Methods for reporting task progress
    void x_SetTaskName(const string& name) {
        if (m_Callback) {
            m_Callback->SetTaskName(name);
        }
    }
    void x_SetTaskCompleted(int completed) {
        if (m_Callback) {
            m_Callback->SetTaskCompleted(completed);
        }
    }
    void x_SetTaskTotal(int total) {
        if (m_Callback) {
            m_Callback->SetTaskTotal(total);
        }
    }

    /// Check if the task should be interrupted
    bool x_InterruptTask() {
        if (m_Callback) {
            return m_Callback->InterruptTask();
        }
        return false;
    }

    /// Callback accessor
    ITaskProgressCallback* x_GetTaskProgressCallback() const {
        return m_Callback;
    }

private:
    ITaskProgressCallback* m_Callback;
};


END_NCBI_SCOPE

#endif // OBJECTS_ALNMGR___TASK_PROGRESS__HPP
