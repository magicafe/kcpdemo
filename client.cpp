#include <iostream>
#include <fstream>
#include <sstream>
#include <fcntl.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "ikcp.h"
#include "common.h"

typedef struct
{
    int sock_fd;
    ikcpcb *kcp;

    sockaddr_in srv_addr;
} kcp_data;

char buf[BUF_SIZE];

int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    kcp_data *data = (kcp_data *)user;
    sendto(data->sock_fd, buf, len, 0, (sockaddr *)&(data->srv_addr), sizeof(sockaddr_in));
    return 0;
}

void init(kcp_data *data)
{
    data->sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (data->sock_fd < 0)
    {
        exit(EXIT_FAILURE);
    }
    bzero(&data->srv_addr, sizeof(data->srv_addr));
    data->srv_addr.sin_family = AF_INET;
    data->srv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    data->srv_addr.sin_port = htons(PORT);
}

void loop(kcp_data *data)
{
    int counter = 1;
    unsigned int sock_len = sizeof(sockaddr_in);

    while (true)
    {
        usleep(1000);
        ikcp_update(data->kcp, clock());

        memset(buf, 0, BUF_SIZE);
        int nret = recvfrom(data->sock_fd, buf, BUF_SIZE, MSG_DONTWAIT, (sockaddr *)&data->srv_addr, &sock_len);
        if (nret < 0)
        {
            continue;
        }
        std::cout << "Received data size: " << nret << std::endl;

        int nkcp = ikcp_input(data->kcp, buf, nret);
        if (nkcp < 0)
        {
            continue;
        }

        while (true)
        {
            int len = ikcp_recv(data->kcp, buf, nret);
            if (len < 0)
            {
                break;
            }
        }

        std::cout << "Receive: " << buf << std::endl;
        // std::string echo("");
        // counter++;
        // echo = "Hello from client: " + std::to_string(counter);
        // ikcp_send(data->kcp, echo.c_str(), echo.length() + 1);
    }
}

int main()
{
    kcp_data kcp_data;

    init(&kcp_data);

    ikcpcb *kcp = ikcp_create(KCP_CONV, (void *)&kcp_data);
    kcp->output = udp_output;
    ikcp_nodelay(kcp, 0, 100, 1, 0);
    ikcp_wndsize(kcp, 128, 128);
    kcp_data.kcp = kcp;

    std::ifstream t("../data.txt");
    std::stringstream buffer;
    buffer << t.rdbuf();
    std::string str_data = buffer.str();
    std::cout << "Data size: " << str_data.length() << std::endl;
    ikcp_send(kcp, str_data.c_str(), str_data.length());

    loop(&kcp_data);

    return 0;
}