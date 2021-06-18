

程序启动
```
在build/examples目录下
./kademlia_bootstrap 9000
./kademlia_cli 9001 127.0.0.1:9000
./kademlia_cli 9002 127.0.0.1:9000
```





session_impl
    io_service_
    engine_
    

template< typename UnderlyingSocketType >     这里的type为boost::asio::ip::udp::socket    
engine 
    主要的逻辑实现处
    主要成员
        network_
        my_id_
        routing_table_
        value_store_
    主要方法
        process_new_message()  根据相应的消息类型进行具体的逻辑处理
        
        handle_new_message()  这是个回调函数，要传入到network类的on_message_received_中

network
    io_service_
    主要方法
        start_message_reception()
            schedule_receive_on_socket()
                socket.async_receive( on_new_message )
                    on_message_received_()
        send()                


message_socket
    负责socket的监听、发送、接收
    构造时进行监听
    buffer reception_buffer_;

    async_receive()
        sock.async_receive_from()
    async_send()


message
    代表一个网络消息
    从socket的buffer中解析成message的header和body
    
    
ip_endpoint
    boost::asio::ip::address address_;
    uint16_t port_;







id  
    代表160bit的id



routing_table
    k桶的路由表


