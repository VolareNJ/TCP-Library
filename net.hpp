#pragma once

#include<vector>
#include<type_traits>
#include<asm-generic/socket.h>
#include<stdio.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<string.h>
#include<stdlib.h>
#include<endian.h>

namespace net
{
    // 编译期判断主机字节序，避免在每个对象中存储运行时标记
    // 使用 POSIX 标准的 endian.h 宏，在编译时确定大小端
#if BYTE_ORDER == LITTLE_ENDIAN
    constexpr bool kIsLittleEndian = true;
#else
    constexpr bool kIsLittleEndian = false;
#endif

    struct recv_ret
    {
        bool success;
        std::vector<char> content;
    };




    class socket
    {
        class package
        {
        public:
            package(socket& sock): super(sock) {}

            // 设置套接字选项的泛型方法，模板参数自动推导
            template<typename optival_t>
            package& set_opt(int level, int optname, optival_t optval)
            {
                if(::setsockopt(super.m_fd, level, optname, &optval, sizeof(optval)) == -1)
                {
                    ::perror("setsockopt failed\n");
                    ::exit(1);
                }
                return *this;
            }

            // 预配置本地地址（不执行绑定），供后续 bind() 使用
            // 端口参数为主机字节序，内部转为网络字节序存储
            package& set_addr(const int sin_family, const ::in_addr_t sin_addr, const int sin_port)
            {
                // 每次调用先清空结构体，确保 sin_zero 等字段确定
                super.m_addr = {};
                super.m_addr.sin_family = sin_family;
                super.m_addr.sin_addr.s_addr = sin_addr;
                // 编译期字节序检测：小端系统需要 htons 转换，大端直接赋值
                super.m_addr.sin_port = kIsLittleEndian ? ::htons(sin_port) : sin_port;
                return *this;
            }

            socket& finish()
            {
                return super;
            }
        private:
            friend class socket; // 允许 socket 类访问私有成员
            socket& super;
        };

        enum class socket_role
        {
            listener,
            client
        };

        class listener_pack: public package
        {
        public:
            listener_pack(socket& sock): package(sock), role(socket_role::listener) {}

            // 无参 bind：直接使用已经设定好的 m_addr 执行系统调用
            package& bind()
            {
                if(::bind(super.m_fd, (::sockaddr*)&super.m_addr, sizeof(super.m_addr)) == -1)
                {
                    ::perror("bind failed\n");
                    ::exit(1);
                }
                return *this;
            }

            // 带参 bind：先设置地址再绑定，避免双重字节序转换
            // （原先有参 bind 自行转换端口，导致与 set_addr 发生双重转换的 bug）
            package& bind(const int sin_family, const ::in_addr_t sin_addr, const int sin_port)
            {
                // 先设置地址，再调用无参 bind()
                this->set_addr(sin_family, sin_addr, sin_port);
                return this->bind();
            }

        private:
            socket_role role = socket_role::listener;
        };

        class client_pack: public package
        {
        public:
            client_pack(socket& sock): package(sock), role(socket_role::client) {}
        private:
            socket_role role = socket_role::client;
        };

    public:
        // 创建监听 socket 的构造函数
        // 初始化 m_param 记录协议族，m_fd 通过系统调用创建，m_addr 置零，
        // m_is_client_sock 初始化为 false（监听角色）
        socket(int domain, int type, int protocol)
            : m_param{ domain, type, protocol }
            , m_fd(::socket(domain, type, protocol))
            , m_addr{}                     // 零初始化，避免未定义行为
            , impl(listener_pack())
        {
            // 注意：socket() 失败返回 -1，而非 0（0 是有效 fd，此前错误地判断 ==0）
            if(m_fd == -1)
            {
                ::perror("socket creation failed\n");
                ::exit(1);
            }
        }

        // 移动构造函数：转移 fd 所有权，并将源对象置为无效状态
        // 确保同一时刻只有一个对象拥有有效的文件描述符
        socket(socket&& other) noexcept
            : m_fd(other.m_fd)
            , m_addr(other.m_addr)
            , m_param(other.m_param)
            , impl(std::move(other.impl))
        {
            other.m_fd = -1;                 // 源对象 fd 无效化
            other.m_addr = {};
            other.m_param = {};
            other.impl = package();
        }

        // 移动赋值运算符：先释放当前资源，再转移所有权
        socket& operator=(socket&& other) noexcept
        {
            if(this != &other)
            {
                // 关闭当前持有的 fd，防止资源泄漏
                if(m_fd != -1)
                    ::close(m_fd);

                m_fd = other.m_fd;
                m_addr = other.m_addr;
                m_param = other.m_param;
                impl = std::move(other.impl);

                // 将源对象重置为“空”状态
                other.m_fd = -1;
                other.m_addr = {};
                other.m_param = {};
                other.impl = package();
            }
            return *this;
        }

        // 禁止拷贝：socket 对象独占文件描述符，拷贝会导致双重关闭
        socket(const socket& other) = delete;
        socket& operator=(const socket& other) = delete;

        // 析构：仅在持有有效 fd 时才关闭
        ~socket()
        {
            if(m_fd != -1)
            {
                ::close(m_fd);
            }
        }



        // 带参 bind：先设置地址再绑定，避免双重字节序转换
        // （原先有参 bind 自行转换端口，导致与 set_addr 发生双重转换的 bug）


        // 开始监听，max_connections 为内核完成队列的最大长度
        socket& listen(int max_connections)
        {
            require_listener();
            if(::listen(m_fd, max_connections) == -1)
            {
                ::perror("listen failed\n");
                ::exit(1);
            }
            // 打印端口时需将网络字节序转回主机字节序，保证可读性
            ::printf("Server is listening on port %d...\n",
                kIsLittleEndian ? ::ntohs(m_addr.sin_port) : m_addr.sin_port);
            return *this;
        }

        // 接受客户端连接，返回一个新的 socket 对象（代表已连接 socket）
        // 使用局部变量接收客户端地址，避免覆盖服务器 m_addr
        socket accept()
        {
            require_listener();
            ::sockaddr_in client_addr{};                     // 值初始化
            ::socklen_t addr_len = sizeof(client_addr);
            int client_fd = ::accept(m_fd, (::sockaddr*)&client_addr, &addr_len);
            if(client_fd == -1)
            {
                ::perror("accept failed\n");
                ::exit(1);
            }
            // 通过私有构造函数创建客户端 socket，标记为 m_is_client_sock = true
            // 客户端 socket 继承监听 socket 的协议参数 m_param
            return socket(client_fd, m_param, client_addr);
        }

        recv_ret receive(::ssize_t buffer_size)
        {
            require_client();
            std::vector<char> buffer(buffer_size);
            ::ssize_t bytes_received = ::recv(m_fd, buffer.data(), buffer.size(), 0);
            if(bytes_received == -1)
            {
                return { false,std::move(std::vector<char>()) };
            }
            buffer.resize(bytes_received);
            return { true,std::move(buffer) };
        }

        socket& send(const std::vector<char>& msg)
        {
            require_client();
            ::send(m_fd, msg.data(), msg.size(), 0);
            return *this;
        }

        template<typename op_t, typename cond_t>
        socket& loop(::ssize_t buffer_size, op_t op, cond_t cond)
            requires(std::invocable<op_t, std::vector<char>>&& std::invocable<cond_t, std::vector<char>>)
        {
            require_listener();
            socket new_socket = this->accept();
            recv_ret msg_received;
            while(1)
            {
                msg_received = new_socket.receive(buffer_size);
                if(msg_received.success)
                {
                    new_socket.send(op(msg_received.content));
                }
                if(cond(msg_received.content))
                {
                    break;
                }
            }
            return *this;
        }

    protected:
        int m_fd;                                     // 文件描述符，-1 表示无效
        struct param_t { int domain; int type; int protocol; } m_param; // 创建参数记录
        ::sockaddr_in m_addr;                     // 本地地址（监听 socket）或对端地址（客户端 socket）

        // 私有构造函数，仅由 accept 调用，用于构造已连接的客户端 socket
        // 直接传入 fd 和已填充的客户端地址，并标记为客户端角色
        explicit socket(int fd, struct param_t param, const ::sockaddr_in addr)
            : m_fd(fd)
            , m_param(param)
            , m_addr(addr)
            , impl(client_pack())
        {}

        package impl;

        // 断言当前对象是监听 socket，否则报错退出
        // 用于 bind、listen、accept 的前置检查
        void require_listener() const
        {
            if(impl.role != socket_role::listener)
            {
                ::perror("operation only valid on listening socket\n");
                ::exit(1);
            }
        }

        // 断言当前对象是客户端 socket，用于 recv/send 的前置检查
        void require_client() const
        {
            if(impl.role != socket_role::client)
            {
                ::perror("operation only valid on connected socket\n");
                ::exit(1);
            }
        }
    };

}