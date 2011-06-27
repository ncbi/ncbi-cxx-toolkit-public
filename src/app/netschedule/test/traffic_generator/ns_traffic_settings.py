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

    SubmitDropLoader = True




class SubmitDropSettings:
    " Settings for the submit-drop loader "

    packageSize = 100  # Number of submit-drop ops in a raw
    pause = 0          # Pause in secs between packages
    packagesCount = 10 # Number of packages. -1 unlimited



