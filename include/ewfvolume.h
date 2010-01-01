#include "config.h"
#ifndef EWFVOLUME_H
#define EWFVOLUME_H

#if HAVE_EWF

#include <libewf.h>

CLASS(EWFVolume, AFF4Volume)
   libewf_handle_t *handle;
END_CLASS

CLASS(EWFStream, FileLikeObject)
     RDFURN stored;
END_CLASS

void EWF_init();

#endif
#endif
