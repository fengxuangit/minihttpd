//
//  main.h
//  jmp2httpd
//
//  Created by jmpews on 15/12/12.
//  Copyright © 2015年 jmpews. All rights reserved.
//

//1. 每一个while除了需要判断需要的条件，还需要限制循环次数(即for的第三个参数)。
//2. string的结尾字符\0
//3. recv的MSG_PEEK参数，仅仅copy读取的buff，并不删除。
//4. malloc 要考虑所有可能的异常情况然后free
//5. segmentation fault (core dumped),内存问题,有没有初始化等等!
//6. strcp的坑
// malloc() 和""不一样，对于'\0',""默认长度+1
/*  char * file1=(char *)malloc(2*sizeof(char));
 file1[0]='a';
 file1[1]='b';
 file1[2]='c';
 char * file2="ab";
 printf("ABequal:%d\n",strcmp(file1,file2));
 */

//7. Content-Type: 有坑,要注意设置.
//8. 对于错误处理要果断,如果该错误是自己assert不会出错,那就直接打印error code然后exit,不要强行继续处理.
//9. 发送缓冲区的处理,8k左右字节,read了多少数据,就send多少数据,如果send返回缓冲区满了异常,那就重新将事件丢入,循环.
//10.规范化状态码
//11.header状态码处理
//12.如何读一个长度很大并且未知的数据,先分配一个缓存char buf[1024],每次读到buf,并且记录每次读取的数量,然后realloc重新分配空间,直至终>点
//13.读取body一行没有\n最后的处理
//14. 正确关闭socket！！！！！！ 先关闭写，然后等待客户端去关闭。shutdown(i,SHUT_WR); 会把缓冲区数据发送完毕，接受到ACK，再发送FIN，告诉浏览器没有需要写的数据了，等待客户端关闭。『客户端关』

#ifndef main_h
#define main_h


#endif /* main_h */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <memory.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define is_space(x) isspace((INT_32)(x))
#define TIP if(1){}else
#define INT_32 int
#define free_buf(buf) if(buf){free(buf);buf=NULL;}
#define SERVER_STRING "Server: jmp2httpd 0.1.0\r\n"
#define PRINT_LINE_TITLE(str) printf("\n----------------%s----------------\n", str);
#define PRINT_ERROR(str)      printf("\nERROR: %s;\n",str);

#define IO_ERROR -1

 //*******************通用性配置********************
#define PRINT_HADER 1


//********************通用性链表**********************

typedef  void ElemType;
typedef struct node
{
    ElemType *data;
    struct node * next;
}ListNode;


typedef int (*FindElemFunc)(ElemType *,ElemType *key);

int list_append(ListNode *,ElemType *);  //追加元素节点
ElemType *list_get_by_func(ListNode *,FindElemFunc,ElemType *);  //查找元素
ListNode *new_list_node();    //新建元素节点

/* 新节点加在head之后 */
int list_append(ListNode *head,ElemType *elem)
{
    ListNode *node;
    node=new_list_node();
    node->data=elem;
    node->next=head->next;
    head->next=node;
    return 0;
}

/* 通过自定义函数查找节点 */
ElemType *list_get_by_func(ListNode *head,FindElemFunc func,void *key)
{
    ListNode *tmp=head->next;
    if(tmp==NULL)
        return NULL;
    do{
        if(func(tmp->data,key))
        {
            return tmp->data;
        }
    }while(tmp=tmp->next);
    return NULL;
}

/* 新建链表节点 */
ListNode *new_list_node(){
    ListNode *tmp;
    tmp=(ListNode *)malloc(sizeof(ListNode));
    tmp->data=NULL;
    tmp->next=NULL;
    return tmp;
}

//******************* socket节点 ***********************

/* 请求结构体 */
typedef struct {
    char *read_cache;       //缓存读取内容
    int read_cache_len;     //缓存的内容长度
    char *header_dump;      //缓存请求头
    int header_dump_len;    //请求头的长度
    char method;            //请求方法
    char *request_path;     //请求路径
    long body_len;          //请求的body长度
}Req;

/* 响应结构体 */
typedef struct {
    long response_cache_len;    //响应内容长度
    char *response_path;        //响应文件路径
}Resp;

/* socket连接节点 */
typedef struct sn{
    int client_fd;          //socket连接描述符
    char IO_STATUS;         //socket状态
    Req request;            //socket对应的请求
    Resp response;          //socket对应的响应
    struct sn *next;
}SocketNode;

/* new socket节点 */
SocketNode *new_socket_node() {
    SocketNode *tmp = (SocketNode *) malloc(sizeof(SocketNode));
    memset(tmp, 0, sizeof(SocketNode));
    tmp->request.read_cache=NULL;
    tmp->request.header_dump=NULL;
    return tmp;
}

/* 查找socket描述符节点 */
SocketNode *find_socket_node(SocketNode *head,INT_32 client_fd) {
    SocketNode *tmp = head;
    if (head == NULL) {
        PRINT_ERROR("Socket-Head-Node is None");
        exit(1);
    }
    do{
        if (tmp->client_fd == client_fd)
            return tmp;
    }while(tmp=tmp->next);
    return NULL;

}

/* 添加节点到Header-Node与其他Node之间 */
void add_socket_node(SocketNode *head,SocketNode *client) {
    client->next = head->next;
    head->next = client;
}

/* 根据socket描述符释放节点 */
void free_socket_node(SocketNode *head,INT_32 client_fd) {
    SocketNode *tmp = head;
    SocketNode *k=NULL;
    //空链表
    if (tmp->next == NULL) {
        printf("! free_socket_node ERROR\n");
        exit(1);
    }
    if (head->client_fd==client_fd) {
        head=head->next;
        free_buf(k->request.read_cache);
        free_buf(k->request.header_dump);
        free_buf(k->request.request_path);
        free_buf(k->response.response_path);
        free_buf(k);
        close(client_fd);
        return;
    }
    while((tmp->next)!=NULL){
        if (tmp->next->client_fd == client_fd)
            break;
        tmp=tmp->next;
    }

    k = tmp->next;
    //没找到node
    if (k == NULL) {
        PRINT_ERROR("socket-node not found for client_fd");
        close(client_fd);
        exit(1);
    }

    tmp->next = k->next;
    printf("FREE:ID-%d,PATH-%s\n",client_fd,k->request.request_path);
    free_buf(k->request.read_cache);
    free_buf(k->request.header_dump);
    free_buf(k->request.request_path);
    free_buf(k->response.response_path);
    free_buf(k);

    close(client_fd);
    TIP printf("> SOCKET[%d] ready close.\n", client_fd);
}

//*****************************************  服务器初始化模块  ************************************

INT_32 set_nonblocking(INT_32 sockfd) {
    INT_32 opts;
    opts = fcntl(sockfd, F_GETFL);
    if (opts < 0) {
        PRINT_ERROR("fcntl: F_GETFL");
        return -1;
    }

    opts = opts | O_NONBLOCK;
    if (fcntl(sockfd, F_SETFL, opts) < 0) {
        PRINT_ERROR("fcntl: F_SETFL");
        return -1;
    }

    TIP printf("> Socket[%d] non-blocking.\n", sockfd);
    return 0;
}

SocketNode *SocketHead;

//保存根目录
char rootpath[256];

INT_32 startup(INT_32 *port) {
    INT_32 httpd = 0;
    struct sockaddr_in server_addr;
    if ((httpd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        PRINT_ERROR("httpd start error.");
        exit(1);
    }

    INT_32 opt = SO_REUSEADDR;
    if (setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        PRINT_ERROR("Reuse error.");
        exit(1);
    }

    //设置非阻塞
    set_nonblocking(httpd);

    SocketNode *tmp = new_socket_node();
    tmp->client_fd = httpd;
    SocketHead = tmp;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(*port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(httpd, (struct sockaddr *) &server_addr, sizeof(struct sockaddr_in)) == -1) {
        PRINT_ERROR("Bind error");
        exit(1);
    }

    if (listen(httpd, 1024) == -1) {
        PRINT_ERROR("Listen error");
        exit(1);
    }
    //设置root路径
    memset(rootpath, 0, sizeof(rootpath));
    getcwd(rootpath, sizeof(rootpath));
    return httpd;
}

//*****************************************  Request模块  ************************************

#define IO_EAGAIN EAGAIN
#define IO_DONE 0
#define IO_LINE_DONE 1
#define IO_LINE_YET 2

/*
 * 读到buf,返回状态码
 * 1. 读取状态码(失败、成功、EAGAIN)
 * 2. 读取内容长度
 * 3. 一行数据有没有读取完毕
*/
INT_32 read_line(INT_32 sock, char *buf,int BUF_SIZE,int *len) {
    char c='\0' ;
    INT_32 r=0,t=0;
    *len=0;
    //buf[BUF_SIZE-1] must be '\0'
    while ((t < BUF_SIZE - 1) && (c != '\n')) {
        r = recv(sock, &c, 1, 0);
        if (r > 0) {
            // 判断下一个符号是否是\r，如果是则表明为\r\n结束符
            if (c == '\r') {
                // MSG_PEEK:从缓冲区copy数据，并不删除数据，如果符合再次读取数据
                r = recv(sock, &c, 1, MSG_PEEK);
                if (r > 0 && c == '\n')
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[t] = c;
            t++;
        }
        else
            break;
    }

    buf[t++]='\0';
    *len=t;

    if (r < 0) {
        // 缓冲区为空,读取失败
        if (errno == EAGAIN) {
            return IO_EAGAIN;
        }
        else {
            PRINT_ERROR("get_line");
            return IO_ERROR;
        }
    }
    else if(r>0)
            return IO_LINE_DONE;
    return IO_ERROR;
}

//读取一行无论数据有多长
int read_line_more(int client_fd, char **malloc_buffer, int *len) {
    int r;
    int n = 0;
    int t;
    char *malloc_buf=NULL;
    int BUF_SIZE=1024;
    char buf[BUF_SIZE];

    memset(buf, 0, BUF_SIZE);
    *len=0;

    r = read_line(client_fd, buf, BUF_SIZE, &t);

    /*
     * 读取流程:
     * 读取一行(buf_size=1024)
     * 拷贝到buf
     * >进行状态判断
     * 1. 最后字符!='\n',且读取状态为IO_LINE_DONE
     *      重新读取
     * 2. 读取错误IO_EAGAIN || IO_ERROR
     *      返回buf+状态码
     * 3. 最后字符为'\n'
     *      读取完毕，返回返回buf+状态码
     */
   while(1){
        if(!malloc_buf)
            malloc_buf = (char *) malloc(sizeof(char) * t);
        else
            malloc_buf = (char *) realloc(malloc_buf, (n + t) * sizeof(char));
        memcpy(malloc_buf + n, buf, t);
        n += t;
        if(buf[t-1-1]!='\n' && r == IO_LINE_DONE ) {
           r=read_line(client_fd, buf, BUF_SIZE, &t);
        }
        else if(buf[t-1-1]=='\n' && r == IO_LINE_DONE)
        {
            *len=n;
            *malloc_buffer=malloc_buf;
            return r;
        }
        else if(r == IO_ERROR || r== IO_EAGAIN)
        {
            *len=n;
            *malloc_buffer=malloc_buf;
            return r;
        }

   }
}

#define M_GET 1
#define M_POST 2
#define M_ERROR -1

#define R_HEADER_INIT 0
#define R_HEADER_START 1
#define R_HEADER_BODY 2
#define R_BODY 3
#define R_RESPONSE 4
#define RESPONSE 5


int handle_error(int client_fd){
    char *malloc_buf;
    int len;
    read_line_more(client_fd, &malloc_buf, &len);
    do{
        if(PRINT_HADER)
            printf("%s",malloc_buf);
        free_buf(malloc_buf);
    }while(read_line_more(client_fd, &malloc_buf, &len)==IO_LINE_DONE);
    free_buf(malloc_buf);
    return IO_ERROR;
}

//处理请求的第一行,获取请求方法,请求路径
int request_header_start(int client_fd){
    int buffer_size=1024;
    int r;
    int i,j;
    char tmp_buf[1024];
    char *malloc_buf=NULL;
    int len;
    SocketNode *client_sock;

    client_sock=find_socket_node(SocketHead,client_fd);
    client_sock->IO_STATUS=R_HEADER_START;

    r=read_line_more(client_fd, &malloc_buf, &len);
    if (r!=IO_LINE_DONE)
        return r;
    //如果已经是当前状态,read_cache内有上次缓存数据
    if(client_sock->IO_STATUS==R_HEADER_START){
        if(client_sock->request.read_cache_len){
            client_sock->request.read_cache=(char *)realloc(client_sock->request.read_cache, (client_sock->request.read_cache_len+len));
            memcpy(client_sock->request.read_cache+client_sock->request.read_cache_len,malloc_buf,client_sock->request.read_cache_len);
            client_sock->request.read_cache_len+=len;
            if (r==IO_EAGAIN)
            {
                free_buf(malloc_buf);
                return IO_EAGAIN;
            }
            free_buf(malloc_buf);
            malloc_buf=client_sock->request.read_cache;
            len=client_sock->request.read_cache_len;
            client_sock->request.read_cache=NULL;
            client_sock->request.read_cache_len=0;
        }
    }


    if (r==IO_LINE_DONE) {
        i = j = 0;
        while (!(is_space(malloc_buf[j])) && i < len) {
            tmp_buf[i] = malloc_buf[j];
            i++;
            j++;
        }
        tmp_buf[i] = '\0';
        if (strcasecmp(tmp_buf, "GET") && strcasecmp(tmp_buf, "POST")) {
            free_buf(malloc_buf);
            len=0;
            printf("! ERROR-BUFFER:\n%s",malloc_buf);
            return handle_error(client_fd);
            //exit(1);
        }
        if (!strcasecmp(tmp_buf, "GET"))
            client_sock->request.method = M_GET;
        else if (!strcasecmp(tmp_buf, "POST"))
            client_sock->request.method = M_POST;
        else
            client_sock->request.method=M_ERROR;
        while (is_space(malloc_buf[j]) && (j < buffer_size))
            j++;

        //设置请求路径
        i = 0;
        while (!is_space(malloc_buf[j]) && (j < len) && (malloc_buf[i]!='?')) {
            tmp_buf[i] = malloc_buf[j];
            i++;
            j++;
        }
        tmp_buf[i]='\0';
        //sprintf(request_path, "%s/htdocs%s", rootpath, url);
        client_sock->request.request_path = (char *) malloc((i + 1) * sizeof(char));
        //tmp->Header.request_path[strlen(request_path)] = '\0';
        memcpy(client_sock->request.request_path, tmp_buf,i+1);

        //打印，保存到header_dump
        //PRINT_LINE_TITLE("header-start");
        if(PRINT_HADER)
            printf("%s",malloc_buf);

        client_sock->request.header_dump=(char *)malloc(len-1);
        memcpy(client_sock->request.header_dump+client_sock->request.header_dump_len, malloc_buf, len-1);
        client_sock->request.header_dump_len+=len-1;

        client_sock->IO_STATUS=R_HEADER_BODY;
        free_buf(malloc_buf);
        len=0;
        TIP printf("Request=ID:%d,PATH:%s\n",client_fd,client_sock->request.request_path);
        return IO_DONE;
    }
    free_buf(malloc_buf);
    return IO_ERROR;
}
void handle_header_kv(int client_fd,char *buf,int len){
    char key[64];
    int i;
    for (i=0; i<len; i++) {
        if(buf[i]==':')
            break;
    }
    memcpy(key, buf, i);
    key[i]='\0';
    i+=1;

    if (!strcasecmp(key, "Content-Length")) {
        SocketNode *client_sock;
        client_sock=find_socket_node(SocketHead, client_fd);
        client_sock->request.body_len=atol(buf+i);
    }
}
/*
 *  请求header的处理流程:
 */
int request_header_body(INT_32 client_fd){
    int buffer_size=1024;
    int r;
    int i;
    char *malloc_buf=NULL;
    int len=0;
    SocketNode *client_sock;

    client_sock=find_socket_node(SocketHead,client_fd);
    client_sock->IO_STATUS=R_HEADER_BODY;
    do{
        free_buf(malloc_buf);
        r=read_line_more(client_fd, &malloc_buf, &len);

        //如果已经是当前状态,read_cache内有上次缓存数据
        if(client_sock->request.read_cache_len){
            client_sock->request.read_cache=(char *)realloc(client_sock->request.read_cache, (client_sock->request.read_cache_len+len));
            memcpy(client_sock->request.read_cache+client_sock->request.read_cache_len,malloc_buf,client_sock->request.read_cache_len);
            client_sock->request.read_cache_len+=len;
            if (r==IO_EAGAIN)
            {
                free_buf(malloc_buf);
                return IO_EAGAIN;
            }
            free_buf(malloc_buf);
            malloc_buf=client_sock->request.read_cache;
            len=client_sock->request.read_cache_len;
            client_sock->request.read_cache=NULL;
            client_sock->request.read_cache_len=0;
        }
        
        //缓存区为空
        if (r==IO_EAGAIN)
        {
            client_sock->request.read_cache=(char *)realloc(client_sock->request.read_cache, (client_sock->request.read_cache_len+len));
            memcpy(client_sock->request.read_cache+client_sock->request.read_cache_len,malloc_buf,client_sock->request.read_cache_len);
            client_sock->request.read_cache_len+=len;
            free_buf(malloc_buf);
            client_sock->IO_STATUS=R_BODY;
            return IO_EAGAIN;
        }
        
        handle_header_kv(client_fd, malloc_buf, len);
        if(PRINT_HADER)
            printf("%s",malloc_buf);
        if(client_sock->request.header_dump)
        {
            client_sock->request.header_dump=(char *)realloc(client_sock->request.header_dump,len+client_sock->request.header_dump_len-1);
            memcpy(client_sock->request.header_dump+client_sock->request.header_dump_len, malloc_buf, len-1);
            client_sock->request.header_dump_len+=len-1;
        }
    }while((strcasecmp(malloc_buf, "\n"))&&r==IO_LINE_DONE);
    free_buf(malloc_buf);
    if(r==IO_LINE_DONE)
    {
        client_sock->IO_STATUS=R_BODY;
        return IO_DONE;
    }
    return IO_ERROR;
}


int request_body(INT_32 client_fd){
    int buffer_size=1024;
    int r;
    char *malloc_buf=NULL;
    int len=0;
    char *buffer;
    SocketNode *client_sock;
    client_sock=find_socket_node(SocketHead,client_fd);
    client_sock->IO_STATUS=R_BODY;
    //PRINT_LINE_TITLE("header-end");
    if (!client_sock->request.body_len) {
        client_sock->IO_STATUS=R_RESPONSE;
        return IO_DONE;
    }
    do{
        free_buf(malloc_buf);
        r=read_line_more(client_fd, &malloc_buf, &len);

        //如果已经是当前状态,read_cache内有上次缓存数据
        if(client_sock->request.read_cache_len){
            client_sock->request.read_cache=(char *)realloc(client_sock->request.read_cache, (client_sock->request.read_cache_len+len));
            memcpy(client_sock->request.read_cache+client_sock->request.read_cache_len,malloc_buf,client_sock->request.read_cache_len);
            client_sock->request.read_cache_len+=len;
            if (r==IO_EAGAIN)
            {
                free_buf(malloc_buf);
                client_sock->IO_STATUS=R_BODY;
                return IO_EAGAIN;
            }
            free_buf(malloc_buf);
            malloc_buf=client_sock->request.read_cache;
            len=client_sock->request.read_cache_len;
            client_sock->request.read_cache=NULL;
            client_sock->request.read_cache_len=0;
        }

        //缓存区为空
        if (r==IO_EAGAIN)
        {
            client_sock->request.read_cache=(char *)realloc(client_sock->request.read_cache, (client_sock->request.read_cache_len+len));
            memcpy(client_sock->request.read_cache+client_sock->request.read_cache_len,malloc_buf,client_sock->request.read_cache_len);
            client_sock->request.read_cache_len+=len;
            free_buf(malloc_buf);
            client_sock->IO_STATUS=R_BODY;
            return IO_EAGAIN;
        }

        malloc_buf[len]='\0';
        TIP printf("%s",buffer);
        printf("\0");
        fflush(stdout);
        //加了一个误差，多余的。
        if (len+5>=client_sock->request.body_len){
            client_sock->IO_STATUS=R_RESPONSE;
            return IO_DONE;
        }
    }while(r==IO_LINE_DONE);
    free_buf(malloc_buf);
    return IO_ERROR;
}

int handle_request(int client_fd){
    int r;
    SocketNode *client_sock;
    client_sock=find_socket_node(SocketHead,client_fd);
    switch (client_sock->IO_STATUS) {
        case R_HEADER_INIT:
            printf("\0");
        case R_HEADER_START:
        {
            r=request_header_start(client_fd);
            if (r==IO_EAGAIN) {
                if (client_sock->request.method==M_ERROR)
                    return IO_ERROR;
                return IO_EAGAIN;
            }
            else if(r==IO_ERROR)
                return IO_ERROR;
        }
        case R_HEADER_BODY:
        {
            r=request_header_body(client_fd);
            if (r==IO_EAGAIN) {
                if (client_sock->request.method==M_ERROR)
                    return IO_ERROR;
                return IO_EAGAIN;
            }
            else if(r==IO_ERROR)
                return IO_ERROR;
        }
        case R_BODY:
        {
            r=request_body(client_fd);
            if (r==IO_EAGAIN||(client_sock->request.method==M_ERROR)) {
                return IO_EAGAIN;
            }
            else if(r==IO_ERROR)
                return IO_ERROR;
        }
        default:
            break;
    }
    return IO_DONE;
}

//*******************************************  Response模块  ********************************
void send_data(INT_32 client,char *data);
INT_32 send_file(INT_32 client_fd,char *file_path,long *len);
void send_not_found(INT_32 client_fd);
//------------------------------------路由匹配-------------------------------
typedef struct urlroute{
    char route[64];
    char *(*func)(SocketNode *);
} URL_ROUTE;
ListNode *head_route;

// 返回一个函数指针,该函数返回接受SocketNode * 参数,返回char *
typedef char *(*RouteFunc)(SocketNode *);

char *route_func1(SocketNode *tmp){
    char *str;
    char *s="route.1:hello world\n\0";
    str=(char *)malloc(1024*sizeof(char));
    strcpy(str,s);
    str[strlen(s)]='\0';
    return str;
}

char *route_func2(SocketNode *tmp){
    char *str;
    char *s="route.2:hello world\n\0";
    str=(char *)malloc(1024*sizeof(char));
    strcpy(str,s);
    str[strlen(s)]='\0';
    return str;
}
char *route_func3(SocketNode *tmp){
    return tmp->request.header_dump;
}

int find_route(ElemType *data,void *key){
    if(!strcasecmp((char *)key, ((URL_ROUTE *)data)->route)){
        printf("匹配到route:%s",(char *)key);
        return 1;
    }
    return 0;
}

URL_ROUTE *new_route_node(char *route_str,RouteFunc func){
    URL_ROUTE *tmp=(URL_ROUTE *)malloc(sizeof(URL_ROUTE));
    strcpy(tmp->route,route_str);
    tmp->func=func;
    return tmp;
}

void init_route(){
    head_route=new_list_node();
    list_append(head_route, new_route_node("/route1", route_func1));
    list_append(head_route, new_route_node("/route2", route_func2));
    list_append(head_route, new_route_node("/echo", route_func3));
}

//------------------------------------路由匹配-------------------------------

char *handle_route(SocketNode *client_sock,char *route_key){
    char *resp;
    ElemType *data;
    data=list_get_by_func(head_route, find_route, route_key);
    if(data)
    {
        resp=((URL_ROUTE *)data)->func(client_sock);
        return resp;
    }
    else{
        return NULL;
    }
}


int handle_response(int client_fd){
    char response_path[256];
    RouteFunc func;
    SocketNode *client_sock;
    char *resp=NULL;
    struct stat st;
    int r;
    memset(response_path, 0, 256);
    //空异常 TODO
    client_sock=find_socket_node(SocketHead, client_fd);
    if((client_sock->IO_STATUS==R_RESPONSE)&&(!client_sock->response.response_cache_len))
    {
        //先进行路由匹配
        resp=handle_route(client_sock, client_sock->request.request_path);
        if (resp) {
            send_data(client_fd,resp);
        }
        else{
            client_sock->IO_STATUS=RESPONSE;
            //strcpy(response_path,"/root/httpd/index.html");
            //strcpy(response_path,"/Users/jmpews/Desktop/jmp2httpd/jmp2httpd/htdocs/index.html");
            sprintf(response_path, "%s/htdocs%s", rootpath, client_sock->request.request_path);
            //sprintf(response_path, "/Users/jmpews/Desktop/jmp2httpd/jmp2httpd/htdocs%s", client_sock->request.request_path);
            if (response_path[strlen(response_path) - 1] == '/')
                strcat(response_path, "index.html");
            response_path[strlen(response_path)] = '\0';
            //printf("%s\n",response_path);
            fflush(stdout);
            if ((stat(response_path, &st) != -1) && ((st.st_mode & S_IFMT) == S_IFREG)) {
                // strcpy(tmp->Header.request_path,request_path);
                // tmp->Header.request_path[strlen(tmp->Header.request_path)]='\0';
                client_sock->response.response_path=(char *)malloc(strlen(response_path)+1);
                memcpy(client_sock->response.response_path,response_path,strlen(response_path)+1);
                r = send_file(client_fd, response_path,&client_sock->response.response_cache_len);
                if(r==IO_EAGAIN)
                    return IO_EAGAIN;
                else if(r==IO_DONE)
                    return IO_DONE;
                else if(r==IO_ERROR){
                    return IO_ERROR;
                }
            }
            else{
                send_not_found(client_fd);
                return IO_DONE;
            }

        }

    }else if(client_sock->IO_STATUS==RESPONSE){
        r=send_file(client_fd,client_sock->response.response_path,&client_sock->response.response_cache_len);
        if(r==IO_EAGAIN)
            return IO_EAGAIN;
        else if(r==IO_DONE)
            return IO_DONE;
        else if(r==IO_ERROR){
            return IO_ERROR;
        }
    }
    else{
        PRINT_ERROR("! socket 状态码错误");
    }
    return IO_DONE;
}

void send_headers(INT_32 client_fd) {
    int buffer_size=1024;
    char buf[buffer_size];
    memset(buf, 0, buffer_size);
    strcat(buf, "HTTP/1.0 200 OK\r\n");
    strcat(buf, SERVER_STRING);
    //strcat(buf, "Content-Type: text/html\r\n");
    strcat(buf, "\r\n");
    send(client_fd, buf, strlen(buf), 0);
}

void send_data(INT_32 client,char *data) {
    int buffer_size=1024;
    //不是我每次返回一个指针,而是,传给函数一个buff指针,然后对这个指针内容做修改.
    char buf[buffer_size];
    memset(buf, 0, buffer_size);
    strcat(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    strcat(buf, SERVER_STRING);
    strcat(buf, "Content-Type: text/html\r\n");
    strcat(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    send(client,data,strlen(data),0);
}

void send_not_found(INT_32 client_fd) {
    int buffer_size=1024;
    char buf[buffer_size];
    memset(buf, 0, buffer_size);
    strcat(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    strcat(buf, SERVER_STRING);
    strcat(buf, "Content-Type: text/html\r\n");
    strcat(buf, "\r\n");
    strcat(buf, "<!DOCTYPE html>\r\n<html lang=\"en\">");
    strcat(buf, "<head>\r\n<meta charset=\"UTF-8\">\r\n<title>Not Found</title></head>\r\n");
    strcat(buf, "<body>\r\n<h2 style=\"text-align: center;\"> not found world.</h2>");
    strcat(buf, "<h5 style=\"text-align: right;\">by jmpews.</h5>\r\n");
    strcat(buf, "</body></html>");
    buf[strlen(buf)] = '\0';
    send(client_fd, buf, strlen(buf), 0);
}

INT_32 send_file(INT_32 client_fd,char *file_path,long *len) {
    int buffer_size=1024;
    FILE *fd;
    long file_length = 0;
    char buf[buffer_size];
    int t=0,r = 0;

    fd = fopen(file_path, "r");
    if (fd == NULL) {
        perror("! send_file/fopen error\n");
        exit(1);
    }

    //内容长度，暂时不用，协议自动计算
    fseek(fd, 0, SEEK_END);
    file_length = ftell(fd);
    rewind(fd);

    //设置文件当前指针,为上次没有读完的
    if (*len)
        fseek(fd, *len, SEEK_SET);
    else
        send_headers(client_fd);
    while (1) {
        memset(buf, 0, buffer_size);
        t = fread(buf, sizeof(char), buffer_size, fd);
        r = send(client_fd, buf, t, 0);
        if (r < 0) {
            if (errno == EAGAIN) {
                fclose(fd);
                return IO_EAGAIN;
            }
            else {
                PRINT_ERROR("Send Error:");
                fclose(fd);
                return IO_ERROR;
            }
        }
        *len+=r;
        if ((*len+2)>=file_length||t<=(buffer_size-2)){
            fclose(fd);
            return IO_DONE;
        }

    }
}




//*******************************************  Select模块  ********************************
void select_loop(INT_32 httpd){
    struct sockaddr_in client_addr;
    struct timeval tv;
    socklen_t addr_len;
    addr_len=sizeof(struct sockaddr_in);

    char c;
    int i,j,r,MAX_CLIENTS;
    MAX_CLIENTS=1024;

    int client_fd;
    int  client_array[MAX_CLIENTS];
    memset(client_array, 0, MAX_CLIENTS*sizeof(int));
    int maxfd;
    maxfd=0;

    fd_set read_fds;
    fd_set write_fds;
    fd_set exception_fds;
    fd_set tmp_read_fds;
    fd_set tmp_write_fds;
    fd_set tmp_exception_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&exception_fds);

    char ipaddr[32];

    FD_SET(httpd,&read_fds);
    client_array[httpd]=1;

    while(1)
    {
        tmp_read_fds=read_fds;
        tmp_write_fds=write_fds;
        tmp_exception_fds=exception_fds;

        for (i=0; i<MAX_CLIENTS; i++)
        {
            if (client_array[i])
                if (maxfd<i)
                    maxfd=i;
        }
        tv.tv_sec=5;
        tv.tv_usec=0;
        r=select(maxfd+2, &tmp_read_fds, &tmp_write_fds, &tmp_exception_fds, &tv);
        if (r<0)
            perror("! select() error.");
        else
        {
            if (FD_ISSET(httpd,&tmp_read_fds)) {
                client_fd=accept(httpd, (struct sockaddr*)&client_addr, &addr_len);
                if (client_fd<0)
                    printf("! client connect error.");
                else
                {
                    inet_ntop(AF_INET, &(client_addr.sin_addr), ipaddr, 32 * sizeof(char));
                    printf("> SOCKET[%d] Accept : %s\n", client_fd, ipaddr);
                    set_nonblocking(client_fd);
                    SocketNode *tmp = new_socket_node();
                    tmp->client_fd = client_fd;
                    add_socket_node(SocketHead,tmp);

                    FD_SET(client_fd,&read_fds);
                    client_array[client_fd]=1;
                    maxfd=(maxfd < client_fd)?client_fd:maxfd;
                }
            }
            else{
                for(i=httpd+1;i<=maxfd;i++){
                    if(client_array[i])
                    {
                        if(FD_ISSET(i,&tmp_read_fds))
                        {
                            r = recv(i, &c, 1, MSG_PEEK);
                            if(r<1){
                                    printf("CLOSE=ID:%d\n",i);
                                    free_socket_node(SocketHead, i);
                                    FD_CLR(i,&read_fds);
                                    client_array[i]=0;
                                    fflush(stdout);
                                    continue;
                            }
                            //read data or close connect
                            r=handle_request(i);
                            if(r==IO_DONE)
                            {
                                FD_CLR(i,&read_fds);
                                FD_SET(i,&write_fds);
                            } else if (r==IO_ERROR) {
                                printf("CLOSE=ID:%d\n",i);
                                FD_CLR(i,&read_fds);
                                free_socket_node(SocketHead, i);
                                client_array[i]=0;
                            }
                            else if(r==IO_EAGAIN){
                                TIP printf("EAGAIN:wow.");
                            }
                        }
                        else if(FD_ISSET(i,&tmp_write_fds))
                        {
                            //send data
                            r=handle_response(i);
                            if(r==IO_DONE||r==IO_ERROR){
                                shutdown(i,SHUT_WR);
                                FD_CLR(i,&write_fds);
                                FD_SET(i,&read_fds);
                            }
                        }
                    }
                }
            }
        }
    }
}

//*******************************************  EPoll模块  ********************************

#ifdef DEF_EPOLL

#include <sys/epoll.h>
void epoll_loop(INT_32 httpd) {
    INT_32 epoll_fd, nfds;
    int buffer_size;
    int MAX_CLIENTS=1024;
    INT_32 client_fd;
    INT_32 s;
    INT_32 i;
    INT_32 r;
    char c;
    struct epoll_event ev;
    struct epoll_event *events;
    struct sockaddr_in client_addr;
    char ipaddr[32];
    socklen_t socket_len = sizeof(struct sockaddr_in);

    epoll_fd = epoll_create(MAX_CLIENTS);
    ev.data.fd = httpd;
    ev.events = EPOLLIN | EPOLLET;
    s = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, httpd, &ev);
    if (s == -1) {
        printf("! epoll_ctl error.\n");
        abort();
    }
    events = (struct epoll_event *) malloc(MAX_CLIENTS * sizeof(struct epoll_event));
    if (events == NULL) {
        printf("! malloc error.\n");
    }
    memset(events, 0, MAX_CLIENTS * sizeof(struct epoll_event));
    for (; ;) {
        nfds = epoll_wait(epoll_fd, events, MAX_CLIENTS, 3000);
        for (i = 0; i < nfds; i++) {
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)) {
                printf("! epoll error.\n");
                //abort();
                continue;
            }
            else if (events[i].events & EPOLLIN) {

                if (events[i].data.fd == httpd) {
                    //ET until get the errno=EAGAIN
                    while ((client_fd = accept(httpd, (struct sockaddr *) &client_addr, &socket_len)) > 0) {
                        inet_ntop(AF_INET, &(client_addr.sin_addr), ipaddr, 32 * sizeof(char));
                        printf("> SOCKET[%d] Accept : %s\n", client_fd, ipaddr);
                        set_nonblocking(client_fd);
                        ev.data.fd = client_fd;
                        ev.events = EPOLLIN | EPOLLET;
                        s = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
                        SocketNode *client_sock = new_socket_node();
                        client_sock->client_fd = client_fd;
                        add_socket_node(SocketHead,client_sock);
                    }
                    if (errno == EAGAIN) TIP printf("! accept EAGAIN\n");
                    //printf("! EAGAIN try again\n");
                    else
                        perror("! Accept Error:");
                    continue;
                }
                else {
                    // 判断读取到0长度数据表明，客户端主动关闭
                    r = recv(i, &c, 1, MSG_PEEK);
                    if(r<1){
                        printf("CLOSE=ID:%d\n",i);
                        free_socket_node(SocketHead, i);
                        FD_CLR(i,&read_fds);
                        client_array[i]=0;
                        fflush(stdout);
                        continue;
                    }
                    r = handle_request(events[i].data.fd);
                    if (r == IO_DONE) {
                        ev.data.fd = events[i].data.fd;
                        ev.events = EPOLLOUT | EPOLLET;
                        s = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, events[i].data.fd, &ev);
                    }else if (r==IO_ERROR) {
                        printf("IO_ERROR=ID:%d",i);
                        free_socket_node(SocketHead, i);
                        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL) == -1)
                            printf("! close epoll_ctl error.\n");
                    }
                    else if(r==IO_EAGAIN){
                        TIP printf("EAGAIN:wow.");
                    }

                }
            }
            else if (events[i].events & EPOLLOUT) {
                r = handle_response(events[i].data.fd);
                if(r==IO_DONE||r==IO_ERROR){
                    //优雅关闭socekt，采用shutdown，先关闭写通道，然后让客户端主动关闭
                    shutdown(i,SHUT_WR);
                    ev.data.fd = events[i].data.fd;
                    ev.events = EPOLLIN | EPOLLET;
                    s = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, events[i].data.fd, &ev);
                }
            }
            else {
                printf("uncatched.");
            }
        }
    }
}
#endif
