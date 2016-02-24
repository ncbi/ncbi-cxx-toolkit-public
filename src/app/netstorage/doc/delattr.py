# Preconditions:
# - MS SQL access required for the service
def DelAttr( locator,
             attrName ):

    if DBRecordExists(locator):
        if IsObjectExpired(locator):
            return "ObjectExpired"

        if DoesObjectAttrExist(locator, attrName):
            DelObjectAttr(locator, attrName)
        else:
            return "Attribute not found"
        return "OK"

    return "Object not found"
