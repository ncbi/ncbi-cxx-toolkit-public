# Preconditions:
# - MS SQL access required for the service
def Create(locator):
    """ Note: the DB record is created when the last data chunk is received """

    # Here: it may take long time since the command is received till
    #       the last chunk is processed. This point is when the last
    #       chunk is here.

    if DBRecordExists(locator):
        UpdateObjectRecord(locator)  # Can prolong the expiration but not reduce
                                     # Note: SetExpTime() could have been called
                                     # while the object is in creation process
        return "OK"

    # Here: object record does not exist
    CreateObjectRecord(locator)
    return "OK"
