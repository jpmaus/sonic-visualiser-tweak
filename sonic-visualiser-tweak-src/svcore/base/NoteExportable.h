/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_NOTE_EXPORTABLE_H
#define SV_NOTE_EXPORTABLE_H

#include "NoteData.h"

class NoteExportable
{
public:
    /**
     * Get all notes in the exportable object.
     */
    virtual NoteList getNotes() const = 0;

    /**
     * Get notes that are active at the given frame, i.e. that start
     * before or at this frame and have not ended by it.
     */
    virtual NoteList getNotesActiveAt(sv_frame_t frame) const = 0;

    /**
     * Get notes that start within the range in frames defined by the
     * given start frame and duration.
     */
    virtual NoteList getNotesStartingWithin(sv_frame_t startFrame,
                                            sv_frame_t duration) const = 0;
};

#endif
