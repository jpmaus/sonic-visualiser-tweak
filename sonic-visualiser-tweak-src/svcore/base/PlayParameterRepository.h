/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_PLAY_PARAMETER_REPOSITORY_H
#define SV_PLAY_PARAMETER_REPOSITORY_H

#include "PlayParameters.h"
#include "Command.h"

class Playable;

#include <map>
#include <memory>

#include <QObject>
#include <QString>

class PlayParameterRepository : public QObject
{
    Q_OBJECT

public:
    static PlayParameterRepository *getInstance();

    virtual ~PlayParameterRepository();

    /**
     * Register a playable. The id can be anything you like, so long
     * as it is unique among playables.
     */
    void addPlayable(int id, const Playable *);

    /**
     * Unregister a playable. This must happen before a playable is
     * deleted.
     */
    void removePlayable(int id);

    /**
     * Copy the play parameters from one playable to another.
     */
    void copyParameters(int fromId, int toId);

    /**
     * Retrieve the play parameters for a playable.
     */
    std::shared_ptr<PlayParameters> getPlayParameters(int id);

    void clear();

    class EditCommand : public Command
    {
    public:
        EditCommand(std::shared_ptr<PlayParameters> params);
        void setPlayMuted(bool);
        void setPlayAudible(bool);
        void setPlayPan(float);
        void setPlayGain(float);
        void setPlayClipId(QString);
        void execute() override;
        void unexecute() override;
        QString getName() const override;

    protected:
        std::shared_ptr<PlayParameters> m_params;
        PlayParameters m_from;
        PlayParameters m_to;
    };

signals:
    void playParametersChanged(int playableId);
    void playClipIdChanged(int playableId, QString);

protected slots:
    void playParametersChanged();
    void playClipIdChanged(QString);

protected:
    typedef std::map<int, std::shared_ptr<PlayParameters>> PlayableParameterMap;
    PlayableParameterMap m_playParameters;

    static PlayParameterRepository *m_instance;
};

#endif
