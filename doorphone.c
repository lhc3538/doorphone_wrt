#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include <string.h>

#define DEFAULT_PORT 8081
#define BUFLEN 1024
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

typedef struct
{
    unsigned long long id; //package's id
    unsigned char data[BUFLEN];    //audio data
} Package;

int fd_audio;   //audiocard
int isbusy_audio;   //isn't audio busy
pthread_mutex_t mutex_isbusy_audio,mutex_isbusy_audio;    //Mutex

unsigned long long current_recv_id,current_send_id;

pthread_mutex_t mutex_local_recv,mutex_local_send,lock_local;    //Mutex
pthread_cond_t cond_local;

void resetID()
{
    current_recv_id = 0;
    pthread_mutex_lock(&mutex_local_send);   //lock
    current_send_id = 0;
    pthread_mutex_unlock(&mutex_local_send);   //unlock

    pthread_mutex_lock(&mutex_isbusy_audio);   //lock
    isbusy_audio = 0;
    pthread_mutex_unlock(&mutex_isbusy_audio);   //unlock
}

#include "local.h"
#include "remote.h"

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
    current_recv_id = 0;
    current_send_id = 0;
    isbusy_audio = 0;

    fd_audio = initDsp();   //init audiocard

    sock_thread_local();

    close(fd_audio);
    return 0;
}
