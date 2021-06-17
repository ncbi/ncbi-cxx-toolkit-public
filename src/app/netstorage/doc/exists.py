# Preconditions:
# - MS SQL access required for the service
def Exists( locator,
            AllowBackendFallback = True ):
    """ Note: the DB record is not created if it does not exist """

    if DBRecordExists(locator):
        if IsObjectExpired(locator):
            return "ObjectExpired"
        return True

    # Here: record in the MS SQL does not exist
    if AllowBackendFallback:
        objectExist = GetExistanceFromBackend(locator)
        return objectExist

    return False
