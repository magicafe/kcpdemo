#include <iostream>
#include <fcntl.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "ikcp.h"
#include "common.h"

typedef struct
{
    int sock_fd;
    ikcpcb *kcp;

    sockaddr_in srv_addr;
    sockaddr_in cli_addr;
} kcp_data;

char buf[BUF_SIZE];

int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    kcp_data *data = (kcp_data *)user;
    sendto(data->sock_fd, buf, len, 0, (sockaddr *)&(data->cli_addr), sizeof(sockaddr_in));
    return 0;
}

void init(kcp_data *data)
{
    data->sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (data->sock_fd < 0)
    {
        exit(EXIT_FAILURE);
    }

    // int opt = 1;
    // if (setsockopt(data->sock_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    // {
    //     close(data->sock_fd);
    //     exit(EXIT_FAILURE);
    // }

    data->srv_addr.sin_family = AF_INET;
    data->srv_addr.sin_addr.s_addr = INADDR_ANY;
    data->srv_addr.sin_port = htons(PORT);
    if (bind(data->sock_fd, (sockaddr *)&(data->srv_addr), sizeof(sockaddr_in)) < 0)
    {
        close(data->sock_fd);
        exit(EXIT_FAILURE);
    }
}

void loop(kcp_data *data)
{
    unsigned int sock_len = sizeof(sockaddr_in);
    int counter = 0;

    while (true)
    {
        usleep(1000);
        ikcp_update(data->kcp, clock());

        while (true)
        {
            memset(buf, 0, BUF_SIZE);
            int nret = recvfrom(data->sock_fd, buf, BUF_SIZE, MSG_DONTWAIT, (sockaddr *)&data->cli_addr, &sock_len);
            if (nret < 0)
            {
                break;
            }
            counter++;
            std::cout << "Received data size: " << nret << " at " << counter << std::endl;

            int nkcp = ikcp_input(data->kcp, buf, nret);
            std::cout << nkcp << std::endl;
            if (nkcp == IKCP_CMD_ACK)
            {
                break;
            }

            uint32_t recv_len = 0;

            while (recv_len < nret)
            {
                int32_t msg_len = ikcp_peeksize(data->kcp);
                std::cout << "Peek size: " << msg_len << std::endl;
                if (msg_len <= 0)
                {
                    break;
                }

                char *recv_buf = new char[msg_len + 1];
                int bytes_len = ikcp_recv(data->kcp, recv_buf, msg_len + 1);
                if (bytes_len > 0)
                {
                    recv_len += bytes_len;
                    recv_buf[msg_len] = '\0';
                    std::cout << "Receive data: " << recv_buf << "\tLength: " << bytes_len << std::endl;
                }
                else
                {
                    break;
                }
            }
        }

        // std::cout << "Receive: " << buf << std::endl;
        // std::string echo(buf);
        // echo += " from server.";
        // ikcp_send(data->kcp, echo.c_str(), echo.length() + 1);
    }
}

void deinit(kcp_data *data)
{
    close(data->sock_fd);
}

int main(int, char **)
{
    kcp_data kcp_data;

    init(&kcp_data);
    std::cout << "UDP Socket Created!" << std::endl;

    ikcpcb *kcp = ikcp_create(KCP_CONV, (void *)&kcp_data);
    kcp->output = udp_output;
    ikcp_nodelay(kcp, 0, 100, 1, 0);
    ikcp_wndsize(kcp, 128, 128);
    kcp_data.kcp = kcp;

    loop(&kcp_data);

    deinit(&kcp_data);

    return 0;
}
