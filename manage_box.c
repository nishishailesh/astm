#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>


#include <my_global.h>
#include <mysql.h>

//to set last part of file name, to ensure more than 1 file given different name if required in <1 sec
int file_counter;

//gcc manage_box.c -o manage_box  `mysql_config --cflags --libs`
//while (uname) do ls -l; sleep 3;done;	//to see files

MYSQL* get_mysql_connection(char* mysqlserverip,char* username, char* password)
{
	MYSQL *conn;
	conn = mysql_init(NULL);

	if (conn == NULL) 
	{
	  printf("Error %u: %s\n", mysql_errno(conn), mysql_error(conn));
	  return NULL;
	}

	if (mysql_real_connect(conn, mysqlserverip, username,password, NULL, 0, NULL, 0) == NULL) 
	{
	  printf("Error %u: %s\n", mysql_errno(conn), mysql_error(conn));     
	  return NULL;
	}

	return conn;
}

MYSQL_RES * run_mysql_query(MYSQL* conn, char* database, char* sql)
{
	if (mysql_select_db(conn, database)!=0) 
	{
		printf("Error1 %u: %s\n", mysql_errno(conn), mysql_error(conn));
		return NULL;
	}

	if (mysql_query(conn, sql)!=0 ) 
	{
		printf("Error2 %u: %s\n", mysql_errno(conn), mysql_error(conn));
	}

	MYSQL_RES *result = mysql_store_result(conn);

	if (result == NULL) 
	{
		//update query will also reach here
		printf("Error3 %u: %s\n", mysql_errno(conn), mysql_error(conn));
	}

	return result;
	//donot forget to use 	mysql_free_result(result) when not required 
}

int do_something_with_mysql_result(MYSQL_RES *result)
{
	int num_fields = mysql_num_fields(result);

	MYSQL_ROW row;

	while ((row = mysql_fetch_row(result))) 
	{ 
		for(int i = 0; i < num_fields; i++) 
		{ 
			printf("%s\t", row[i] ? row[i] : "NULL"); 
		} 
		printf("\n"); 
	}

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


char* replace_char(char* buf,char* modified)
{
	bzero(modified,sizeof(modified));
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


void cchecksum(char* str,char* ret)
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
	left=(sum&0b11110000)>>4;	//mistake in toupper
	sprintf(ret,"%X%X",left,right);
	ret[2]=0;
}


void checksum(char* str,char* ret)
{
	int sum=0;				// can not be char, otherwise sum will overrun it, creating unpredictable problem
							//evenif %256 taken on each addition, sum must be int because before modulo, it can exceed 256
							//on 32 bit system it is 4 byte long so, overrun not likely
							
	int len=strlen(str);
	for(int i=0;i<len;i++)
	{
		//printf("%d,%d ",str[i],sum);
		sum=sum+str[i];
	}
	sum=sum%256;
	printf("\n");
	sprintf(ret,"%02X",sum);
	ret[2]=0;
	printf("in function ret=%s\n",ret);
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

int prepare_order_for_query(MYSQL_RES *result)
{
	int num_fields = mysql_num_fields(result);

	MYSQL_ROW row;

	char header[1100];
	bzero(header,sizeof(header));
	strcpy(header,"1H|`^&");

	char patient[1100];
	bzero(patient,sizeof(patient));
	strcpy(patient,"P|1|0");	
	
	char terminator[1100];
	bzero(terminator,sizeof(terminator));
	strcpy(terminator,"L|1|N");	
	
	char order[1100];
	bzero(order,sizeof(order));
	
	char examination[1100];
	bzero(examination,sizeof(examination));
	
	char sample_type[100];
	bzero(sample_type,sizeof(sample_type));

	char sid[1100];
	bzero(sid,sizeof(sid));	
	
	while (row = mysql_fetch_row(result)) 
	{ 
		if(row[1]=="Urine")
		{
			strcpy(sample_type,"Urine");
		}
		else
		{
			strcpy(sample_type,"Serum");
		}
		
		strcpy(sid,row[0]);
		strcat(examination,"^^^");
		strcat(examination,row[2]);
		strcat(examination,"`");
	}
	
	sprintf(order,"O|1|%s||%s|R||||||A||||%s",sid,examination,sample_type);
	printf("order line:\n%s\n\n",order);
	
	/*
	 
	 1H|`^&||||||||||P|E 1394-97|20181012093838
	P|1|0|||fifi|||U||||||||0|0
	O|1|444||^^^ALT`^^^CR|R||||||A||||Serum
	L|1|N
	DD

	 */
	
	//for use all below
	char modified[1100];
	//zeroed by function before use
	
	//not testing for max size
	//requre ETB if more than 1024, but, query is not that big
	char pre_final_order[1100];
	bzero(pre_final_order,sizeof(pre_final_order));
	sprintf(pre_final_order,"%s%c%s%c%s%c%s%c%c",header,13,patient,13,order,13,terminator,13,3);
	
	replace_char(pre_final_order,modified);
	printf("prefinal order:\n%s\n\n",modified);
	
	char ret[3];
	bzero(ret,sizeof(ret));
	
	checksum(pre_final_order,ret);
	printf("checksum:%s\n",ret);
	
	replace_char(pre_final_order,modified);
	printf("prefinal order TWO:\n%s\n\n",modified);	
	
	
	char final_order[1100];
	bzero(final_order,sizeof(final_order));
	
	//sprintf(final_order,"%c%s%s%c%c",2,pre_final_order,ret,13,10);
	char stx[2];stx[0]=2;stx[1]=0;
	char cr[2];cr[0]=13;cr[1]=0;
	char lf[2];lf[0]=10;lf[1]=0;
	
	strcpy(final_order,stx);
	strcat(final_order,pre_final_order);
	strcat(final_order,ret);
	strcat(final_order,cr);
	strcat(final_order,lf);
	
	replace_char(final_order,modified);
	printf("final_order:\n%s\n\n",modified);
	

	char fname[200];
	bzero(fname,200);
	strcpy(fname,"/root/outbox/");
	filepath(fname);
	FILE* frm=fopen(fname,"w");
	fwrite(final_order,strlen(final_order),1,frm);
	fclose(frm);
}



int read_first_file(char* outbox, char* file_data)
{
	char fullpath[200];
	
	char buf[5000];
	bzero(buf,5000);
	
	char modified[5000];
	bzero(modified,5000);
	
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
				//char fullpath[200];
				bzero(fullpath,sizeof(fullpath));
				strcpy(fullpath,outbox);
				strcat(fullpath,dir->d_name);
				printf("Reading <%s>\n",fullpath);				
				
				FILE* fp=fopen(fullpath,"r");
				fread(buf,5000,1,fp);
				//fread donot return number of bytes returned, be careful, 
				//bzero of buff is must because file donot have last zero
				//buf[strlen(buf)]=0; //this is useless, because strlen use last byte as zero
				//printf("data: %s\n",buf);
				replace_char(buf,modified);
				//printf("file content:\n%s\nlength of data=%d\n",modified,strlen(buf));
				fclose(fp);
				strcpy(file_data,buf);
				
				printf("Trying to delete <%s>\n",fullpath);			
				if(remove(fullpath)==0)
				{
					printf("Deleted <%s>\n",fullpath);			
				}
				else
				{
					printf("Failed to delete <%s>\n",fullpath);			
				}
												
				break;
			}
		}
		closedir(d);
	}
}

void join_etb_etx(char* buf,char* ret)
{
	bzero(ret,sizeof(ret));
	int length=strlen(buf);
	int ret_counter=0;
	for(int i=0;i<length;i++)
	{
		if(buf[i]==2){}		//remove stx
		else if(i==1){}		//remove frame number hust before 'H'
		else if(buf[i]==23) //if etb, jump 6 character
		{
			i=i+6; 
			//6 with i++
			//98|mmol/<ETB>44<CR><LF><STX>2l|^DEFAULT|A|N|
			//          2  34  5   6   7  89
			//if ETB is 2 increase i to 8. 9th read in next loop   
		}
		else if(buf[i]==3)	//if etx, nothing further
		{
			break;
		}
		else
		{
			ret[ret_counter]=buf[i];
			ret_counter++;
		}
	}
}


//for single character delimiters
//remember to pass 1100 
void my_strtok(char* buf,char delimiter, char line[][1100],int max_line_numbers, int max_line_size)
{
	int length=strlen(buf);
	int current_line=0;
	int current_position=0;	//character in line
	for(int i=0;i<length;i++)
	{
		if(current_position==0){bzero(line[current_line],max_line_size);}
		
		if(buf[i]==delimiter)
		{
			line[current_line][current_position]=0;
			
			
			if(current_line>=max_line_numbers)
			{
				printf("no more line numbers will be read by my_strtok()\n");
				break;
			}
			
			current_line++;		
			current_position=0;
		}
		else
		{
			line[current_line][current_position]=buf[i];
			current_position++;
		}
	}
		
	for(int j=0;j<=current_line;j++)
	{
		printf("%d:%s\n",j,line[j]);
	}
}

void manage_query(char* query_record,MYSQL* conn)
{
	//strtok modify the string, so, copy before use
	char qr[2000];
	bzero(qr,sizeof(qr));
	strcpy(qr,query_record);
	
	char sid[2000];
	bzero(sid,sizeof(sid));
			
	printf("Query:\n%s\n",query_record);
	char fields[50][1100];
	my_strtok(qr,'|',fields,50,1100);

	//ensure that analyser send only one sample query at a time
	//change in erba software menu
	char subfields[50][1100];
	my_strtok(fields[2],'^',subfields,50,1100);
	
	strcpy(sid,subfields[1]);
	
	char sql[2000];
	bzero(sql,sizeof(sql));
	strcpy(sql,"select sample_id,sample_type,code from examination where sample_id='");
	strcat(sql,sid);
	strcat(sql,"'");
	printf("SQL:%s\n",sql);
	
	MYSQL_RES *result=run_mysql_query(conn,"biochemistry",sql);
	prepare_order_for_query(result);
	mysql_free_result(result);
	
}


void manage_result(char* order_line, char* result_line, MYSQL* conn)
{
	//strtok modify the string, so, copy before use
	char ol[2000];
	bzero(ol,sizeof(ol));
	strcpy(ol,order_line);
	
	char rl[2000];
	bzero(rl,sizeof(rl));
	strcpy(rl,result_line);
	
/* Find Sample ID */	
	char sid[2000];
	bzero(sid,sizeof(sid));
			
	printf("Order Line:\n%s\n",ol);
	char fields[50][1100];
	my_strtok(ol,'|',fields,50,1100);

	//Result are one sample at a time
	//unline query, so following is not required
	//char subfields[50][1100];
	//my_strtok(fields[2],'^',subfields,50,1100);
	
	strcpy(sid,fields[2]);	//third field is sample id
	
/* find code and result */	

//O|1|8974||^^^Na`^^^K`^^^Cl`^^^Li|||||||||||SERUM
//R|2|^^^K|3.76|mmol/l|^DEFAULT|A|N|F||||20181017065952
//C|1|I|Instrument Flag A

	char code[100];
	bzero(code,sizeof(code));

	char ex_result[100];
	bzero(ex_result,sizeof(ex_result));
				
	printf("Result Line:\n%s\n",rl);
	char rields[50][1100];				//rields means fields of result data!!
	my_strtok(rl,'|',rields,50,1100);

	strcpy(ex_result,rields[3]);	//forth field is result 

	char subrields[50][1100];
	my_strtok(rields[2],'^',subrields,50,1100);	 //third field is examination code ^^^GLC
	strcpy(code,subrields[3]);		//forth field in ^^^GLC is code
	
	char sql[2000];
	bzero(sql,sizeof(sql));
	strcpy(sql,"update examination set result='");
	strcat(sql,ex_result);
	strcat(sql,"' where sample_id='");
	strcat(sql,sid);
	strcat(sql,"' and code='");
	strcat(sql,code);
	strcat(sql,"'");
	printf("SQL:%s\n",sql);
	
	MYSQL_RES *result=run_mysql_query(conn,"biochemistry",sql);
	
	mysql_free_result(result);
}


//pass special char striped, plain file_data returned from join_etc_etx()
void analyse_file_data(char* buf,MYSQL* conn)
{
	/*
		inbox have following type of data
		  -R results (header-patient-order-(result-comment)n-terminator)
		  -Q query (header-patient-order-terminator)
	*/
	
	//char modified[15000];
	//replace_char(buf,modified);
	//printf("=============\nfile content:\n%s\nlength of data=%d\n",modified,strlen(buf));

	int length=strlen(buf);
	char line[200][1100];
	int current_line=0;
	int current_position=0;	//character in line
	
	//prepare line array
	for(int i=0;i<length;i++)
	{
		if(current_position==0){bzero(line[current_line],1100);}
		
		if(buf[i]==13)
		{
			line[current_line][current_position]=0;
			current_line++;
			current_position=0;
		}
		else
		{
			line[current_line][current_position]=buf[i];
			current_position++;
		}
	}

	//analyse line array
	int O_line_number=0;
	
	for(int j=0;j<current_line;j++)
	{
		printf("%s\n",line[j]);
		if(line[j][0]=='H')
		{
			printf("Header Record Found\n");
		}
		if(line[j][0]=='P')
		{
			printf("Patient Record Found\n");
		}
		if(line[j][0]=='O')
		{
			printf("Order Record Found\n");
			O_line_number=j;
		}
		if(line[j][0]=='Q')
		{
			printf("Query Record Found\n");
			manage_query(line[j],conn);
		}
		if(line[j][0]=='R')
		{
			printf("Result Record Found\n");
			manage_result(line[O_line_number],line[j],conn);
		}
		if(line[j][0]=='C')
		{
			printf("Comment Record Found\n");
		}
		if(line[j][0]=='L')
		{
			printf("Terminator Record Found\n");
		}
	}
}

///////////////////
int main(int argc,char *argv[])
{
	printf("Storage size for int : %d \n", sizeof(int));
	MYSQL* conn=get_mysql_connection("127.0.0.1","root","passwordxxx");
	//run_mysql_query(conn,"biochemistry","select * from user");
	file_counter=1;	//for use by filepath()
	
	while(3==3)
	{
		char file_data[15000];
		bzero(file_data,sizeof(file_data));
		
		//char modified[15000];
		//bzero(modified,sizeof(modified));
		
/* Read file */
		read_first_file("/root/inbox/",file_data);
		
		//replace_char(file_data,modified);
		//printf("file content:\n%s\n",modified);

/* Remove std,etb,stx and last cr,lf */
		char plain[15000];
		bzero(plain,sizeof(plain));	
		join_etb_etx(file_data,plain);
		
		//bzero(modified,sizeof(modified));
		//replace_char(plain,modified);
		//printf("joined file content:\n%s\n",modified);
		//printf("joined file raw content:\n%s\n",plain);
		//printf("File: %s\n",fullpath); //fullpath not required in main

/* prepare line array, analyse to find if Q or R */		
		analyse_file_data(plain,conn);
		sleep(5);
	}
	mysql_close(conn);

}
