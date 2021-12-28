#include "ping.h"

Napi::Object Ping::Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env, "Ping",
                                      {
                                          InstanceMethod("start", &Ping::start),
                                      });

    Napi::FunctionReference *constructor = new Napi::FunctionReference();
    *constructor = Napi::Persistent(func);
    env.SetInstanceData(constructor);

    exports.Set("Ping", func);
    return exports;
}

Ping::Ping(const Napi::CallbackInfo &info) : Napi::ObjectWrap<Ping>(info) {
    Napi::Env env = info.Env();
    this->env = env;

    int length = info.Length();

    if (length <= 0) {
        Napi::TypeError::New(env, "Option should provide").ThrowAsJavaScriptException();
        return;
    }
    if (!info[0].IsObject()) {
        Napi::TypeError::New(env, "Option should be object").ThrowAsJavaScriptException();
        return;
    }
    Napi::Object obj = info[0].ToObject();
    if (obj.Has("addr") && obj.Get("addr").IsString()) {
        string originAddr = obj.Get("addr").As<Napi::String>();
        this->options.addr = (char *)originAddr.c_str();
    } else {
        Napi::TypeError::New(env, "addr should be string").ThrowAsJavaScriptException();
        return;
    }
    if (obj.Has("retry") && obj.Get("retry").IsNumber()) {
        this->options.retry = obj.Get("retry").As<Napi::Number>();
    } else {
        Napi::TypeError::New(env, "retry should be number").ThrowAsJavaScriptException();
        return;
    }
    if (obj.Has("timeout")) {
        if (obj.Get("timeout").IsNumber()) {
            this->options.timeout = obj.Get("timeout").As<Napi::Number>();
        } else {
            Napi::TypeError::New(env, "retry should be number").ThrowAsJavaScriptException();
            return;
        }
    } else {
        this->options.timeout = 60 * 1000;
    }
    this->send_pack_num = 0;
    this->recv_pack_num = 0;
    this->lost_pack_num = 0;
}

Ping::~Ping() {
    if (close(sock_fd) == -1) {
        fprintf(stderr, "Close socket error:%s \n\a", strerror(errno));
        Napi::TypeError::New(env, "Close socket Error").ThrowAsJavaScriptException();
        exit(1);
    }
}

unsigned short Ping::CheckSum(unsigned short *header, int length) {
    int check_sum = 0;           //校验和
    int nleft = length;          //还未计算校验和的数据长度
    unsigned short *p = header;  //用来做临时指针

    while (nleft > 1) {
        check_sum += *p++;  // check_sum先加以后，p的指针才向后移
        nleft -= sizeof(unsigned short);
    }

    if (nleft) {
        check_sum += *(unsigned char *)p;
    }

    check_sum = (check_sum >> 16) + (check_sum & 0xffff);  //将之前计算结果的高16位和低16位相加
    check_sum += (check_sum >> 16);                        //防止上一步也出现溢出

    return (unsigned short)(~check_sum);
}

unsigned short Ping::GetPid() {
    unsigned short pid = getpid();
    // MACOS pid will be more than short
#ifdef __APPLE__
    pid = getpid() >> 1;
#endif
    return pid;
}

void Ping::CreateSocket() {
    struct protoent *protocol;
    unsigned long in_addr;
    struct hostent *host_pointer;

    if ((protocol = getprotobyname("icmp")) == NULL) {
        fprintf(stderr, "Get protocol error:%s \n\a", strerror(errno));
        exit(1);
    }
// MACOS use SOCK_DGRAM
#ifdef __APPLE__
    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, protocol->p_proto)) == -1) {
        fprintf(stderr, "Create RAW socket error:%s \n\a", strerror(errno));
        exit(1);
    }
#endif
    send_addr.sin_family = AF_INET;

    if ((in_addr = inet_addr(input_domain.c_str())) == INADDR_NONE) {
        host_pointer = gethostbyname(input_domain.c_str());
        if (host_pointer == NULL) {
            fprintf(stderr, "Get host by name error:%s \n\a", strerror(errno));
            exit(1);
        } else {
            this->send_addr.sin_addr.s_addr = (*(struct in_addr *)host_pointer->h_addr).s_addr;
        }
    } else {
        this->send_addr.sin_addr.s_addr = in_addr;
    }
    this->backup_ip = inet_ntoa(send_addr.sin_addr);
    gettimeofday(&first_send_time, NULL);
}

int Ping::GeneratePacket() {
    int pack_size;
    ICMP_HEADER *icmp_pointer;
    struct timeval time_pointer;
    gettimeofday(&time_pointer, NULL);
    pack_size = PACK_SIZE;

    memset(send_pack, 0, sizeof(send_pack));
    memset(recv_pack, 0, sizeof(recv_pack));

    icmp_pointer = (ICMP_HEADER *)send_pack;

    icmp_pointer->icmp_type = ICMP_ECHO;
    icmp_pointer->icmp_code = 0;
    icmp_pointer->icmp_seq = send_pack_num + 1;
    icmp_pointer->icmp_id = this->GetPid();
    icmp_pointer->timestamp_s = time_pointer.tv_sec;
    icmp_pointer->timestamp_us = time_pointer.tv_usec;
    icmp_pointer->icmp_checksum = this->CheckSum((unsigned short *)send_pack, pack_size);
    return pack_size;
}

void Ping::SendPacket() {
    int pack_size = GeneratePacket();
    if ((sendto(sock_fd, send_pack, pack_size, 0, (const struct sockaddr *)&send_addr, sizeof(send_addr))) < 0) {
        fprintf(stderr, "Sendto error:%s \n\a", strerror(errno));
        exit(1);
    }
    this->send_pack_num++;
}

int Ping::ResolvePacket(int pack_size) {
    int icmp_len, ip_header_len;
    ICMP_HEADER *icmp_pointer;
    IP_HEADER *ip_pointer = (IP_HEADER *)recv_pack;
    double rtt;
    struct timeval time_end;

    gettimeofday(&time_end, NULL);

    ip_header_len = ip_pointer->header_length << 2;             // ip报头长度=ip报头的长度标志乘4
    icmp_pointer = (ICMP_HEADER *)(recv_pack + ip_header_len);  // pIcmp指向的是ICMP头部，因此要跳过IP头部数据
    icmp_len = pack_size - ip_header_len;                       // ICMP报头及ICMP数据报的总长度

    //收到的ICMP包长度小于报头
    if (icmp_len < 8) {
        printf("received ICMP pack length:%d(%d) is error!\n", pack_size, icmp_len);
        lost_pack_num++;
        return -1;
    }

    if ((icmp_pointer->icmp_type == ICMP_ECHO_REPLY) && (backup_ip == inet_ntoa(recv_addr.sin_addr)) &&
        (icmp_pointer->icmp_code == ICMP_REPLY_CODE)) {
        unsigned long time_send_s = icmp_pointer->timestamp_s;
        unsigned long time_send_us = icmp_pointer->timestamp_us;
        if ((recv_time.tv_usec - time_send_us) < 0) {
            --recv_time.tv_sec;
            recv_time.tv_usec += 10000000;
        }
        rtt = (recv_time.tv_sec - time_send_s) * 1000 + (double)recv_time.tv_usec / 1000.0;

        printf("%d byte from %s : icmp_seq=%u ttl=%d time=%.1fms\n", icmp_len, inet_ntoa(recv_addr.sin_addr),
               icmp_pointer->icmp_seq, ip_pointer->ttl, rtt);
        recv_pack_num++;
    } else {
        lost_pack_num++;
        return -1;
    }
}

void Ping::RecvPacket() {
    int recv_size, fromlen;
    fromlen = sizeof(struct sockaddr);
    //还在集合中则说明收到了回显的数据包
    if ((recv_size = recvfrom(sock_fd, recv_pack, sizeof(recv_pack), 0, (struct sockaddr *)&recv_addr,
                              (socklen_t *)&fromlen)) < 0) {
        fprintf(stderr, "packet error(size:%d):%s \n\a", recv_size, strerror(errno));
        lost_pack_num++;
    } else {
        //收到了可能合适的数据包
        gettimeofday(&recv_time, NULL);
        ResolvePacket(recv_size);
    }
}

Napi::Value Ping::start(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    return Napi::Number::New(env, 100);
}
