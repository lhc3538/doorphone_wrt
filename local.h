int sock_local;
struct sockaddr_in addr_local;


void *sock_thread_local_timer()
{
    Package pack;
    int pack_len = sizeof(Package);
    char buf_sock[pack_len];
    pack.id = 0;
    pack.data[0] = 'H';
    memcpy(buf_sock,&pack,pack_len);

    unsigned long long tempId;
    while(1)
    {
        pthread_mutex_lock(&mutex_local_recv);   //lock
        tempId = current_recv_id;
        pthread_mutex_unlock(&mutex_local_recv);     //unlock

        sleep(2);

        pthread_mutex_lock(&mutex_local_recv);   //lock
        if (current_recv_id == tempId)  //drop the connect
        {
            printf("drop connect\n");
            resetID();
        }
        if (current_recv_id == 0)
        {
            write(sock_local, buf_sock, pack_len);  //send heart's package
        }
        pthread_mutex_unlock(&mutex_local_recv);     //unlock
    }
}

void *sock_thread_local_recv()
{
    Package pack;
    int pack_len = sizeof(Package);
    char buf_sock[pack_len];
    int ret;
    while(1)
    {
        pthread_mutex_lock(&mutex_isbusy_audio);
        if (isbusy_audio == 2)  //be used in remote
        {
            pthread_mutex_unlock(&mutex_isbusy_audio);
            continue;
        }
        else
            pthread_mutex_unlock(&mutex_isbusy_audio);

        ret = read(sock_local, buf_sock, pack_len);
        if (ret == -1)
        {
            perror("recvfrom err");
            continue;
        }

        memcpy(&pack,buf_sock,pack_len);
        printf("recv%llu:%s\n",pack.id,pack.data);

        //write audio buf to audio-card
        pthread_mutex_lock(&mutex_local_recv);   //lock
        if (pack.id > current_recv_id)
        {
            pthread_mutex_lock(&mutex_isbusy_audio);
            if (isbusy_audio == 0)
                isbusy_audio = 1;   //set be used in local
            pthread_mutex_unlock(&mutex_isbusy_audio);   //unlock

            pthread_mutex_lock(&mutex_local_send);
            pthread_cond_signal(&cond_local);   //open send
            pthread_mutex_unlock(&mutex_local_send);   //unlock


            current_recv_id = pack.id;
            pthread_mutex_unlock(&mutex_local_recv);   //unlock

            ret = write(fd_audio, pack.data, BUFLEN); // 放音
            if (ret != BUFLEN)
                perror("wrote wrong number of bytes");
        }
        else
            pthread_mutex_unlock(&mutex_local_recv);   //unlock

    }
}


void *sock_thread_local_send()
{
    Package pack;
    int pack_len = sizeof(Package);
    char buf_sock[pack_len];
    int ret;
    while(1)
    {
        pthread_mutex_lock(&mutex_isbusy_audio);
        if (isbusy_audio == 2)
        {
            pthread_mutex_unlock(&mutex_isbusy_audio);
            continue;
        }
        else
            pthread_mutex_unlock(&mutex_isbusy_audio);

        pthread_mutex_lock(&mutex_local_send);
        while(current_send_id == 0)
        {
            pthread_cond_wait(&cond_local,&mutex_local_send);   //wait connnect
            current_send_id = 1;
        }
        pack.id = current_send_id++;
        pthread_mutex_unlock(&mutex_local_send);
        //read audio from audio-card
        ret = read(fd_audio, pack.data, BUFLEN); // 录音
        memcpy(buf_sock,&pack,pack_len);
        printf("send%llu:%s\n",pack.id,pack.data);
        //addNum(sendbuf);
        write(sock_local, buf_sock, pack_len);
    }
}

void sock_thread_local()
{
    if ((sock_local = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        perror("create socket_local failed!");

    memset(&addr_local, 0, sizeof(addr_local));
    addr_local.sin_family = AF_INET;
    addr_local.sin_port = htons(8082);
    if(inet_pton(AF_INET, "192.168.0.80", &addr_local.sin_addr) <= 0)
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

    pthread_mutex_init(&mutex_local_recv,NULL);  //init Mutex
    pthread_mutex_init(&mutex_local_send,NULL);  //init Mutex
    pthread_mutex_init(&mutex_isbusy_audio,NULL);  //init Mutex
    pthread_cond_init(&cond_local,NULL);  //init Cond

    pthread_t thread_local_recv,thread_local_send,thread_local_timer;
    memset(&thread_local_recv, 0, sizeof(thread_local_recv));
    memset(&thread_local_send, 0, sizeof(thread_local_send));
    memset(&thread_local_timer, 0, sizeof(thread_local_timer));

    if ((pthread_create(&thread_local_recv, NULL, sock_thread_local_recv, NULL)) < 0)
        perror("create sock_thread_local failed!");
    if ((pthread_create(&thread_local_send, NULL, sock_thread_local_send, NULL)) < 0)
        perror("create sock_thread_local failed!");
    if ((pthread_create(&thread_local_timer, NULL, sock_thread_local_timer, NULL)) < 0)
        perror("create sock_thread_local failed!");

    if (thread_local_recv != 0)
        pthread_join(thread_local_recv,NULL);//等待线程退出
    if (thread_local_send != 0)
        pthread_join(thread_local_send,NULL);//等待线程退出
    if (thread_local_timer != 0)
        pthread_join(thread_local_timer,NULL);//等待线程退出

    pthread_mutex_destroy(&mutex_local_recv);    //destory mutex
    pthread_mutex_destroy(&mutex_local_send);    //destory mutex
    pthread_mutex_destroy(&mutex_isbusy_audio);    //destory mutex
    pthread_cond_destroy(&cond_local);   //destory cond

    close(sock_local);  //close socket

}
