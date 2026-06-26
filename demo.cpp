#include<iostream>
#include"net.hpp"

int main()
{
    net::socket listener(AF_INET, SOCK_STREAM, 0);
    listener.set_opt(SOL_SOCKET, SO_REUSEADDR, 1)
        .set_addr(AF_INET, INADDR_ANY, 8080)
        .bind()
        .listen(10)
        .loop(1024,
            [](std::vector<char> msg)
            {
                std::cout << msg.data() << std::endl;
                return msg;
            },
            [](std::vector<char> msg)
            {
                return msg.data() == "exit\n" ? true : false;
            });
    return 0;
}