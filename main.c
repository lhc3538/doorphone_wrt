#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include <string.h>

#define DEFAULT_PORT 8081
#define MAXLINE 128
//---------------------------
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/soundcard.h>
#define LENGTH 1 	/* 存储秒数 */
#define RATE 22050   /* 采样频率 */
#define SIZE 16      /* 量化位数 */
#define CHANNELS 2  /* 声道数目 */
//--------------------------
#include <pthread.h>
//#include "public.h"

int sock_local,sock_remote;     //socket
struct sockaddr_in addr_local,addr_remote;  //address
int fd_audio;   //audiocard

unsigned char current_recvNum[5],current_sendNum[5]; //current audio package's id
int isbusy_audio;


void initNum()
{
    memset(current_recvNum,0,sizeof(current_recvNum));
    memset(current_sendNum,0,sizeof(current_sendNum));
}

int greaterCurrent(char* num)
{
    int i,j;
    for (i=0;i<5;i++)
    {
        if (num[i]>current_recvNum[i])
        {
            for (j=0;j<5;j++)
                current_recvNum[j] = num[j];
            return 1;
        }
    }
    return 0;
}   //judge pakeage's id ,and update current id

void *sock_thread_local_recv()
{
    unsigned char recvbuf[1024];
    int ret;
    while(1)
    {
        ret = read(sock_local, recvbuf, sizeof(recvbuf));
        if (ret == -1)
            perror("recvfrom err");

        //if(greaterCurrent(recvbuf))
        //{
            //write audio buf to audio-card
            ret = write(fd_audio, recvbuf, sizeof(recvbuf)); // 放音
            if (ret != sizeof(recvbuf))
                perror("wrote wrong number of bytes");
        //}
    }
}

void addNum(char* buf)
{
    int i;
    for (i=0;i<5;i++)
        buf[i] = current_sendNum[i];
    current_sendNum[4]++;
    for (i=4;i>=1;i--)
    {
        if (current_sendNum[i] == 255)
        {
            current_sendNum[i] = 0;
            current_sendNum[i-1]++;
        }
    }
    if (current_sendNum[0] == 255)
        memset(current_sendNum,0,sizeof(current_sendNum));
}   //copy current id to buf,and add current id

void *sock_thread_local_send()
{
    unsigned char sendbuf[1024];
    int ret;
    while(1)
    {
        //read audio from audio-card
        ret = read(fd_audio, sendbuf, sizeof(sendbuf)); // 录音
        //addNum(sendbuf);
        write(sock_local, sendbuf, strlen(sendbuf));
    }
}

void *sock_thread_local()
{
    if ((sock_local = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        perror("create socket_local failed!");

    memset(&addr_local, 0, sizeof(addr_local));
    addr_local.sin_family = AF_INET;
    addr_local.sin_port = htons(8082);
    if(inet_pton(AF_INET, "127.0.0.1", &addr_local.sin_addr) <= 0)
    {
        printf("not a valid IPaddress\n");
        exit(1);
    }
    int yes = 1;
    setsockopt(sock_local , SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
    if(connect(sock_local, (struct sockaddr *)&addr_local, sizeof(addr_local)) == -1)
    {
        perror("connect error");
        exit(1);
    }

    pthread_t thread_local_recv,thread_local_send;
    memset(&thread_local_recv, 0, sizeof(thread_local_recv));
    memset(&thread_local_send, 0, sizeof(thread_local_send));

    if ((pthread_create(&thread_local_recv, NULL, sock_thread_local_recv, NULL)) < 0)
        perror("create sock_thread_local failed!");
    if ((pthread_create(&thread_local_send, NULL, sock_thread_local_send, NULL)) < 0)
        perror("create sock_thread_local failed!");

    if (thread_local_recv != 0)
        pthread_join(thread_local_recv,NULL);//等待线程退出
    if (thread_local_send != 0)
        pthread_join(thread_local_send,NULL);//等待线程退出
}

void *sock_thread_remote()
{
    /*if ((sock_remote = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
        perror("create socket_remote failed!");

    memset(&addr_remote, 0, sizeof(addr_remote));
    addr_remote.sin_family = AF_INET;
    addr_remote.sin_port = htons(8082);
    addr_remote.sin_addr.s_addr = inet_addr("115.159.23.237");

    pthread_t thread_remote_recv,thread_remote_send,thread_remote_heartbeat;
    memset(&thread_remote_recv, 0, sizeof(thread_remote_recv));
    memset(&thread_remote_send, 0, sizeof(thread_remote_send));
    memset(&thread_remote_heartbeat, 0, sizeof(thread_remote_heartbeat));*/


}

/*
*声卡初始化函数
*
*形参：
*/
int initDsp()
{
	int fd;	/* 声音设备的文件描述符 */
	int arg;	/* 用于ioctl调用的参数 */
	int status;   /* 系统调用的返回值 */
	/* 打开声音设备 */
	fd = open("/dev/dsp", O_RDWR);
	if (fd < 0) {
	perror("open of /dev/dsp failed");
	exit(1);
	}
	/* 设置采样时的量化位数 */
	arg = SIZE;
	status = ioctl(fd, SOUND_PCM_WRITE_BITS, &arg);
	if (status == -1)
	perror("SOUND_PCM_WRITE_BITS ioctl failed");
	if (arg != SIZE)
	perror("unable to set sample size");
	/* 设置采样时的声道数目 */
	arg = CHANNELS;
	status = ioctl(fd, SOUND_PCM_WRITE_CHANNELS, &arg);
	if (status == -1)
	perror("SOUND_PCM_WRITE_CHANNELS ioctl failed");
	if (arg != CHANNELS)
	perror("unable to set number of channels");
	/* 设置采样时的采样频率 */
	arg = RATE;
	status = ioctl(fd, SOUND_PCM_WRITE_RATE, &arg);
	if (status == -1)
	perror("SOUND_PCM_WRITE_WRITE ioctl failed");

	return fd;
}

int main()
{
    initNum();

    fd_audio = initDsp();   //init audiocard
    isbusy_audio = 0;

    pthread_t thread_local,thread_remote;
    memset(&thread_local, 0, sizeof(thread_local));
    //memset(&thread_remote, 0, sizeof(thread_remote));

    if ((pthread_create(&thread_local, NULL, sock_thread_local, NULL)) < 0)
        perror("create sock_thread_local failed!");
    //if ((pthread_create(&thread_remote, NULL, sock_thread_remote, NULL)) < 0)
     //   perror("create sock_thread_remote failed!");


    if (thread_local != 0)
        pthread_join(thread_local,NULL);//等待线程退出
    //if (thread_remote != 0)
    //    pthread_join(thread_remote,NULL);//等待线程退出

    close(sock_local);
    //close(sock_remote);

    return 0;
}
