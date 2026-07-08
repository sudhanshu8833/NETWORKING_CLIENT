#ifndef LINUX_SERVER
#define LINUX_SERVER

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string>

namespace http{
    class TcpServer
    {
        public:
            TcpServer(std::string ip_address, int port);
            ~TcpServer();
            void startListening();
        private:
            std::string m_ip_address;
            int m_port;
            int m_socket;
            int m_new_socket;
            long m_incomingMessage;
            struct sockaddr_in m_socketAddress;
            unsigned int m_socketAddress_len;
            std::string m_serverMessage;

            int startServer();
            void closeServer();
            std::string buildResponse(std::string message);
            void acceptConnection(int &new_socket);
            void sendResponse(const std::string &message = "");
            void sendUpgradeRequest(std::string &message);
    };
}

#endif