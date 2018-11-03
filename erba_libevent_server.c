
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include <arpa/inet.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>

//gcc tcp.c -o tcp -levent

char state[50];

//str is outbox file reading buffer used to write to erba
//must be big supposing bigger files with multiple etb/etx
char str[15000];

//to receive multiple etb-etx
char recv_buf[15000];

int sample_id;
char cr[2];
char stx[2];
char lf[2];
char etx[2];
char ret[3];
char inbox[100];
char outbox[100];
char delete_target[100];
int file_counter;

//static void
//echo_read_cb(struct bufferevent *bev, void *ctx)
//{
        ///* This callback is invoked when there is data to read on bev. */
        //struct evbuffer *input = bufferevent_get_input(bev);
        //struct evbuffer *output = bufferevent_get_output(bev);

        ///* Copy all the data from the input buffer to the output buffer. */
        //evbuffer_add_buffer(output, input);
//}

/*
void checksum(char* str,char* ret)
{
	//sum(int) character(char) one by one
	//devide each sum by 256 every time, take remonder(it is same as taking right most eight bits)
	//start after STX, include last CR and ETX for erba
	//get right 4 bit and left 4 bit in upper case hex
	int sum=0;
	char right,left;
	int len=strlen(str);
	for(int i=0;i<len;i++)
	{
		sum=(sum+str[i])%256;
	}
	
	right=sum&0b00001111;
	left=toupper(sum&0b11110000)>>4;
	sprintf(ret,"%X%X",left,right);
	ret[2]=0;
}
*/
/*
void checksum(char* str,char* ret)
{
	char sum=0;
	bzero(ret,sizeof(ret));
	int len=strlen(str);
	for(int i=0;i<len;i++)
	{
		sum=sum+str[i];
	}
	sprintf(ret,"%02X",sum);
	ret[2]=0;
}
* */


void alarm_to_reset (int sig)
{
	//printf("Alarm event. signal number is:%d\n",sig);
	//printf("Timer over. reseting state\n");
	//printf("prevous state:%s\n",state);
	//EOT needs to be sent if ACK is expected but not received
	if (strcmp(state,"ACK_RECEIVE_READY")==0 || strcmp(state,"SECOND_ACK_RECEIVE_READY")==0)
	{
		strcpy(state,"SEND_EOT");
		//char eot[2];
		//eot[0]=4;
		//eot[1]=0;
		
		
			//struct evbuffer * wr=evbuffer_new();
			//int added=evbuffer_add(wr,eot,1);
			
			//struct evbuffer *output = bufferevent_get_output(bev);
			//evbuffer_add_buffer(output,wr);
			
		//printf("ACK expected but nothing received\n...So EOT sent\n");
	}
	else
	{
		strcpy(state,"ENQ_SEND_RECV_READY");
	}
	//printf("state is now reset to :%s\n",state);
	//read important.
	//signal (sig, alarm_to_reset); //not required??
}


int start_alarm(void)
{
	struct itimerval old;
	struct itimerval new;
	
  new.it_interval.tv_sec = 0; 		//for repeat
  new.it_interval.tv_usec = 0; 
  new.it_value.tv_sec = 10;			//first time
  new.it_value.tv_usec = 0;
   
  old.it_interval.tv_sec = 0;
  old.it_interval.tv_usec = 0;
  old.it_value.tv_sec = 0;
  old.it_value.tv_usec = 0;
   
  if (setitimer (ITIMER_REAL, &new, &old) < 0)
      printf("timer init failed\n");
  else
      printf("timer for 10 second is started\n");
  return EXIT_SUCCESS;
}

int stop_alarm(void)
{
	struct itimerval old;
	struct itimerval new;
	
  new.it_interval.tv_sec = 0; 		//for repeat
  new.it_interval.tv_usec = 0; 
  new.it_value.tv_sec = 00;			//first time
  new.it_value.tv_usec = 0;
   
  old.it_interval.tv_sec = 0;
  old.it_interval.tv_usec = 0;
  old.it_value.tv_sec = 0;
  old.it_value.tv_usec = 0;
   
  if (setitimer (ITIMER_REAL, &new, &old) < 0)
      printf("timer init failed\n");
  else
      printf("old timer stopped \n");
  return EXIT_SUCCESS;
}


char* replace_char(char* buf,char* modified)
{
	int str_length=strlen(buf);
	for (int j = 0; j < str_length; j ++) 
	{
		if(buf[j]==2){strcat(modified,"<STX>");}
		else if(buf[j]==3){strcat(modified,"<ETX>");}
		else if(buf[j]==4){strcat(modified,"<EOT>");}
		else if(buf[j]==5){strcat(modified,"<ENQ>");}
		else if(buf[j]==6){strcat(modified,"<ACK>");}
		else if(buf[j]==10){strcat(modified,"<LF>");}
		else if(buf[j]==13){strcat(modified,"<CR>");}
		else if(buf[j]==23){strcat(modified,"<ETB>");}
		else{
				char temp[2];
				temp[0]=buf[j];
				temp[1]=0;
				strcat(modified,temp);
			}
    }
    modified[strlen(modified)]=0;
}


void current_date_time(char* buffer)
{
	time_t rawtime;
	struct tm * timeinfo;
	//char buffer [80];
	time (&rawtime);
	timeinfo = localtime (&rawtime);
	strftime(buffer,80,"%F:%T",timeinfo);
	//return buffer;
}

void filepath(char* prefix)
{
	char dt[200];
	current_date_time(dt);
	strcat(prefix,dt);
	//following to ensure that multiple files of same name are not created in one second
	sprintf(prefix,"%s:%02d",prefix,file_counter);
	if(file_counter>=99)
	{
		file_counter=1;
	}
	else
	{
		file_counter=file_counter+1;
	}
}



static void echo_read_cb(struct bufferevent *bev, void *ctx)
{
	char buf[5000];
	bzero(buf,5000);
	
	char modified[5000];
	bzero(modified,5000);
	
	struct evbuffer *input = bufferevent_get_input(bev);
	int len = evbuffer_remove(input, buf, 5000);
	
	replace_char(buf,modified);
	printf("-----------------------------\nreceived data:\n%s\nlength of data=%d\n",modified,len);

	//useful to debug
	//for (int j = 0; j < len; j ++) {
        //printf(" %2x", buf[j]);
    //}
    //putchar('\n');

	printf("current state:%s\n",state);
	
	if (strcmp(state,"ENQ_SEND_RECV_READY")==0)
	{
		stop_alarm();	
		char enq[2];
		enq[0]=5;
		enq[1]=0;
		if(strcmp(buf,enq)==0)
		{
			char ack[2];
			ack[0]=6;
			ack[1]=0;
			
			printf("ENQ received\n");
			struct evbuffer * wr=evbuffer_new();
			int added=evbuffer_add(wr,ack,1);
			
			struct evbuffer *output = bufferevent_get_output(bev);
			evbuffer_add_buffer(output,wr);
        			
			printf("ACK Sent. ready to receive frame \n");
			strcpy(state,"FRAME_RECEIVE_READY");
			printf("state is now changed to :%s\n",state);			
			start_alarm();
		}
		else
		{
			printf("ENQ expected, but ENQ not received (length of data=%d)(data=[%s])\n",len,buf);
			strcpy(state,"ENQ_SEND_RECV_READY");
			//shutdown (sockfd,2);
			//close(sockfd); 
			//close(connfd); 
			//sleep(4);
		}
	}
	else if(strcmp(state,"FRAME_RECEIVE_READY")==0)
	{	
		stop_alarm();	
		if(len>=6)
		{
			if(buf[2]=='H' && buf[0]==2)
			{
				printf("<STX> and Header received. So zeroing recv_buf\n");
				bzero(recv_buf,sizeof(recv_buf));
			}
			
			if(buf[len-5]==23)
			{				
				printf("Frame STX to ETB-checksum-cr-lf received\n");
				
				//make sure recv_buf is zeroed
				strcat(recv_buf,buf);
				printf("<ETB> so no file writted. only recv_buf extended\n");
				//char fname[200];
				//bzero(fname,200);
				//strcpy(fname,inbox);
				//filepath(fname);
				//FILE* frm=fopen(fname,"w");
				//fwrite(buf,len,1,frm);
				//fclose(frm);
				
			char ack[2];
			ack[0]=6;
			ack[1]=0;
							
			struct evbuffer * wr=evbuffer_new();
			int added=evbuffer_add(wr,ack,1);
			
			struct evbuffer *output = bufferevent_get_output(bev);
			evbuffer_add_buffer(output,wr);
							
				//printf("ACK Sent to receive next frame \n");
				strcpy(state,"FRAME_RECEIVE_READY");
				printf("state is now changed to :%s\n",state);			
				start_alarm();
			}
			else if(buf[len-5]==3)
			{
				char x=6;
				printf("Frame STX to ETX-checksum-cr-lf received\n");
				strcat(recv_buf,buf);
				printf("<ETX> so recv_buf extended. Now, inbox file needs to be written\n");
				char fname[200];
				bzero(fname,200);
				strcpy(fname,"/root/inbox/");
				filepath(fname);
				FILE* frm=fopen(fname,"w");
				
				fwrite(recv_buf,strlen(recv_buf),1,frm);
				fclose(frm);
				//ensure recv_buf is zeroed for next use
				bzero(recv_buf,sizeof(recv_buf));


			char ack[2];
			ack[0]=6;
			ack[1]=0;
							
			struct evbuffer * wr=evbuffer_new();
			int added=evbuffer_add(wr,ack,1);
			
			struct evbuffer *output = bufferevent_get_output(bev);
			evbuffer_add_buffer(output,wr);
			
				printf("ACK sent to receive EOT\n");
				strcpy(state,"EOT_READY");
				printf("state is now changed to :%s\n",state);			
				start_alarm();
			}
			else
			{
				printf("Frame received donot have ETX/ETB as fifth from last (length of data=%d)(data=[%s])\n",len,buf);
				strcpy(state,"ENQ_SEND_RECV_READY");
			}
		}
	}
	else if(strcmp(state,"ACK_RECEIVE_READY")==0)
	{
		stop_alarm();	
		char ack[2];
		ack[0]=6;
		ack[1]=0;
		if(strcmp(buf,ack)==0)
		{
			printf("ACK received\n");
							
			struct evbuffer * wr=evbuffer_new();
			//str is global variable
			//prepared by write_cb
			int added=evbuffer_add(wr,str,strlen(str));
			
			struct evbuffer *output = bufferevent_get_output(bev);
			evbuffer_add_buffer(output,wr);
			
			printf("Message Sent:\n%s\n",str);
			strcpy(state,"SECOND_ACK_RECEIVE_READY");
			printf("state is now changed to :%s\n",state);			
			start_alarm();
		}
		else
		{
			printf("ACK expected, but something other than ACK received (length of data=%d)(data=[%s])\n",len,buf);
			char eot[2];
			eot[0]=4;
			eot[1]=0;
			
										
			struct evbuffer * wr=evbuffer_new();
			int added=evbuffer_add(wr,eot,1);
			
			struct evbuffer *output = bufferevent_get_output(bev);
			evbuffer_add_buffer(output,wr);
			
			printf("EOT sent\n");
			strcpy(state,"ENQ_SEND_RECV_READY");
			printf("state is now reset to :%s\n",state);			
		}
	}
	else if(strcmp(state,"SECOND_ACK_RECEIVE_READY")==0)
	{	
		stop_alarm();	
		char ack[2];
		ack[0]=6;
		ack[1]=0;
		if(strcmp(buf,ack)==0)
		{
			printf("Second ACK received. ending transection\n");
			char eot[2];
			eot[0]=4;
			eot[1]=0;
			
			struct evbuffer * wr=evbuffer_new();
			int added=evbuffer_add(wr,eot,1);
			
			struct evbuffer *output = bufferevent_get_output(bev);
			evbuffer_add_buffer(output,wr);
			
			printf("Trying to delete <%s>\n",delete_target);			
			if(remove(delete_target)==0)
			{
				printf("Deleted <%s>\n",delete_target);			
			}
			else
			{
				printf("Failed to delete <%s>\n",delete_target);			
			}
			
			printf("EOT sent\n");
			strcpy(state,"ENQ_SEND_RECV_READY");
			printf("state is now changed to :%s\n",state);			
		}
		else
		{
			printf("second ACK expected, but ACK not received (length of data=%d)(data=[%s])\n",len,buf);
			char eot[2];
			eot[0]=4;
			eot[1]=0;
			
			struct evbuffer * wr=evbuffer_new();
			int added=evbuffer_add(wr,eot,1);
			
			struct evbuffer *output = bufferevent_get_output(bev);
			evbuffer_add_buffer(output,wr);
			
			
			printf("EOT sent\n");
			strcpy(state,"ENQ_SEND_RECV_READY");
			printf("state is now reset to :%s\n",state);	
		}
	}

	else if(strcmp(state,"EOT_READY")==0)
	{
		stop_alarm();			
		char eot[2];
		eot[0]=4;
		eot[1]=0;
		if(strcmp(buf,eot)==0)
		{
			printf("EOT received\n");
			printf("Sending  Nothing. ready for ENQ\n");
			strcpy(state,"ENQ_SEND_RECV_READY");
			printf("state is now changed to :%s\n",state);						
		}
		else
		{
			printf("EOT expected, but EOT not received (length of data=%d)(data=[%s])\n",len,buf);
			printf("Sending  Nothing. ready for ENQ\n");
			strcpy(state,"ENQ_SEND_RECV_READY");
			printf("state is now changed to :%s\n",state);						
		}
	}	
	else
	{
		printf("state unexpected by this function (length of data=%d)(data=[%s])(state=%s)\n",len,buf,state);
		//state may be used by some other callback function. donot touch it here
		//forexample SEND_EOT state when ACK is delayed
	}
	printf("-----------------------------\n");	
}




//write initiated by server
static void echo_write_cb (evutil_socket_t fd, short what, void *bev)
{
	printf("================================\n");	
	
	char buf[5000];
	bzero(buf,5000);
	
	char modified[5000];
	bzero(modified,5000);
	
	//if ENQ ready , see if anything in outbox, if yes then do something
	if(strcmp(state,"ENQ_SEND_RECV_READY")==0)
	{	
		stop_alarm();
	    DIR *d;
		struct dirent *dir;
		d = opendir(outbox);
		if(d)
		{
			while(dir = readdir(d))
			{
				if(dir->d_name[0]!='.')
				{
					printf("Found <%s> in folder <%s>\n",dir->d_name,outbox);				
					char fullpath[200];
					bzero(fullpath,200);
					strcpy(fullpath,outbox);
					strcat(fullpath,dir->d_name);
					printf("Reading <%s>\n",fullpath);				
					
					
					FILE* fp=fopen(fullpath,"r");
					fread(buf,5000,1,fp);
					//fread donot return number of bytes returned, be careful
					//so, how string is identified?
					//so bzero is must
					//buf[strlen(buf)]=0;
					printf("data: %s\n",buf);
					replace_char(buf,modified);
					printf("file content:\n%s\nlength of data=%d\n",modified,strlen(buf));
					fclose(fp);
					bzero(str,sizeof(str));
					strcpy(str,buf);

					char enq[2];
					enq[0]=5;
					enq[1]=0;
			
						struct evbuffer * wr=evbuffer_new();
						int added=evbuffer_add(wr,enq,1);
			
						struct evbuffer *output = bufferevent_get_output(bev);
						evbuffer_add_buffer(output,wr);
			
					printf("ENQ Sent. ready to receive ACK \n");
					strcpy(state,"ACK_RECEIVE_READY");
					printf("state is now changed to :%s\n",state);	
					
					bzero(delete_target,sizeof(delete_target));
					strcpy(delete_target,fullpath);
					printf("<%s> is now set as delete target\n",delete_target);	
										
					break;
				}
			}
		closedir(d);
		}
		else
		{
			printf("opendir() failed,%d",errno);
		}
		start_alarm();
	}
	
	//When alarm callback say that ack was required but not obtained
	//then send eot
	//shifted from alarm callback to here to have minimum in alarm callback.
	else if(strcmp(state,"SEND_EOT")==0)
	{
		char eot[2];
		eot[0]=4;
		eot[1]=0;
		
		
			struct evbuffer * wr=evbuffer_new();
			int added=evbuffer_add(wr,eot,1);
			
			struct evbuffer *output = bufferevent_get_output(bev);
			evbuffer_add_buffer(output,wr);
			
		printf("ACK expected but nothing received\n...So EOT sent\n");
		strcpy(state,"ENQ_SEND_RECV_READY");
		printf("state is now changed to :%s\n",state);						
	}

	else
	{
		printf("!!!!!NOT Ready to Send ENQ/EOT because current state is [%s]\n",state);
	}
	printf("================================\n");	
}



static void
echo_event_cb(struct bufferevent *bev, short events, void *ctx)
{
        if (events & BEV_EVENT_ERROR)
                perror("Error from bufferevent");
        if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
                bufferevent_free(bev);
                //ctx is having writee pointer
                event_del(ctx);
        }
        
}

static void
accept_conn_cb(struct evconnlistener *listener,
    evutil_socket_t fd, struct sockaddr *address, int socklen,
    void *ctx)
{
        /* We got a new connection! Set up a bufferevent for it. */
        struct event_base *base = evconnlistener_get_base(listener);
        //struct bufferevent *bev = bufferevent_socket_new(
        struct bufferevent *bev = bufferevent_socket_new(
                base, fd, BEV_OPT_CLOSE_ON_FREE);


		//fd is passed as ctx to use in callback , but not used in callback
		//bev is passed to use buffer input output in callback
		
		struct timeval ten_seconds={10,0};
		struct event* writee= event_new(base, fd ,EV_PERSIST, echo_write_cb , bev);
		event_add(writee,&ten_seconds);

        bufferevent_setcb(bev, echo_read_cb, NULL, echo_event_cb, writee);
        bufferevent_enable(bev, EV_READ|EV_WRITE);
}

static void
accept_error_cb(struct evconnlistener *listener, void *ctx)
{
        struct event_base *base = evconnlistener_get_base(listener);
        int err = EVUTIL_SOCKET_ERROR();
        fprintf(stderr, "Got an error %d (%s) on the listener. "
                "Shutting down.\n", err, evutil_socket_error_to_string(err));

        event_base_loopexit(base, NULL);
}

int
main(int argc, char **argv)
{
        struct event_base *base;
        struct evconnlistener *listener;
        struct sockaddr_in sin;
        file_counter=1;

        bzero(str,sizeof(str));
        bzero(recv_buf,sizeof(recv_buf));
        
		strcpy(state,"ENQ_SEND_RECV_READY");
		
		strcpy(inbox,"/root/inbox/");
		strcpy(outbox,"/root/outbox/");
		
        int port = 9999;

        if (argc > 1) {
                port = atoi(argv[1]);
        }
        if (port<=0 || port>65535) {
                puts("Invalid port");
                return 1;
        }

        base = event_base_new();
        if (!base) {
                puts("Couldn't open event base");
                return 1;
        }

		signal (SIGALRM, alarm_to_reset);

        /* Clear the sockaddr before using it, in case there are extra
         * platform-specific fields that can mess us up. */
        memset(&sin, 0, sizeof(sin));
        /* This is an INET address */
        sin.sin_family = AF_INET;
        /* Listen on 0.0.0.0 */
        sin.sin_addr.s_addr = htonl(0);
        /* Listen on the given port. */
        sin.sin_port = htons(port);

        listener = evconnlistener_new_bind(base, accept_conn_cb, NULL,
            LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, -1,
            (struct sockaddr*)&sin, sizeof(sin));
                
        if (!listener) {
                perror("Couldn't create listener");
                return 1;
        }
        
        evconnlistener_set_error_cb(listener, accept_error_cb);

        event_base_dispatch(base);
        return 0;
}
