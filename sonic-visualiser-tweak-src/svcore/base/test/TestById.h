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

#include "../ById.h"

#include <QObject>
#include <QtTest>

#include <iostream>

using namespace std;

struct WithoutId {};

// We'll need to change access levels for getId() and getUntypedId()
// to test the raw calls

struct A : public WithTypedId<A> { public: using WithTypedId<A>::getId; };
struct B1 : public A {};
struct B2 : public A {};

struct M {};

typedef TypedById<A, A::Id> AById;

struct X : virtual public WithId { public: using WithId::getUntypedId; };
struct Y : public X, public B2, public M {};

class TestById : public QObject
{
    Q_OBJECT

private slots:
    void ids() {
        // Verify that ids are unique across all classes, not just
        // within a class. These must be the first two WithId objects
        // allocated in the first test in the suite, otherwise they
        // could be different even if they were allocated from
        // separate pools.
        A a;
        X x;
        if (a.getId().untyped == x.getUntypedId()) {
            std::cerr << "ERROR: a and x have the same id: " << a.getId()
                      << std::endl;
        }
        QVERIFY(a.getId().untyped != x.getUntypedId());

        A aa;
        QVERIFY(aa.getId().untyped != a.getId().untyped);
        QVERIFY(aa.getId().untyped != x.getUntypedId());

        // Check the actual ids that have been allocated. This is
        // supposed to be a hidden implementation detail, but we want
        // to make sure the test itself hasn't become broken in terms
        // of allocation order (see comment above)
        QCOMPARE(a.getId().untyped, 0);
        QCOMPARE(x.getUntypedId(), 1);
        QCOMPARE(aa.getId().untyped, 2);

        QVERIFY(!a.getId().isNone());
        QVERIFY(A::Id().isNone());
    }

    // NB each test must release all the items it adds to the ById store
    
    void anyEmpty() {
        auto p = AnyById::get(0);
        QVERIFY(!p);
    }

    void anySimple() {
        auto a = std::make_shared<A>();
        int id = AnyById::add(a);
        QCOMPARE(id, a->getId().untyped);

        auto aa = AnyById::getAs<A>(id);
        QVERIFY(!!aa);
        QCOMPARE(aa->getId(), a->getId());
        QCOMPARE(aa.get(), a.get()); // same object, not just same id!
        AnyById::release(id);
    }
    
    void typedEmpty() {
        auto p = AById::get({});
        QVERIFY(!p);
    }

    void typedSimple() {
        auto a = std::make_shared<A>();
        AById::add(a);

        auto aa = AById::get(a->getId());
        QVERIFY(!!aa);
        QCOMPARE(aa->getId(), a->getId());
        QCOMPARE(aa.get(), a.get()); // same object, not just same id!
        AById::release(a);
    }

    void typedReleaseById() {
        auto a = std::make_shared<A>();
        auto aid = AById::add(a);

        auto aa = AById::get(aid);
        QVERIFY(!!aa);
        AById::release(aid);

        aa = AById::get(aid);
        QVERIFY(!aa);
    }

    void typedReleaseByItem() {
        auto a = std::make_shared<A>();
        auto aid = AById::add(a);

        auto aa = AById::get(aid);
        QVERIFY(!!aa);
        AById::release(a);

        aa = AById::get(aid);
        QVERIFY(!aa);
    }

    void typedDowncast() {
        auto a = std::make_shared<A>();
        auto b1 = std::make_shared<B1>();
        AById::add(a);
        AById::add(b1);

        auto bb1 = AById::getAs<B1>(a->getId());
        QVERIFY(!bb1);

        bb1 = AById::getAs<B1>(b1->getId());
        QVERIFY(!!bb1);
        QCOMPARE(bb1->getId(), b1->getId());

        auto bb2 = AById::getAs<B2>(b1->getId());
        QVERIFY(!bb2);

        AById::release(a);
        AById::release(b1);
    }

    void typedCrosscast() {
        auto y = std::make_shared<Y>();
        AById::add(y);

        auto yy = AById::getAs<Y>(y->getId());
        QVERIFY(!!yy);
        QCOMPARE(yy->getId(), y->getId());
        
        yy = AnyById::getAs<Y>(y->getId().untyped);
        QVERIFY(!!yy);
        QCOMPARE(yy->getId(), y->getId());

        auto xx = AById::getAs<X>(y->getId());
        QVERIFY(!!xx);
        QCOMPARE(xx->getUntypedId(), y->getId().untyped);
        QCOMPARE(xx.get(), yy.get());
        
        xx = AnyById::getAs<X>(y->getId().untyped);
        QVERIFY(!!xx);
        QCOMPARE(xx->getUntypedId(), y->getId().untyped);
        QCOMPARE(xx.get(), yy.get());
        
        auto mm = AnyById::getAs<M>(y->getId().untyped);
        QVERIFY(!!mm);
        QCOMPARE(mm.get(), yy.get());

        AById::release(y);
    }

    void duplicateAdd() {
        auto a = std::make_shared<A>();
        AById::add(a);
        try {
            AById::add(a);
            std::cerr << "Failed to catch expected exception in duplicateAdd"
                      << std::endl;
            QVERIFY(false);
        } catch (const std::logic_error &) {
        }
        AById::release(a);
    }

    void unknownRelease() {
        auto a = std::make_shared<A>();
        auto b1 = std::make_shared<B1>();
        AById::add(a);
        try {
            AById::release(b1);
            std::cerr << "Failed to catch expected exception in unknownRelease"
                      << std::endl;
            QVERIFY(false);
        } catch (const std::logic_error &) {
        }
        AById::release(a);
    }
};

