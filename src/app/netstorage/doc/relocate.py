# Preconditions:
# - MS SQL access required for the service
def Relocate( locator,
              createIfNotFound = True ):

    if DBRecordExists(locator):
        if IsObjectExpired(locator):
            return "ObjectExpired"

        RelocateBetweenBackends(locator)   # it may take a long time

        UpdateObjectRecord(locator)  # Can prolong the expiration but not reduce
                                     # Note: SetExpTime() could have been called
                                     # while the object is relocating
        return "OK"

    # Here: object record does not exist

    if createIfNotFound:
        RelocateBetweenBackends(locator)   # it may take a long time
        CreateObjectRecord(locator)
        return "OK"
    
    return "Object not found"
