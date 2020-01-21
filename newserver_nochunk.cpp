#include<iostream>
#include<stdio.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<pthread.h>
#include<netinet/in.h>
#include<fcntl.h>
#include<vector>
#include<set>
#include<unordered_map>
#include<stdlib.h>
#include<openssl/sha.h>
#include<assert.h>
#define PORT 20000
#define BACKLOG 1000
#define BUF_MAX 2048
using namespace std;
pthread_mutex_t mutx=PTHREAD_MUTEX_INITIALIZER;
unordered_map<string,string> auth;
unordered_map<string,pair<int,string>>cli_list;
struct thread_args
{
int client_sockfd;
int client_port;
};
static int client_id=1;
typedef struct 
{	
	int client_num;
	int cl_port;
	char filename[BUF_MAX];
	char filehash[BUF_MAX];
}client_list;
client_list *clients[BUF_MAX];
void add_to_list(client_list *cl)
{
	pthread_mutex_lock(&mutx);
	for(int i=0;i<BUF_MAX;i++)
	{
		if(!clients[i])
		{
			clients[i]=cl;
			break;
		}
	}
	pthread_mutex_unlock(&mutx);
}
void remove_from_list(int cl_num)
{
	pthread_mutex_lock(&mutx);
	for(long int i=0;i<BUF_MAX;i++)
	{
		if(clients[i])
		{
			if(clients[i]->client_num=cl_num)
			clients[i]=NULL;
			break;
		}
	}
	pthread_mutex_unlock(&mutx);
}

bool authenticate(string username,string password)
{
unordered_map<string,string>::iterator it;
if(auth.find(username)!=auth.end())
{
	cout<<"existing user"<<endl;
	string pswd=auth[username];
	if(pswd!=password)
	{
		cout<<"Wrong password!"<<endl;
		return false;
	}
	else if(pswd==password)
	{	
		cout<<"Authentication successful"<<endl;
		return true;
	}
}
else if(auth.find(username)==auth.end())
{
	auth[username]=password;
	cout<<"New client created"<<endl;
	return true;
}
}
int search_for_peer(char *file_name)
{
	int share_port=0;
	int found=0;
	cout<<"port found ! "<<endl;
	for(long int i=0;i<BUF_MAX;i++)
	{
		if(clients[i])
		{
			if(strcmp(clients[i]->filename,file_name)==0)
			share_port=clients[i]->cl_port;
			found=1;
			break;
		}
	}
	if(found==0)
	{
		cout<<"No client with filename "<<file_name<<" found \n";
		return share_port;
	}
	else
	{	cout<<"port found ! "<<share_port<<endl;
		return share_port;
	}
}

void show_clients()
{
	for(int i=0;i<BUF_MAX;i++)
	{
		if(clients[i])
		{
			cout<<" "<<clients[i]->client_num<<" "<<clients[i]->cl_port<<" "<<clients[i]->filename<<" "<<clients[i]->filehash<<" ";
		}
	}
}
void save_peer_details(int cli_fd)
{	
	char store[BUF_MAX]={'\0'};
	char file_name[BUF_MAX]={'\0'};
	char hash[BUF_MAX]={'\0'};
	int portnum;
	int buf_size=recv(cli_fd,store,sizeof(store),0);
	sscanf(store,"%s %s %d",file_name,hash,&portnum);
	pthread_mutex_lock(&mutx);
	cli_list[string(file_name,strlen(file_name))]=make_pair(portnum,string(hash,strlen(hash)));
	unordered_map<string,pair<int,string>>::iterator it;
	for(it=cli_list.begin();it!=cli_list.end();it++)
	{
		cout<<it->first<<" "<<it->second.first<<" "<<it->second.second<<"\n";	
	}
	//client_list *cl=(client_list*)malloc(sizeof(client_list));
	//cl->client_num=client_id++;
	//cl->cl_port=portnum;
	//strcpy(cl->filename,file_name);
	//strcpy(cl->filehash,hash);	
	//add_to_list(cl);
	pthread_mutex_unlock(&mutx);
	printf("Added to client list \n");
	//free(cl);	
}
void send_peer_details(int client_sockfd)
{
	printf("inside download request \n");
	char file_name[BUF_MAX];
	int name_size=recv(client_sockfd,file_name,sizeof(file_name),0);
	cout<<"the name of file is "<<file_name<<endl;
	string fname(file_name,strlen(file_name));
	if(cli_list.find(fname)==cli_list.end())
	{
	int share_port=0;
	char port_buf[BUF_MAX];
	sprintf(port_buf,"%d",share_port);
	send(client_sockfd,port_buf,BUF_MAX,0);
	return;
	}
	else
	{
	int port=(cli_list[fname].first);
	char port_buf[BUF_MAX];
	sprintf(port_buf,"%d",port);
	cout<<"sending port number "<<port<<" to peer"<<endl;
	send(client_sockfd,port_buf,BUF_MAX,0);
	}	
	////int share_port=search_for_peer(file_name);
	//cout<<"sharing port number "<<share_port <<"of the peer \n";
		//if(share_port!=0)
		//{
			//char port_buf[BUF_MAX];
			//sprintf(port_buf,"%d",share_port);
			//send(client_sockfd,port_buf,BUF_MAX,0);
		//}
}
void create_new_user(int cl_fd)
{
char ids[BUF_MAX];
int id_size=recv(cl_fd,ids,sizeof(ids),0);
char usn[BUF_MAX]{'\0'};
char psw[BUF_MAX]={'\0'};
sscanf(ids,"%s %s",usn,psw);
string us=usn;
string ps=psw;
if(auth.find(usn)==auth.end())
{
if(authenticate(us,ps))
{
char msg[BUF_MAX]="Client added successfully \n";
send(cl_fd,msg,BUF_MAX,0);
}
}
else
{
char msg[BUF_MAX]="Client already exists,Please re-connect and login again \n";
send(cl_fd,msg,BUF_MAX,0);
}
printf("new user with name %s created \n",usn);
}
void *command_manager(void *thread_arg)
{
	int client_sfd;
	int cl_port;
	client_sfd=((struct thread_args*)thread_arg)->client_sockfd;
	cl_port=((struct thread_args*)thread_arg)->client_port;
	char command[BUF_MAX]={'\0'};
	int cmd_size=recv(client_sfd,command,sizeof(command),0);
	cout<<"command is "<<command<<"\n";
	if(strcmp(command,"share")==0)
	{
	cout<<"inside share"<<endl;
	save_peer_details(client_sfd);
	}	
	else if(strcmp(command,"download")==0)
	{
	cout<<"inside download"<<"\n";
	send_peer_details(client_sfd);
	}
	else if(strcmp(command,"create_user")==0)
	{
		cout<<"inside create user \n";
		create_new_user(client_sfd);
	}
	else if(strcmp(command,"create_user")==0)
	{
		cout<<"inside create user \n";
		create_new_user(client_sfd);
	}
	show_clients();
	close(client_sfd);
	pthread_exit(NULL);
}

int main(int argc,char *argv[])
{

struct sockaddr_in server_addr,client_addr;
int server_fd,client_fd,port_no;
socklen_t client_len;
pthread_t threadid;
struct thread_args *thread_arg;
int listen_client,bind_to;
char buffer[4096];
int op=1;
int n;
if(argc<2)
{
	perror("Port Not Provided!!\n");
	exit(-1);
}
server_fd=socket(AF_INET,SOCK_STREAM,0);
if(server_fd<0)
{
	perror("Server connection cannot be established\n");
	exit(-1);
}
server_addr.sin_family=AF_INET;
server_addr.sin_port=htons(atoi(argv[1]));
server_addr.sin_addr.s_addr=INADDR_ANY;
//int option=setsockopt(server_fd,SOL_SOCKET,(SO_REUSEPORT|SO_REUSEADDR),(char*)&op,sizeof(op));
bind_to=bind(server_fd,(struct sockaddr*)&server_addr,sizeof(server_addr));
if(bind_to<0)
{
	perror("Binding Error\n");
	exit(-1);
}
cout<<"entering listening state"<<endl;
listen_client=listen(server_fd,BACKLOG);
if(listen_client<0)
{
	perror("Couldn't establish connection to any client \n");
	exit(-1);
}
for(;;){
client_len=sizeof(client_addr);
client_fd=accept(server_fd,(struct sockaddr*)&client_addr,&client_len);
cout<<"New client connected on socket number  :"<<client_fd<<endl;
if(client_fd<0)
{
	perror("Couldn't accept Client\n");
	exit(-1);
}
if((thread_arg=(struct thread_args*)malloc(sizeof(struct thread_args)))==NULL)
{
perror("Couldnt get thread \n");
exit(-1);
}
thread_arg->client_sockfd=client_fd;
thread_arg->client_port=client_addr.sin_port;
if(pthread_create(&threadid,NULL,command_manager,(void *)thread_arg)!=0)
{
perror("couldn't create new thread \n");
exit(-1);
}
pthread_detach(threadid);
}
}
