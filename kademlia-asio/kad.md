

## 程序启动
```
在build/examples目录下
./kademlia_bootstrap 9000
./kademlia_cli 9001 127.0.0.1:9000
./kademlia_cli 9002 127.0.0.1:9000
```



## 主要实现类
```
examples/
    booststrap  节点程序，不连接其他节点，只负责监听
    cli          

session_base
    仅在此类中定义了一些类型和常量

first_session:session_base
    对外接口类，实现的方式为pimpl，session_impl为实现类
    用于初次启动节点，启动时不连接其他节点，只负责监听

session
    对外接口类，实现的方式为pimpl，session_impl为实现类
    节点启动会连接其他节点
session_impl
    io_service_   asio核心服务类，它会作为引用参数往下层传，engine、network都会用到
    engine_       
    
    run()
    async_save()
    async_load()
    

template< typename UnderlyingSocketType >     这里的type为boost::asio::ip::udp::socket    
engine 
    kad协议主要的逻辑实现处
    主要成员
        network_
        my_id_
        routing_table_
        value_store_
    主要方法
        process_new_message()  根据相应的消息类型进行具体的逻辑处理
            handle_ping_request()
            handle_store_request()
            handle_find_peer_request()
            handle_find_value_request()
            handle_new_response
        handle_new_message()   这是个回调函数，要传入到network类的on_message_received_中
            反序列化网络消息成detail::header对象，deserialize()
            并交由 process_new_message()处理
        async_save()
        async_load()
        
        
        discover_neighbors() 初始启动时连接initial_peer并发现节点
            start_discover_neighbors_task()
                discover_neighbors_task::start()
                    search_ourselves(discover_neighbors_task)
                        task->tracker_.send_request()
                            network_.send( message, e, on_request_sent );
                                get_socket_for( e ).async_send( message, e, on_message_sent ); 

-------------------------------
# task任务类
discover_neighbors_task
    寻找临近节点类

    search_ourselves()
        主要功能时发送FIND_PEER_REQUEST协议
    handle_initial_contact_response()
        处理节点响应，这个方法的回调注册是放在发送完成时


notify_peer_task
    发送find_peer_request协议


store_value_task

find_value_task

lookup_task




response_router
    handle_new_response()
    register_temporary_callback()
        注册响应的回调，响应超时了移除回调
    成员
        response_callbacks
        
        
response_callbacks
    保存了响应的回调函数
    

                
tracker
    send_request()
        函数模板，第一个参数为Request
    response_router_
        保存着on_response_received的callback
             
                
network
    io_service_
    主要方法
        start_message_reception()
            schedule_receive_on_socket()
                socket.async_receive( on_new_message )
                    回调方法 on_message_received_ =  engine::handle_new_message()     
        send()   
            调用message_socket的async_send发送网络消息


message_socket
    支持ipv4和ipv6
    负责socket的监听、发送、接收
    构造时进行监听
    buffer reception_buffer_;

    async_receive()
        sock.async_receive_from()
    async_send()

    message_socket::ipv4()  静态方法，解析host并生成监听ipv4和ipv6的ip::udp::socket
        message_socket()的构造方法
            create_underlying_socket()
                创建asio的socket对象并绑定指定的端口

message
    代表一个网络消息
    协议的结构定义在这里
    从socket的buffer中解析成message的header和body
    有一系列的serialize和deserialize方法

message_serializer
    序列化message对象成buffer数据流
    它是一个函数模板，根据不同的协议对象产生不同的协议type

endpoint
    代表一个address + service的组合
    
ip_endpoint
    boost::asio::ip::address address_;
    uint16_t port_;

id  
    根据随机数引擎生成
    代表160bit的id



routing_table
    k桶的路由表
    主要保存k_buckets， 
    using value_type = std::pair< id, peer_type >;
    using k_bucket = std::list< value_type >;
    using k_buckets = std::vector< k_bucket >;
    
    
    
timer
    定时器
    
```


## 网络相关操作

### 所有协议
    PING_REQUEST
    PING_RESPONSE
    STORE_REQUEST
    FIND_PEER_REQUEST
    FIND_PEER_RESPONSE
    FIND_VALUE_REQUEST
    FIND_VALUE_RESPONSE


### 网络消息如何监听
network构造时创建message_socket对象，
message_socket构造时创建asio的socket对象
bind地址和端口号


### 网络消息如何接收
network对象构造时调用start_message_reception()
message_socket::async_receive()
ip::udp::socket::async_receive_from()


### 初次启动如何发现节点
    engine::discover_neighbors() 初始启动时连接initial_peer并发现节点
        start_discover_neighbors_task()   
            创建一个discover_neighbors_task对象，并开启任务
            start_discover_neighbors_task最后一个参数是回调函数，对方回复响应后调用回调on_discovery-->notify_neighbors
            discover_neighbors_task::start()
                search_ourselves(discover_neighbors_task)
                    task->tracker_.send_request()   发送FIND_PEER_REQUEST协议




handle_initial_contact_response


### 如何发送



### 缓冲区如何处理




## kad协议

### k_bucket如何更新？

当自己节点接收到消息时调用handle_new_message，handle_new_message中调用routing_table的push方法加入对方节点





















