# Preconditions:
# - MS SQL access required for the service
def Exists( locator,
            ConsultBackendIfNoDBRecord = True ):
    """ Note: the DB record is not created if it does not exist """
    ObjectSizeInDBIsNULL = False
    if DBRecordExists(locator):
        if IsObjectExpired(locator):
            return "ObjectExpired"
        if ObjectSizeInDB is not NULL:
            return ObjectSizeInDB
        ObjectSizeInDBIsNULL = True

    if ConsultBackendIfNoDBRecord or ObjectSizeInDBIsNULL:
        size = GetSizeFromBackend( locator )
        if ObjectSizeInDBIsNULL:
            UpdateSizeInDBIfStillNULL( locator, size )
        return size

    return "ObjectNotFound"
