#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
//商品最大数量
#define MAX_GOODS_ITEMS 100
//订单中的商品最大数量
#define MAX_ORDERFORM_ITEMS 10
//商品清单结构体
typedef struct goodsItems
{
    char type[20];
    char goodsID[12];
    char goodsname[10];
    float price;
    int repertory;
}goodsItems;
//商品清单
goodsItems goodslist[MAX_GOODS_ITEMS];
//订单结构体
typedef struct orderFormItem{
    char goodsID[12];
    int num;
}orderFormItem;
//订单
orderFormItem orderForm[MAX_ORDERFORM_ITEMS];
//商品总价（从服务器传来）
float totalPrice = 0;
//初始化订单信息
int initOrderInfo(){
    for(int i = 0; i < MAX_ORDERFORM_ITEMS;i++){
        memset(orderForm[i].goodsID,0,12);
        orderForm[i].num = 0;
    }
    totalPrice = 0;
    return 0;
}
//与服务器建立连接
int serverCon(int *sockfd,pid_t *cpid){
    int len = 0;
    struct sockaddr_in address;
    int result;
    *sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(*sockfd < 0){
        exit(-1);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(8081);
    if(inet_aton("127.0.0.1",&address.sin_addr) < 0){
        printf("inet_aton error.\n");
        exit(-1);
    }
     len = sizeof(address);
    *cpid = getpid();
    printf("客户机%d开始connect服务器……\n",*cpid);
    result = connect(*sockfd,(struct sockaddr *)&address,len);
    if(result == -1){
        perror("客户机connect服务器失败！\n");
        exit(-1);
    }
    else{
        printf("客户机连接服务器成功!\n");
        printf("将客户机进程号发给服务器……\n");
        write(*sockfd,cpid,sizeof(cpid));
    }
}
//接收服务器发来的商品信息
int getGoods(int sockfd){
    int goodsNum = 0;
    read(sockfd,goodslist,sizeof(goodslist));
    for(goodsNum = 0; goodsNum < MAX_GOODS_ITEMS;goodsNum++){
        if(goodslist[goodsNum].repertory == 0) break;
    }
    //检索从服务器接收过来的商品数量
    return goodsNum;
}
//打印商品信息和操作提示语
int printGoodsAndOpe(int goodsNum){
    printf("=========================================================\n");
    printf("=    商品类\t商品ID\t商品名称\t价格\t当前库存=\n");
    printf("=========================================================\n");
    for(int i = 0; i < goodsNum; i++){
            printf("=%10s\t%s\t%s\t\t%.2f\t%d\t=\n",
                  goodslist[i].type,goodslist[i].goodsID,goodslist[i].goodsname,goodslist[i].price,goodslist[i].repertory);
    }
    printf("=========================================================\n");
    printf("=  请输入操作符：    1.挑选商品      2.查看购物车       =\n");
    printf("=  请输入操作符：    3.提交订单      4.退出客户端       =\n");
    printf("=========================================================\n");
}
//根据商品ID查询商品名称和库存
int searchGoodsInfo(char *goodsID_tmp,int goodsNum){
    //找到商品ID对应的goodslist下标返回，找不到返回-1
    for(int i = 0;i < goodsNum;i++){
        if(strncmp(goodsID_tmp,goodslist[i].goodsID,6) == 0 && goodslist[i].repertory != 0)   return i;
    }
    return -1;
}
//根据商品ID查找对应的orderForm下标，找不到就返回-1
int searchOrderInfo(char *goodsID_tmp,int orderNum){
        for(int i = 0;i < orderNum;i++){
        if(strcmp(goodsID_tmp,goodslist[i].goodsID) == 0)   return i;
    }
    return -1;
}
//加入购物车
int addShoppingCart(int goodsNum,int orderNum){
     char goodsID_tmp[12]; char goodsname_tmp[10];int goodsNum_tmp;
    //goods_index指的是商品在goodslist的下标
    //order_index指的是商品在orderForm的下标
    int goods_index,order_index;
    printf("===添加商品至购物车===\n");
    while(1){
        memset(goodsID_tmp,0,12);goodsNum_tmp = 0;
        goods_index = 0;order_index = 0;
        printf("请输入商品ID：(0:quit)");
        scanf("%s",goodsID_tmp);
        if(strcmp(goodsID_tmp,"0") == 0)  {printf("退出挑选商品\n");return orderNum;}
        //查询商品ID是否存在
        goods_index = searchGoodsInfo(goodsID_tmp,goodsNum);
        if(goods_index == -1)    {printf("商品ID不存在，请重新输入！\n");continue;}
        //商品存在，就判断是否已在购物车中
        order_index = searchOrderInfo(goodsID_tmp,orderNum);
        //若不在购物车中，商品信息将记录在新的orderFormItem中
        if(order_index == -1) {
            order_index = orderNum; orderNum++;
            strcpy(orderForm[order_index].goodsID,goodsID_tmp);
            orderForm[order_index].num = 0;
            }
        printf("您选择的商品是：%s\n",goodslist[goods_index].goodsname);
        printf("请输入需要的数量：");
        scanf("%d",&goodsNum_tmp);
        printf("%d",goodsNum_tmp);
        printf("\n");
        //倘若未提交订单的商品数量大于库存，则报错
        if(goodsNum_tmp+orderForm[order_index].num > goodslist[goods_index].repertory){
            printf("订购数目大于库存，请重新输入\n");continue;
        }
        else
            orderForm[order_index].num += goodsNum_tmp;
       }
    return orderNum;
}
//查看购物车
void printShoppingCart(int orderNum){
    printf("当前购物车的内容为:\n");
    for(int i = 0;i < orderNum; i++){
        printf("%d   %s   %d\n",i,orderForm[i].goodsID,orderForm[i].num);
    }
}
//提交订单
void submitOrderForm(int sockfd,int orderNum){
    //发送缓冲区和接收缓冲区
    char send_buf[1024];
    char rcv_buf[1024];
    totalPrice = 0;
    if(orderNum){
        memset(send_buf,0,1024);
        totalPrice = 0;
        strcpy(send_buf,"submitForm");
        write(sockfd,send_buf,sizeof(send_buf));
        write(sockfd,orderForm,sizeof(orderForm));
        memset(rcv_buf,0,1024);
        read(sockfd,rcv_buf,sizeof(rcv_buf));
        printf("%s",rcv_buf);
        //接收价格
        read(sockfd,&totalPrice,sizeof(totalPrice));
        return;
    }
    else{
        printf("orderForm is null。\n");
    }
    return;
}
void quitClient(int sockfd,char *snd_buf){
    printf("客户端即将退出。\n");
    //清空发送缓冲区
    memset(snd_buf,0,1024);
    strcpy(snd_buf,"!q");
    write(sockfd,snd_buf,sizeof(snd_buf));
    close(sockfd);
    printf("客户端已退出。\n");
    exit(0);
}
int main(){
    int sockfd;
    //商品数目
    int goodsNum = 0;
    //订单数目
    int orderNum = 0;
    pid_t cpid;
    char snd_buf[1024];
    char rcv_buf[1024];
    //连接服务器
    serverCon(&sockfd,&cpid);
    printf("客户机%d,sockfd = %d 等待服务器发送商品信息……\n",cpid,sockfd);
    //连接成功后，获取商品信息
    goodsNum = getGoods(sockfd);
    printGoodsAndOpe(goodsNum);
    int oper = 0;
    while (1)
    {
        printf("操作符:");
        scanf("%d",&oper);
        switch (oper)
        {
        case 0:system("clear");printGoodsAndOpe(goodsNum);break;
        case 1:orderNum = addShoppingCart(goodsNum,orderNum);break;
        case 2:printShoppingCart(orderNum);break;
        case 3:
                submitOrderForm(sockfd,orderNum);
                if(!totalPrice) printf("订单提交失败，请稍后再试\n");
                else {
                    printf("订单提交成功！\n");
                    printf("来自服务器:您所选商品价格为：%.2f\n",totalPrice);initOrderInfo();orderNum = 0;
                    printf("系统即将刷新……\n");sleep(10);
                    system("clear");goodsNum = getGoods(sockfd);printGoodsAndOpe(goodsNum);
                    }
                break;
        case 4:quitClient(sockfd,snd_buf);break;
        default:printf("输入操作符号不存在，请重新输入！\n");break;
        }
    }
    printf("客户机%d与服务器sockfd = %d对话结束\n",cpid,sockfd);
    close(sockfd);
}
