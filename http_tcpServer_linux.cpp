#include "http_tcpServer_linux.h"
#include "http_tcpServer_parser.h"
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>

namespace {

    void log(const std::string &message){
        std::cout<< message <<std::endl;
    }

    void exitWithError(const std::string &errorMessage){
        log("ERROR: " + errorMessage);
        exit(1);
    }
}

namespace http
{
    const int BUFFER_SIZE = 30720;

    TcpServer::TcpServer(std::string ip_address, int port)
        : m_ip_address(ip_address), m_port(port), m_socket(),
        m_new_socket(), m_incomingMessage(), m_socketAddress(),
        m_socketAddress_len(sizeof(m_socketAddress)),
        m_serverMessage()
    {
        m_socketAddress.sin_family = AF_INET;
        m_socketAddress.sin_port = htons(m_port);
        m_socketAddress.sin_addr.s_addr = inet_addr(m_ip_address.c_str());
        if (startServer() != 0){
            std::ostringstream ss;
            ss << "Failed to start server with PORT: " << ntohs(m_socketAddress.sin_port);
            log(ss.str());
        }
    }

    TcpServer::~TcpServer(){
        closeServer();
    }



    int TcpServer::startServer(){
        m_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (m_socket < 0){
            exitWithError("Cannot create socket");
            return 1;
        }

        int opt = 1;
        setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        if (bind(m_socket, (sockaddr *)&m_socketAddress, m_socketAddress_len) < 0){
            exitWithError("Cannot connect socket to address");
            return 1;
        }
        return 0;
    }

    std::string TcpServer::buildResponse(std::string message){
        if (message == "") message = "start from server";
        std::string htmlFile = "<!DOCTYPE html><html><body><h1>" + message + " </h1></body></html>";
        std::ostringstream ss;
        ss << "HTTP/1.1 200 ok\nContent-Type: text/html\nContent-Length: "
            << htmlFile.size() << "\n\n"
            << htmlFile;
        return ss.str();
    }

    std::string decodeFrame(const char *buffer, int length){
        unsigned char byte0 = buffer[0];
        unsigned char byte1 = buffer[1];

        int opcode = byte0 & 0x0F;

        bool masked = byte1 & 0x80;

        uint64_t payloadLen = byte1 & 0x7F;

        size_t pos = 2;

        if (payloadLen == 126){
            payloadLen = (static_cast<unsigned char>(buffer[2]) << 8) | static_cast<unsigned char>(buffer[3]);
            pos = 4;
        } else if (payloadLen == 127){
            payloadLen = 0;
            for(int i = 0; i < 8; i++){
                payloadLen = (payloadLen << 8) | static_cast<unsigned char>(buffer[2 + i]);
            }
            pos = 10;
        }

        unsigned char maskKey[4] = {0 , 0, 0 ,0};

        if (masked){
            for(int i =0 ;i < 4; i++){
                maskKey[i] = buffer[pos + i];
            }
            pos += 4;
        }

        std::string payload(payloadLen, '\0');
        for(uint64_t i =0; i< payloadLen ; ++i){
            char c = buffer[pos + i];
            if (masked){
                c ^= maskKey[i%4];
            }
            payload[i] = c;
        }
        return payload;
    }

    std::string encodeFrame(const std::string &message){
        std::string frame;

        frame.push_back(static_cast<char>(0x81));

        size_t len = message.size();

        if (len <= 125){
            frame.push_back(static_cast<char>(len));
        } else if (len <= 65535) {
            frame.push_back(static_cast<char>(126));
            frame.push_back(static_cast<char>((len >> 8) & 0xFF));
            frame.push_back(static_cast<char>(len & 0xFF));
        } else {
            frame.push_back(static_cast<char>(127));
            for(int i = 7; i>=0 ; --i){
                frame.push_back(static_cast<char>((len >> (i*8)) & 0xFF));
            }
        }
        frame += message;
        return frame;
    }

    void TcpServer::startListening(){
        if (listen(m_socket, 20) < 0){
            exitWithError("Socket listen failed");
        }

        std::ostringstream ss;
        ss<< "\n*** Listening on ADDRESS: " << inet_ntoa(m_socketAddress.sin_addr) << "PORT :" << ntohs(m_socketAddress.sin_port) << " ***\n\n";
        log(ss.str());

        int bytesReceived;

        while(true){
            log("====== Waiting for a new connection =====\n\n\n");
            acceptConnection(m_new_socket);

            char buffer[BUFFER_SIZE] = {0};

            bytesReceived = read(m_new_socket, buffer, BUFFER_SIZE);

            if (bytesReceived < 0){
                exitWithError("Failed to read bytes from client socket connnection");
            }

            std::ostringstream ss;

            ss<< "----- Recieved request from client ---- \n\n";
            log(ss.str());


            std::string message(buffer, bytesReceived);
            bool isWebsocket = httpParser::checkUpgradeRequest(message);


            if (isWebsocket){
                log(message);
                sendUpgradeRequest(message);
                char frame_buffer[BUFFER_SIZE] = {0};
                while(true){
                    int bytes_recieved = read(m_new_socket, frame_buffer, BUFFER_SIZE);
                    if (!bytes_recieved) break;
                    if (bytes_recieved < 0) exitWithError("Connection closed with some error");
                    std::string value = decodeFrame(frame_buffer, bytes_recieved);
                    log(value);
                    std::string response = encodeFrame(value);
                    write(m_new_socket, response.c_str(), response.size());
                }
                log("Connection closed");
                close(m_new_socket);
            }else {
                sendResponse();
                close(m_new_socket);
            }
        }
    }

    std::string extractWebSocketKey(const std::string &message){
        size_t pos = message.find("Sec-WebSocket-Key: ");
        if (pos == std::string::npos) return "";

        pos += 19;
        size_t end = message.find("\r\n", pos);
        return message.substr(pos, end - pos);
    }

    std::string calculateWebsocketAccept(std::string websocketKey){
        std::string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        std::string combined = websocketKey + magic;

        unsigned char digest[SHA_DIGEST_LENGTH];
        SHA1(reinterpret_cast<const unsigned char*>(combined.c_str()), combined.size(), digest);

        BIO *b64 = BIO_new(BIO_f_base64());
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
        BIO *mem = BIO_new(BIO_s_mem());
        BIO_push(b64, mem);
        BIO_write(b64, digest, SHA_DIGEST_LENGTH);
        BIO_flush(b64);

        BUF_MEM *bufferPtr;
        BIO_get_mem_ptr(b64, &bufferPtr);
        std::string result(bufferPtr->data, bufferPtr->length);
        BIO_free_all(b64);

        return result;
    }

    void TcpServer::sendUpgradeRequest(std::string &message){
        std::string websocketKey = extractWebSocketKey(message);

        if(websocketKey.empty()){
            exitWithError("No Sec-WebSocket-Key");
        }

        std::string acceptKey = calculateWebsocketAccept(websocketKey);

        std::ostringstream ss;
        ss  << "HTTP/1.1 101 Switching Protocols\r\n"
            << "Upgrade: websocket\r\n"
            << "Connection: Upgrade\r\n"
            << "Sec-WebSocket-Accept: " << acceptKey<< "\r\n"
            << "\r\n";

        std::string response = ss.str();
        long bytesSent = write(m_new_socket, response.c_str(), response.size());
        if (bytesSent < 0){
            exitWithError("Failed to send Websocket upgrade message");
        }

    }

    void TcpServer::acceptConnection(int &new_socket){
        new_socket = accept(m_socket, (sockaddr *)&m_socketAddress, &m_socketAddress_len);
        if(new_socket < 0){
            std::ostringstream ss;
            ss<< "Server failed to accpet incoming connection from ADDRESS: " << inet_ntoa(m_socketAddress.sin_addr) << "; PORT: " << ntohs(m_socketAddress.sin_port);
            exitWithError(ss.str());
        }
    }

    void TcpServer::sendResponse(const std::string &message){
        long bytesSent;
        std::string response = buildResponse(message);

        if (message == ""){
            bytesSent = write(m_new_socket, response.c_str(), response.size());
        } else {
            bytesSent = write(m_new_socket, response.c_str(), response.size());
        }

        if(bytesSent == response.size()){
            log("------------- Server Response Sent to client ------- \n\n");
        }
        else{
            log("Error while sending the response");
        }
    }

    void TcpServer::closeServer(){
        close(m_socket);
        close(m_new_socket);
        exit(0);
    }
}