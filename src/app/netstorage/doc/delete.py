# Preconditions:
# - MS SQL access required for the service
def Delete( locator,
            allowBackendFallback = True ):

    if DBRecordExists(locator):

        if IsObjectExpired(locator):
            return "ObjectExpired"

        DeleteFromBackend(locator)  # Warning could be generated
                                    # Backend object not found

        DeleteAttributes(locator)
        DeleteObject(locator)
        return "OK"

    # Here: object record does not exist

    if allowBackendFallback:
        report = DeleteFromBackend(locator)
        if report == "NotFound":
            return Warning( "ObjectNotFound" )
        return "OK"
    else:
        return Warning( "ObjectNotFound" )
