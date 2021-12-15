#include "codec.cc"

#include "muduo/net/TcpServer.h"
#include "muduo/net/EventLoop.h"
#include <set>
#include <string>

using namespace std::placeholders;

// 每当定义一个服务器对象时，就得设置连接时的回调函数和有消息发来时的回调函数
class ChatServer {
    
    public:
        // 当调用bind时绑定的是类成员函数，那么第一个参数应该是类对象指针（this等）
        // 先把onStringMessage丢到codec_里面封装一下（编解码器），然后将codec_里的onMessage函数作为回调函数
        ChatServer(muduo::net::EventLoop* loop,
                    const muduo::net::InetAddress& listenAddr)
        : loop_(loop),
          server_(loop, listenAddr, "ChatServer"),
          codec_(std::bind(&ChatServer::onStringMessage, this, _1, _2, _3))
        {
            server_.setConnectionCallback(
                std::bind(&ChatServer::onConnection, this, _1)
            );
            server_.setMessageCallback(
                std::bind(&LengthHeaderCodec::onMessage, codec_, _1, _2, _3)   
            );
        }

        void start()
        {
            server_.start();
        }

    private:

        void onConnection(const muduo::net::TcpConnectionPtr& conn)
        {
            LOG_INFO << conn->localAddress().toIpPort() << " -> "
                     << conn->peerAddress().toIpPort() << " is "
                     << (conn->connected() ? "UP" : "DOWN");

            if (conn->connected()) {
                connections_.insert(conn); // 将该连接加入到set里
            } else {
                connections_.erase(conn); // 从set里去除
            }
        }


        void onStringMessage(const muduo::net::TcpConnectionPtr&,
                             const std::string& message,
                             muduo::Timestamp)
        {
            // 每次一有人发消息来，就把该消息转发给set里的所有tcp连接
            for (ConnectionList::iterator it = connections_.begin();
                 it != connections_.end();
                 ++it)
            {
                codec_.send((*it).get(), message); // 将智能指针转成普通指针
            }
        }

        typedef std::set<muduo::net::TcpConnectionPtr> ConnectionList;
        muduo::net::EventLoop* loop_;
        muduo::net::TcpServer server_;
        LengthHeaderCodec codec_; // 编解码器
        ConnectionList connections_; // 保存当前的所有tcp连接
};


int main(int argc, char* argv[])
{
    LOG_INFO << "pid = " << getpid();
    if (argc > 1) {
        muduo::net::EventLoop loop;
        uint16_t port = static_cast<uint16_t>(atoi(argv[1]));
        muduo::net::InetAddress serverAddr(port);
        ChatServer server(&loop, serverAddr);
        server.start();
        loop.loop();
    } else {
        printf("Usage: %s port\n", argv[0]);
    }
    return 0;
}