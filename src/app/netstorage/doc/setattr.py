# Preconditions:
# - MS SQL access required for the service
def SetAttr( locator,
             attrName,
             attrValue,
             createIfNotFound = True ):

    if DBRecordExists(locator):
        if IsObjectExpired(locator):
            return "ObjectExpired"

        UpdateOrCreateObjectAttr(locator, attrName, attrValue)
        return "OK"

    # Here: object record does not exist
    if createIfNotFound:
        CreateObjectRecord(locator) # The expiration time is what is 
                                    # configured for the service
        UpdateOrCreateObjectAttr(locator, attrName, attrValue)
        return "OK"

    return "Object not found"
