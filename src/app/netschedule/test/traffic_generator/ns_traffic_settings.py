#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Settings for the NetSchedule traffic loader
"""


class IndividualLoaders:
    " Holds what loaders should be switched on "

    # Use True to enable a loader

    SubmitDropLoader       = True
    BatchSubmitDropLoader  = True
    SingleFullOKLoopLoader = True



class SubmitDropSettings:
    " Settings for the submit-drop loader "

    packageSize = 100  # Number of submit-drop ops in a raw
    pause = 2          # Pause in secs between packages
    packagesCount = 10 # Number of packages. -1 unlimited


class BatchSubmitDropSettings:
    " Settings for the batch submit-drop loader "

    packageSize = 100  # Number of submit-drop ops in a raw
    jobsInBatch = 32   # Number of jobs in each batch submit
    pause = 2          # Pause in secs between packages
    packagesCount = 10 # Number of packages. -1 unlimited



class SingleFullOKLoopSettings:
    " Settings for the Submit/Get/Commit/Read/Confirm loader "

    packageSize = 100  # Number of submit-drop ops in a raw
    pause = 2          # Pause in secs between packages
    packagesCount = 10 # Number of packages. -1 unlimited




