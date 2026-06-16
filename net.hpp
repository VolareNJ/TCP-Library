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
            : domain(domain)
            , type(type)
            , protocol(protocol)
            , fd(clib::socket(domain, type, protocol))
        {
            if(fd == 0)
            {
                clib::perror("socket creation failed\n");
                clib::exit(1);
            }
        }

        ~socket()
        {
            clib::close(fd);
        }

        template<typename optival_t>
        socket& set_opt(int level, int optname, optival_t optval)
        {
            if(clib::setsockopt(fd, level, optname, &optval, sizeof(optval)) == -1)
            {
                clib::perror("setsockopt failed\n");
                clib::exit(1);
            }
            return *this;
        }

    private:
        int domain;
        int type;
        int protocol;
        int fd;
    };

}