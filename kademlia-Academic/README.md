

此项目为Kademlia网络的一个简单实现版本，原项目github地址为[kademlia](https://github.com/davidepi/kademlia)|[README](README_original.md)，作者是一个外国友人，以下的内容是对整个代码实现的梳理。

## 程序启动
参数说明：
```
Flags:
-i [char*] The ip to connect to on the remote host
-p [ uint] lower-case,The port to connect to on the remote host
-P [ uint] upper-case,The port to use on this host
-x [     ] Use a private network
-l [ uint] Set the log level:
	0 - Log nothing
	1 - Log only to stdout <default>
	2 - Log only to file
	3 - Log everywhere
-h [     ] Print this wonderful help :)

# 启动
./kad -P 6000 -x -l3

./kad -p 6000 -P 6001 -i 192.168.1.11 -x
./kad -p 6000 -P 6002 -i 192.168.1.11 -x
```



## 所有线程
udp监听线程，Messenger::init
处理message队列线程
处理SearchNode中状态为PENDING的超时的节点
处理pong回应的pong queue


    
## 程序初始化工作
```
Messenger中启动udp监听线程
创建Performer对象，其在构造函数中创建两个线程
    处理接收到的其他节点发来的message消息
    清除SearchNode中状态为PENDING的超时的节点
向自己所连接的节点发送find_node消息，key为自己节点的key
初始化菜单，并根据菜单选项执行相应动作


初始化k桶
    a:main.cpp 
        向Performer的searchInProgress中添加SearchNode
        Messenger::getInstance()).sendMessage(gatewaynode, msg)
    a:发送 RPC_FIND_NODE， 初始化连接节点时key为自己节点的key
    b:从std::queue<Message*>取Message，并进行处理
    b:处理 RPC_FIND_NODE
        findKClosestNodes()
        返回key(请求者发来的) + 查到的node
    a:查找请求的key的SearchNode
        将返回的KClosestNodes加入到SearchNode中，并进行处理节点状态   
            SearchNode::addAnswer()
        从SearchNode中选取合适的节点继续查找，SearchNode::queryTo()
        queryTo()查找是否有UNKNOWN(未探测过的)的node？
            有：继续RPC_FIND_NODE
            无：RPC_FIND_NODE结束
        
    如何把自己的k桶填满?
        初始化的时候一共可以请求最多6*6个节点，即往k桶中最多添加36个(若返回的node不重复的话)
            6*6是因为向请求节点发送RPC_FIND_NODE最大可以获取6个节点，然后再依次向这6个节点发送RPC_FIND_NODE
        之后有节点访问自己时再更新和添加节点
```

## 主要类说明
```
Spinlock
    自旋锁
Logger
    多线程打印日志到屏幕或文件，此方法要加锁，防止打印到一半另一个线程又打印了
Ip
    It is essentially a wrapper around the integer representation of an IP

Key
    generates by node or value

Distance
    Constructor given two keys
    
Node
    Class used to define a tuple <Ip,port,Key>

Kbucket
    Class used to store a collection of nodes，the amount is KBUCKET_SIZE = 6.
    保存数据结构为：std::list<Node>，头部的节点最新(活跃且稳定)，尾部的最旧
    
    addNode()
        待加入的节点存在
            把位置前移,通过replaceNode(n, n)实现
        待加入的节点不存在
            Kbucket不满
                直接加入list的最前端
            Kbucket满
                ping旧节点
                    能ping通
                        移到前边，因为这个节点存在的时间更长，而且目前是畅通的，所以更加稳定
                    不能ping通  
                        删除旧节点，添加新节点

Updater(单例)
    Class used to set a timeout for non answering nodes，
    当获得新节点时，处理k-bucket中的新旧节点是否替换的逻辑，此时需要ping旧节点，但旧节点可能不在线，此时需要做超时处理
    新旧节点保存在updateNodesMap结构中
    构造时会创建一个线程 scan_queue()定时处理updateNodesMap里的节点
    scan_queue()
        从存放PONG回复消息的queue中pop，并从 updateNodesMap 中查找node，有的话erase掉，说明这个节点现在是存活着呢，不用替换
    checkUpdateBucket() 检查是否更新bucket  Kbucket::addNode添加节点时调用
        rpc_ping(oldNode) 向旧节点发送ping消息
        vars.updateNodesMap.insert(std::pair<Node, Node>(oldNode, newNode));
        起一个线程等待pong，线程执行的方法为removeAfterTimeout
            removeAfterTimeout()
                sleep(TIMEOUT)之后，
                如果ping超时了，旧节点没有回应的，就将updateNodesMap里面的旧节点换成新节点
                如果pong回来的话scan_queue线程会把updateNodesMap中相应的节点erase掉，removeAfterTimeout()方法也就不会进行节点替换了
    processPong()
        当有pong消息回来时，push到存放PONG消息的queue中，scan_queue()的线程会进行处理
    
    
NeighbourManager
    Class used to store a collection of Kbuckets,相当于一个Routing table，一个路由表由多个Kbucket组成 
    数据结构为：数组，自己的节点不存放在k-bucket中,0-bucket中放的是和自己距离为1的key
    类中还存放了Node myself，记录自己的节点
    
    findKClosestNodes()
        先找距离相等的，再找远的，再找近的。为什么这么找？似乎近的离目标节点更近？
        找到K个节点后，按照和key由近到远的距离排序
    findClosestNode()
        从findKClosestNodes()中取第一个，即最近的节点
    insertNode(const Node node)
        算出node和自己的距离，然后插入到相应的Kbucket中

Message
    Class used to represent a Message sent across the network
    reserve 12 bytes for ip、port、flags. flag存放的是协议的名称
    Sender node and sender port will be automatically added by the Messenger class upon sending the Message
    
Messenger
    Class used to send and receive messages
        init() 程序初始化时调用  
            1.创建线程进行udp监听，循环recvfrom  
            2.接收到udp网络数据后处理成Message并push到std::queue<Message*>
            3.通知Performer处理工作线程去处理网络消息 Messenger::getInstance()).cond_var.notify_all()
            注意：将接收网络消息和处理网络消息分开是典型的用法，这样可以提高网络消息的处理能力
        sendMessage(const Node node, const Message& msg) 
            sendto()
    
Performer
    对象创建的时候起了两个线程
        1.execute()   
            等待queue中的Message，有的话处理协议消息
                附加操作：更新k桶节点
                    对消息来源的节点进行插入或更新，即更新k桶：p->neighbours->insertNode(sender)
            若消息需要回应的话，根据消息的类型进行回应
        2.pendingNodesCleaner()   
                定时清除SearchNode中处于PENDING状态的并且已经超时了的node，超时时间为TIMEOUT = 1s
    startSearch()  寻找key
    
    storeTmpMap
    
    
SearchNode
    Class used to ensure persistence between various FIND_NODE calls    
        findkey:要寻找的key  
        std::list<pnode> askme:存着所有请求中的节点
        std::list<pnode> reserve：存着所有备选的节点，reserve中的距离比askme中的更远
        addAnswer()   
            往askme中添加收到的节点
            askme标记状态，探测过的标记为ACTIVE，刚加进来的为UNKNOWN     UNKNOWN = 0, PENDING = 1, ACTIVE = 2
            且askme按距离排序
            askme去重，探测状态设为两者之中更大的
            把askme中数量超过KBUCKET_SIZE的节点放到reserve中
            删除reserve中UNKNOWN的的节点，因为有距离更近的节点了，你这更远就走开吧         
        queryTo()，从SearchNode的askme中选取α个将要查询的节点 
            若没有状态是UNKNOWN
                说明该找的都找了，直接返回
            若有UNKNOWN节点
                从askme中选取ALPHA_REQUESTS个状态为UNKNOWN，把其标记为PENDING
                记录当时的时机，以备节点超时无响应将其删除掉
        clean()
            清除SearchNode中处于PENDING状态的，并且已经超时了的node，超时时间为TIMEOUT = 1s

```
   


## kad所有协议
```
RPC_FIND_NODE
    startSearch()
        1.先从自己的k桶中findKClosestNodes()
        2.将上步查到的Kbucket节点添加到Performer的searchInProgress中
        3.queryTo(),从SearchNode的askme中选取α个将要查询的节点 
        4.向这些节点发送find_node消息
        5.其他的节点发送其他节点的KClosestNodes
        6.本节点收到其他节点发来的节点后添加到SearchNode的askme中
        7.重复3-6步骤
        8.当SearchNode的askme中的节点都探测过之后停止find_node
        9.此时SearchNode的askme中状态为ACTIVE的列表即为寻找到的节点
        
FLAG_FIND_VALUE
    先查看自己本地有没有myselfHasValue(),data存储在filesMap中
    startSearch()
    发送 RPC_FIND_NODE请求，请求的flag添加FLAG_FIND_VALUE标记
    被请求的节点在自己本地找key
    找到了则标记一个 FLAG_VALUE_FOUND 标记并把key和value发送给请求者
    请求者接受到后则停止搜索

      
RPC_STORE
  发起者首先定位k个ID值最接近key的节点，然后分别发起 STORE 操作
    构造请求协议：RPC_FIND_NODE|FLAG_STORE_REQUEST
    startSearch()
        先从自己的k桶中findKClosestNodes
        然后向这些node发送find_node
    find_node完成后得到最近的6个node，然后再从storeTmpMap中找到key和value，向他们发送RPC_STORE
    对方接收到RPC_STORE后直接存在自己的filesMap中：p->filesMap.insert(std::make_pair(key,text))

RPC_PING
    发送RPC_PING协议
    对方收到后然后响应
    
 

k-bucket的维护及更新机制
    每个bucket里的节点都按最后一次接触的时间倒序排列：头最新，尾最旧
    每次执行四个指令中的任意一个都会触发更新
    当一个节点与自己接触时，检查它是否在K-bucket中
        如果在，将它挪到k-bucket列表的头
        如果不在，PING一下列表最尾的一个节点
            如果PING通了，将旧节点挪到列表头(说明它比新来的节点稳定)，并丢弃新节点
            如果PING不通，删除旧节点，并将新节点加入列表最前端


K-桶刷新时机
    1.主动FIND_NODE
    2.有其他节点向自己节点发送消息时时更新此节点
    3.PING定期探测路由表中的每一个节点
    
```


## 边界条件
* k-bucket中的0-bucket存的是距离为1的node，自己的node不存在k-bucket中



## 参考链接

- [Academic implementation of the Kademlia protocol, for the Distributed Algorithms course](https://github.com/davidepi/kademlia)



    














