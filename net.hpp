namespace net
{
    namespace clib
    {
#include<asm-generic/socket.h>
#include<stdio.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<string.h>
#include<stdlib.h>
    }

    class socket
    {
    public:
        socket(int domain, int type, int protocol)
            : m_param{ domain, type, protocol }
            , m_fd(clib::socket(domain, type, protocol))
        {
            if(m_fd == 0)
            {
                clib::perror("socket creation failed\n");
                clib::exit(1);
            }
        }

        ~socket()
        {
            clib::close(m_fd);
        }

        template<typename optival_t>
        socket& set_opt(int level, int optname, optival_t optval)
        {
            if(clib::setsockopt(m_fd, level, optname, &optval, sizeof(optval)) == -1)
            {
                clib::perror("setsockopt failed\n");
                clib::exit(1);
            }
            return *this;
        }

        socket& set_addr(const int sin_family, const clib::in_addr sin_addr, const int sin_port)
        {
            m_addr.sin_family = sin_family;
            m_addr.sin_addr = sin_addr;
            int i = 1;
            m_addr.sin_port = *(char*)&i == 1 ? clib::htons(sin_port) : sin_port;
            return *this;
        }

        socket& bind()
        {
            bind(m_addr.sin_family, m_addr.sin_addr, m_addr.sin_port);
            return *this;
        }

        socket& bind(const int sin_family, const clib::in_addr sin_addr, const int sin_port)
        {
            m_addr.sin_family = sin_family;
            m_addr.sin_addr = sin_addr;
            int i = 1;
            m_addr.sin_port = *(char*)&i == 1 ? clib::htons(sin_port) : sin_port;
            
            if(clib::bind(m_fd, (clib::sockaddr*)&m_addr, sizeof(m_addr)) == -1)
            {
                clib::perror("bind failed\n");
                clib::exit(1);
            }

            return *this;
        }

        socket& listen(int max_connections)
        {
            if(clib::listen(m_fd, max_connections) == -1)
            {
                clib::perror("listen failed\n");
                clib::exit(1);
            }
            clib::printf("Server is listening on port %d...\n", m_addr.sin_port);
            return *this;
        }

    private:
        int m_fd;
        struct param_t { int domain; int type; int protocol; }m_param;
        clib::sockaddr_in m_addr;
    };

}