#include<asm-generic/socket.h>
#include<stdio.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<string.h>

int main()
{
    //定义各种变量
    int server_fd = 0; //接受别的客户端来连接，接受后，交给new_socket
    int new_socket = 0;
    struct sockaddr_in address; //ip和端口
    int opt = 1;
    int addrlen = sizeof address; //结构体长度
    const int PORT = 8080;//监听的端口号

    //第一步：建立socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0); //成功，返回描述符，否则返回-1，并存errno
    // 参数含义看文档，一定要看
    if (server_fd == 0)
    {
        fprintf(stderr, "Socket creation failed!\n");
        return 1;
    }

    //配置socket，这句话可要可不要，允许重用地址
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);

    address.sin_family = AF_INET; //使用ipv4
    address.sin_addr.s_addr = INADDR_ANY; //允许接收任何网卡上的数据
    address.sin_port = htons(PORT); //设置监听的端口号 //host to net short (htons) 小端转大端

    //第二步：绑定 - 分配ip和端口
    if (bind(server_fd, (struct sockaddr*)&address, sizeof address) < 0)
    {
        fprintf(stderr, "Bind failed!\n");
        return 1;
    }

    //第三步：监听
    //第二个参数是连接队列的最大长度，包括已完成的链接（已经完成三次握手）和未完成（正在建立三次握手）的
    if (listen(server_fd, 3) < 0)
    {
        fprintf(stderr, "Listen failed\n");
        return 1;
    }

    printf("Server is listening on port %d...\n", PORT);

    //第四步：接收链接
    while (1)
    {
        new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        //阻塞模式：如果没有链接来，就会等待，直到有客户端链接进来，随后返回新socket的描述符
        if (new_socket < 0)
        {
            fprintf(stderr, "Accept failed!\n");
            continue;
        }

        //接收信息
        char buffer[1024] = { 0 }; //创建缓冲区
        //第五步：读取信息
        ssize_t bytes_read = recv(new_socket, buffer, sizeof buffer, 0); //接收数据
        if (bytes_read > 0)
        {
            printf("Client: %s\n", buffer); //输出发送的信息
            //响应信息
            const char* message = "Hello from server\n";
            //第六步：发送信息
            send(new_socket, message, strlen(message), 0);
        }

        close(new_socket);
    }

    close(server_fd);

    return 0;
}
