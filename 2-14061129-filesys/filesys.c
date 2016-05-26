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
char fileNAME[64][3]={"0","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18","19","20"
,"21","22","23","24","25","26","27","28","29","30","31","32","33","34","35","36","37","38","39","40","41","42","43","44","45","46","47","48","49","50"
,"51","52","53","54","55","56","57","58","59","60","61","62","63"};
/*
*功能：打印启动项记录
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
	bdptor.MediaType = buf[0x15];
	bdptor.SectorsPerFAT = RevByte( buf[0x16],buf[0x17] );
	bdptor.SectorsPerTrack = RevByte(buf[0x18],buf[0x19]);
	bdptor.Heads = RevByte(buf[0x1a],buf[0x1b]);
	bdptor.HiddenSectors = RevByte(buf[0x1c],buf[0x1d]);


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

/*日期*/
void findDate(unsigned short *year,
			  unsigned short *month,
			  unsigned short *day,
			  unsigned char info[2])
{
	int date;
	date = RevByte(info[0],info[1]);

	*year = ((date & MASK_YEAR)>> 9 )+1900;
	*month = ((date & MASK_MONTH)>> 5)+1;
	*day = (date & MASK_DAY);
}

/*时间*/
void findTime(unsigned short *hour,
			  unsigned short *min,
			  unsigned short *sec,
			  unsigned char info[2])
{
	int time;
	time = RevByte(info[0],info[1]);

	*hour = ((time & MASK_HOUR )>>11)+8;
	*min = (time & MASK_MIN)>> 5;
	*sec = (time & MASK_SEC) *2;
}

/*
*文件名格式化，便于比较
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

/*参数：entry，类型：struct Entry*
*返回值：成功，则返回偏移值；失败：返回负值
*功能：从根目录或文件簇中得到文件表项
*/
int GetEntry(struct Entry *pentry)
{
	int ret,i;
	int count = 0;
	unsigned char buf[DIR_ENTRY_SIZE], info[2];

	/*读一个目录表项，即32字节*/
	if( (ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
		perror("read entry failed");
	count += ret;

	if(buf[0]==0xe5 || buf[0]== 0x00)
		return -1*count;
	else
	{
		/*长文件名，忽略掉*/
		while (buf[11]== 0x0f)
		{
			if((ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
				perror("read root dir failed");
			count += ret;
		}

		/*命名格式化，主义结尾的'\0'*/
		for (i=0 ;i<=10;i++)
			pentry->short_name[i] = buf[i];
		//pentry->short_name[i] = '\0';

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
*功能：显示当前目录的内容
*返回值：1，成功；-1，失败
*/
int fd_ls(int mode)
{
	int ret, offset,cluster_addr;
	unsigned short next,seed;
	struct Entry entry;
	unsigned char buf[DIR_ENTRY_SIZE];
	if( (ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
		perror("read entry failed");
	if(curdir==NULL)
		printf("Root_dir\n");
	else
		printf("%s_dir\n",curdir->short_name);
    if(mode==1)
        printf("\tname\tdate\t\t time\t\tcluster\tsize\t\tattr\n");

	if(curdir==NULL)  /*显示根目录区*/
	{
		/*将fd定位到根目录区的起始地址*/
		if((ret= lseek(fd,ROOTDIR_OFFSET,SEEK_SET))<0)
			perror("lseek ROOTDIR_OFFSET failed");

		offset = ROOTDIR_OFFSET;

		/*从根目录区开始遍历，直到数据区起始地址*/
		while(offset < (DATA_OFFSET))
		{
			ret = GetEntry(&entry);

			offset += abs(ret);
			if(ret > 0)
			{
                if(mode==1){
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
					else{
					printf("%s ",entry.short_name);
					}
			}
		}
	}

	else /*显示子目录*/
	{
		cluster_addr = DATA_OFFSET + (curdir->FirstCluster-2) * CLUSTER_SIZE ;
		if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
			perror("lseek cluster_addr failed");

		offset = cluster_addr;
		/*只读一簇的内容*/
		while(offset<cluster_addr +CLUSTER_SIZE)
		{
				ret = GetEntry(&entry);
				offset += abs(ret);
				if(ret > 0)
				{
				if(mode==1){
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
                    else{
                    printf("%s ",entry.short_name);
                    }
				}
		}
		seed = curdir->FirstCluster;
 		while((next = GetFatCluster(seed))!=0xffff)
              	{
			seed = next;
			cluster_addr = DATA_OFFSET + (seed-2) * CLUSTER_SIZE ;
			while(offset<cluster_addr +CLUSTER_SIZE)
			{
				ret = GetEntry(&entry);
				offset += abs(ret);
				if(ret > 0)
				{
				if(mode==1){
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
                    else{
                    printf("%s ",entry.short_name);
                    }
				}
			}
             	}


	}
	if(mode!=1)
	{
        printf("\n");
	}
	return 0;
}


/*
*参数：entryname 类型：char
：pentry    类型：struct Entry*
：mode      类型：int，mode=1，为目录表项；mode=0，为文件
*返回值：偏移值大于0，则成功；-1，则失败
*功能：搜索当前目录，查找文件或目录项
*/
int ScanEntry (char *entryname,struct Entry *pentry,int mode)
{
	int ret,offset,i;
	int cluster_addr;
	char uppername[80];
	unsigned short next,seed;
	for(i=0;i< strlen(entryname);i++)
		uppername[i]= toupper(entryname[i]);
	uppername[i]= '\0';
	/*扫描根目录*/
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

	/*扫描子目录*/
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
		seed = curdir->FirstCluster;
		cluster_addr = DATA_OFFSET + (next-2) * CLUSTER_SIZE ;
 		while((next = GetFatCluster(seed))!=0xffff)
              	{
			seed = next;
			cluster_addr = DATA_OFFSET + (seed-2) * CLUSTER_SIZE ;
			while(offset<cluster_addr +CLUSTER_SIZE)
			{
				ret= GetEntry(pentry);
				offset += abs(ret);
				if(pentry->subdir == mode &&!strcmp((char*)pentry->short_name,uppername))
					return offset;
			}
             	}
		return -1;
	}
}
int judgebackslack(char *dir)
{
int i;
    for(i=0;i<strlen(dir);i++)
    {
        if(dir[i]=='/')
            break;
    }
    if(i==strlen(dir))
        return -1;
    return 1;
}
int jumptotarget(char * dir)
{
    int i;
    int pos=strlen(dir);

    if(dir[strlen(dir)-1]!='/')
    {
        for(i = strlen(dir)-1;i>=0;i--)
        {
            if(dir[i]=='/')
            {
                pos = i;
                break;
            }
        }
    }
    char * path = (char * )malloc(sizeof(char)*(strlen(dir)+1));
    strcpy(path,dir);
    path[pos+1]='\0';


    if(dir[0]=='/'){
        oldcur = curdir;
        olddirno=dirno;
        for(i=0;i<=dirno;i++)
        {
            oldfatherdir[i]=fatherdir[i];
        }
        dirno=0;
        curdir = NULL;
        dir++;
        path++;
    }
    if(strlen(path)==0)
    {
    return 1;
    }


    struct Entry *pentry;
    char tmp[200];
	int ret;
	if(olddirno == -1)
        olddirno=dirno;
    while(strlen(path)!=1 && strlen(path)!=0){
	pentry = (struct Entry*)malloc(sizeof(struct Entry));
    for(i=0;path[i]!='/';i++)
    {
        tmp[i] = path[i];
    }
    path += i;
    if(path[0]=='/')
        path++;
    tmp[i]='\0';
	ret = ScanEntry(tmp,pentry,1);
	if(ret < 0)
	{
		printf("no such dir\n");
		free(pentry);
		return -1;
	}
	dirno ++;
	fatherdir[dirno] = curdir;
	curdir = pentry;
	}
	return 1;
}
void returnorigin(){
    if(olddirno == -1)
        return;
    int i;
    curdir = oldcur;
    dirno = olddirno;
    for(i=0;i<=dirno;i++)
        fatherdir[i]=oldfatherdir[i];
    olddirno = -1;
    return;
}
/*
*参数：dir，类型：char
*返回值：1，成功；-1，失败
*功能：改变目录到父目录或子目录
*/
int fd_cd(char *dir)
{
	struct Entry *pentry;
	int ret,i,j;
    char dirname[20];
    if(!strcmp(dir,"."))
	{
		return 1;
	}
	if(!strcmp(dir,"..") && curdir==NULL)
		return 1;
	/*返回上一级目录*/
	if(!strcmp(dir,"..") && curdir!=NULL)
	{
		curdir = fatherdir[dirno];
		dirno--;
		return 1;
	}
	pentry = (struct Entry*)malloc(sizeof(struct Entry));

	ret = ScanEntry(dir,pentry,1);
	if(ret < 0)
	{
        if(judgebackslack(dir)>0){
        olddirno = -1;
        ret = jumptotarget(dir);
        }
        if(ret<0){
            printf("no such dir\n");
            returnorigin();
            free(pentry);
            return -1;
        }
        else{
            if(dir[strlen(dir)-1]!='/'){
                for(i=strlen(dir)-1;i>=0;i--){
                    if(dir[i]=='/')
                        break;
                }
                for(j=0,i++;i<strlen(dir);i++,j++)
                {
                    dirname[j] = dir[i];
                }
                dirname[j]='\0';
                fd_cd(dirname);
            }
            return 1;
        }
		free(pentry);
		return -1;
	}
	dirno ++;
	fatherdir[dirno] = curdir;
	curdir = pentry;
	return 1;
}

/*
*参数：prev，类型：unsigned char
*返回值：下一簇
*在fat表中获得下一簇的位置
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
*参数：cluster，类型：unsigned short
*返回值：void
*功能：清除fat表中的簇信息
*/
void ClearFatCluster(unsigned short cluster)
{
	int index;
	index = cluster * 2;

	fatbuf[index]=0x00;
	fatbuf[index+1]=0x00;

}


/*
*将改变的fat表值写回fat表
*/
int WriteFat()
{
	if(lseek(fd,FAT_ONE_OFFSET,SEEK_SET)<0)
	{
		perror("lseek failed");
		return -1;
	}
	if(write(fd,fatbuf,512*256)<0)
	{
		perror("read failed");
		return -1;
	}
	if(lseek(fd,FAT_TWO_OFFSET,SEEK_SET)<0)
	{
		perror("lseek failed");
		return -1;
	}
	if((write(fd,fatbuf,512*256))<0)
	{
		perror("read failed");
		return -1;
	}
	return 1;
}

/*
*读fat表的信息，存入fatbuf[]中
*/
int ReadFat()
{
	if(lseek(fd,FAT_ONE_OFFSET,SEEK_SET)<0)
	{
		perror("lseek failed");
		return -1;
	}
	if(read(fd,fatbuf,512*256)<0)
	{
		perror("read failed");
		return -1;
	}
	return 1;
}


/*
*参数：filename，类型：char
*返回值：1，成功；-1，失败
*功能;删除当前目录下的文件
*/
int fd_df(char *filename)
{
	struct Entry *pentry;
	int ret,i,j;
	unsigned char c;
	unsigned short seed,next;
    char dirname[20];

	pentry = (struct Entry*)malloc(sizeof(struct Entry));

	/*扫描当前目录查找文件*/
	ret = ScanEntry(filename,pentry,0);
	if(ret<0)
	{
        if(judgebackslack(filename)>0){
        if(filename[strlen(filename)-1]=='/')
        {
            printf("check your format");
            return 1;
        }
        olddirno = -1;
        ret = jumptotarget(filename);
        }
        if(ret<0){
            printf("no such file\n");
            returnorigin();
            free(pentry);
            return -1;
        }
        else{
            if(filename[strlen(filename)-1]!='/'){
                for(i=strlen(filename)-1;i>=0;i--){
                    if(filename[i]=='/')
                        break;
                }
                for(j=0,i++;i<strlen(filename);i++,j++)
                {
                    dirname[j] = filename[i];
                }
                dirname[j]='\0';
                ret = fd_df(dirname);
                returnorigin();
            }
            return ret;
        }
		free(pentry);
		return -1;
	}

	/*清除fat表项*/
	seed = pentry->FirstCluster;
	while((next = GetFatCluster(seed))!=0xffff)
	{
		ClearFatCluster(seed);
		seed = next;

	}

	ClearFatCluster( seed );

	/*清除目录表项*/
	c=0xe5;

    ret = ret-32;
	if(lseek(fd,ret,SEEK_SET)<0)
		perror("lseek fd_df failed");
	if(write(fd,&c,1)<0)
		perror("write failed");


	/*if(lseek(fd,ret-0x40,SEEK_SET)<0)
		perror("lseek fd_df failed");
	if(write(fd,&c,1)<0)
		perror("write failed");*/

	free(pentry);
	if(WriteFat()<0)
		exit(1);
	return 1;
}

void itoa(int n)
{
if(n) itoa(n/2);
else return;
printf("%d",n%2);
}
/*
*参数：filename，类型：char，创建文件的名称
size，    类型：int，文件的大小
*返回值：1，成功；-1，失败
*功能：在当前目录下创建文件
*/
int fd_cf1(char *filename,int size)
{

	struct Entry *pentry;
	int ret,i=0,cluster_addr,offset;
	unsigned short cluster,clusterno[100];
	unsigned char c[DIR_ENTRY_SIZE];
	int index,clustersize;
	unsigned char buf[DIR_ENTRY_SIZE];
	pentry = (struct Entry*)malloc(sizeof(struct Entry));
	unsigned short next,seed;
    char dirname[20];
    int j;

	clustersize = (size / (CLUSTER_SIZE));

	if(size % (CLUSTER_SIZE) != 0)
		clustersize ++;

	//扫描根目录，是否已存在该文件名
	ret = ScanEntry(filename,pentry,0);
	if(ret<0 && judgebackslack(filename)>0)
	{
        if(judgebackslack(filename)>0){
        if(filename[strlen(filename)-1]=='/')
        {
            printf("check your format");
            return 1;
        }
        olddirno = -1;
        ret = jumptotarget(filename);
        }
        if(ret<0){
            printf("no such dir\n");
            returnorigin();
            free(pentry);
            return -1;
        }
        else{
            if(filename[strlen(filename)-1]!='/'){
                for(i=strlen(filename)-1;i>=0;i--){
                    if(filename[i]=='/')
                        break;
                }
                for(j=0,i++;i<strlen(filename);i++,j++)
                {
                    dirname[j] = filename[i];
                }
                dirname[j]='\0';
                fd_cf1(dirname,size);
                returnorigin();
            }
            return 1;
        }
		free(pentry);
		return -1;
    }

	if (ret<0)
	{
		/*查询fat表，找到空白簇，保存在clusterno[]中*/
		for(cluster=2;cluster<65536;cluster++)
		{
			index = cluster *2;
			if(fatbuf[index]==0x00&&fatbuf[index+1]==0x00)
			{
				clusterno[i] = cluster;

				i++;
				if(i==clustersize)
					break;

			}

		}

		/*在fat表中写入下一簇信息*/
		for(i=0;i<clustersize-1;i++)
		{
			index = clusterno[i]*2;

			fatbuf[index] = (clusterno[i+1] &  0x00ff);
			fatbuf[index+1] = ((clusterno[i+1] & 0xff00)>>8);


		}
		/*最后一簇写入0xffff*/
		index = clusterno[i]*2;
		fatbuf[index] = 0xff;
		fatbuf[index+1] = 0xff;

		if(curdir==NULL)  /*往根目录下写文件*/
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


				/*找出空目录项或已删除的目录项*/
				else
				{
					offset = offset-abs(ret);
					for(i=0;i<=strlen(filename);i++)
					{
						c[i]=toupper(filename[i]);
					}
					for(;i<=10;i++)
						c[i]='\0';

					c[11] = 0x01;
                    time_t timep;
                    struct tm *p;
                    time(&timep);
                    p=gmtime(&timep);
                    c[23]=((p->tm_hour)<<3)|((p->tm_min)>>3);
                    c[22]=((p->tm_min & (0x7))<<5)|((p->tm_sec)/2);

                    c[25]=((p->tm_year)<<1)|((p->tm_mon)>>3);
                    c[24]=((p->tm_mon & 0x7)<<5)|(p->tm_mday);

					/*写第一簇的值*/
					c[26] = (clusterno[0] &  0x00ff);
					c[27] = ((clusterno[0] & 0xff00)>>8);

					/*写文件的大小*/
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

					return 1;
				}

			}
		}
		else
		{
			seed = curdir->FirstCluster;
			next = curdir->FirstCluster;
			do{
			seed = next;
			cluster_addr = (seed -2 )*CLUSTER_SIZE + DATA_OFFSET;
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
						c[i]='\0';

					c[11] = 0x01;
                    			time_t timep;
                    			struct tm *p;
                    			time(&timep);
                    			p=gmtime(&timep);
                    			c[23]=((p->tm_hour)<<3)|((p->tm_min)>>3);
                    			c[22]=((p->tm_min & (0x7))<<5)|((p->tm_sec)/2);

                    			c[25]=((p->tm_year)<<1)|((p->tm_mon)>>3);
                   	 		c[24]=((p->tm_mon & 0x7)<<5)|(p->tm_mday);

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

					return 1;
				}

			}
			}while((next = GetFatCluster(seed))!=0xffff);
		}
	}
	else
	{
		printf("This filename is exist\n");
		free(pentry);
		return -1;
	}
	return 1;

}

void do_usage()
{
	printf("please input a command, including followings:\n\t"
	"ls\t(-l)\t\tlist all files\n\tcd <dir>\t\tchange direcotry\n\t"
	"cf <filename> <size>\tcreate a file\n\tdf <file>\t\t"
	"delete a file\n\texit\t\t\texit this system\n\t"
	"cfc <filename> <content/<+outside file> \tcreate a new file with real content\n\t"
	"cat <filename>\tdisplay the file content\n"
	"\tbat <integer> <integer>\t test command\n"
	"\tmkdir <dirname> make a subdir\n\t"
	"rm (-r)\t<file/dir name>\n\t"
	"rmdir \t<dirname> \n");
}

int fd_mkdir(char *filename)
{

	struct Entry *pentry;
	int ret,i=0,j,cluster_addr,offset;
	unsigned short cluster,clusterno[100];
	char dirname[25];
	unsigned char c[DIR_ENTRY_SIZE];
	int index,clustersize;
	unsigned char buf[DIR_ENTRY_SIZE];
	pentry = (struct Entry*)malloc(sizeof(struct Entry));
    unsigned short next,seed;

	clustersize = (32*512 / (CLUSTER_SIZE));

	if(32*512 % (CLUSTER_SIZE) != 0)
		clustersize ++;

	//扫描根目录，是否已存在该文件名
	ret = ScanEntry(filename,pentry,1);
	if(ret<0 && judgebackslack(filename)>0)
	{
        if(judgebackslack(filename)>0){
        if(filename[strlen(filename)-1]=='/')
        {
            printf("check your format");
            return 1;
        }
        olddirno = -1;
        ret = jumptotarget(filename);
        }
        if(ret<0){
            printf("no such dir\n");
            returnorigin();
            free(pentry);
            return -1;
        }
        else{
            if(filename[strlen(filename)-1]!='/'){
                for(i=strlen(filename)-1;i>=0;i--){
                    if(filename[i]=='/')
                        break;
                }
                for(j=0,i++;i<strlen(filename);i++,j++)
                {
                    dirname[j] = filename[i];
                }
                dirname[j]='\0';
                fd_mkdir(dirname);
                returnorigin();
            }
            return 1;
        }
		free(pentry);
		return -1;
    }
	if (ret<0)
	{
		/*查询fat表，找到空白簇，保存在clusterno[]中*/
		for(cluster=2;cluster<65536;cluster++) //???????????????
		{
			index = cluster *2;
			if(fatbuf[index]==0x00&&fatbuf[index+1]==0x00)
			{
				clusterno[i] = cluster;

				i++;
				if(i==clustersize)
					break;

			}

		}

		/*在fat表中写入下一簇信息*/
		for(i=0;i<clustersize-1;i++)
		{
			index = clusterno[i]*2;

			fatbuf[index] = (clusterno[i+1] &  0x00ff);
			fatbuf[index+1] = ((clusterno[i+1] & 0xff00)>>8);


		}
		/*最后一簇写入0xffff*/
		index = clusterno[i]*2;
		fatbuf[index] = 0xff;
		fatbuf[index+1] = 0xff;

		if(curdir==NULL)  /*往根目录下写文件*/
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


				/*找出空目录项或已删除的目录项*/
				else
				{
					offset = offset-abs(ret);
					for(i=0;i<=strlen(filename);i++)
					{
						c[i]=toupper(filename[i]);
					}
					for(;i<=10;i++)
						c[i]='\0';

					c[11] = 0x11;
                    time_t timep;
                    struct tm *p;
                    time(&timep);
                    p=gmtime(&timep);
                    c[23]=((p->tm_hour)<<3)|((p->tm_min)>>3);
                    c[22]=((p->tm_min & (0x7))<<5)|((p->tm_sec)/2);

                    c[25]=((p->tm_year)<<1)|((p->tm_mon)>>3);
                    c[24]=((p->tm_mon & 0x7)<<5)|(p->tm_mday);
					/*写第一簇的值*/
					c[26] = (clusterno[0] &  0x00ff);
					c[27] = ((clusterno[0] & 0xff00)>>8);

					/*写文件的大小*/
					c[28] = ((0) &  0x000000ff);
					c[29] = (((0) & 0x0000ff00)>>8);
					c[30] = (((0)& 0x00ff0000)>>16);
					c[31] = (((0)& 0xff000000)>>24);

					if(lseek(fd,offset,SEEK_SET)<0)
						perror("lseek fd_cf failed");
					if(write(fd,&c,DIR_ENTRY_SIZE)<0)
						perror("write failed");




					free(pentry);
					if(WriteFat()<0)
						exit(1);

					return 1;
				}

			}
		}
		else
		{
            seed = curdir->FirstCluster;
			next = curdir->FirstCluster;
			do{
			seed = next;
			cluster_addr = (seed -2 )*CLUSTER_SIZE + DATA_OFFSET;
			if((ret= lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("lseek cluster_addr failed");
			offset = cluster_addr;//////////////////////////////////////////////////////////////////////
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
						c[i]='\0';

					c[11] = 0x11;
                    time_t timep;
                    struct tm *p;
                    time(&timep);
                    p=gmtime(&timep);
                    c[23]=((p->tm_hour)<<3)|((p->tm_min)>>3);
                    c[22]=((p->tm_min & (0x7))<<5)|((p->tm_sec)/2);

                    c[25]=((p->tm_year)<<1)|((p->tm_mon)>>3);
                    c[24]=((p->tm_mon & 0x7)<<5)|(p->tm_mday);

					c[26] = (clusterno[0] &  0x00ff);
					c[27] = ((clusterno[0] & 0xff00)>>8);

					c[28] = ((0) &  0x000000ff);
					c[29] = (((0) & 0x0000ff00)>>8);
					c[30] = (((0)& 0x00ff0000)>>16);
					c[31] = (((0)& 0xff000000)>>24);

					if(lseek(fd,offset,SEEK_SET)<0)
						perror("lseek fd_cf failed");
					if(write(fd,&c,DIR_ENTRY_SIZE)<0)
						perror("write failed");




					free(pentry);
					if(WriteFat()<0)
						exit(1);

					return 1;
				}

			}
			}while((next = GetFatCluster(seed))!=0xffff);
		}
	}
	else
	{
		printf("This filename is exist\n");
		free(pentry);
		return -1;
	}
	return 1;
}
int fd_rmdir(char * filename)
{
    struct Entry *pentry,*tmpentry;
	int ret,oldret,i,j;
	unsigned char c,buf[32];
	unsigned short seed,next;
	char dirname[20];
	int offset;

	pentry = (struct Entry*)malloc(sizeof(struct Entry));
    if(filename[0]=='\0')
    {
        printf("emptyname");
    }

	/*扫描当前目录查找文件*/
	ret = ScanEntry(filename,pentry,1);
	if(ret<0)
	{
        if(judgebackslack(filename)>0){
        if(filename[strlen(filename)-1]=='/')
        {
            printf("check your format");
            return 1;
        }
        olddirno = -1;
        ret = jumptotarget(filename);
        }
        if(ret<0){
            printf("no such dir\n");
            returnorigin();
            free(pentry);
            return -1;
        }
        else{
            if(filename[strlen(filename)-1]!='/'){
                for(i=strlen(filename)-1;i>=0;i--){
                    if(filename[i]=='/')
                        break;
                }
                for(j=0,i++;i<strlen(filename);i++,j++)
                {
                    dirname[j] = filename[i];
                }
                dirname[j]='\0';
                fd_rmdir(dirname);
                returnorigin();
            }
            return 1;
        }
		free(pentry);
		return -1;
	}
    oldret = ret;
    int cluster_addr = (pentry->FirstCluster -2 )*CLUSTER_SIZE + DATA_OFFSET;
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
					offset = offset - abs(ret);
					char subdirname[11];
					for(i=0;i<11;i++)
                        {
                            subdirname[i]=buf[i];
                        }
                    printf("%s is deleted.\n",subdirname);
					if((buf[11]&(0x10)) !=0 )
					{
                        tmpentry = (struct Entry*)malloc(sizeof(struct Entry));
                        dirno ++;
                        fatherdir[dirno] = curdir;
                        curdir = pentry;
                        ret = ScanEntry(subdirname,tmpentry,1);
                        fd_rmdir(subdirname);
                        curdir = fatherdir[dirno];
                        dirno--;
                        lseek(fd,offset,SEEK_SET);
                        read(fd,buf,DIR_ENTRY_SIZE);
					}
					if(buf[0]!=0xe5&&buf[0]!=0x00){
                    buf[0]=0x00;
					buf[1]=0x00;
					int firstCluster = RevByte(buf[26],buf[27]);
						seed = firstCluster;
                        while((next = GetFatCluster(seed))!=0xffff)
                        {
                            ClearFatCluster(seed);
                            seed = next;
                        }
                    ClearFatCluster( seed );
					if(lseek(fd,offset,SEEK_SET)<0)
						perror("lseek fd_cf failed");
					if(write(fd,&buf,10)<0)
						perror("write failed");
					if(WriteFat()<0)
						exit(1);
                    }
                    if((ret= lseek(fd,offset,SEEK_SET))<0)
                        perror("lseek cluster_addr failed");
                }
            }
	/*清除fat表项*/
	seed = pentry->FirstCluster;
	while((next = GetFatCluster(seed))!=0xffff)
	{
		ClearFatCluster(seed);
		seed = next;
	}

	ClearFatCluster( seed );

	/*清除目录表项*/
	c=0xe5;

    oldret = oldret - 32;
	if(lseek(fd,oldret,SEEK_SET)<0)
		perror("lseek fd_df failed");
	if(write(fd,&c,1)<0)
		perror("write failed");


	free(pentry);
	if(WriteFat()<0)
		exit(1);
	return 1;
}
int dirIsEmpty(char * filename)
{
    struct Entry *pentry,*tmpentry;
	int ret,oldret,i,j;
	unsigned char c,buf[32];
	unsigned short seed,next;
	char dirname[20];
	int offset;

	pentry = (struct Entry*)malloc(sizeof(struct Entry));
    if(filename[0]=='\0')
    {
        printf("emptyname");
    }

	/*扫描当前目录查找文件*/
	ret = ScanEntry(filename,pentry,1);
	if(ret<0)
	{
        if(judgebackslack(filename)>0){
        if(filename[strlen(filename)-1]=='/')
        {
            printf("check your format");
            return 1;
        }
        olddirno = -1;
        ret = jumptotarget(filename);
        }
        if(ret<0){
            printf("no such dir\n");
            returnorigin();
            free(pentry);
            return -1;
        }
        else{
            if(filename[strlen(filename)-1]!='/'){
                for(i=strlen(filename)-1;i>=0;i--){
                    if(filename[i]=='/')
                        break;
                }
                for(j=0,i++;i<strlen(filename);i++,j++)
                {
                    dirname[j] = filename[i];
                }
                dirname[j]='\0';
                ret = dirIsEmpty(dirname);
                returnorigin();
            }
            return ret;
        }
		free(pentry);
		return -1;
	}
    oldret = ret;
    int cluster_addr = (pentry->FirstCluster -2 )*CLUSTER_SIZE + DATA_OFFSET;
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
					return -1;
                }
            }

	return 1;
}
int fd_cat(char *filename)
{
    struct Entry *pentry;
	int ret,i=0,cluster_addr,offset;
	unsigned short cluster,clusterno[100];
	unsigned char c[DIR_ENTRY_SIZE];
	int index,clustersize;
	unsigned char buf[DIR_ENTRY_SIZE];
	pentry = (struct Entry*)malloc(sizeof(struct Entry));
	unsigned short next,seed;
    char dirname[20];
    char content[2048];
    int j;

	//扫描根目录，是否已存在该文件名
	ret = ScanEntry(filename,pentry,0);
    clustersize = (pentry->size / (CLUSTER_SIZE));
    if(pentry->size % (CLUSTER_SIZE)!=0)
        clustersize++;
	if(ret<0 && judgebackslack(filename)>0)
	{
        if(judgebackslack(filename)>0){
        if(filename[strlen(filename)-1]=='/')
        {
            printf("check your format");
            return 1;
        }
        olddirno = -1;
        ret = jumptotarget(filename);
        }
        if(ret<0){
            printf("no such file\n");
            returnorigin();
            free(pentry);
            return -1;
        }
        else{
            if(filename[strlen(filename)-1]!='/'){
                for(i=strlen(filename)-1;i>=0;i--){
                    if(filename[i]=='/')
                        break;
                }
                for(j=0,i++;i<strlen(filename);i++,j++)
                {
                    dirname[j] = filename[i];
                }
                dirname[j]='\0';
                fd_cat(dirname);
                returnorigin();
            }
            return 1;
        }
		free(pentry);
		return -1;
    }
	if(ret<0)
	{
        printf("file not found\n");
        free(pentry);
        return -1;
	}
	else{
	seed = pentry->FirstCluster;
	for(i=0;i<clustersize;i++){
			cluster_addr = DATA_OFFSET + (seed-2) * CLUSTER_SIZE ;
			if(lseek(fd,cluster_addr,SEEK_SET)<0)
				perror("lseek ROOTDIR_OFFSET failed");

			if((read(fd,content,CLUSTER_SIZE))<0)
				perror("write entry failed");
			puts(content);
			seed = GetFatCluster(seed);
			if(seed==0xffff)
                break;
		}

	}
}
int fd_cf2(char *filename,int size,char *input_content)
{

	struct Entry *pentry;
	int ret,i=0,j,cluster_addr,offset,last_size;
	unsigned short cluster,clusterno[100];
	unsigned char c[DIR_ENTRY_SIZE];
	int index,clustersize;
	unsigned char buf[DIR_ENTRY_SIZE];
	unsigned short seed,next;
	char temp[CLUSTER_SIZE+1];
	char dirname[20];
	pentry = (struct Entry*)malloc(sizeof(struct Entry));

	last_size = size;
	clustersize = (size / (CLUSTER_SIZE));

	if(size % (CLUSTER_SIZE) != 0)
		clustersize ++;

	//扫描根目录，是否已存在该文件名
	ret = ScanEntry(filename,pentry,0);
	if(ret<0 && judgebackslack(filename)>0)
	{
        if(judgebackslack(filename)>0){
        if(filename[strlen(filename)-1]=='/')
        {
            printf("check your format");
            return 1;
        }
        olddirno = -1;
        ret = jumptotarget(filename);
        }
        if(ret<0){
            printf("no such dir\n");
            returnorigin();
            free(pentry);
            return -1;
        }
        else{
            if(filename[strlen(filename)-1]!='/'){
                for(i=strlen(filename)-1;i>=0;i--){
                    if(filename[i]=='/')
                        break;
                }
                for(j=0,i++;i<strlen(filename);i++,j++)
                {
                    dirname[j] = filename[i];
                }
                dirname[j]='\0';
                fd_cf2(dirname,size,input_content);
                returnorigin();
            }
            return 1;
        }
		free(pentry);
		return -1;
    }
	if (ret<0)
	{
		/*查询fat表，找到空白簇，保存在clusterno[]中*/
		for(cluster=2;cluster<65536;cluster++)
		{
			index = cluster *2;
			if(fatbuf[index]==0x00&&fatbuf[index+1]==0x00)
			{
				clusterno[i] = cluster;

				i++;
				if(i==clustersize)
					break;

			}

		}

		/*在fat表中写入下一簇信息*/
		for(i=0;i<clustersize-1;i++)
		{
			index = clusterno[i]*2;

			fatbuf[index] = (clusterno[i+1] &  0x00ff);
			fatbuf[index+1] = ((clusterno[i+1] & 0xff00)>>8);


		}
		/*最后一簇写入0xffff*/
		index = clusterno[i]*2;
		fatbuf[index] = 0xff;
		fatbuf[index+1] = 0xff;

		for(i=0;i<clustersize;i++){
			cluster_addr = DATA_OFFSET + (clusterno[i]-2) * CLUSTER_SIZE ;
			if(lseek(fd,cluster_addr,SEEK_SET)<0)
				perror("lseek ROOTDIR_OFFSET failed");

			if((write(fd,input_content,CLUSTER_SIZE))<0)
				perror("write entry failed");
			input_content += CLUSTER_SIZE;
		}

		if(curdir==NULL)  /*往根目录下写文件*/
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


				/*找出空目录项或已删除的目录项*/
				else
				{
					offset = offset-abs(ret);
					for(i=0;i<=strlen(filename);i++)
					{
						c[i]=toupper(filename[i]);
					}
					for(;i<=10;i++)
						c[i]='\0';

					c[11] = 0x01;
                    time_t timep;
                    struct tm *p;
                    time(&timep);
                    p=gmtime(&timep);
                    c[23]=((p->tm_hour)<<3)|((p->tm_min)>>3);
                    c[22]=((p->tm_min & (0x7))<<5)|((p->tm_sec)/2);

                    c[25]=((p->tm_year)<<1)|((p->tm_mon)>>3);
                    c[24]=((p->tm_mon & 0x7)<<5)|(p->tm_mday);

					/*写第一簇的值*/
					c[26] = (clusterno[0] &  0x00ff);
					c[27] = ((clusterno[0] & 0xff00)>>8);

					/*写文件的大小*/
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

					return 1;
				}

			}
		}
		else
		{
			seed = curdir->FirstCluster;
			next = curdir->FirstCluster;
			do{
			seed = next;
			cluster_addr = (seed -2 )*CLUSTER_SIZE + DATA_OFFSET;
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
						c[i]='\0';

					c[11] = 0x01;
                    time_t timep;
                    struct tm *p;
                    time(&timep);
                    p=gmtime(&timep);
                    c[23]=((p->tm_hour)<<3)|((p->tm_min)>>3);
                    c[22]=((p->tm_min & (0x7))<<5)|((p->tm_sec)/2);

                    c[25]=((p->tm_year)<<1)|((p->tm_mon)>>3);
                    c[24]=((p->tm_mon & 0x7)<<5)|(p->tm_mday);

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

					return 1;
				}
			}
			}while((next = GetFatCluster(seed))!=0xffff);
		}
	}
	else
	{
		printf("This filename is exist\n");
		free(pentry);
		return -1;
	}
	return 1;

}
int main()
{
	char input[10000],input_file[10000];
	int size=0,i,j,k;
	char name[12];
	olddirno = -1;
	if((fd = open(DEVNAME,O_RDWR))<0)
		perror("open failed");
	ScanBootSector();
	if(ReadFat()<0)
		exit(1);
	do_usage();
	while (1)
	{
        if(dirno!=0)
        {
            printf("/");
            for(i=2;i<=dirno;i++)
            {
                printf("%s/",fatherdir[i]->short_name);
            }
            printf("%s/",curdir->short_name);
        }
		printf(">");
		scanf("%s",input);

		if (strcmp(input, "exit") == 0)
			break;
		else if (strcmp(input, "ls") == 0){
			char c='\0';
			while(c!='\n')
			{
                c=getchar();
                if(c=='-')
                {
                    if((c=getchar())=='l')
                    {
                        fd_ls(1);goto SKIP;
                    }
                    else{
                        printf("Wrong arguments!");goto SKIP;
                    }
                }

			}
			fd_ls(0);
			SKIP:c=3;
        }
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
		else if(strcmp(input, "rm")==0)
		{
            scanf("%s", name);
            if(strcmp(name,"-r")==0)
            {
                scanf("%s",name);
                fd_rmdir(name);
            }
            else{
                i = fd_df(name);
                if(i<0)
                {
                    i=dirIsEmpty(name);
                    if(i>0)
                    {
                        i = fd_rmdir(name);
                        if(i>0)
                        {
                            printf("The empty dir is deleted.\n");
                        }
                    }
                    else{
                        printf("no such dir or the dir is not empry.\n");
                    }
                }
            }
		}
		else if(strcmp(input, "cf") == 0)
		{
			scanf("%s", name);
			scanf("%s", input);
			size = atoi(input);
			fd_cf1(name,size);
		}
		else if(strcmp(input,"mkdir")==0)
		{
            scanf("%s", name);
			fd_mkdir(name);
		}
		else if(strcmp(input,"rmdir")==0)
		{
            scanf("%s",name);
            fd_rmdir(name);
		}
		else if(strcmp(input,"bat")==0)
		{
            int begin,end,ite;
            scanf("%d",&begin);scanf("%d",&end);
            for(ite = begin; ite != end; ite++)
            {
                fd_cf1(fileNAME[ite],4);
            }
		}
		else if(strcmp(input,"cat")==0)
		{
            scanf("%s",name);
            fd_cat(name);
		}
		else if(strcmp(input,"cfc")==0)
		{
            scanf("%s",name);
            scanf("%s", input);
            if(input[0]=='<'&&input[1]=='\0'){
				k=0;
				scanf("%s", input_file);
				FILE *fp=fopen(input_file,"r");
				while(!feof(fp)){
					fscanf(fp,"%c",&input[k]);
					k++;
				}
				size = k-1;
				fd_cf2(name,size,input);
			}
			else{
				for(j=0;j<10000;j++){
					if(input[j] == '\0'){
						size = j;
						break;
					}
				}

			fd_cf2(name,size,input);
            }
		}
		else
			do_usage();
	}

	return 0;
}



