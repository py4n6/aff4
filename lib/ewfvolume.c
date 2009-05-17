/** This file implements support for ewf volumes */
#include "zip.h"
#include <fcntl.h>
#include <uuid/uuid.h>


VIRTUAL(EWFVolume, ZipFile)
END_VIRTUAL

VIRTUAL(EWFStream, FileLikeObject)
END_VIRTUAL
