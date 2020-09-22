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

#ifndef SV_TRANSFORM_USER_CONFIGURATOR_H
#define SV_TRANSFORM_USER_CONFIGURATOR_H

#include "transform/ModelTransformerFactory.h"

class TransformUserConfigurator : public ModelTransformerFactory::UserConfigurator
{
public:
    // This is of course absolutely gross

    bool configure(ModelTransformer::Input &input,
                   Transform &transform,
                   Vamp::PluginBase *plugin,
                   ModelId &inputModel,
                   AudioPlaySource *source,
                   sv_frame_t startFrame,
                   sv_frame_t duration,
                   const QMap<QString, ModelId> &modelMap,
                   QStringList candidateModelNames,
                   QString defaultModelName) override;

    static void setParentWidget(QWidget *);
    // JPMAUS  Add option for Configurator to get less user Input .. for running calculations Transforms  in background.
    bool NoUserDialog=false;
    bool includeTempoTransform=false;
private:
    bool getChannelRange(TransformId identifier,
                         Vamp::PluginBase *plugin, int &min, int &max);

};

#endif
