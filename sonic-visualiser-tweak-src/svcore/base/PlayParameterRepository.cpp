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

#include "PlayParameterRepository.h"
#include "PlayParameters.h"
#include "Playable.h"

#include <iostream>

PlayParameterRepository *
PlayParameterRepository::m_instance = new PlayParameterRepository;

PlayParameterRepository *
PlayParameterRepository::getInstance()
{
    return m_instance;
}

PlayParameterRepository::~PlayParameterRepository()
{
}

void
PlayParameterRepository::addPlayable(int playableId, const Playable *playable)
{
    if (!getPlayParameters(playableId)) {
        
        // Give all playables the same type of play parameters for the
        // moment

        auto params = std::make_shared<PlayParameters>();
        m_playParameters[playableId] = params;

        params->setPlayClipId
            (playable->getDefaultPlayClipId());

        params->setPlayAudible
            (playable->getDefaultPlayAudible());
        
        connect(params.get(), SIGNAL(playParametersChanged()),
                this, SLOT(playParametersChanged()));
        
        connect(params.get(), SIGNAL(playClipIdChanged(QString)),
                this, SLOT(playClipIdChanged(QString)));
    }
}    

void
PlayParameterRepository::removePlayable(int playableId)
{
    if (m_playParameters.find(playableId) == m_playParameters.end()) {
        return;
    }
    m_playParameters.erase(playableId);
}

void
PlayParameterRepository::copyParameters(int from, int to)
{
    if (!getPlayParameters(from)) {
        cerr << "ERROR: PlayParameterRepository::copyParameters: source playable unknown" << endl;
        return;
    }
    if (!getPlayParameters(to)) {
        cerr << "ERROR: PlayParameterRepository::copyParameters: target playable unknown" << endl;
        return;
    }
    getPlayParameters(to)->copyFrom(getPlayParameters(from).get());
}

std::shared_ptr<PlayParameters>
PlayParameterRepository::getPlayParameters(int playableId) 
{
    if (m_playParameters.find(playableId) == m_playParameters.end()) {
        return nullptr;
    }
    return m_playParameters.find(playableId)->second;
}

void
PlayParameterRepository::playParametersChanged()
{
    PlayParameters *params = dynamic_cast<PlayParameters *>(sender());
    for (auto i: m_playParameters) {
        if (i.second.get() == params) {
            emit playParametersChanged(i.first);
            return;
        }
    }
}

void
PlayParameterRepository::playClipIdChanged(QString id)
{
    PlayParameters *params = dynamic_cast<PlayParameters *>(sender());
    for (auto i: m_playParameters) {
        if (i.second.get() == params) {
            emit playClipIdChanged(i.first, id);
            return;
        }
    }
}

void
PlayParameterRepository::clear()
{
    m_playParameters.clear();
}

PlayParameterRepository::EditCommand::EditCommand(std::shared_ptr<PlayParameters> params) :
    m_params(params)
{
    m_from.copyFrom(m_params.get());
    m_to.copyFrom(m_params.get());
}

void
PlayParameterRepository::EditCommand::setPlayMuted(bool muted)
{
    m_to.setPlayMuted(muted);
}

void
PlayParameterRepository::EditCommand::setPlayAudible(bool audible)
{
    m_to.setPlayAudible(audible);
}

void
PlayParameterRepository::EditCommand::setPlayPan(float pan)
{
    m_to.setPlayPan(pan);
}

void
PlayParameterRepository::EditCommand::setPlayGain(float gain)
{
    m_to.setPlayGain(gain);
}

void
PlayParameterRepository::EditCommand::setPlayClipId(QString id)
{
    m_to.setPlayClipId(id);
}

void
PlayParameterRepository::EditCommand::execute()
{
    m_params->copyFrom(&m_to);
}

void
PlayParameterRepository::EditCommand::unexecute()
{
    m_params->copyFrom(&m_from);
}
    
QString
PlayParameterRepository::EditCommand::getName() const
{
    QString name;
    QString multiname = tr("Adjust Playback Parameters");

    int changed = 0;

    if (m_to.isPlayAudible() != m_from.isPlayAudible()) {
        name = tr("Change Playback Mute State");
        if (++changed > 1) return multiname;
    }

    if (m_to.getPlayGain() != m_from.getPlayGain()) {
        name = tr("Change Playback Gain");
        if (++changed > 1) return multiname;
    }

    if (m_to.getPlayPan() != m_from.getPlayPan()) {
        name = tr("Change Playback Pan");
        if (++changed > 1) return multiname;
    }

    if (m_to.getPlayClipId() != m_from.getPlayClipId()) {
        name = tr("Change Playback Sample");
        if (++changed > 1) return multiname;
    }

    if (name == "") return multiname;
    return name;
}

