# The Doomsday Engine Project
# Copyright (c) 2012 Jaakko Keränen <jaakko.keranen@iki.fi>

include(../config.pri)

TEMPLATE = subdirs

deng_tests: SUBDIRS += \
    archive \
    log \
    record \
    script \
    stringpool \
    vectors
    #basiclink
