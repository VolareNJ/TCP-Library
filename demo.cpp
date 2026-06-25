#include<iostream>
#include"net.hpp"

int main()
{
    net::socket listener(AF_INET, SOCK_STREAM, 0);
    return 0;
}