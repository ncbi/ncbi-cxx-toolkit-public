# Preconditions:
# - MS SQL access required for the service
def GetAttrList( locator ):

    if DBRecordExists(locator):
        if IsObjectExpired(locator):
            return "ObjectExpired"
        return GetObjectAttrNames(locator)

    return "Object not found"
