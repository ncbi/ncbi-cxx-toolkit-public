# Preconditions:
# - MS SQL access required for the service
def GetAttr( locator,
             attrName ):

    if DBRecordExists(locator):
        if IsObjectExpired(locator):
            return "ObjectExpired"
        if ObjectAttrExists(locator, attrName):
            return GetAttrValue(locator, attrName)
        return "Attribute not found"

    return "Object not found"
