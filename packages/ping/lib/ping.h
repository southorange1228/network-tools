#include "napi.h"
#include <iostream>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>

using namespace std;

#define PACK_SIZE 32
#define IP_HEADER_SIZE 20
#define ICMP_ECHO 0
#define ICMP_ECHO_REPLY 8
#define ICMP_REPLY_CODE 0

struct PingOptions {
    char *addr;
    int timeout;
    int retry;
};

struct IP_HEADER {
    unsigned char header_length :4; // header of length
    unsigned char version :4;  // version
    unsigned char tos;  // type of service
    unsigned short total_length; // total length of packet
    unsigned short identifier; // id
    unsigned short frag_and_flags; // fragment and flag
    unsigned char ttl; // ttl
    unsigned char protocol; // protocol eg: TCP UDP etc.
    unsigned short checksum; // checksum
    unsigned long source_ip; // source ip
    unsigned long dest_ip;  // destination ip
} ;


struct ICMP_HEADER {
    unsigned char icmp_type;
    unsigned char icmp_code;
    unsigned short icmp_checksum;
    unsigned short icmp_id;
    unsigned short icmp_seq;
    unsigned long timestamp_s;
    unsigned long timestamp_us;
};



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

    int GeneratePacket();
    int ResolvePacket(int pack_size);
    unsigned short GetPid();

    PingOptions options;

    std::string input_domain;       //用来存储通过main函数的参数传入的域名或者ip
    std::string backup_ip;          //通过输入的域名或者ip转化成为的ip备份

    int sock_fd;

    int send_pack_num;              //发送的数据包数量
    int recv_pack_num;              //收到的数据包数量
    int lost_pack_num;              //丢失的数据包数量

    struct sockaddr_in send_addr;   //发送到目标的套接字结构体
    struct sockaddr_in recv_addr;   //接受来自目标的套接字结构体

    char send_pack[PACK_SIZE];      //用于保存发送的ICMP包
    char recv_pack[PACK_SIZE + IP_HEADER_SIZE];      //用于保存接收的ICMP包

    struct timeval first_send_time; //第一次发送ICMP数据包时的UNIX时间戳
    struct timeval recv_time;       //接收ICMP数据包时的UNIX时间戳

    Napi::Env env = NULL;
};