
# Add to RESAMPLE_DEFINES the relevant options for your desired
# third-party library support.
#
# Available options are
#
#  -DHAVE_IPP            Intel's Integrated Performance Primitives are available
#  -DHAVE_LIBRESAMPLE    The libresample library is available
#  -DHAVE_LIBSAMPLERATE  The libsamplerate library is available
#  -DUSE_SPEEX           Compile the built-in Speex-derived resampler
#
# You may define more than one of these. If you define USE_SPEEX, the
# code will be compiled in and will be used when it is judged to be
# the best available option for a given quality setting. If no flags
# are supplied, the code will refuse to compile.

RESAMPLE_DEFINES	:= -DUSE_SPEEX


# Add to VECTOR_DEFINES and ALLOCATOR_DEFINES any options desired for
# the bqvec library (that are not already defined in RESAMPLE_DEFINES).
# See the bqvec build documentation for more details.
#
VECTOR_DEFINES 		:= 
ALLOCATOR_DEFINES 	:= 


# Add any related includes and libraries here
#
THIRD_PARTY_INCLUDES	:=
THIRD_PARTY_LIBS	:=


# If you are including a set of bq libraries into a project, you can
# override variables for all of them (including all of the above) in
# the following file, which all bq* Makefiles will include if found

-include ../Makefile.inc-bq


# This project-local Makefile describes the source files and contains
# no routinely user-modifiable parts

include build/Makefile.inc
