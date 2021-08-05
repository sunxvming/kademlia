# kademlia

Kademlia是一种分布式存储及路由的算法，主要用来实现p2p分布式网络。

本仓库中有两个Kademlia协议实现的例子，分别是[kademlia-Academic](https://github.com/davidepi/kademlia)、[kademlia-asio](https://github.com/DavidKeller/kademlia)，这两个例子都是老外写的。

其中[kademlia-Academic](https://github.com/davidepi/kademlia)实现的方式简单些，代码量小，比较容易看懂。这个代码作者可能是个研究生，写这个代码可能是为了完成作业。

[kademlia-asio](https://github.com/DavidKeller/kademlia)实现稍复杂些，网络层用的是boost的asio网络库，代码量稍大些，读起来比较难一点。


在阅读这两个项目的的时候做了关于其代码的笔记，主要包括以下内容：

* 程序的启动和运行
* 主要的实现类
* 程序启动后开启的线程
* 代码中关于kad协议的定义
* kad各个协议实现的逻辑和交互流程


笔记的内容分别存放在这两个代码目录下的README.md中:

* [kademlia-Academic|代码讲解](kademlia-Academic/README.md)
* [kademlia-asio|代码讲解](kademlia-asio/README.md)