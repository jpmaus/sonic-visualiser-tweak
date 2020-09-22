
bqresample
==========

A small C++ library wrapping various audio sample rate conversion
libraries.

Requires the bqvec library.

This code originated as part of the Rubber Band Library written by the
same authors (see https://hg.sr.ht/~breakfastquay/rubberband/).
It has been pulled out into a separate library and relicensed under a
more permissive licence.

C++ standard required: C++98 (does not use C++11 or newer features)

 * To compile: read and follow the notes in Makefile, edit the Makefile,
   then make test. Or else use one of the pre-edited Makefiles in the
   build directory.

[![Build Status](https://travis-ci.org/breakfastquay/bqresample.svg?branch=master)](https://travis-ci.org/breakfastquay/bqresample)

Copyright 2007-2017 Particular Programs Ltd.
Uses Speex code, see speex/COPYING for copyright and licence information.

