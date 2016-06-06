
# Preconditions:
# - MS SQL access required for the service
def Delete( locator,
            allowBackendFallback = True ):

    # status is one of the following:
    # - expired
    # - not found
    status = DBRemoveObject(locator)
    if status == Expired:
        return "ObjectExpired"

    if status == NotFound:
        if not allowBackendFallback:
            return "NotFound"

    report = DeleteFromBackend(locator)
    if report == "NotFound":
        return "ObjectNotFound"
    return "OK"
