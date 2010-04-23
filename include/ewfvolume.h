#ifndef _EWFVOLUME_H
#define _EWFVOLUME_H

#if HAVE_EWF

#include <libewf.h>

/** An abstraction to bring Encase volumes into AFF4 universe.

    Read only access to EWF files is provided through this volume -
    this allows any application using AFF4 to also transparently use
    EWF files.

    Since EWF files are limited in the type of metadata they can
    represent we dont get the full flexibility of the ZipFile volume
    for example.

    EWF does not make a distinction between a file (The FileLikeObject
    containing the volume), volume (A container which contains the
    data) and a Stream (the data within the container). Due to this
    limitation it is only possible to have a single stream per
    container.

    When loading the EWF volume, we create a psuedo stream which is
    contained in the volume. We make a URI for the volume using an
    "ewf" method.
**/
CLASS(EWFVolume, AFF4Volume)
   libewf_handle_t handle;
   RDFURN stored;
END_CLASS

   /** A FileLikeObject which provides access to an EWF stream */
CLASS(EWFStream, FileLikeObject)
     RDFURN stored;
END_CLASS

void EWF_init();

#endif
#endif
