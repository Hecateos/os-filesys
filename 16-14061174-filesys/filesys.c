#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<string.h>
#include<ctype.h>
#include<time.h>
#include "filesys.h"


#define RevByte(low,high) ((high)<<8|(low))
#define RevWord(lowest,lower,higher,highest) ((highest)<< 24|(higher)<<16|(lower)<<8|lowest) 

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
		"MediaType \t\t%d\n"
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

	*year = ((date & MASK_YEAR)>> 9 )+1980;
	*month = ((date & MASK_MONTH)>> 5);
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

	*hour = ((time & MASK_HOUR )>>11);
	*min = (time & MASK_MIN)>> 5;
	*sec = (time & MASK_SEC) * 2;
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

		/*命名格式化，注意结尾的'\0'*/
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
*功能：显示当前目录的内容
*返回值：1，成功；-1，失败
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

	else /*显示子目录*/
	{
        unsigned short cluster_iterator = curdir->FirstCluster;
        do{
            cluster_addr = DATA_OFFSET + (cluster_iterator-2) * CLUSTER_SIZE ;
            if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
                perror("lseek cluster_addr failed");

            offset = cluster_addr;
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
        }while((cluster_iterator = GetFatCluster(cluster_iterator))!=0xffff);
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
		return -1;
	}
}



/*
*参数：dir，类型：char
*返回值：1，成功；-1，失败
*功能：改变目录到父目录或子目录
*/
int fd_cd(char *dir)
{
	struct Entry *pentry;
	int ret;
    int offset;
    int i;
    dir[strlen(dir)]=dir[strlen(dir)+1]='\0';
    struct Entry* copy_fatherdir[10];
    unsigned char target_dir[12];
    for(i=0;i<10;i++)
        copy_fatherdir[i]=fatherdir[i];
    struct Entry* copy_curdir=curdir;
    int copy_dirno=dirno;
    i=0;
	if(dir[i]=='.'){
        i++;
        if(dir[i]=='\0')
            return 1;
        if(dir[i]=='.'){
            if(copy_curdir!=NULL){
                copy_curdir = copy_fatherdir[dirno];
                copy_dirno--;
            }
            i++;
            if(dir[i]=='\0'){
                for(i=0;i<10;i++)
                    fatherdir[i]=copy_fatherdir[i];
                curdir=copy_curdir;
                dirno=copy_dirno;
                return 1;
            }
        }
        if(dir[i]!='/'){
            i=0;
            copy_curdir=curdir;
            copy_dirno=dirno;
        }
	}
	
    
    if(dir[i]=='/'){
          //返回到根目录
        if(i==0){
            copy_curdir=NULL;
            copy_dirno=0;
        }
        i++;
        int j,k;
        while(dir[i]!='\0'){
            j=0;
            while(dir[i]!='/'&&dir[i]!='\0'){
                target_dir[j]=dir[i];
                i++;
                j++;
            }
            i++;
            target_dir[j]='\0';
            int cluster_addr;
            char uppername[80];
            for(k=0;k<j;k++)
                uppername[k]= toupper(target_dir[k]);
            uppername[k]= '\0';
            /*扫描当前目录*/
            if(copy_curdir == NULL)  
            {
                if((ret = lseek(fd,ROOTDIR_OFFSET,SEEK_SET))<0)
                    perror ("lseek ROOTDIR_OFFSET failed");
                offset = ROOTDIR_OFFSET;
                while(offset<DATA_OFFSET)
                {
                    pentry = (struct Entry*)malloc(sizeof(struct Entry));
                    ret = GetEntry(pentry);
                    offset +=abs(ret);

                    if(pentry->subdir == 1 &&!strcmp((char*)pentry->short_name,uppername))
                        break;    
                }
                if(ret<0){
                    printf("no such dir\n");
                    free(pentry);
                    return -1;
                }
                else{
                    copy_dirno++;
                    copy_fatherdir[copy_dirno] = copy_curdir;
                    copy_curdir = pentry;
                    continue;
                }
            }

            /*扫描子目录*/
            else  
            {
                unsigned short cluster_iterator = copy_curdir->FirstCluster;
                do{
                    cluster_addr = DATA_OFFSET + (cluster_iterator -2)*CLUSTER_SIZE;
                    if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
                        perror("lseek cluster_addr failed");
                    offset= cluster_addr;
                    while(offset<cluster_addr + CLUSTER_SIZE)
                    {
                        pentry = (struct Entry*)malloc(sizeof(struct Entry));
                        ret= GetEntry(pentry);
                        offset += abs(ret);
                        if(pentry->subdir == 1 &&!strcmp((char*)pentry->short_name,uppername)){
                            goto lable1;
                        }
                    }
                     
                }while((cluster_iterator = GetFatCluster(cluster_iterator))!=0xffff);
    lable1:     if(cluster_iterator!=0xffff){
                    copy_dirno++;
                    copy_fatherdir[copy_dirno]=copy_curdir;
                    copy_curdir=pentry;
                    continue;
                }
                else{
                    printf("no such dir\n");
                    free(pentry);
                    return -1;
                }
            }
        }
        for(i=0;i<10;i++)
            fatherdir[i]=copy_fatherdir[i];
        curdir=copy_curdir;
        dirno=copy_dirno;
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
	dirno++;
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
	if(write(fd,fatbuf,512*238)<0)
	{
		perror("read failed");
		return -1;
	}
	if(lseek(fd,FAT_TWO_OFFSET,SEEK_SET)<0)
	{
		perror("lseek failed");
		return -1;
	}
	if((write(fd,fatbuf,512*238))<0)
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
	if(read(fd,fatbuf,512*238)<0)
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
	int ret;
	unsigned char c;
	unsigned short seed,next;

	pentry = (struct Entry*)malloc(sizeof(struct Entry));

	/*扫描当前目录查找文件*/
	ret = ScanEntry(filename,pentry,0);
	if(ret<0)
	{
		printf("no such file\n");
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
	if(lseek(fd,ret-0x20,SEEK_SET)<0)
		perror("lseek fd_df failed");
	if(write(fd,&c,1)<0)
		perror("write failed");  
//	if(lseek(fd,ret-0x40,SEEK_SET)<0)
//		perror("lseek fd_df failed");
//	if(write(fd,&c,1)<0)
//		perror("write failed");
	free(pentry);
	if(WriteFat()<0)
		exit(1);
	return 1;
}

int fd_cf(char *filename)
{
    time_t rawtime;
    struct tm * timeinfo;
    time(&rawtime);
    timeinfo = localtime ( &rawtime );
    int ye,mo,da,ho,mi,se;
    ye = timeinfo->tm_year-80;
    mo = timeinfo->tm_mon+1;
    da = timeinfo->tm_mday;
    ho = timeinfo->tm_hour;
    mi = timeinfo->tm_min;
    se = timeinfo->tm_sec;
    int ltime = (ho<<11)|(mi<<5)|(se>>1);
    int ldate = (ye<<9)|(mo<<5)|(da);
	struct Entry *pentry;
	int ret,i=0,j=0,cluster_addr,offset,size=0;
	unsigned short cluster,clusterno[100];
	unsigned char c[DIR_ENTRY_SIZE];
    char t,content[CLUSTER_SIZE];
	int index,clustersize;
	unsigned char buf[DIR_ENTRY_SIZE];
	pentry = (struct Entry*)malloc(sizeof(struct Entry));

	//扫描根目录，是否已存在该文件名
	ret = ScanEntry(filename,pentry,0);
	if (ret<0)
	{
        printf("请输入内容:\n");
        getchar();
		/*查询fat表，找到空白簇，保存在clusterno[]中*/
		for(cluster=2;cluster<1000;cluster++)
		{
            j=0;
			index = cluster *2;
			if(fatbuf[index]==0x00&&fatbuf[index+1]==0x00)
			{
				clusterno[i] = cluster;
				i++;
                do{
                    t=getchar();
                    content[j++]=t;
                }while(j<CLUSTER_SIZE && t!=EOF);
                size+=j;
                
                cluster_addr = (cluster -2 )*CLUSTER_SIZE + DATA_OFFSET;
                if((lseek(fd,cluster_addr,SEEK_SET))<0)
                    perror("lseek cluster_addr failed");
                if(write(fd,&content,j)<0)
                    perror("write failed");
                if(t==EOF){
                    putchar('\n');
                    break;
                }
			}
		}
        clustersize=i;
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
						c[i]=' ';

					c[11] = 0x01;
                    c[22] = ltime & 0x00ff;
                    c[23] = (ltime & 0xff00)>>8;
                    c[24] = ldate & 0x00ff;
                    c[25] = (ldate & 0xff00)>>8;
					/*写第一簇的值*/
					c[26] = (clusterno[0] &  0x00ff);
					c[27] = ((clusterno[0] & 0xff00)>>8);

					/*写文件的大小*/
					c[28] = (size &  0x000000ff);
					c[29] = ((size & 0x0000ff00)>>8);
					c[30] = ((size & 0x00ff0000)>>16);
					c[31] = ((size & 0xff000000)>>24);

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
							perror("read son dir failed");
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

					c[11] = 0x20;
                    c[22] = ltime & 0x00ff;
                    c[23] = (ltime & 0xff00)>>8;
                    c[24] = ldate & 0x00ff;
                    c[25] = (ldate & 0xff00)>>8;
					c[26] = (clusterno[0] &  0x00ff);
					c[27] = ((clusterno[0] & 0xff00)>>8);

					c[28] = (size &  0x000000ff);
					c[29] = ((size & 0x0000ff00)>>8);
					c[30] = ((size & 0x00ff0000)>>16);
					c[31] = ((size & 0xff000000)>>24);

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
	}
	else
	{
		printf("This filename is exist\n");
		free(pentry);
		return -1;
	}
	return 1;

}
/* 创建目录 */
int fd_mkdir(char *dir){
    //时间信息
    time_t rawtime;
    struct tm * timeinfo;
    time(&rawtime);
    timeinfo = localtime ( &rawtime );
    int ye,mo,da,ho,mi,se;
    ye = timeinfo->tm_year-80;
    mo = timeinfo->tm_mon+1;
    da = timeinfo->tm_mday;
    ho = timeinfo->tm_hour;
    mi = timeinfo->tm_min;
    se = timeinfo->tm_sec;
    int ltime = (ho<<11)|(mi<<5)|(se>>1);
    int ldate = (ye<<9)|(mo<<5)|(da);
    struct Entry *pentry;
	int ret,i=0,cluster_addr,offset;
	unsigned short cluster;
	unsigned char c[DIR_ENTRY_SIZE];
	int index;
	unsigned char buf[DIR_ENTRY_SIZE];
	pentry = (struct Entry*)malloc(sizeof(struct Entry));

	//扫描当前目录，是否已存在该目录名
	ret = ScanEntry(dir,pentry,0);
	if (ret<0)
	{
		/*查询fat表，找到空白簇*/
        ReadFat();
		for(cluster=2;cluster<1000;cluster++)
		{
			index = cluster *2;
			if(fatbuf[index]==0x00&&fatbuf[index+1]==0x00){
                ClearFatCluster(cluster);
                break;
            }
		}
		/*最后一簇写入0xffff*/
		fatbuf[index] = 0xff;
		fatbuf[index+1] = 0xff;

		if(curdir==NULL)  /*往根目录下写入目录*/
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
					for(i=0;i<=strlen(dir);i++)
					{
						c[i]=toupper(dir[i]);
					}			
					for(;i<=10;i++)
						c[i]=' ';

					c[11] = 0x10;
                    c[22] = ltime & 0x00ff;
                    c[23] = (ltime & 0xff00)>>8;
                    c[24] = ldate & 0x00ff;
                    c[25] = (ldate & 0xff00)>>8;
					/*写第一簇的值*/
					c[26] = (cluster & 0x00ff);
					c[27] = (cluster & 0xff00)>>8;

					/*写文件的大小*/
					c[28] = 0;
					c[29] = 0;
					c[30] = 0;
					c[31] = 0;

					if(lseek(fd,offset,SEEK_SET)<0)
						perror("lseek fd_mkdir failed");
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
							perror("read son dir failed");
						offset +=abs(ret);
					}
				}
				else
				{ 
					offset = offset - abs(ret);      
					for(i=0;i<=strlen(dir);i++)
					{
						c[i]=toupper(dir[i]);
					}
					for(;i<=10;i++)
						c[i]=' ';

					c[11] = 0x10;
                    c[22] = ltime & 0x00ff;
                    c[23] = (ltime & 0xff00)>>8;
                    c[24] = ldate & 0x00ff;
                    c[25] = (ldate & 0xff00)>>8;
					c[26] = (cluster & 0x00ff);
					c[27] = (cluster & 0xff00)>>8;

					c[28] = 0;
					c[29] = 0;
					c[30] = 0;
					c[31] = 0;

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
            //执行到这里说明curdir已满，要再给curdir分配一个簇
            for(i = 2;i<1000;i++){
                index = i *2;
                if(fatbuf[index]==0x00&&fatbuf[index+1]==0x00){
                    ClearFatCluster(i);
                    break;
                } 
            }
            fatbuf[index] = 0xff;
            fatbuf[index+1] = 0xff;
            index=curdir->FirstCluster*2;
            fatbuf[index] = i&0x00ff;
            fatbuf[index+1] = (i&0xff00)>>8;
            cluster_addr = (i-2 )*CLUSTER_SIZE + DATA_OFFSET;
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
							perror("read son dir failed");
						offset +=abs(ret);
					}
				}
				else
				{ 
					offset = offset - abs(ret);      
					for(i=0;i<=strlen(dir);i++)
					{
						c[i]=toupper(dir[i]);
					}
					for(;i<=10;i++)
						c[i]=' ';

					c[11] = 0x10;
                    c[22] = ltime & 0x00ff;
                    c[23] = (ltime & 0xff00)>>8;
                    c[24] = ldate & 0x00ff;
                    c[25] = (ldate & 0xff00)>>8;
					c[26] = (cluster & 0x00ff);
					c[27] = (cluster & 0xff00)>>8;

					c[28] = 0;
					c[29] = 0;
					c[30] = 0;
					c[31] = 0;

					if(lseek(fd,offset,SEEK_SET)<0)
						perror("lseek fd_mkdir failed");
					if(write(fd,&c,DIR_ENTRY_SIZE)<0)
						perror("write failed");
					free(pentry);
					if(WriteFat()<0)
						exit(1);
					return 1;
                }
            }
		}
	}
	else	{
		printf("This filename is exist\n");
		free(pentry);
		return -1;
	}
	return 1;
}

void do_usage()
{
	printf("please input a command, including followings:\n\tls\t\t\tlist all files\n\tcd <dir>\t\tchange direcotry\n\tcf <filename> <size>\tcreate a file\n\tdf <file>\t\tdelete a file\n\texit\t\t\texit this system\n");
}


int main()
{
	char input[10];
	int size=0;
	char name[12];
    char dir[100];
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
			scanf("%s", dir);
			fd_cd(dir);
		}
		else if(strcmp(input, "df") == 0)
		{
			scanf("%s", name);
			fd_df(name);
		}
		else if(strcmp(input, "cf") == 0)
		{
			scanf("%s", name);
			fd_cf(name);
		}
        else if(strcmp(input, "mkdir") == 0)
		{
			scanf("%s", name);
			fd_mkdir(name);
		}
		else
			do_usage();
	}	

	return 0;
}


