#include "ping.h"
#include "netdb.h"

using namespace std;

Napi::Object Ping::Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env,"Ping",{
        InstanceMethod("start",&Ping::start),
    });

    Napi::FunctionReference *constructor = new Napi::FunctionReference();
    *constructor = Napi::Persistent(func);
    env.SetInstanceData(constructor);

    exports.Set("Ping", func);
    return exports;
}

Ping::Ping(const Napi::CallbackInfo &info): Napi::ObjectWrap<Ping>(info) {
    Napi::Env env = info.Env();
    this->env = env;

    int length = info.Length();

    if (length <= 0) {
        Napi::TypeError::New(env, "Option should provide").ThrowAsJavaScriptException();
        return;
    }
    if (!info[0].IsObject()) {
        Napi::TypeError::New(env,"Option should be object").ThrowAsJavaScriptException();
        return;
    }
    Napi::Object obj = info[0].ToObject();
    if (obj.Has("addr") && obj.Get("addr").IsString()){
        string originAddr = obj.Get("addr").As<Napi::String>();
        this->options.addr = (char *)originAddr.c_str();
    }else{
        Napi::TypeError::New(env,"addr should be string").ThrowAsJavaScriptException();
        return;
    }
    if (obj.Has("retry") && obj.Get("retry").IsNumber()){
        this->options.retry = obj.Get("retry").As<Napi::Number>();
    }else{
        Napi::TypeError::New(env,"retry should be number").ThrowAsJavaScriptException();
        return;
    }
    if (obj.Has("timeout")){
        if (obj.Get("timeout").IsNumber()){
            this->options.timeout = obj.Get("timeout").As<Napi::Number>();
        }else{
            Napi::TypeError::New(env,"retry should be number").ThrowAsJavaScriptException();
            return;
        }
    }else{
        this->options.timeout = 60 * 1000;
    }
    this->send_pack_num = 0;
    this->recv_pack_num = 0;
    this->lost_pack_num = 0;

    this->min_time = 0;
    this->max_time = 0;
    this->sum_time = 0;

}

Ping::~Ping() {
    if(close(sock_fd) == -1) {
        fprintf(stderr, "Close socket error:%s \n\a", strerror(errno));
        Napi::TypeError::New(env,"Close socket Error").ThrowAsJavaScriptException();
        exit(1);
    }
}

unsigned short Ping::CheckSum(unsigned short * header, int length) {
    int check_sum = 0;              //校验和
    int nleft = length;          //还未计算校验和的数据长度
    unsigned short * p = header; //用来做临时指针
    unsigned short temp;            //用来处理字节长度为奇数的情况

    while(nleft > 1){
        check_sum += *p++;          //check_sum先加以后，p的指针才向后移
        nleft -= 2;
    }

    //奇数个长度
    if(nleft == 1){
        //利用char类型是8个字节，将剩下的一个字节压入unsigned short（16字节）的高八位
        *(unsigned char *)&temp = *(unsigned char *)p;
        check_sum += temp;
    }

    check_sum = (check_sum >> 16) + (check_sum & 0xffff);   //将之前计算结果的高16位和低16位相加
    check_sum += (check_sum >> 16);                         //防止上一步也出现溢出
    temp = ~check_sum;              //temp是最后的校验和

    return temp;
}

void Ping::CreateSocket() {
    struct protoent * protocol;             //获取协议用
    unsigned long in_addr;                  //用来保存网络字节序的二进制地址
    struct hostent host_info, * host_pointer; //用于gethostbyname_r存放IP信息
    char buff[2048];                         //gethostbyname_r函数临时的缓冲区，用来存储过程中的各种信息
    int errnop = 0;                         //gethostbyname_r函数存储错误码

    //通过协议名称获取协议编号
    if((protocol = getprotobyname("icmp")) == NULL){
        fprintf(stderr, "Get protocol error:%s \n\a", strerror(errno));
        exit(1);
    }

    //创建原始套接字，这里需要root权限，申请完成之后应该降权处理
    if((sock_fd = socket(AF_INET, SOCK_RAW, protocol->p_proto)) == -1){
        fprintf(stderr, "Greate RAW socket error:%s \n\a", strerror(errno));
        exit(1);
    }

    //降权处理，使该进程的EUID，SUID的值变成RUID的值
    setuid(getuid());

    //设置send_addr结构体
    send_addr.sin_family = AF_INET;

    //判断用户输入的点分十进制的ip地址还是域名，如果是域名则将其转化为ip地址，并备份
    //inet_addr()将一个点分十进制的IP转换成一个长整数型数
    if((in_addr = inet_addr(input_domain.c_str())) == INADDR_NONE){
        //输入的不是点分十进制的ip地址
        if(gethostbyname_r(input_domain.c_str(), &host_info, buff, sizeof(buff), &host_pointer, &errnop)){
            //非法域名
            fprintf(stderr, "Get host by name error:%s \n\a", strerror(errno));
            exit(1);
        } else{
            //输入的是域名
            this->send_addr.sin_addr = *((struct in_addr *)host_pointer->h_addr);
        }
    } else{
        //输入的是点分十进制的地址
        this->send_addr.sin_addr.s_addr = in_addr;
    }

    //将ip地址备份下来
    this->backup_ip = inet_ntoa(send_addr.sin_addr);

    printf("PING %s (%s) %d(%d) bytes of data.\n", input_domain.c_str(),
           backup_ip.c_str(), PACK_SIZE - 8, PACK_SIZE + 20);

    gettimeofday(&first_send_time, NULL);
}

Napi::Value Ping::start(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    return Napi::Number::New(env,100);
}

