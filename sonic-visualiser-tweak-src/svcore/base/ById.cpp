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

#include "ById.h"

#include <unordered_map>
#include <typeinfo>

//#define DEBUG_BY_ID 1

int IdAlloc::getNextId()
{
    static int nextId = 0;
    static QMutex mutex;
    QMutexLocker locker(&mutex);
    int i = nextId;
    if (nextId == INT_MAX) {
        nextId = INT_MIN;
    } else {
        ++nextId;
        if (nextId == 0 || nextId == NO_ID) {
            throw std::runtime_error("Internal ID limit exceeded!");
        }
    }
    return i;
}

// "warning: expression with side effects will be evaluated despite
// being used as an operand to 'typeid'"
#ifdef __clang__
#pragma clang diagnostic ignored "-Wpotentially-evaluated-expression"
#endif

class AnyById::Impl
{
public:    
    ~Impl() {
        QMutexLocker locker(&m_mutex);
        bool empty = true;
        for (const auto &p: m_items) {
            if (p.second && p.second.use_count() > 0) {
                empty = false;
                break;
            }
        }
        if (!empty) {
            SVCERR << "WARNING: ById map is not empty at close; some items have not been released" << endl;
            SVCERR << "         Unreleased items are:" << endl;
            for (const auto &p: m_items) {
                auto ptr = p.second;
                if (ptr && ptr.use_count() > 0) {
                    QString message = QString("id #%1: type %2")
                        .arg(p.first).arg(typeid(*ptr.get()).name());
                    if (auto qobj = std::dynamic_pointer_cast<QObject>(ptr)) {
                        message += QString(", object name \"%1\"")
                            .arg(qobj->objectName());
                    }
                    message += QString(", use count %1").arg(ptr.use_count());
                    SVCERR << "         - " << message << endl; 
                }
            }
        }
    }
        
    int add(std::shared_ptr<WithId> item) {
        int id = item->getUntypedId();
        if (id == IdAlloc::NO_ID) {
            throw std::logic_error("item id should never be NO_ID");
        }
#ifdef DEBUG_BY_ID
        SVCERR << "ById::add(#" << id << ") of type "
               << typeid(*item.get()).name() << endl;
#endif
        QMutexLocker locker(&m_mutex);
        if (m_items.find(id) != m_items.end()) {
            SVCERR << "ById::add: item with id " << id
                   << " is already recorded (existing item type is "
                   << typeid(*m_items.find(id)->second.get()).name()
                   << ", proposed is "
                   << typeid(*item.get()).name() << ")" << endl;
            throw std::logic_error("item id is already recorded in add");
        }
        m_items[id] = item;
        return id;
    }

    void release(int id) {
        if (id == IdAlloc::NO_ID) {
            return;
        }
#ifdef DEBUG_BY_ID
        SVCERR << "ById::release(#" << id << ")" << endl;
#endif
        QMutexLocker locker(&m_mutex);
        if (m_items.find(id) == m_items.end()) {
            SVCERR << "ById::release: unknown item id " << id << endl;
            throw std::logic_error("unknown item id in release");
        }
        m_items.erase(id);
    }
    
    std::shared_ptr<WithId> get(int id) const {
        if (id == IdAlloc::NO_ID) {
            return {}; // this id cannot be added: avoid locking
        }
        QMutexLocker locker(&m_mutex);
        const auto &itr = m_items.find(id);
        if (itr != m_items.end()) {
            return itr->second;
        } else {
            return {};
        }
    }

private:
    mutable QMutex m_mutex;
    std::unordered_map<int, std::shared_ptr<WithId>> m_items;
};

int
AnyById::add(std::shared_ptr<WithId> item)
{
    return impl().add(item);
}

void
AnyById::release(int id)
{
    impl().release(id);
}

std::shared_ptr<WithId>
AnyById::get(int id)
{
    return impl().get(id);
}

AnyById::Impl &
AnyById::impl()
{
    static Impl impl;
    return impl;
}
