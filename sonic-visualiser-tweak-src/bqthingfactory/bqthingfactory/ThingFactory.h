/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    bqthingfactory

    Copyright 2007-2015 Particular Programs Ltd.

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
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
    ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    Except as contained in this notice, the names of Chris Cannam and
    Particular Programs Ltd shall not be used in advertising or
    otherwise to promote the sale, use or other dealings in this
    Software without prior written authorization.
*/

#ifndef BQ_THING_FACTORY_H
#define BQ_THING_FACTORY_H

#include <string>
#include <set>
#include <map>
#include <vector>
#include <iostream>

//#define DEBUG_THINGFACTORY 1

namespace breakfastquay {

template <typename Thing, typename Parameters>
class AbstractThingBuilder;

class UnknownThingException : virtual public std::exception {
public:
    UnknownThingException(std::string uri) throw() { m_w = ("Unknown thing: " + uri); }
    virtual ~UnknownThingException() throw() { }
    virtual const char *what() const throw() { return m_w.c_str(); }
protected:
    std::string m_w;
};

class UnknownTagException : virtual public std::exception {
public:
    UnknownTagException(std::string tag) throw() { m_w = ("Unknown tag: " + tag); }
    virtual ~UnknownTagException() throw() { }
    virtual const char *what() const throw() { return m_w.c_str(); }
protected:
    std::string m_w;
};


/**
 * A factory for objects from classes that share a common base class,
 * have identical single-argument constructors, can be identified by
 * URI, and that can register their existence with the factory (so
 * that the factory does not have to know about all buildable
 * classes).
 *
 * ** How to use these classes **
 *
 * Given a base class A with many subclasses B, C, D, etc, all of
 * which need to be passed parameters class P in their constructor:
 *
 *  -- in a header associated with A,
 *
 *     - Create a template class ABuilder<T> which inherits from
 *       ConcreteThingBuilder<T, A, P>.  This is your class which will
 *       be specialised to provide a builder for each subclass of A.
 *       Its constructor must accept a std::string containing the URI
 *       that identifies the class of object being built, which is
 *       passed to the parent class's constructor.  Optionally, it may
 *       also accept and pass to the parent class a vector<string> of
 *       "tags", which are strings used to identify the sorts of
 *       facility this builder's object supports -- for example, file
 *       extensions or MIME types that the object can parse.  If two
 *       builders register support for the same tag, only the first to
 *       register will be used (note that which one this is may depend
 *       on static object construction ordering, so it's generally
 *       better if tags are unique to a builder).
 * 
 *     - You may also wish to typedef ThingFactory<A, P> to something
 *       like AFactory, for convenience.
 *
 *  -- in a .cpp file associated with each of B, C, D etc,
 *
 *     - Define a static variable of class ABuilder<B>, ABuilder<C>,
 *       ABuilder<D>, etc, passing the class's identifying URI and
 *       optional supported tag list to its constructor.  (If you
 *       like, this could be a static member of some class.)
 *
 * You can then do the following:
 *
 *  -- call AFactory::getInstance()->getURIs() to retrieve a list of
 *     all registered URIs for this factory.
 * 
 *  -- call AFactory::getInstance()->create(uri, parameters), where
 *     parameters is an object of type P, to construct a new object
 *     whose class is that associated with the URI uri.
 *
 *  -- call AFactory::getInstance()->getTags() to retrieve a list of
 *     all tags known to be supported by some builder.  Remember that
 *     builders do not have to support any tags and many builders
 *     could support the same tag, so you cannot retrieve all builders
 *     by starting from the tags list: use getURIs for that.
 *
 *  -- call AFactory::getInstance()->getURIFor(tag), where tag is a
 *     std::string corresponding to one of the tags supported by some
 *     builder, to obtain the URI of the first builder to have
 *     registered its support for the given tag.
 *  
 *  -- call AFactory::getInstance()->createFor(tag, parameters), where
 *     tag is a std::string corresponding to one of the tags supported by
 *     some builder and parameters is an object of type P, to
 *     construct a new object whose class is that built by the first
 *     builder to have registered its support for the given tag. 
 */
template <typename Thing, typename Parameters>
class ThingFactory
{
protected:
    typedef AbstractThingBuilder<Thing, Parameters> Builder;
    typedef std::map<std::string, Builder *> BuilderMap;
    typedef std::map<std::string, std::string> TagURIMap;

public:
    typedef std::set<std::string> URISet;

    static ThingFactory<Thing, Parameters> *getInstance() {
        static ThingFactory<Thing, Parameters> instance;
	return &instance;
    }
    
    URISet getURIs() const {
        std::vector<std::string> keys = m_registry.keys();
	URISet s;
	for (size_t i = 0; i < keys.size(); ++i) s.insert(keys[i]);
	return s;
    }
    
    std::vector<std::string> getTags() const {
        std::vector<std::string> tags;
        for (TagURIMap::const_iterator i = m_tags.begin(); i != m_tags.end(); ++i) {
            tags.push_back(i->first);
        }
        return tags;
    }
    
    std::string getURIFor(std::string tag) const {
        if (m_tags.find(tag) == m_tags.end()) {
            throw UnknownTagException(tag);
        }
        return m_tags.at(tag);
    }

    Thing *create(std::string uri, Parameters p) const {
	if (m_registry.find(uri) == m_registry.end()) {
	    throw UnknownThingException(uri);
	}
	return m_registry.at(uri)->build(p);
    }

    Thing *createFor(std::string tag, Parameters p) const {
        return create(getURIFor(tag), p);
    }

    void registerBuilder(std::string uri, Builder *builder) {
        if (m_registry.find(uri) != m_registry.end()) {
            std::cerr << "Turbot::ThingFactory::registerBuilder: WARNING: Duplicate URI: "
                      << uri << std::endl;
        }
	m_registry[uri] = builder;
    }
    
    void registerBuilder(std::string uri, Builder *builder, std::vector<std::string> tags) {
#ifdef DEBUG_THINGFACTORY
        std::cerr << "ThingFactory::registerBuilder: uri " << uri << " (" << tags.size() << " tag(s))" << std::endl;
        if (m_registry.find(uri) != m_registry.end()) {
            std::cerr << "Turbot::ThingFactory::registerBuilder: WARNING: Duplicate URI: "
                      << uri << " (with tags: ";
            for (size_t i = 0; i < tags.size(); ++i) {
                std::cerr << tags[i] << " ";
            }
            std::cerr << ")" << std::endl;
        }
#endif
	m_registry[uri] = builder;
        for (size_t i = 0; i < tags.size(); ++i) {
            if (m_tags.find(tags[i]) != m_tags.end()) continue;
#ifdef DEBUG_THINGFACTORY
            std::cerr << "ThingFactory::registerBuilder: tag " << tags[i] 
                      << " -> " << uri << std::endl;
#endif
            m_tags[tags[i]] = uri;
        }
    }
    
protected:
    BuilderMap m_registry;
    TagURIMap m_tags;
};

template <typename Thing, typename Parameters>
class AbstractThingBuilder
{
public:
    virtual ~AbstractThingBuilder() { }
    virtual Thing *build(Parameters) = 0;
};

template <typename ConcreteThing, typename Thing, typename Parameters>
class ConcreteThingBuilder : public AbstractThingBuilder<Thing, Parameters>
{
public:
    ConcreteThingBuilder(std::string uri) {
	ThingFactory<Thing, Parameters>::getInstance()->registerBuilder(uri, this);
    }
    ConcreteThingBuilder(std::string uri, std::vector<std::string> tags) {
	ThingFactory<Thing, Parameters>::getInstance()->registerBuilder(uri, this, tags);
    }
    virtual Thing *build(Parameters p) {
	return new ConcreteThing(p);
    }
};

}

#endif
