#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<string.h>
#include<ctype.h>
#include "filesys.h"


#define RevByte(low,high) ((high)<<8|(low))
#define RevWord(lowest,lower,higher,highest) ((highest)<< 24|(higher)<<16|(lower)<<8|lowest) 

int dirflag = 0;
/*
*���ܣ���ӡ�������¼
*/
void ScanBootSector()
{
	unsigned char buf[SECTOR_SIZE];
	int ret,i;

	if((ret = read(fd,buf,SECTOR_SIZE))<0)
		perror("read boot sector failed");
	for(i = 0; i < 8; i++)
		bdptor.Oem_name[i] = buf[i+0x03];
	bdptor.Oem_name[i] = '\0';

	bdptor.BytesPerSector = RevByte(buf[0x0b],buf[0x0c]);
	bdptor.SectorsPerCluster = buf[0x0d];
	bdptor.ReservedSectors = RevByte(buf[0x0e],buf[0x0f]);
	bdptor.FATs = buf[0x10];
	bdptor.RootDirEntries = RevByte(buf[0x11],buf[0x12]);    
	bdptor.LogicSectors = RevByte(buf[0x13],buf[0x14]);
	if(!bdptor.LogicSectors) bdptor.LogicSectors=RevWord(buf[0x20],buf[0x21],buf[0x22],buf[0x23]);
	bdptor.MediaType = buf[0x15];
	bdptor.SectorsPerFAT = RevByte( buf[0x16],buf[0x17] );
	bdptor.SectorsPerTrack = RevByte(buf[0x18],buf[0x19]);
	bdptor.Heads = RevByte(buf[0x1a],buf[0x1b]);
	bdptor.HiddenSectors = RevByte(buf[0x1c],buf[0x1d]);
printf("%d %d\n",FAT_ONE_OFFSET,FAT_SIZE);

	fatbuf = (unsigned char*)malloc(sizeof(unsigned char)*FAT_SIZE);

	printf("Oem_name \t\t%s\n"
		"BytesPerSector \t\t%d\n"
		"SectorsPerCluster \t%d\n"
		"ReservedSector \t\t%d\n"
		"FATs \t\t\t%d\n"
		"RootDirEntries \t\t%d\n"
		"LogicSectors \t\t%d\n"
		"MedioType \t\t%d\n"
		"SectorPerFAT \t\t%d\n"
		"SectorPerTrack \t\t%d\n"
		"Heads \t\t\t%d\n"
		"HiddenSectors \t\t%d\n",
		bdptor.Oem_name,
		bdptor.BytesPerSector,
		bdptor.SectorsPerCluster,
		bdptor.ReservedSectors,
		bdptor.FATs,
		bdptor.RootDirEntries,
		bdptor.LogicSectors,
		bdptor.MediaType,
		bdptor.SectorsPerFAT,
		bdptor.SectorsPerTrack,
		bdptor.Heads,
		bdptor.HiddenSectors);
}

/*����*/
void findDate(unsigned short *year,
			  unsigned short *month,
			  unsigned short *day,
			  unsigned char info[2])
{
	int date;
	date = RevByte(info[0],info[1]);

	*year = ((date & MASK_YEAR)>> 9 )+1980;
	*month = ((date & MASK_MONTH)>> 5);
	*day = (date & MASK_DAY);
}

/*ʱ��*/
void findTime(unsigned short *hour,
			  unsigned short *min,
			  unsigned short *sec,
			  unsigned char info[2])
{
	int time;
	time = RevByte(info[0],info[1]);

	*hour = ((time & MASK_HOUR )>>11);
	*min = (time & MASK_MIN)>> 5;
	*sec = (time & MASK_SEC) * 2;
}

/*
*�ļ�����ʽ�������ڱȽ�
*/
void FileNameFormat(unsigned char *name)
{
	unsigned char *p = name;
	while(*p!='\0')
		p++;
	p--;
	while(*p==' ')
		p--;
	p++;
	*p = '\0';
}

/*������entry�����ͣ�struct Entry*
*����ֵ���ɹ����򷵻�ƫ��ֵ��ʧ�ܣ����ظ�ֵ
*���ܣ��Ӹ�Ŀ¼���ļ����еõ��ļ�����
*/
int GetEntry(struct Entry *pentry)
{
	int ret,i;
	int count = 0;
	unsigned char buf[DIR_ENTRY_SIZE], info[2];

	/*��һ��Ŀ¼�����32�ֽ�*/
	if( (ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
		perror("read entry failed");
	count += ret;

	if(buf[0]==0xe5 || buf[0]== 0x00)
		return -1*count;
	else
	{
		/*���ļ��������Ե�*/
		while (buf[11]== 0x0f) 
		{
			if((ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
				perror("read root dir failed");
			count += ret;
		}

		/*������ʽ���������β��'\0'*/
		for (i=0 ;i<=10;i++)
			pentry->short_name[i] = buf[i];
		pentry->short_name[i] = '\0';

		FileNameFormat(pentry->short_name); 



		info[0]=buf[22];
		info[1]=buf[23];
		findTime(&(pentry->hour),&(pentry->min),&(pentry->sec),info);  

		info[0]=buf[24];
		info[1]=buf[25];
		findDate(&(pentry->year),&(pentry->month),&(pentry->day),info);

		pentry->FirstCluster = RevByte(buf[26],buf[27]);
		pentry->size = RevWord(buf[28],buf[29],buf[30],buf[31]);

		pentry->readonly = (buf[11] & ATTR_READONLY) ?1:0;
		pentry->hidden = (buf[11] & ATTR_HIDDEN) ?1:0;
		pentry->system = (buf[11] & ATTR_SYSTEM) ?1:0;
		pentry->vlabel = (buf[11] & ATTR_VLABEL) ?1:0;
		pentry->subdir = (buf[11] & ATTR_SUBDIR) ?1:0;
		pentry->archive = (buf[11] & ATTR_ARCHIVE) ?1:0;

		return count;
	}
}

/*
*���ܣ���ʾ��ǰĿ¼������
*����ֵ��1���ɹ���-1��ʧ��
*/
int fd_ls()
{
	int ret, offset,cluster_addr;
	struct Entry entry;
	unsigned char buf[DIR_ENTRY_SIZE];
	if( (ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
		perror("read entry failed");
	if(curdir==NULL)
		printf("Root_dir\n");
	else
		printf("%s_dir\n",curdir->short_name);
	printf("\tname\tdate\t\t time\t\tcluster\tsize\t\tattr\n");

	if(curdir==NULL)  /*��ʾ��Ŀ¼��*/
	{
		/*��fd��λ����Ŀ¼������ʼ��ַ*/
		if((ret= lseek(fd,ROOTDIR_OFFSET,SEEK_SET))<0)
			perror("lseek ROOTDIR_OFFSET failed");

		offset = ROOTDIR_OFFSET;

		/*�Ӹ�Ŀ¼����ʼ������ֱ����������ʼ��ַ*/
		while(offset < (DATA_OFFSET))
		{
			ret = GetEntry(&entry);

			offset += abs(ret);
			if(ret > 0)
			{
				printf("%12s\t"
					"%d:%d:%d\t"
					"%d:%d:%d   \t"
					"%d\t"
					"%d\t\t"
					"%s\n",
					entry.short_name,
					entry.year,entry.month,entry.day,
					entry.hour,entry.min,entry.sec,
					entry.FirstCluster,
					entry.size,
					(entry.subdir) ? "dir":"file");
			}
		}
	}

	else /*��ʾ��Ŀ¼*/
	{
		int current_cluster;
		for(current_cluster = curdir->FirstCluster;current_cluster!=0xffff;current_cluster = RevByte(fatbuf[current_cluster<<1],fatbuf[current_cluster<<1|1])){
printf("%d %d\n",fatbuf[current_cluster<<1],fatbuf[current_cluster<<1|1]);
		cluster_addr = DATA_OFFSET + (current_cluster-2) * CLUSTER_SIZE ;
		if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
			perror("lseek cluster_addr failed");

		offset = cluster_addr;

		/*ֻ��һ�ص�����*/
		while(offset<cluster_addr +CLUSTER_SIZE)
		{
			ret = GetEntry(&entry);
			offset += abs(ret);
			if(ret > 0)
			{
				printf("%12s\t"
					"%d:%d:%d\t"
					"%d:%d:%d   \t"
					"%d\t"
					"%d\t\t"
					"%s\n",
					entry.short_name,
					entry.year,entry.month,entry.day,
					entry.hour,entry.min,entry.sec,
					entry.FirstCluster,
					entry.size,
					(entry.subdir) ? "dir":"file");
			}
		}
}
puts("xxxxx");
	}
	return 0;
} 


/*
*������entryname ���ͣ�char
��pentry    ���ͣ�struct Entry*
��mode      ���ͣ�int��mode=1��ΪĿ¼���mode=0��Ϊ�ļ�
*����ֵ��ƫ��ֵ����0����ɹ���-1����ʧ��
*���ܣ�������ǰĿ¼�������ļ���Ŀ¼��
*/
int ScanEntry (char *entryname,struct Entry *pentry,int mode)
{
	int ret,offset,i;
	int cluster_addr;
	char uppername[80];
	for(i=0;i< strlen(entryname);i++)
		uppername[i]= toupper(entryname[i]);
	uppername[i]= '\0';
	/*ɨ���Ŀ¼*/
	if(curdir ==NULL)  
	{
		if((ret = lseek(fd,ROOTDIR_OFFSET,SEEK_SET))<0)
			perror ("lseek ROOTDIR_OFFSET failed");
		offset = ROOTDIR_OFFSET;


		while(offset<DATA_OFFSET)
		{
			ret = GetEntry(pentry);
			offset +=abs(ret);

			if(pentry->subdir == mode &&!strcmp((char*)pentry->short_name,uppername))

				return offset;

		}
		return -1;
	}

	/*ɨ����Ŀ¼*/
	else  
	{
		cluster_addr = DATA_OFFSET + (curdir->FirstCluster -2)*CLUSTER_SIZE;
		if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
			perror("lseek cluster_addr failed");
		offset= cluster_addr;

		while(offset<cluster_addr + CLUSTER_SIZE)
		{
			ret= GetEntry(pentry);
			offset += abs(ret);
			if(pentry->subdir == mode &&!strcmp((char*)pentry->short_name,uppername))
				return offset;



		}
		return -1;
	}
}



/*
*������dir�����ͣ�char
*����ֵ��1���ɹ���-1��ʧ��
*���ܣ��ı�Ŀ¼����Ŀ¼����Ŀ¼
*/
int fd_cd(char *dir)
{
	struct Entry *pentry,*curdir1;
	int ret,retu,dirno1;
	int i,flag = 0;
	
	if (dir[0] == '/'){
		curdir1 = curdir; dirno1 = dirno;
		curdir = NULL; dirno = 0; retu = fd_cd(dir+1);
		if (retu < 0) {curdir = curdir1; dirno = dirno1;} 
		return retu;
	} 
	for (i = 0; dir[i] && dir[i] != '/'; i++) ;
	printf("%d %c\n",i,dir[i]);
	if (dir[i] == '/'){ 
		if (dir[i+1] == '/') {printf("no such dir\n"); return -1;}
		flag = 1;
		dir[i] = '\0';
	}

	if(!strcmp(dir,".")){
		if (flag == 1) return fd_cd(dir+(i+1)); else return 1;
	}
	if(!strcmp(dir,"..") && curdir==NULL){
		if (flag == 1) return fd_cd(dir+(i+1)); else return 1;
	}
	/*������һ��Ŀ¼*/
	if(!strcmp(dir,"..") && curdir!=NULL)
	{
		curdir1 = curdir;
		curdir = fatherdir[dirno];
		dirno--;
		if (flag == 1) {
			retu = fd_cd(dir+(i+1));
			if (retu < 0) {
				dirno++; curdir = curdir1;
			}
 			return retu;
		}
		return 1;
	}
	pentry = (struct Entry*)malloc(sizeof(struct Entry));

	ret = ScanEntry(dir,pentry,1);
	if(ret < 0)
	{
		printf("no such dir\n");
		free(pentry);
		return -1;
	}
	dirno ++;
	fatherdir[dirno] = curdir;
	curdir = pentry;
	if (flag == 1) { retu = fd_cd(dir+(i+1));
	if (retu < 0) {
		curdir = fatherdir[dirno--];
		return retu;
	}	}
	return 1;
}

/*
*������prev�����ͣ�unsigned char
*����ֵ����һ��
*��fat���л����һ�ص�λ��
*/
unsigned short GetFatCluster(unsigned short prev)
{
	unsigned short next;
	int index;

	index = prev * 2;
	next = RevByte(fatbuf[index],fatbuf[index+1]);

	return next;
}

/*
*������cluster�����ͣ�unsigned short
*����ֵ��void
*���ܣ����fat���еĴ���Ϣ
*/
void ClearFatCluster(unsigned short cluster)
{
	int index;
	index = cluster * 2;

	fatbuf[index]=0x00;
	fatbuf[index+1]=0x00;

}


/*
*���ı��fat��ֵд��fat��
*/
int WriteFat()
{
	if(lseek(fd,FAT_ONE_OFFSET,SEEK_SET)<0)
	{
		perror("lseek failed");
		return -1;
	}
	if(write(fd,fatbuf,FAT_SIZE)<0)
	{
		perror("read failed");
		return -1;
	}
	if(lseek(fd,FAT_TWO_OFFSET,SEEK_SET)<0)
	{
		perror("lseek failed");
		return -1;
	}
	if((write(fd,fatbuf,FAT_SIZE))<0)
	{
		perror("read failed");
		return -1;
	}
	return 1;
}

/*
*��fat�����Ϣ������fatbuf[]��
*/
int ReadFat()
{
int i;
	if(lseek(fd,FAT_ONE_OFFSET,SEEK_SET)<0)
	{
		perror("lseek failed");
		return -1;
	}
	if(read(fd,fatbuf,FAT_SIZE)<0)
	{
		perror("read failed");
		return -1;
	}
	return 1;
}


/*
*������filename�����ͣ�char
*����ֵ��1���ɹ���-1��ʧ��
*����;ɾ����ǰĿ¼�µ��ļ�
*/
int fd_df(char *filename)
{
	struct Entry *pentry;
	int ret,offset,cluster_addr,current_cluster;
	unsigned char c;
	unsigned short seed,next;

	pentry = (struct Entry*)malloc(sizeof(struct Entry));

	/*ɨ�赱ǰĿ¼�����ļ�*/
	ret = ScanEntry(filename,pentry,0);
	if(ret<0)
	{
		ret = ScanEntry(filename,pentry,1);
		if(ret<0){
			printf("no such file\n");
			free(pentry);
			return -1;
		}
	}
	
	if (pentry->subdir){
		int ret;
		printf("%d\n",fd_cd(filename));	
		for(current_cluster = curdir->FirstCluster;current_cluster!=0xffff;current_cluster = RevByte(fatbuf[current_cluster<<1],fatbuf[current_cluster<<1|1])){
			cluster_addr = DATA_OFFSET + (current_cluster-2) * CLUSTER_SIZE ;
			if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("lseek cluster_addr failed");

			offset = cluster_addr;

			while(offset<cluster_addr +CLUSTER_SIZE)
			{
				struct Entry entry;
				ret = GetEntry(&entry);
				offset += abs(ret);
				if(ret > 0 && strcmp(entry.short_name,".") && strcmp(entry.short_name,".."))
				{
					puts(entry.short_name);
					fd_df(entry.short_name);
				}
			}
			
		}
		fd_cd("..");
	}
	/*���fat����*/
	seed = pentry->FirstCluster;
	while((next = GetFatCluster(seed))!=0xffff)
	{
		ClearFatCluster(seed);
		seed = next;

	}

	ClearFatCluster( seed );

	/*���Ŀ¼����*/
	c=0xe5;


	if(lseek(fd,ret-0x20,SEEK_SET)<0)
		perror("lseek fd_df failed");
	if(write(fd,&c,1)<0)
		perror("write failed");  


/*	if(lseek(fd,ret-0x40,SEEK_SET)<0)
		perror("lseek fd_df failed");
	if(write(fd,&c,1)<0)
		perror("write failed");
*/
	free(pentry);
	if(WriteFat()<0)
		exit(1);
	return 1;
}


/*
*������filename�����ͣ�char�������ļ�������
size��    ���ͣ�int���ļ��Ĵ�С
*����ֵ��1���ɹ���-1��ʧ��
*���ܣ��ڵ�ǰĿ¼�´����ļ�
*/
int min(char *content,int cluster_size){
	if(strlen(content) <=cluster_size)
		return strlen(content);
	else
		return cluster_size;
}

void ud_cf(unsigned short first_cluster,char *content,int len){
	unsigned short cluster;
	int i=0,size=0;
	char *zero=(char*)malloc(CLUSTER_SIZE);
	memset(zero,0,CLUSTER_SIZE);
	
	for(cluster = first_cluster;cluster!=0xffff;cluster=GetFatCluster(cluster)){
		if(0&&content[i]=='\0'){
			break;		
		}
		lseek(fd,DATA_OFFSET+(cluster-2)*CLUSTER_SIZE,SEEK_SET);
		size = len?len:min(content,CLUSTER_SIZE);
		if(len)len=0;
printf("%d\n",size);
		if(size) write(fd,content,size);
		lseek(fd,DATA_OFFSET+(cluster-2)*CLUSTER_SIZE+size,SEEK_SET);
		if(CLUSTER_SIZE-size) write(fd,zero,CLUSTER_SIZE-size);
printf("%d\n",size);
		i+=size;
	}
}

int fd_cf(char *filename,char *content)
{
	struct Entry *pentry;
	int ret,i=0,cluster_addr,offset;
	unsigned short cluster,clusterno[100];
	unsigned char c[DIR_ENTRY_SIZE];
	int index,clustersize,size = content?strlen(content):64;
	unsigned char buf[DIR_ENTRY_SIZE];
	char file_content[65];
	pentry = (struct Entry*)malloc(sizeof(struct Entry));
	clustersize = (size / (CLUSTER_SIZE));
	for(i=0;i<65;++i) file_content[i]=0;
i=0;
	//puts(filename);
//printf("%d\n",clustersize);

	if(size % (CLUSTER_SIZE) != 0)
		clustersize ++;


	//ɨ���Ŀ¼���Ƿ��Ѵ��ڸ��ļ���
//puts(filename);
	ret = ScanEntry(filename,pentry,0);
//puts(filename);
	if (ret<0)
	{
//puts(filename);
		/*��ѯfat���ҵ��հ״أ�������clusterno[]��*/
		for(cluster=2;cluster<1000;cluster++)
		{
//puts(filename);
			index = cluster *2;
			if(fatbuf[index]==0x00&&fatbuf[index+1]==0x00)
			{
				clusterno[i] = cluster;
printf("%d %d\n",i,clustersize);
				i++;
				if(i==clustersize)
					break;

			}

		}
//puts(filename);
		/*��fat����д����һ����Ϣ*/
		for(i=0;i<clustersize-1;i++)
		{
			index = clusterno[i]*2;

			fatbuf[index] = (clusterno[i+1] &  0x00ff);
			fatbuf[index+1] = ((clusterno[i+1] & 0xff00)>>8);


		}
		/*���һ��д��0xffff*/
		index = clusterno[i]*2;
		fatbuf[index] = 0xff;
		fatbuf[index+1] = 0xff;
//puts(filename);
		if(curdir==NULL)  /*����Ŀ¼��д�ļ�*/
		{ 

			if((ret= lseek(fd,ROOTDIR_OFFSET,SEEK_SET))<0)
				perror("lseek ROOTDIR_OFFSET failed");
			offset = ROOTDIR_OFFSET;
			while(offset < DATA_OFFSET)
			{
				if((ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
					perror("read entry failed");

				offset += abs(ret);

				if(buf[0]!=0xe5&&buf[0]!=0x00)
				{
					while(buf[11] == 0x0f)
					{
						if((ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
							perror("read root dir failed");
						offset +=abs(ret);
					}
				}


				/*�ҳ���Ŀ¼�����ɾ����Ŀ¼��*/ 
				else
				{       
					offset = offset-abs(ret);     
//puts(filename);
					for(i=0;i<=strlen(filename);i++)
					{
						c[i]=toupper(filename[i]);
					}			
					for(;i<=10;i++)
						c[i]=' ';

					c[11] = 0x01;
					if(dirflag) {			
						int i=0;
						c[11]=ATTR_SUBDIR;
						for(;i<11;i++){
							file_content[i]=file_content[i+32]=i?' ':'.';
						}
						file_content[33]='.';
						file_content[11]=file_content[11+32]=ATTR_SUBDIR;
						file_content[26]=clusterno[0]&0xff;
						file_content[27]=(clusterno[0]&0xff00)>>8;
						file_content[26+32]=0;
						file_content[27+32]=0;
						content=file_content;
						puts("xxx");
					}

					/*д��һ�ص�ֵ*/
					c[26] = (clusterno[0] &  0x00ff);
					c[27] = ((clusterno[0] & 0xff00)>>8);

					/*д�ļ��Ĵ�С*/
					c[28] = (size &  0x000000ff);
					c[29] = ((size & 0x0000ff00)>>8);
					c[30] = ((size& 0x00ff0000)>>16);
					c[31] = ((size& 0xff000000)>>24);

					if(lseek(fd,offset,SEEK_SET)<0)
						perror("lseek fd_cf failed");
					if(write(fd,&c,DIR_ENTRY_SIZE)<0)
						perror("write failed");


					free(pentry);
					if(WriteFat()<0)
						exit(1);
					ud_cf(clusterno[0],content,dirflag?64:0);
//puts("ffff");
for(i=0;i<=10;++i) putchar(c[i]); putchar('\n');
					return 1;
				}

			}
		}
		else 
		{
			cluster_addr = (curdir->FirstCluster -2 )*CLUSTER_SIZE + DATA_OFFSET;
			if((ret= lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("lseek cluster_addr failed");
			offset = cluster_addr;
			while(offset < cluster_addr + CLUSTER_SIZE)
			{
				if((ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
					perror("read entry failed");

				offset += abs(ret);

				if(buf[0]!=0xe5&&buf[0]!=0x00)
				{
					while(buf[11] == 0x0f)
					{
						if((ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
							perror("read root dir failed");
						offset +=abs(ret);
					}
				}
				else
				{ 
					offset = offset - abs(ret);      
					for(i=0;i<=strlen(filename);i++)
					{
						c[i]=toupper(filename[i]);
					}
					for(;i<=10;i++)
						c[i]=' ';

					c[11] = 0x01;
					if(dirflag) {		
						int i=0;
						c[11]=ATTR_SUBDIR;
						for(;i<11;i++){
							file_content[i]=file_content[i+32]=i?' ':'.';
						}
						file_content[33]='.';
						file_content[11]=file_content[11+32]=ATTR_SUBDIR;
						file_content[26]=clusterno[0]&0xff;
						file_content[27]=(clusterno[0]&0xff00)>>8;
						file_content[26+32]=curdir->FirstCluster&0xff;
						file_content[27+32]=(curdir->FirstCluster&0xff00)>>8;
						content=file_content;
					}


					c[26] = (clusterno[0] &  0x00ff);
					c[27] = ((clusterno[0] & 0xff00)>>8);

					c[28] = (size &  0x000000ff);
					c[29] = ((size & 0x0000ff00)>>8);
					c[30] = ((size& 0x00ff0000)>>16);
					c[31] = ((size& 0xff000000)>>24);

					if(lseek(fd,offset,SEEK_SET)<0)
						perror("lseek fd_cf failed");
					if(write(fd,&c,DIR_ENTRY_SIZE)<0)
						perror("write failed");



					free(pentry);
					if(WriteFat()<0)
						exit(1);
					ud_cf(clusterno[0],content,dirflag?64:0);
//puts("ffff");
					return 1;
				}

			}
		}
	}
	else
	{
		printf("This filename is exist\n");
		free(pentry);
		return -1;
	}
	return -1;

}

void fd_makedir(char *menuname)
{
	dirflag = 1;
	fd_cf(menuname,0);
	dirflag = 0; 
}

void do_usage()
{
	printf("please input a command, including followings:\n\tls\t\t\tlist all files\n\tcd <dir>\t\tchange direcotry\n\tcf <filename> <size>\tcreate a file\n\tdf <file>\t\tdelete a file\n\texit\t\t\texit this system\n");
}


int main()
{
	char input[10];
	char content[1024];
	char name[12];
	if((fd = open(DEVNAME,O_RDWR))<0)
		perror("open failed");
	ScanBootSector();
	if(ReadFat()<0)
		exit(1);
	do_usage();
	while (1)
	{
		printf(">");
		scanf("%s",input);

		if (strcmp(input, "exit") == 0)
			break;
		else if (strcmp(input, "ls") == 0)
			fd_ls();
		else if(strcmp(input, "cd") == 0)
		{
			scanf("%s", name);
			fd_cd(name);
		}
		else if(strcmp(input, "df") == 0)
		{
			scanf("%s", name);
			fd_df(name);
		}
		else if(strcmp(input, "cf") == 0)
		{
			scanf("%s", name);
			scanf("%s", content);
			fd_cf(name,content);
		}
		else if(strcmp(input, "mdir") == 0){
			scanf("%s", name);
			fd_makedir(name);
		}
		else
			do_usage();
	}	

	return 0;
}



