#include "napi.h"
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string>
#include <string.h>
#include <netinet/ip_icmp.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <arpa/inet.h>

using namespace std;

struct PingOptions {
    char * addr;
    int timeout;
    int retry;
};

#define PACK_SIZE 32;

class Ping: public Napi::ObjectWrap<Ping> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    Ping(const Napi::CallbackInfo &info);
    ~Ping();

private:
    Napi::Value start(const Napi::CallbackInfo &info);

    // 校验和
    unsigned short CheckSum(unsigned short *header,int length);
    void CreateSocket();

    void SendPacket();
    void RecvPacket();

    void statistic();

    int GeneratePacket();
    int ResolvePakcet(int pack_szie);

    PingOptions options;

    std::string input_domain;       //用来存储通过main函数的参数传入的域名或者ip
    std::string backup_ip;          //通过输入的域名或者ip转化成为的ip备份

    int sock_fd;

    int max_wait_time;              //最大等待时间

    int send_pack_num;              //发送的数据包数量
    int recv_pack_num;              //收到的数据包数量
    int lost_pack_num;              //丢失的数据包数量

    struct sockaddr_in send_addr;   //发送到目标的套接字结构体
    struct sockaddr_in recv_addr;   //接受来自目标的套接字结构体

    char* send_pack;      //用于保存发送的ICMP包
    char* recv_pack;      //用于保存接收的ICMP包

    struct timeval first_send_time; //第一次发送ICMP数据包时的UNIX时间戳
    struct timeval recv_time;       //接收ICMP数据包时的UNIX时间戳

    double min_time;
    double max_time;
    double sum_time;

    Napi::Env env = NULL;
};