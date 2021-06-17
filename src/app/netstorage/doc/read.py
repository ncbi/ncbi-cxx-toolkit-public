# Preconditions:
# - MS SQL access required for the service
def Read( locator,
          AllowBackendFallback = True ):

    if DBRecordExists(locator):

        if IsObjectExpired(locator):
            return "ObjectExpired"

        ReadFromBackend()

        UpdateObjectRecord(locator)  # Can prolong the expiration but not reduce
                                     # Note: SetExpTime() could have been called
                                     # while the object is in creation process
        return "OK"

    # Here: object record does not exist

    if AllowBackendFallback:
        ReadFromBackend()   # It may fail with NotFound
        CreateObjectRecord(locator)
        return "OK"

    return "Object not found"
