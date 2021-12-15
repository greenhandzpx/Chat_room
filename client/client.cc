#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThread.h"
#include "muduo/net/TcpClient.h"
#include "muduo/base/Mutex.h"

#include "codec.cc"

#include <functional>
#include <string>
#include <iostream>
#include <stdio.h>

using namespace std::placeholders;

// 客户端分为两个线程，main线程负责读取标准输入，eventloop线程负责作为reactive
class ChatClient {
    public:
        ChatClient(muduo::net::EventLoop* loop, 
                   const muduo::net::InetAddress& serverAddr,
                   std::string name)
            : loop_(loop),
              client_(loop, serverAddr, "ChatClient"),
              codec_(std::bind(&ChatClient::onStringMessage, this, _1, _2, _3)),
              name_(name)
        {
            client_.setConnectionCallback(
                std::bind(&ChatClient::onConnection, this, _1)
            );
            client_.setMessageCallback(
                std::bind(&LengthHeaderCodec::onMessage, &codec_, _1, _2, _3)
            );
            client_.enableRetry();
        }

        void connect()
        {
            client_.connect();
        }

        // 目前为空，客户端的连接由操作系统在进程终止时关闭
        void disconnect()
        {
            //client_.disconnect();
        } 

        // write由main函数调用，多线程需要加锁，为了保护shared_ptr
        void write(const muduo::StringPiece& message)
        {
            muduo::MutexLockGuard lock(mutex_);
            //printf("%s\n", message.data());
            if (connection_) {

                // std::string msg(message.data());
                // muduo::StringPiece name_and_message(std::string(name_ + ">> " + msg));
                //printf("%s\n", name_and_message.data());
                //codec_.send(connection_.get(), name_and_message); 

                codec_.send(connection_.get(), message);  // 智能指针转为裸指针
            }
        }
    


    private:
        // onConnection由Eventloop线程调用，需要加锁保护shared_ptr
        void onConnection(const muduo::net::TcpConnectionPtr& conn)
        {
            LOG_INFO << conn->localAddress().toIpPort() << " -> "
                     << conn->peerAddress().toIpPort() << " is "
                     << (conn->connected() ? "UP" : "DOWN");
            
            muduo::MutexLockGuard lock(mutex_);
            if (conn->connected()) {
                connection_ = conn;
            } else {
                connection_.reset();
            }
        }

        // 把收到的消息打印到屏幕上，不用加锁，因为printf是线程安全的
        void onStringMessage(const muduo::net::TcpConnectionPtr&,
                             const std::string& message,
                             muduo::Timestamp)
        {
            //printf(">>> %s\n", message.c_str());
            printf("%s\n", message.c_str());
        }    
    
        muduo::net::EventLoop* loop_;
        muduo::net::TcpClient client_;
        LengthHeaderCodec codec_;
        muduo::MutexLock mutex_;
        muduo::net::TcpConnectionPtr connection_;    

        std::string name_; // 客户的名字      
};

int main(int argc, char* argv[])
{
    LOG_INFO << "pid = " << getpid();
    if (argc > 2) {
        std::string name;
        std::cout << "Please input your name: ";
        std::cin >> name;

        muduo::net::EventLoopThread loopThread;
        uint16_t port = static_cast<uint16_t>(atoi(argv[2]));
        muduo::net::InetAddress serverAddr(argv[1], port);

         // startloop会创建一个线程
        ChatClient client(loopThread.startLoop(), serverAddr, name);
        client.connect();

        std::string line;
        // 从标准输入读取用户输入
        while (std::getline(std::cin, line)) {
            std::string name_and_msg = name + ">> " + line;
            client.write(name_and_msg);
            //std::cout << line << std::endl;
        }
        client.disconnect();
    } else {
        printf("Usage: %s host_ip port\n", argv[0]);
    }
    return 0;
}