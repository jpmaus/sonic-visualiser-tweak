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

#ifndef SV_BY_ID_H
#define SV_BY_ID_H

#include "Debug.h"

#include <memory>
#include <iostream>
#include <climits>
#include <stdexcept>

#include <QMutex>
#include <QString>

#include "XmlExportable.h"

/*
 * ById - central pool of objects to be retrieved by persistent id.
 *
 * This is a pretty simple mechanism for obtaining safe "borrowed"
 * references to shared objects, including across threads, based on an
 * object ID.
 *
 * A class (call it C) inherits WithTypedId<C>. This produces a type
 * C::Id containing a numerical id. Each instance of C (or subclass
 * thereof) has an internal id of type C::Id whose value is unique
 * among all ids ever possessed by any instances of all classes that
 * use this id mechanism (within a single run of the program).
 *
 * Then we have a static store of type TypedById<C, C::Id>. This holds
 * a set of heap-allocated C objects (or subclass thereof) and hands
 * out shared_ptr references to them when queried by id. The
 * application calls add() to pass an object to the store (which takes
 * ownership of it), and the application calls release() when it
 * thinks it has finished with an object, to request the store to
 * delete it.
 *
 * Note that an object's id can't (without shenanigans) be queried
 * directly from that object - it is returned when the object is added
 * to a ById store. So if you have an object id, you know that the
 * object must have been added to a store at some point.
 *
 * The goal is to improve code that would previously have retained a
 * bare pointer to a heap-allocated object that it did not own. For
 * example, in Sonic Visualiser we have a Model hierarchy of complex
 * mutable objects, and any given model may be referred to by many
 * different layers, transforms (as both source and target) etc. Using
 * bare pointers for those references means that we need everything to
 * be notified (and act properly on the notification) if a model is
 * about to be deleted. Using a Model::Id instead gives the code a
 * guarantee: if the model has been deleted since you last looked at
 * it, then the ById store will return a null shared_ptr from its
 * get() function for that id; but if it returns a non-null
 * shared_ptr, then the object being pointed to can't be deleted while
 * that shared_ptr is in scope.
 *
 * Example:
 *
 * class Thing : public WithTypedId<Thing> { Thing(int x) { } };
 * typedef TypedById<Thing, Thing::Id> ThingById;
 *
 * // application creates a new Thing
 * // ...
 * auto thing = std::make_shared<Thing>(10);
 * auto thingId = ThingById::add(thing);
 *
 * // application then passes thingId to something else, without
 * // storing the shared_ptr anywhere - the ById store manages that
 * 
 * // code elsewhere now has the thingId, and needs to use the Thing
 * // ...
 * void doSomething() {
 *     auto thing = ThingById::get(m_thingId);
 *     if (!thing) { // the Thing has been deleted, stop acting on it
 *         return;   // (this may be an error or it may be unexceptional)
 *     }
 *     // now we have a guarantee that the thing ptr will be valid
 *     // until it goes out of scope when doSomething returns
 * }
 *
 * // application wants to be rid of the Thing
 * ThingById::release(thingId);
 */

//!!! to do: review how often we are calling getAs<...> when we could
// just be using get

struct IdAlloc {

    // The value NO_ID (-1) is never allocated
    static const int NO_ID = -1;
    
    static int getNextId();
};

template <typename T>
struct TypedId {
    
    int untyped;
    
    TypedId() : untyped(IdAlloc::NO_ID) {}

    TypedId(const TypedId &) =default;
    TypedId &operator=(const TypedId &) =default;

    bool operator==(const TypedId &other) const {
        return untyped == other.untyped;
    }
    bool operator!=(const TypedId &other) const {
        return untyped != other.untyped;
    }
    bool operator<(const TypedId &other) const {
        return untyped < other.untyped;
    }
    bool isNone() const {
        return untyped == IdAlloc::NO_ID;
    }
};

template <typename T>
std::ostream &
operator<<(std::ostream &ostr, const TypedId<T> &id)
{
    // For diagnostic purposes only. Do not use these IDs for
    // serialisation - see XmlExportable instead.
    if (id.isNone()) {
        return (ostr << "<none>");
    } else {
        return (ostr << "#" << id.untyped);
    }
}

class WithId
{
public:
    WithId() :
        m_id(IdAlloc::getNextId()) {
    }
    virtual ~WithId() {
    }

protected:
    friend class AnyById;
    
    /**
     * Return an id for this object. The id is a unique number for
     * this object among all objects that implement WithId within this
     * single run of the application.
     */
    int getUntypedId() const {
        return m_id;
    }

private:
    int m_id;
};

template <typename T>
class WithTypedId : virtual public WithId
{
public:
    typedef TypedId<T> Id;
    
    WithTypedId() : WithId() { }

protected:
    template <typename Item, typename Id>
    friend class TypedById;
    
    /**
     * Return an id for this object. The id is a unique value for this
     * object among all objects that implement WithTypedId within this
     * single run of the application.
     */
    Id getId() const {
        Id id;
        id.untyped = getUntypedId();
        return id;
    }
};

class AnyById
{
public:
    static int add(std::shared_ptr<WithId>);
    static void release(int);
    static std::shared_ptr<WithId> get(int); 

    template <typename Derived>
    static bool isa(int id) {
        std::shared_ptr<WithId> p = get(id);
        return bool(std::dynamic_pointer_cast<Derived>(p));
    }
   
    template <typename Derived>
    static std::shared_ptr<Derived> getAs(int id) {
        std::shared_ptr<WithId> p = get(id);
        return std::dynamic_pointer_cast<Derived>(p);
    }

private:
    class Impl;
    static Impl &impl();
};

template <typename Item, typename Id>
class TypedById
{
public:
    static Id add(std::shared_ptr<Item> item) {
        Id id;
        id.untyped = AnyById::add(item);
        return id;
    }

    static void release(Id id) {
        AnyById::release(id.untyped);
    }
    static void release(std::shared_ptr<Item> item) {
        release(item->getId());
    }

    template <typename Derived>
    static bool isa(Id id) {
        return AnyById::isa<Derived>(id.untyped);
    }

    template <typename Derived>
    static std::shared_ptr<Derived> getAs(Id id) {
        return AnyById::getAs<Derived>(id.untyped);
    }

    static std::shared_ptr<Item> get(Id id) {
        return getAs<Item>(id);
    }
    
    /**
     * If the Item type is an XmlExportable, return the export ID of
     * the given item ID. A call to this function will fail to compile
     * if the Item is not an XmlExportable.
     *
     * The export ID is a simple int, and is only allocated when first
     * requested, so objects that are never exported don't get one.
     */
    static int getExportId(Id id) {
        auto exportable = getAs<XmlExportable>(id);
        if (exportable) {
            return exportable->getExportId();
        } else {
            return XmlExportable::NO_ID;
        }
    }
};

#endif

