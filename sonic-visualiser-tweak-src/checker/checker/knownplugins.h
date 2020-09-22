/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/*
    Copyright (c) 2016-2018 Queen Mary, University of London

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    Except as contained in this notice, the names of the Centre for
    Digital Music and Queen Mary, University of London shall not be
    used in advertising or otherwise to promote the sale, use or other
    dealings in this Software without prior written authorization.
*/

#ifndef KNOWN_PLUGINS_H
#define KNOWN_PLUGINS_H

#include <string>
#include <map>
#include <vector>

/**
 * Class to provide information about a hardcoded set of known plugin
 * formats.
 *
 * Requires C++11 and the Qt5 QtCore library.
 */
class KnownPlugins
{
    typedef std::vector<std::string> stringlist;
    
public:
    enum PluginType {
        VampPlugin,
        LADSPAPlugin,
        DSSIPlugin
    };

    enum BinaryFormat {
        FormatNative,
        FormatNonNative32Bit // i.e. a 32-bit plugin but on a 64-bit host
    };
    
    KnownPlugins(BinaryFormat format);

    std::vector<PluginType> getKnownPluginTypes() const;
    
    std::string getTagFor(PluginType type) const {
        return m_known.at(type).tag;
    }

    std::string getPathEnvironmentVariableFor(PluginType type) const {
        return m_known.at(type).variable;
    }
    
    stringlist getDefaultPathFor(PluginType type) const {
        return m_known.at(type).defaultPath;
    }

    stringlist getPathFor(PluginType type) const {
        return m_known.at(type).path;
    }

    std::string getDescriptorFor(PluginType type) const {
        return m_known.at(type).descriptor;
    }
    
private:
    struct TypeRec {
        std::string tag;
        std::string variable;
        stringlist defaultPath;
        stringlist path;
        std::string descriptor;
    };
    typedef std::map<PluginType, TypeRec> Known;
    Known m_known;

    std::string getUnexpandedDefaultPathString(PluginType type);
    std::string getDefaultPathString(PluginType type);
    
    stringlist expandPathString(std::string pathString);
    stringlist expandConventionalPath(PluginType type, std::string variable);

    BinaryFormat m_format;
};

#endif
