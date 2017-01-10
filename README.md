CocainePlugins
==============

Default [Cocaine](https://github.com/cocaine/cocaine-core) configuration provides only basic components required to work
with it. It includes:

 - Locator service.
 - Storage (local) service.
 - Logging service.
 - And ... wait, that's all!

Using listed components you can configure your cloud for local testing, but for production usage it's not sufficient.

By production we mean that you'd probably want to execute custom code or to connect all your cloud nodes using
distributed storage. Or even configure strong-consistency configuration service.

To fill this gap Cocaine provides a powerful support of external extensions using **plugins**. It includes both
implementation of predefined interfaces (like gateway or storage) and custom services with any number of methods.

After implementing any cloud user can start using it without specification or protocol definition.

See [related](pages.html) pages for more information.

## What is the Cocaine Plugin?

Any plugin is a C++ module that implements either predefined interface named *category* or defines the custom one using
*service* interface.

To make a *category* plugin you just need to implement the appropriate methods for given category.

To make a *service* you need to declare an interface definition using some `boost::mpl` magic.

## Current Implementations

 - Cache
 - Chrono
 - Docker
 - Graphite
 - IPVS
 - Mongo
 - Node
 - Unicorn
 - Urlfetch
