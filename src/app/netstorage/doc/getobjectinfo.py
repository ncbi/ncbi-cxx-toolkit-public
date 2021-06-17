# Preconditions:
# - MS SQL access required for the service
def GetObjectInfo( locator ):
    """ Note: the DB record is not created if it does not exist """

    if DBRecordExists(locator):
        if IsObjectExpired(locator):
            return "ObjectExpired"
        result = FillResponceWithObjectInfo(locator)
    else:
        result = FillResponceWithNoObjectInfo()

    # Here: record in the MS SQL does not exist
    # Note: asking backend happens anyway
    info = GetObjectInfoFromBackend(locator)
    result.append( info )
    return result
