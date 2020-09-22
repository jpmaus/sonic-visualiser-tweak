
bqthingfactory
==============

A single-header C++ library providing a factory for objects from
classes that share a common base class, have identical single-argument
constructors, can be identified by URI, and that can register their
existence with the factory, so that the factory does not have to know
in advance about all buildable classes.

C++ standard required: C++98 (does not use C++11)

Copyright 2007-2015 Particular Programs Ltd.  Under a permissive
BSD/MIT-style licence: see the file COPYING for details.


How to use these classes
------------------------

Given a base class A with many subclasses B, C, D, etc, all of
which need to be passed parameters class P in their constructor:

 -- in a header associated with A,

    - Create a template class ABuilder<T> which inherits from
      ConcreteThingBuilder<T, A, P>.  This is your class which will
      be specialised to provide a builder for each subclass of A.
      Its constructor must accept a std::string containing the URI
      that identifies the class of object being built, which is
      passed to the parent class's constructor.  Optionally, it may
      also accept and pass to the parent class a vector<string> of
      "tags", which are strings used to identify the sorts of
      facility this builder's object supports -- for example, file
      extensions or MIME types that the object can parse.  If two
      builders register support for the same tag, only the first to
      register will be used (note that which one this is may depend
      on static object construction ordering, so it's generally
      better if tags are unique to a builder).

    - You may also wish to typedef ThingFactory<A, P> to something
      like AFactory, for convenience.

 -- in a .cpp file associated with each of B, C, D etc,

    - Define a static variable of class ABuilder<B>, ABuilder<C>,
      ABuilder<D>, etc, passing the class's identifying URI and
      optional supported tag list to its constructor.  (If you
      like, this could be a static member of some class.)

You can then do the following:

 -- call AFactory::getInstance()->getURIs() to retrieve a list of
    all registered URIs for this factory.

 -- call AFactory::getInstance()->create(uri, parameters), where
    parameters is an object of type P, to construct a new object
    whose class is that associated with the URI uri.

 -- call AFactory::getInstance()->getTags() to retrieve a list of
    all tags known to be supported by some builder.  Remember that
    builders do not have to support any tags and many builders
    could support the same tag, so you cannot retrieve all builders
    by starting from the tags list: use getURIs for that.

 -- call AFactory::getInstance()->getURIFor(tag), where tag is a
    std::string corresponding to one of the tags supported by some
    builder, to obtain the URI of the first builder to have
    registered its support for the given tag.
 
 -- call AFactory::getInstance()->createFor(tag, parameters), where
    tag is a std::string corresponding to one of the tags supported by
    some builder and parameters is an object of type P, to
    construct a new object whose class is that built by the first
    builder to have registered its support for the given tag. 
