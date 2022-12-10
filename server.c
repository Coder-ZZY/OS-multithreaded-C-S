#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
//最大连接数
#define maxLinkNum 5
//货物清单中的最大数目
#define MAX_GOODS_ITEMS 100
//订单中的商品最大数量
#define MAX_ORDERFORM_ITEMS 10
//缓冲区大小
#define BUF_SIZE 200
//订单结构体
typedef struct orderFormItem{
    char goodsID[12];
    int num;
}orderFormItem;
//订单
orderFormItem orderForm[MAX_ORDERFORM_ITEMS];
//商品清单结构体
typedef struct goodsItems
{
    char type[20];
    char goodsID[12];
    char goodsname[10];
    float price;
    int repertory;
}goodsItems;
//时间戳结构体
typedef struct {
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
}Time_YMD_HMS;
//记录服务器端的socket和连接的多个客户端的socket
int client_sockfd[maxLinkNum];
//记录客户端的pid
pid_t client_pid[maxLinkNum];
//命名套接字
int server_sockfd = -1;
//商品信息
goodsItems goodslist[100];
//当前连接数
int curLink;
//是否允许写
int writePermission = 1;
//连接数资源信号量
sem_t mutex;
//写入文件信号量
sem_t writeNum;
//商品总数
int goodsNum = 0;
//订单总数
int orderNum = 0;
//销售号
int sellid = 0;
//按照yyyy-mm-dd格式获取当前日期
char* getNowTime()
{
	Time_YMD_HMS *curDate =(Time_YMD_HMS *)malloc(sizeof(Time_YMD_HMS));
	char *timeBuf =(char *)malloc(BUF_SIZE);
	bzero(timeBuf,BUF_SIZE);
	bzero(curDate,sizeof(Time_YMD_HMS));
	time_t now;
	struct tm *timeNow;
	time(&now);
	timeNow = localtime(&now);
	curDate->year=timeNow->tm_year+1900;
	curDate->month=timeNow->tm_mon+1;
	curDate->day=timeNow->tm_mday;
	curDate->hour=timeNow->tm_hour;
	curDate->minute=timeNow->tm_min;
	curDate->second=timeNow->tm_sec;
	// yyyy-MM-dd HH:mm:ss
	if(curDate->second < 10)
		sprintf(timeBuf, "%d-%d-%d %d:%d:0%d",curDate->year,curDate->month,curDate->day,
								curDate->hour,curDate->minute,curDate->second);
	else
		sprintf(timeBuf, "%d-%d-%d %d:%d:%d",curDate->year,curDate->month,curDate->day,
								curDate->hour,curDate->minute,curDate->second);
	free(curDate);
	return timeBuf;
}
//Linux stdlib.h头文件不包含itoa转换字符串函数，我们自己实现它
char *itoa(int value, char str[], int radix)
{
	char tmp_buff[33] = {0};
	char ch;
	int i = 0, j = 0;
	/* 超出转换进制范围，退出 */
	if ((radix < 2) || (radix > 36))
	{
		printf("radix err...\n");
		return NULL;
	}
	/* 不是10进制的负数，退出函数 */
	if ((value < 0) && (radix != 10))
	{
		printf("value err...\n");
		return NULL;
	}
	/* 10进制支持负数转换 */
	if ((value < 0) && (radix == 10))
	{
		value = -value;
		*str++ = '-';
	}
	/* 转换 */
	while (value)
	{
		ch = value % radix;
		value /= radix;
		
		if (ch < 10)
			tmp_buff[i++] = ch + '0';
		else 
			tmp_buff[i++] = ch + 'a' - 10;
	}	
	/* 逆序 */
	for (j=i-1; j>=0; j--)
	{
		*str++ = tmp_buff[j];
	}
	*str = '\0';		// 加上结束符
	return str;
}
//停止服务信息
char stopmsg[100];
void *serverQuit(void *arg){
//客户机与服务器断开连接，释放资源处理函数
    char *msg = "服务器即将关闭\n";
    while (1)
    {
        //服务器发送消息缓存区
        if(strcmp(stopmsg,"quit")==0){
            printf("%s",msg);
            for(int i = 0; i < maxLinkNum; i++){
                if(client_sockfd[i] != -1){
                    write(client_sockfd[i],msg,sizeof(msg));
                }
            }
            close(server_sockfd);
            sem_destroy(&mutex);
            exit(0);
        }
    } 
}
//从文件中读取商品信息
void initGoodsInfo(){
    goodsNum = 0;
    for(int i = 0; i < MAX_GOODS_ITEMS;i++){
        memset(goodslist[i].goodsID,0,12);
        memset(goodslist[i].type,0,20);
        memset(goodslist[i].goodsname,0,10);
        goodslist[i].price = 0;goodslist[i].repertory = 0;
    }
}
void getGoodsInfo(){
    FILE* fp;
    initGoodsInfo();
    if ((fp = fopen("/home/zzy/桌面/VSWorkSpace/goods.txt", "r")) == NULL)
	{
		printf("Can't open the goodsfile!\n"); exit(0);
	}
    else{
        while (!feof(fp))
        {
            //从goods.txt文件中读取数据保存到结构体数组中
            fscanf(fp,"%s",goodslist[goodsNum].type);
            fscanf(fp,"%s",goodslist[goodsNum].goodsID);
            fscanf(fp,"%s",goodslist[goodsNum].goodsname);
            fscanf(fp,"%f%d\n",&goodslist[goodsNum].price,&goodslist[goodsNum].repertory);
           // fscanf(fp,"%s%s%s%f%d\n",
           //     goodslist[goodsNum].type,goodslist[goodsNum].goodsID,goodslist[goodsNum].goodsname,&goodslist[goodsNum].price,&goodslist[goodsNum].repertory);
            goodsNum++;
        }
        fclose(fp);
        //打印商品信息
    //      for(int i = 0; i < goodsNum; i++ ){
    //         printf("%s %s %s %.2f  %d\n",
    //               goodslist[i].type,goodslist[i].goodsID,goodslist[i].goodsname,goodslist[i].price,goodslist[i].repertory);
    //   }
         return;
    }
}
//初始化订单信息
void initOrderInfo(){
    orderNum = 0;
    for(int i = 0; i < MAX_ORDERFORM_ITEMS;i++){
        memset(orderForm[i].goodsID,0,12);
        orderForm[i].num = 0;
    }
}
//获取订单信息
void getOrderInfo(){
    for(int i = 0; i < MAX_ORDERFORM_ITEMS;i++){
        if(orderForm[i].num == 0)   continue;
        else orderNum++;
    }
}
//根据商品ID查goodslist下标
int searchGoodsInfo(char *goodsID_tmp){
    //找到商品ID对应的goodslist下标返回，找不到返回-1
    for(int i = 0;i < MAX_GOODS_ITEMS;i++){
        if(goodslist[i].price == 0) return -1;
        if(strncmp(goodsID_tmp,goodslist[i].goodsID,6) == 0)   return i;
    }
    return -1;
}
//将销售信息写入库存中
void writeList(long *write_flag){
    printf("正在将订单信息写入内存……\n");
    int goods_index = searchGoodsInfo(orderForm[*write_flag].goodsID);
    if(goodslist[goods_index].repertory >= orderForm[*write_flag].num){
        goodslist[goods_index].repertory = goodslist[goods_index].repertory-orderForm[*write_flag].num;
    }
    else *write_flag = -1;
    writePermission++;
   sem_post(&writeNum);
}
//将销售信息写入文件中
void writeOrder(long *n){
    FILE *fp1,*fp2;
    printf("正在将文件写入数据库中……\n");
    //写入goods.txt
    fp1 = fopen("/home/zzy/桌面/VSWorkSpace/goods.txt", "w");
    fp2 = fopen("/home/zzy/桌面/VSWorkSpace/sell.txt","a");
    if(fp1== NULL || fp2 == NULL){
        printf("Can't open the file!\n");*n = -1;return;
    }
    else{
        for(int i = 0;i < goodsNum;i++){
                    fprintf(fp1,"%s %s %s %.2f %d\n",
                    goodslist[i].type,goodslist[i].goodsID,goodslist[i].goodsname,goodslist[i].price,goodslist[i].repertory);
        }
        for(int i = 0;i < orderNum;i++){
                    printf("%d",orderNum);
                    int goods_index = 0;
                    sellid++;
                    goods_index = searchGoodsInfo(orderForm[i].goodsID);
                    fprintf(fp2,"%s    %d              %d        %s   %d   %.2f\n",
                        getNowTime(),sellid,client_pid[*n],goodslist[goods_index].goodsID,orderForm[i].num,(float)goodslist[goods_index].price*orderForm[i].num);
        }
    }
    fclose(fp1);fclose(fp2);
    writePermission++;
    sem_post(&writeNum);
}
//计算总价
float calcTotalPrice(long n){
    int oderFormNum = 0;
    int repertory_tmp = 0;
    int goods_index;
    //用于返回判断writeOrder线程是否正确修改文件
    //用于传递要修改的商品下标
    long write_flag = -1;
    float totalPrice = 0;
    for(int i = 0; i < MAX_ORDERFORM_ITEMS; i++){
        if(orderForm[i].num == 0)   continue;
        goods_index = 0;
        goods_index = searchGoodsInfo(orderForm[i].goodsID);
        write_flag = i;
        //repertory_tmp = goodslist[goods_index].repertory;
        //若有其他订单正在处理，writeOrder线程将会阻塞。
        if(!writePermission)    printf("当前有其他订单正在提交，请稍候……\n");
        writePermission--;
        sem_wait(&writeNum);
        writeList(&write_flag);
        if(write_flag == -1) {printf("库存修改失败!\n");return 0;}
        totalPrice += (float)goodslist[goods_index].price*orderForm[i].num;
    }
    //打印商品信息
    for(int i = 0; i < goodsNum; i++ ){
            printf("%s %s %s %.2f  %d\n",
                  goodslist[i].type,goodslist[i].goodsID,goodslist[i].goodsname,goodslist[i].price,goodslist[i].repertory);
      }
    if(!writePermission)    printf("当前有其他订单正在提交，请稍候……\n");
    writePermission--;
    sem_wait(&writeNum);
    writeOrder(&n);
    initOrderInfo();
    return  totalPrice;
}
//卖家线程
void seller(long n){
    int i = 0;
    int retval;
    //接收缓冲区
    char rcv_buf[1024];
    //发送缓冲区
    char send_buf[1024];
    float totalPrice = 0;
    int client_len = 0;
    int rcv_num;
    pthread_t tid;
    tid = pthread_self();
    printf("服务器seller线程id=%lu使用套接字%d,n=%ld与客户端进行对话开始……\n",tid,client_sockfd[n],n);
    printf("服务器seller线程向该客户端发送客户端商品信息\n");
    //把商品信息发送给客户端
    write(client_sockfd[n],goodslist,sizeof(goodslist));
    printf("服务器信息已发送\n");
    //监听客户端是否断开服务或发来订单
    char tidString[10];
    do{
            printf("等待客户端发来信号……\n");
            sleep(2);
            //将接收缓冲区重置
            memset(rcv_buf,0,1024);
            rcv_num = read(client_sockfd[n],rcv_buf,sizeof(rcv_buf));
            if(rcv_num == 0)    continue;
            else if(strcmp(rcv_buf,"submitForm")== 0){
                initOrderInfo();
                read(client_sockfd[n],orderForm,sizeof(orderForm));
                getOrderInfo();
                memset(send_buf,0,1024);
                memset(tidString,0,10);
                itoa(tid,tidString,10);
                sprintf(send_buf, "服务器seller线程id=%s已经收到订单，正在处理……\n",tidString); 
                write(client_sockfd[n],send_buf,sizeof(send_buf));
                totalPrice = calcTotalPrice(n);
               printf("总价为%.2f\n",totalPrice);
                write(client_sockfd[n],&totalPrice,sizeof(totalPrice));
                printf("截至订单号%d处理完成",sellid);
                if(totalPrice){
                getGoodsInfo();
                write(client_sockfd[n],goodslist,sizeof(goodslist));
                }
                }
            else if(strcmp(rcv_buf,"!q") == 0){
                printf("客户端发来退出请求。\n");break;         
            }
    }while (strncmp(rcv_buf,"!q",2) != 0);   
    printf("服务器线程 tid = %lu，套接字%d,n = %ld与客户机对话完毕\n",tid,client_sockfd[n],n);
    close(client_sockfd[n]);
    client_sockfd[n] = -1;
    curLink--;
    printf("线程tid=%lu即将销毁，当前server连接数为%d\n",tid,curLink);
    sem_post(&mutex);
    pthread_exit(&retval);
}
int main(){
    char recv_buf[1024];
    char send_buf[1024];
    int client_len = 0;
    pthread_t thread;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    server_sockfd = socket(AF_INET,SOCK_STREAM,0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr= htonl(INADDR_ANY);
    server_addr.sin_port = htons(8081);
    long i = 0;
    int ret = bind(server_sockfd,(struct sockaddr *)&server_addr,sizeof(server_addr));
    printf("ret = %d\n",ret);
    printf("服务器开始监听……\n");
    listen(server_sockfd,maxLinkNum);
    signal(SIGCHLD,SIG_IGN);
    printf("输入!q，服务结束。\n");
    pthread_create(&thread,NULL,serverQuit,NULL);
    for (int i = 0; i < maxLinkNum; i++)
        client_sockfd[i] = -1;
     printf("当前连接数为0(<=%d)\n",maxLinkNum);
     //初始化信号量
    sem_init(&mutex,0,maxLinkNum);
    sem_init(&writeNum,0,writePermission);
    while (1)
    {
        for(i = 0; i < maxLinkNum;){
            if(client_sockfd[i] == -1 )    break;
            i++;
        }
        // for(int j = 0; j < maxLinkNum; j++)
        //     printf("client_sockfd[%d]=%d\n",j,client_sockfd[j]);
        if(i == maxLinkNum){
            printf("已到达最大连接数，请等待其他用户释放连接……\n");
            sem_wait(&mutex);
            continue;
        }
        client_len = sizeof(client_addr);
        printf("服务器开始accept……i = %ld\n",i);
        getGoodsInfo();
        client_sockfd[i] = accept(server_sockfd,(struct sockaddr *)&client_addr,&client_len);
        curLink++;
        printf("当前连接数为%d(<=%d)\n",curLink,maxLinkNum);
        read(client_sockfd[i],&client_pid[i],sizeof(client_pid[i]));
        printf("连接来自：连接套接字号=%d,IP Addr = %s,Port = %d\n",client_sockfd[i],inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));
        pthread_create(malloc(sizeof(pthread_t)),NULL,(void *)(&seller),(void *)i);
        sem_wait(&mutex);
    }
}