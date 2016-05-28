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
*¹¦ÄÜ£º´òÓ¡Æô¶¯Ïî¼ÇÂ¼
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

/*ÈÕÆÚ*/
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

/*Ê±¼ä*/
void findTime(unsigned short *hour,
			  unsigned short *min,
			  unsigned short *sec,
			  unsigned char info[2])
{
	int time;
	time = RevByte(info[0],info[1]);

	*hour = ((time)>>11)+8;
	*min = (time & MASK_MIN)>>5;
	*sec = (time & MASK_SEC)*2;
}

/*
*ÎÄ¼þÃû¸ñÊ½»¯£¬±ãÓÚ±È½Ï
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

/*²ÎÊý£ºentry£¬ÀàÐÍ£ºstruct Entry*
*·µ»ØÖµ£º³É¹¦£¬Ôò·µ»ØÆ«ÒÆÖµ£»Ê§°Ü£º·µ»Ø¸ºÖµ
*¹¦ÄÜ£º´Ó¸ùÄ¿Â¼»òÎÄ¼þ´ØÖÐµÃµ½ÎÄ¼þ±íÏî
*/
int GetEntry(struct Entry *pentry)
{
	int ret,i;
	int count = 0;
	unsigned char buf[DIR_ENTRY_SIZE], info[2];

	/*¶ÁÒ»¸öÄ¿Â¼±íÏî£¬¼´32×Ö½Ú*/
	if( (ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
		perror("read entry failed");
	count += ret;

	if(buf[0]==0xe5 || buf[0]== 0x00)
		return -1*count;
	else
	{
		/*³¤ÎÄ¼þÃû£¬ºöÂÔµô*/
		while (buf[11]== 0x0f) 
		{
			if((ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
				perror("read root dir failed");
			count += ret;
		}

		/*ÃüÃû¸ñÊ½»¯£¬Ö÷Òå½áÎ²µÄ'\0'*/
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
int clone(struct Entry *pentry,struct Entry *pentry1)
{
	if(pentry1!=NULL)
	{
		int i = 0;
		for (i=0 ;i<=10;i++)
			{
				pentry->short_name[i] = pentry1->short_name[i];
			}
		pentry->short_name[i] = '\0';


		pentry->hour = pentry1->hour;
		pentry->min = pentry1->min;
		pentry->sec = pentry1->sec;
		pentry->year = pentry1->year;
		pentry->month = pentry1->month;
		pentry->day = pentry1->day;
		pentry->FirstCluster = pentry1->FirstCluster;
		pentry->size = pentry1->size;
		pentry->readonly = pentry1->readonly;
		pentry->hidden = pentry1->hidden;
		pentry->system = pentry1->system;
		pentry->vlabel = pentry1->vlabel;
		pentry->subdir = pentry1->subdir;
		pentry->archive = pentry1->archive;
	}
}

/*
*¹¦ÄÜ£ºÏÔÊ¾µ±Ç°Ä¿Â¼µÄÄÚÈÝ
*·µ»ØÖµ£º1£¬³É¹¦£»-1£¬Ê§°Ü
*/
/*
*功能：显示当前目录的内容
*返回值：1，成功；-1，失败
*/
int fd_ls()
{
	int ret, offset,cluster_addr,size;
	struct Entry entry;
	unsigned short next;
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
			//printf("%d###%d###%d\n",ret,offset,DATA_OFFSET);
			if(ret > 0)
			{
				if(entry.subdir==1)
				{
						size = dirsize(entry.short_name);
						entry.size = size;
						if((lseek(fd,offset,SEEK_SET))<0)
							perror("lseek offset failed");
				}
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
				if(entry.subdir==1)
				{
						size = dirsize(entry.short_name);
						entry.size = size;
						if((lseek(fd,offset,SEEK_SET))<0)
							perror("lseek offset failed");
				}
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

		next = curdir->FirstCluster;
		while(GetFatCluster(next)!=0xffff){
			next = GetFatCluster(next);
			cluster_addr = DATA_OFFSET + (next-2) * CLUSTER_SIZE ;

			while(offset<cluster_addr +CLUSTER_SIZE)
			{
				ret = GetEntry(&entry);
				offset += abs(ret);
				if(ret > 0)
				{
					if(entry.subdir==1)
					{
						size = dirsize(entry.short_name);
						entry.size = size;
						if((lseek(fd,offset,SEEK_SET))<0)
							perror("lseek offset failed");
					}
					if((lseek(fd,offset,SEEK_SET))<0)
						perror("lseek offset failed");
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
	}
	return 0;
} 
int fd_lsaa()
{
	int ret, offset,cluster_addr,size;
	struct Entry entry;
	unsigned short next;
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
			//printf("%d###%d###%d\n",ret,offset,DATA_OFFSET);
			if(ret > 0)
			{
				if(entry.subdir==1)
				{
						size = dirsize(entry.short_name);
						entry.size = size;
						if((lseek(fd,offset,SEEK_SET))<0)
							perror("lseek offset failed");
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
						fd_lsa(entry.short_name);
						if((lseek(fd,offset,SEEK_SET))<0)
							perror("lseek offset failed");
				}
				else
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
				if(entry.subdir==1)
				{
						size = dirsize(entry.short_name);
						entry.size = size;
						if((lseek(fd,offset,SEEK_SET))<0)
							perror("lseek offset failed");
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
						fd_lsa(entry.short_name);
						if((lseek(fd,offset,SEEK_SET))<0)
							perror("lseek offset failed");
				}
				else
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

		next = curdir->FirstCluster;
		while(GetFatCluster(next)!=0xffff){
			next = GetFatCluster(next);
			cluster_addr = DATA_OFFSET + (next-2) * CLUSTER_SIZE ;

			while(offset<cluster_addr +CLUSTER_SIZE)
			{
				ret = GetEntry(&entry);
				offset += abs(ret);
				if(ret > 0)
				{
					if(entry.subdir==1)
					{
						size = dirsize(entry.short_name);
						entry.size = size;
						if((lseek(fd,offset,SEEK_SET))<0)
							perror("lseek offset failed");
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
						fd_lsa(entry.short_name);
						if((lseek(fd,offset,SEEK_SET))<0)
							perror("lseek offset failed");
					}
					if((lseek(fd,offset,SEEK_SET))<0)
						perror("lseek offset failed");
				}
				else
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
	return 0;
} 
int fd_lsf()
{
	int ret, offset,cluster_addr,size;
	struct Entry entry;
	unsigned short next;
	unsigned char buf[DIR_ENTRY_SIZE];
	if( (ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
		perror("read entry failed");
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
			//printf("%d###%d###%d\n",ret,offset,DATA_OFFSET);
			if(ret > 0)
			{
				if(entry.subdir==1)
				{
						size = dirsize(entry.short_name);
						entry.size = size;
						if((lseek(fd,offset,SEEK_SET))<0)
							perror("lseek offset failed");
				}
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
				if(entry.subdir==1)
				{
						size = dirsize(entry.short_name);
						entry.size = size;
						if((lseek(fd,offset,SEEK_SET))<0)
							perror("lseek offset failed");
				}
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

		next = curdir->FirstCluster;
		while(GetFatCluster(next)!=0xffff){
			next = GetFatCluster(next);
			cluster_addr = DATA_OFFSET + (next-2) * CLUSTER_SIZE ;

			while(offset<cluster_addr +CLUSTER_SIZE)
			{
				ret = GetEntry(&entry);
				offset += abs(ret);
				if(ret > 0)
				{
					if(entry.subdir==1)
					{
						size = dirsize(entry.short_name);
						entry.size = size;
						if((lseek(fd,offset,SEEK_SET))<0)
							perror("lseek offset failed");
					}
					if((lseek(fd,offset,SEEK_SET))<0)
						perror("lseek offset failed");
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
	}
	return 0;
} 
int fd_lsa(char *filename){
	struct Entry *pentry, entry1;
	int ret,cluster_addr,offset;
	unsigned char c;
	unsigned short seed,next;
	pentry = (struct Entry*)malloc(sizeof(struct Entry));
	//clone(pentry,curdir);
	ret = ScanEntry(filename,pentry,1);
		if(ret < 0)
			return -1;
		seed = pentry->FirstCluster;
		//if(fatbuf[seed*2] == 0x00 && fatbuf[seed*2+1] == 0x00)
		fd_cd(filename);
		while((next = GetFatCluster(seed))!=0xffff){
			cluster_addr = DATA_OFFSET + (seed-2) * CLUSTER_SIZE ;
			if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("lseek cluster_addr failed");

			offset = cluster_addr;

		/*Ö»¶ÁÒ»´ØµÄÄÚÈÝ*/
		
		

	
				while(offset<cluster_addr +CLUSTER_SIZE)
				{
					ret = GetEntry(&entry1);
					offset += abs(ret);
					if(ret >= 0)
					{
						if(entry1.subdir == 1){//ÊÇ×ÓÄ¿Â¼
							fd_lsf(entry1.short_name);
							fd_lsa(entry1.short_name);
						}
						else{//ÊÇÎÄ¼þ
							fd_lsf(entry1.short_name);
						}
					}
				}
			
				
				seed = next;
		}
		cluster_addr = DATA_OFFSET + (seed-2) * CLUSTER_SIZE ;
		if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
			perror("lseek cluster_addr failed");

		offset = cluster_addr;
		while(offset<cluster_addr +CLUSTER_SIZE){
			ret = GetEntry(&entry1);
			offset += abs(ret);
			if(ret >= 0)
			{
				if(entry1.subdir == 1){//ÊÇ×ÓÄ¿Â¼
					fd_lsf(entry1.short_name);
					fd_lsa(entry1.short_name);
				}
				else{//ÊÇÎÄ¼þ
					fd_lsf(entry1.short_name);
				}
			}
		}
		fd_cd("..");
		free(pentry);
		return 1;		
	}

/*
*²ÎÊý£ºentryname ÀàÐÍ£ºchar
£ºpentry    ÀàÐÍ£ºstruct Entry*
£ºmode      ÀàÐÍ£ºint£¬mode=1£¬ÎªÄ¿Â¼±íÏî£»mode=0£¬ÎªÎÄ¼þ
*·µ»ØÖµ£ºÆ«ÒÆÖµ´óÓÚ0£¬Ôò³É¹¦£»-1£¬ÔòÊ§°Ü
*¹¦ÄÜ£ºËÑË÷µ±Ç°Ä¿Â¼£¬²éÕÒÎÄ¼þ»òÄ¿Â¼Ïî
*/
int ScanEntry (char *entryname,struct Entry *pentry,int mode)
{
	int ret,offset,i;
	int cluster_addr;
	char uppername[80];
	unsigned short 	seed,next;;
	for(i=0;i< strlen(entryname);i++)
		uppername[i]= toupper(entryname[i]);
	uppername[i]= '\0';
	/*É¨Ãè¸ùÄ¿Â¼*/
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

	/*É¨Ãè×ÓÄ¿Â¼*/
	else  
	{
		seed = curdir->FirstCluster;
		while((next = GetFatCluster(seed))!=0xffff){
			cluster_addr = DATA_OFFSET + (seed -2)*CLUSTER_SIZE;
			if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("lseek cluster_addr failed");
			offset= cluster_addr;
			while(offset<cluster_addr + CLUSTER_SIZE)
			{
				ret= GetEntry(pentry);
				offset += abs(ret);
				if(pentry->subdir == mode &&!strcmp((char*)pentry->short_name,uppername))
					return offset;

				//printf("ScanEntrybegin4\n");

			}
			seed = next;
		}
		cluster_addr = DATA_OFFSET + (seed -2)*CLUSTER_SIZE;
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
*²ÎÊý£ºdir£¬ÀàÐÍ£ºchar
*·µ»ØÖµ£º1£¬³É¹¦£»-1£¬Ê§°Ü
*¹¦ÄÜ£º¸Ä±äÄ¿Â¼µ½¸¸Ä¿Â¼»ò×ÓÄ¿Â¼
*/
int fd_cd(char *dir)
{
	struct Entry *pentry;
	int ret;

	if(!strcmp(dir,"."))
	{
		return 1;
	}
	if(!strcmp(dir,"..") && curdir==NULL)
		return 1;
	/*·µ»ØÉÏÒ»¼¶Ä¿Â¼*/
	if(!strcmp(dir,"..") && curdir!=NULL)
	{
		curdir = fatherdir[dirno];
		dirno--; 
		return 1;
	}
	if(!strcmp(dir,"root"))
	{
		while(dirno!=0)
		{
			curdir = fatherdir[dirno];
			dirno--; 
		}
		return 1;
	}
	pentry = (struct Entry*)malloc(sizeof(struct Entry));
	//clone(pentry,curdir);
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
	return 1;
}

/*
*²ÎÊý£ºprev£¬ÀàÐÍ£ºunsigned char
*·µ»ØÖµ£ºÏÂÒ»´Ø
*ÔÚfat±íÖÐ»ñµÃÏÂÒ»´ØµÄÎ»ÖÃ
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
*²ÎÊý£ºcluster£¬ÀàÐÍ£ºunsigned short
*·µ»ØÖµ£ºvoid
*¹¦ÄÜ£ºÇå³ýfat±íÖÐµÄ´ØÐÅÏ¢
*/
void ClearFatCluster(unsigned short cluster)
{
	int index;
	index = cluster * 2;

	fatbuf[index]=0x00;
	fatbuf[index+1]=0x00;

}


/*
*½«¸Ä±äµÄfat±íÖµÐ´»Øfat±í
*/
int WriteFat()
{
	if(lseek(fd,FAT_ONE_OFFSET,SEEK_SET)<0)
	{
		perror("lseek failed");
		return -1;
	}
	if(write(fd,fatbuf,512*250)<0)
	{
		perror("read failed");
		return -1;
	}
	if(lseek(fd,FAT_TWO_OFFSET,SEEK_SET)<0)
	{
		perror("lseek failed");
		return -1;
	}
	if((write(fd,fatbuf,512*250))<0)
	{
		perror("read failed");
		return -1;
	}
	return 1;
}

/*
*¶Áfat±íµÄÐÅÏ¢£¬´æÈëfatbuf[]ÖÐ
*/
int ReadFat()
{
	if(lseek(fd,FAT_ONE_OFFSET,SEEK_SET)<0)
	{
		perror("lseek failed");
		return -1;
	}
	if(read(fd,fatbuf,512*250)<0)
	{
		perror("read failed");
		return -1;
	}
	return 1;
}


/*
*²ÎÊý£ºfilename£¬ÀàÐÍ£ºchar
*·µ»ØÖµ£º1£¬³É¹¦£»-1£¬Ê§°Ü
*¹¦ÄÜ;É¾³ýµ±Ç°Ä¿Â¼ÏÂµÄÎÄ¼þ
*/
int fd_df(char *filename)
{
	struct Entry *pentry;
	int ret;
	unsigned char c;
	unsigned short seed,next;
	pentry = (struct Entry*)malloc(sizeof(struct Entry));
	//clone(pentry,curdir);
	/*É¨Ãèµ±Ç°Ä¿Â¼²éÕÒÎÄ¼þ*/
	ret = ScanEntry(filename,pentry,0);
	if(ret<0)
	{
		printf("no such file\n");
		free(pentry);
		return -1;
	}
	/*Çå³ýfat±íÏî*/
	seed = pentry->FirstCluster;
	while((next = GetFatCluster(seed))!=0xffff)
	{
		ClearFatCluster(seed);
		seed = next;

	}
	ClearFatCluster( seed );

	/*Çå³ýÄ¿Â¼±íÏî*/
	c=0xe5;


	if(lseek(fd,ret-0x20,SEEK_SET)<0)
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
int fd_dfdic(char *filename)
{
	struct Entry *pentry;
	int ret;
	unsigned char c;
	unsigned short seed,next;
	pentry = (struct Entry*)malloc(sizeof(struct Entry));
	//clone(pentry,curdir);
	/*É¨Ãèµ±Ç°Ä¿Â¼²éÕÒÎÄ¼þ*/
	ret = ScanEntry(filename,pentry,1);
	if(ret<0)
	{
		printf("no such file\n");
		free(pentry);
		return -1;
	}
	/*Çå³ýfat±íÏî*/
	seed = pentry->FirstCluster;
	while((next = GetFatCluster(seed))!=0xffff)
	{
		ClearFatCluster(seed);
		seed = next;

	}
	ClearFatCluster( seed );

	/*Çå³ýÄ¿Â¼±íÏî*/
	c=0xe5;


	if(lseek(fd,ret-0x20,SEEK_SET)<0)
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
int fd_dfs(char *filename)
{
	struct Entry *pentry;
	int ret;
	unsigned char c;
	unsigned short seed,next;
	pentry = (struct Entry*)malloc(sizeof(struct Entry));
	//clone(pentry,curdir);
	/*É¨Ãèµ±Ç°Ä¿Â¼²éÕÒÎÄ¼þ*/
	ret = ScanEntry(filename,pentry,0);
	if(ret<0)
	{
		free(pentry);
		return -1;
	}
	/*Çå³ýfat±íÏî*/
	seed = pentry->FirstCluster;
	while((next = GetFatCluster(seed))!=0xffff)
	{
		ClearFatCluster(seed);
		seed = next;

	}
	ClearFatCluster( seed );

	/*Çå³ýÄ¿Â¼±íÏî*/
	c=0xe5;


	if(lseek(fd,ret-0x20,SEEK_SET)<0)
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
int dirsize(char *filename){
	struct Entry *pentry, entry1;
	int ret,cluster_addr,offset,size=0;
	unsigned char c;
	unsigned short seed,next;
	pentry = (struct Entry*)malloc(sizeof(struct Entry));
	//clone(pentry,curdir);
	ret = ScanEntry(filename,pentry,1);
		if(ret < 0)
			return -1;
		seed = pentry->FirstCluster;
		//if(fatbuf[seed*2] == 0x00 && fatbuf[seed*2+1] == 0x00)
		fd_cd(filename);
		while((next = GetFatCluster(seed))!=0xffff){
			cluster_addr = DATA_OFFSET + (seed-2) * CLUSTER_SIZE ;
			if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("lseek cluster_addr failed");

			offset = cluster_addr;

		/*Ö»¶ÁÒ»´ØµÄÄÚÈÝ*/
		
		

	
				while(offset<cluster_addr +CLUSTER_SIZE)
				{
					ret = GetEntry(&entry1);
					offset += abs(ret);
					if(ret >= 0)
					{
						if(entry1.subdir == 1){//ÊÇ×ÓÄ¿Â¼
							size += dirsize(entry1.short_name);
						}
						else{//ÊÇÎÄ¼þ
							size +=entry1.size;
						}
					}
				}
			
				
				seed = next;
		}
		cluster_addr = DATA_OFFSET + (seed-2) * CLUSTER_SIZE ;
		if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
			perror("lseek cluster_addr failed");

		offset = cluster_addr;
		while(offset<cluster_addr +CLUSTER_SIZE){
			ret = GetEntry(&entry1);
			offset += abs(ret);
			if(ret >= 0)
			{
				if(entry1.subdir == 1){//ÊÇ×ÓÄ¿Â¼
					size += dirsize(entry1.short_name);
				}
				else{//ÊÇÎÄ¼þ
					size +=entry1.size;
				}
			}
		}
		fd_cd("..");
		free(pentry);
		return size;		
	}
int fd_rms(char *filename){
	struct Entry *pentry, entry;
	int ret,cluster_addr,offset;
	unsigned char c;
	unsigned short seed,next;
	pentry = (struct Entry*)malloc(sizeof(struct Entry));
	//clone(pentry,curdir);
		ret = ScanEntry(filename,pentry,1);
		if(ret < 0)
			return -1;
		seed = pentry->FirstCluster;
		//if(fatbuf[seed*2] == 0x00 && fatbuf[seed*2+1] == 0x00)
		fd_cd(filename);
		while((next = GetFatCluster(seed))!=0xffff){
			cluster_addr = DATA_OFFSET + (seed-2) * CLUSTER_SIZE ;
			if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("lseek cluster_addr failed");

			offset = cluster_addr;

		/*Ö»¶ÁÒ»´ØµÄÄÚÈÝ*/
		
		

	
				while(offset<cluster_addr +CLUSTER_SIZE)
				{
					ret = GetEntry(&entry);
					offset += abs(ret);
					if(ret > 0)
					{
						if(entry.subdir == 1){//ÊÇ×ÓÄ¿Â¼
							fd_rm(entry.short_name);
						}
						else{//ÊÇÎÄ¼þ
							fd_df(entry.short_name);
						}
					}
				}
			
				
				seed = next;
		}
		cluster_addr = DATA_OFFSET + (seed-2) * CLUSTER_SIZE ;
		if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
			perror("lseek cluster_addr failed");

		offset = cluster_addr;
		while(offset<cluster_addr +CLUSTER_SIZE){
			ret = GetEntry(&entry);
			offset += abs(ret);
			if(ret > 0)
			{
				if(entry.subdir == 1){//ÊÇ×ÓÄ¿Â¼
					fd_rm(entry.short_name);
				}
				else{//ÊÇÎÄ¼þ
					fd_df(entry.short_name);
				}
			}
		}
		fd_cd("..");
		fd_dfdic(filename);
}
int fd_rmold(char *filename){
	struct Entry *pentry, entry;
	int ret,cluster_addr,offset;
	unsigned char c;
	unsigned short seed,next;
	char str[100];
	pentry = (struct Entry*)malloc(sizeof(struct Entry));
	//clone(pentry,curdir);
	if(ScanEntry(filename,pentry,0)){//ÊÇÎÄ¼þ
		fd_df(filename);
	}
	else{//ÊÇ×ÓÄ¿Â¼
		if(ret<0)
		{
			printf("no such file\n");
			free(pentry);
			return -1;
		}
		ret = ScanEntry(filename,pentry,1);
		seed = pentry->FirstCluster;
		//if(fatbuf[seed*2] == 0x00 && fatbuf[seed*2+1] == 0x00)
		fd_cd(filename);
		while((next = GetFatCluster(seed))!=0xffff){
			cluster_addr = DATA_OFFSET + (seed-2) * CLUSTER_SIZE ;
			if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("lseek cluster_addr failed");

			offset = cluster_addr;

		/*Ö»¶ÁÒ»´ØµÄÄÚÈÝ*/
		
		

	
				while(offset<cluster_addr +CLUSTER_SIZE)
				{
					ret = GetEntry(&entry);
					offset += abs(ret);
					if(ret > 0)
					{
						if(entry.subdir == 1){//ÊÇ×ÓÄ¿Â¼
							fd_rm(entry.short_name);
						}
						else{//ÊÇÎÄ¼þ
							fd_df(entry.short_name);
						}
					}
				}
			
				
				seed = next;
		}
		cluster_addr = DATA_OFFSET + (seed-2) * CLUSTER_SIZE ;
		if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
			perror("lseek cluster_addr failed");

		offset = cluster_addr;
		while(offset<cluster_addr +CLUSTER_SIZE){
			ret = GetEntry(&entry);
			offset += abs(ret);
			if(ret > 0)
			{
				if(entry.subdir == 1){//ÊÇ×ÓÄ¿Â¼
					fd_rm(entry.short_name);
				}
				else{//ÊÇÎÄ¼þ
					fd_df(entry.short_name);
				}
			}
		}
		fd_cd("..");
		fd_dfdic(filename);
	}	

}
int fd_rm(char *filename){
	struct Entry *pentry, entry;
	int ret,cluster_addr,offset;
	unsigned char c;
	unsigned short seed,next;
	char str[100];
	pentry = (struct Entry*)malloc(sizeof(struct Entry));
	if(ScanEntry(filename,pentry,0)>0){//ÊÇÎÄ¼þ
		fd_df(filename);
	}
	else{
		if(ScanEntry(filename,pentry,1)<0)
		{
			printf("no such file\n");
			free(pentry);
			return -1;
		}
		if(dirsize(filename)==0){
			ret = ScanEntry(filename,pentry,1);
			seed = pentry->FirstCluster;
			//if(fatbuf[seed*2] == 0x00 && fatbuf[seed*2+1] == 0x00)
			fd_cd(filename);
			while((next = GetFatCluster(seed))!=0xffff){
				cluster_addr = DATA_OFFSET + (seed-2) * CLUSTER_SIZE ;
				if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
					perror("lseek cluster_addr failed");

				offset = cluster_addr;
			
			

		
					while(offset<cluster_addr +CLUSTER_SIZE)
					{
						ret = GetEntry(&entry);
						offset += abs(ret);
						if(ret > 0)
						{
							if(entry.subdir == 1){//ÊÇ×ÓÄ¿Â¼
								fd_rms(entry.short_name);
							}
							else{//ÊÇÎÄ¼þ
								fd_dfs(entry.short_name);
							}
						}
					}
				
					
					seed = next;
			}
			cluster_addr = DATA_OFFSET + (seed-2) * CLUSTER_SIZE ;
			if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("lseek cluster_addr failed");

			offset = cluster_addr;
			while(offset<cluster_addr +CLUSTER_SIZE){
				ret = GetEntry(&entry);
				offset += abs(ret);
				if(ret > 0)
				{
					if(entry.subdir == 1){//ÊÇ×ÓÄ¿Â¼
						fd_rms(entry.short_name);
					}
					else{//ÊÇÎÄ¼þ
						fd_dfs(entry.short_name);
					}
				}
			}
			fd_cd("..");
			fd_dfdic(filename);
		}
		else
		{
			ret = ScanEntry(filename,pentry,1);
			printf("文件夹不为空是否删除,删除输入Y，否则请按其他键\n");
			scanf("%s",str);
			if(strcmp(str,"Y")==0)
			{
				seed = pentry->FirstCluster;
				//if(fatbuf[seed*2] == 0x00 && fatbuf[seed*2+1] == 0x00)
				fd_cd(filename);
				while((next = GetFatCluster(seed))!=0xffff){
					cluster_addr = DATA_OFFSET + (seed-2) * CLUSTER_SIZE ;
					if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
						perror("lseek cluster_addr failed");

					offset = cluster_addr;

				/*Ö»¶ÁÒ»´ØµÄÄÚÈÝ*/
				
				

			
						while(offset<cluster_addr +CLUSTER_SIZE)
						{
							ret = GetEntry(&entry);
							offset += abs(ret);
							if(ret > 0)
							{
								if(entry.subdir == 1){//ÊÇ×ÓÄ¿Â¼
									fd_rms(entry.short_name);
								}
								else{//ÊÇÎÄ¼þ
									fd_dfs(entry.short_name);
								}
							}
						}
					
						
						seed = next;
				}
				cluster_addr = DATA_OFFSET + (seed-2) * CLUSTER_SIZE ;
				if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
					perror("lseek cluster_addr failed");

				offset = cluster_addr;
				while(offset<cluster_addr +CLUSTER_SIZE){
					ret = GetEntry(&entry);
					offset += abs(ret);
					if(ret > 0)
					{
						if(entry.subdir == 1){//ÊÇ×ÓÄ¿Â¼
							fd_rms(entry.short_name);
						}
						else{//ÊÇÎÄ¼þ
							fd_dfs(entry.short_name);
						}
					}
				}
				fd_cd("..");
				fd_dfdic(filename);
			}
		}
	}
}


int dirempty(char *filename){
struct Entry *pentry, entry1;
	int ret,cluster_addr,offset,size=0;
	unsigned char c;
	unsigned short seed,next;
	pentry = (struct Entry*)malloc(sizeof(struct Entry));
	//clone(pentry,curdir);
	ret = ScanEntry(filename,pentry,1);
		if(ret < 0)
			return -1;
		seed = pentry->FirstCluster;
		//if(fatbuf[seed*2] == 0x00 && fatbuf[seed*2+1] == 0x00)
		fd_cd(filename);
		while((next = GetFatCluster(seed))!=0xffff){
			cluster_addr = DATA_OFFSET + (seed-2) * CLUSTER_SIZE ;
			if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("lseek cluster_addr failed");

			offset = cluster_addr;

		/*Ö»¶ÁÒ»´ØµÄÄÚÈÝ*/
		
		

	
				while(offset<cluster_addr +CLUSTER_SIZE)
				{
					ret = GetEntry(&entry1);
					offset += abs(ret);
					if(ret >= 0)
					{
						if(entry1.subdir == 1){//ÊÇ×ÓÄ¿Â¼
							size += dirsize(entry1.short_name);
						}
						else{//ÊÇÎÄ¼þ
							size +=1;
						}
					}
				}
			
				
				seed = next;
		}
		cluster_addr = DATA_OFFSET + (seed-2) * CLUSTER_SIZE ;
		if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
			perror("lseek cluster_addr failed");

		offset = cluster_addr;
		while(offset<cluster_addr +CLUSTER_SIZE){
			ret = GetEntry(&entry1);
			offset += abs(ret);
			if(ret >= 0)
			{
				if(entry1.subdir == 1){//ÊÇ×ÓÄ¿Â¼
					size += dirsize(entry1.short_name);
				}
				else{//ÊÇÎÄ¼þ
					size +=1;
				}
			}
		}
		fd_cd("..");
		free(pentry);
		return size;		
	}

/*
*²ÎÊý£ºfilename£¬ÀàÐÍ£ºchar£¬´´½¨ÎÄ¼þµÄÃû³Æ
size£¬    ÀàÐÍ£ºint£¬ÎÄ¼þµÄ´óÐ¡
*·µ»ØÖµ£º1£¬³É¹¦£»-1£¬Ê§°Ü
*¹¦ÄÜ£ºÔÚµ±Ç°Ä¿Â¼ÏÂ´´½¨ÎÄ¼þ
*/
int fd_cf(char *filename,int size)
{

	struct Entry *pentry;
	int ret,i=0,cluster_addr,offset;
	unsigned short cluster,clusterno[100],clusterf,clusternof,clustertemp;;
	unsigned char c[DIR_ENTRY_SIZE];
	int index,clustersize;
	unsigned char buf[DIR_ENTRY_SIZE];
	pentry = (struct Entry*)malloc(sizeof(struct Entry));
	//clone(pentry,curdir);
	clustersize = (size / (CLUSTER_SIZE));

	if(size % (CLUSTER_SIZE) != 0)
		clustersize ++;
	//É¨Ãè¸ùÄ¿Â¼£¬ÊÇ·ñÒÑ´æÔÚ¸ÃÎÄ¼þÃû
	ret = ScanEntry(filename,pentry,0);
	if (ret<0)
	{
		/*²éÑ¯fat±í£¬ÕÒµ½¿Õ°×´Ø£¬±£´æÔÚclusterno[]ÖÐ*/
		for(cluster=2;cluster<64000;cluster++)
		{
			index = cluster *2;
			if(fatbuf[index]==0x00&&fatbuf[index+1]==0x00)
			{
				clusterno[i] = cluster;

				i++;
				if(i==clustersize)
					break;
				if(cluster == 64399)
				{
					printf("文件分配表已满\n");
					return -1;
				}

			}

		}

		/*ÔÚfat±íÖÐÐ´ÈëÏÂÒ»´ØÐÅÏ¢*/
		for(i=0;i<clustersize-1;i++)
		{
			index = clusterno[i]*2;

			fatbuf[index] = (clusterno[i+1] &  0x00ff);
			fatbuf[index+1] = ((clusterno[i+1] & 0xff00)>>8);


		}
		/*×îºóÒ»´ØÐ´Èë0xffff*/
		index = clusterno[i]*2;
		fatbuf[index] = 0xff;
		fatbuf[index+1] = 0xff;

		if(curdir==NULL)  /*Íù¸ùÄ¿Â¼ÏÂÐ´ÎÄ¼þ*/
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


				/*ÕÒ³ö¿ÕÄ¿Â¼Ïî»òÒÑÉ¾³ýµÄÄ¿Â¼Ïî*/ 
				else
				{
					if(offset>DATA_OFFSET)
					{
						printf("根目录已满\n");
						free(pentry);
						return -1;
					} 
					offset = offset-abs(ret);     
					for(i=0;i<=strlen(filename);i++)
					{
						c[i]=toupper(filename[i]);
					}			
					for(;i<=10;i++)
						c[i]=' ';

					c[11] = 0x01;
					time_t timep;
                    struct tm *p;
                    time(&timep);
                    p=gmtime(&timep);
                    c[23]=((p->tm_hour)<<3)|((p->tm_min)>>3);
    				c[22]=((p->tm_min )<<5)|((p->tm_sec)>>1);

                    c[25]=((p->tm_year)<<1)|((p->tm_mon)>>3);
                    c[24]=((p->tm_mon & 0x7)<<5)|(p->tm_mday);
					/*Ð´µÚÒ»´ØµÄÖµ*/
					c[26] = (clusterno[0] &  0x00ff);
					c[27] = ((clusterno[0] & 0xff00)>>8);

					/*Ð´ÎÄ¼þµÄ´óÐ¡*/
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
					if(offset>(cluster_addr + CLUSTER_SIZE))
					{
						for(clusterf=2;clusterf<63999;clusterf++)
						{
							index = clusterf *2;
							if(fatbuf[index]==0x00&&fatbuf[index+1]==0x00)
							{
								clusternof = clusterf;
								break;
							}

						}
						/*×îºóÒ»´ØÐ´Èë0xffff*/
						index = clusternof*2;
						fatbuf[index] = 0xff;
						fatbuf[index+1] = 0xff;
						fatbuf[clustertemp*2]=clusternof%256;
						fatbuf[clustertemp*2+1]=clusternof/256;
						clustertemp = clusternof;
						cluster_addr = (clusternof -2 )*CLUSTER_SIZE + DATA_OFFSET;
						if((ret= lseek(fd,cluster_addr,SEEK_SET))<0)
							perror("lseek cluster_addr failed");
						offset = cluster_addr;
						continue;
					}
					offset = offset - abs(ret);      
					for(i=0;i<=strlen(filename);i++)
					{
						c[i]=toupper(filename[i]);
					}
					for(;i<=10;i++)
						c[i]=' ';

					c[11] = 0x01;
					time_t timep;
                    struct tm *p;
                    time(&timep);
                    p=gmtime(&timep);
                    c[23]=((p->tm_hour)<<3)|((p->tm_min)>>3);
    				c[22]=((p->tm_min )<<5)|((p->tm_sec)>>1);

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
int fd_mkdir(char *dirname)
{

	struct Entry *pentry;
	int ret,cluster_addr,offset;
	unsigned short cluster,clusterno,clusterf,clusternof,clustertemp;
	unsigned char c[DIR_ENTRY_SIZE];
	int index,i;
	unsigned char buf[DIR_ENTRY_SIZE];
	pentry = (struct Entry*)malloc(sizeof(struct Entry));
	//clone(pentry,curdir);
	//É¨Ãè¸ùÄ¿Â¼£¬ÊÇ·ñÒÑ´æÔÚ¸ÃÎÄ¼þÃû
	ret = ScanEntry(dirname,pentry,1);
	if (ret<0)
	{
		/*²éÑ¯fat±í£¬ÕÒµ½¿Õ°×´Ø£¬±£´æÔÚclusterno[]ÖÐ*/
		for(cluster=2;cluster<63999;cluster++)
		{
			index = cluster *2;
			if(fatbuf[index]==0x00&&fatbuf[index+1]==0x00)
			{
				clusterno = cluster;
				break;
			}

		}
		/*×îºóÒ»´ØÐ´Èë0xffff*/
		index = clusterno*2;
		fatbuf[index] = 0xff;
		fatbuf[index+1] = 0xff;
		if(curdir==NULL)  /*Íù¸ùÄ¿Â¼ÏÂÐ´ÎÄ¼þ*/
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


				/*ÕÒ³ö¿ÕÄ¿Â¼Ïî»òÒÑÉ¾³ýµÄÄ¿Â¼Ïî*/ 
				else
				{      
					if(offset>DATA_OFFSET)
					{
						printf("根目录已满\n");
						free(pentry);
						return -1;
					} 
					offset = offset-abs(ret);     
					for(i=0;i<=strlen(dirname);i++)
					{
						c[i]=toupper(dirname[i]);
					}			
					for(;i<=10;i++)
						c[i]=' ';

					c[11] = 0x10;
					time_t timep;
                    struct tm *p;
                    time(&timep);
                    p=gmtime(&timep);
                    c[23]=((p->tm_hour)<<3)|((p->tm_min)>>3);
    				c[22]=((p->tm_min )<<5)|((p->tm_sec)>>1);

                    c[25]=((p->tm_year)<<1)|((p->tm_mon)>>3);
                    c[24]=((p->tm_mon & 0x7)<<5)|(p->tm_mday);
					/*Ð´µÚÒ»´ØµÄÖµ*/
					c[26] = (clusterno &  0x00ff);
					c[27] = ((clusterno & 0xff00)>>8);

					/*Ð´ÎÄ¼þµÄ´óÐ¡*/
					c[28] = (0x00);
					c[29] = (0x00);
					c[30] = (0x00);
					c[31] = (0x00);
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
			clustertemp = pentry->FirstCluster;
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
					if(offset>(cluster_addr + CLUSTER_SIZE))
					{
						for(clusterf=2;clusterf<63999;clusterf++)
						{
							index = clusterf *2;
							if(fatbuf[index]==0x00&&fatbuf[index+1]==0x00)
							{
								clusternof = clusterf;
								break;
							}

						}
						/*×îºóÒ»´ØÐ´Èë0xffff*/
						index = clusternof*2;
						fatbuf[index] = 0xff;
						fatbuf[index+1] = 0xff;
						fatbuf[clustertemp*2]=clusternof%256;
						fatbuf[clustertemp*2+1]=clusternof/256;
						clustertemp = clusternof;
						cluster_addr = (clusternof -2 )*CLUSTER_SIZE + DATA_OFFSET;
						if((ret= lseek(fd,cluster_addr,SEEK_SET))<0)
							perror("lseek cluster_addr failed");
						offset = cluster_addr;
						continue;
					}
					offset = offset - abs(ret);      
					for(i=0;i<=strlen(dirname);i++)
					{
						c[i]=toupper(dirname[i]);
					}
					for(;i<=10;i++)
						c[i]=' ';

					c[11] = 0x10;
					time_t timep;
                    struct tm *p;
                    time(&timep);
                    p=gmtime(&timep);
                    c[23]=((p->tm_hour)<<3)|((p->tm_min)>>3);
    				c[22]=((p->tm_min )<<5)|((p->tm_sec)>>1);

                    c[25]=((p->tm_year)<<1)|((p->tm_mon)>>3);
                    c[24]=((p->tm_mon & 0x7)<<5)|(p->tm_mday);
					c[26] = (clusterno &  0x00ff);
					c[27] = ((clusterno & 0xff00)>>8);

					c[28] = (0x00);
					c[29] = (0x00);
					c[30] = (0x00);
					c[31] = (0x00);

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
		printf("This dirname is exist\n");
		free(pentry);
		return -1;
	}
	return 1;

}

void do_usage()
{
	printf("please input a command, including followings:\n\tls\t\t\tlist all files\n\tcd <dir>\t\tchange direcotry\n\tcf <filename> <size>\tcreate a file\n\tdf <file>\t\tdelete a file\n\texit\t\t\texit this system\n\tmkdir\t\t\tcreate a direcotry\n\trm\t\t\tremove a file or direcotry\n");
}
int fd_cfc(char *filename,int size,char *input)
{

	struct Entry *pentry;
	int ret,i=0,cluster_addr,offset,last_size;
	unsigned short cluster,clusterno[100];
	unsigned char c[DIR_ENTRY_SIZE];
	int index,clustersize;
	unsigned char buf[DIR_ENTRY_SIZE];
	pentry = (struct Entry*)malloc(sizeof(struct Entry));

	last_size = size;
	clustersize = (size / (CLUSTER_SIZE));

	if(size % (CLUSTER_SIZE) != 0)
		clustersize ++;

	//É¨Ãè¸ùÄ¿Â¼£¬ÊÇ·ñÒÑ´æÔÚ¸ÃÎÄ¼þÃû
	ret = ScanEntry(filename,pentry,0);
	if (ret<0)
	{
		/*²éÑ¯fat±í£¬ÕÒµ½¿Õ°×´Ø£¬±£´æÔÚclusterno[]ÖÐ*/
		for(cluster=2;cluster<1000;cluster++)
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

		/*ÔÚfat±íÖÐÐ´ÈëÏÂÒ»´ØÐÅÏ¢*/
		for(i=0;i<clustersize-1;i++)
		{
			index = clusterno[i]*2;

			fatbuf[index] = (clusterno[i+1] &  0x00ff);
			fatbuf[index+1] = ((clusterno[i+1] & 0xff00)>>8);


		}
		/*×îºóÒ»´ØÐ´Èë0xffff*/
		index = clusterno[i]*2;
		fatbuf[index] = 0xff;
		fatbuf[index+1] = 0xff;

		for(i=0;i<clustersize;i++){
			cluster_addr = DATA_OFFSET + (clusterno[i]-2) * CLUSTER_SIZE ;
			if(lseek(fd,cluster_addr,SEEK_SET)<0)
				perror("lseek ROOTDIR_OFFSET failed");

			if((write(fd,input,CLUSTER_SIZE))<0)
				perror("write entry failed");
			input = input + CLUSTER_SIZE;
		}

		if(curdir==NULL)  /*Íù¸ùÄ¿Â¼ÏÂÐ´ÎÄ¼þ*/
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


				/*ÕÒ³ö¿ÕÄ¿Â¼Ïî»òÒÑÉ¾³ýµÄÄ¿Â¼Ïî*/
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
    				c[22]=((p->tm_min )<<5)|((p->tm_sec)>>1);

                    c[25]=((p->tm_year)<<1)|((p->tm_mon)>>3);
                    c[24]=((p->tm_mon & 0x7)<<5)|(p->tm_mday);

					/*Ð´µÚÒ»´ØµÄÖµ*/
					c[26] = (clusterno[0] &  0x00ff);
					c[27] = ((clusterno[0] & 0xff00)>>8);

					/*Ð´ÎÄ¼þµÄ´óÐ¡*/
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
						c[i]='\0';

					c[11] = 0x01;
                    time_t timep;
                    struct tm *p;
                    time(&timep);
                    p=gmtime(&timep);
                    c[23]=((p->tm_hour)<<3)|((p->tm_min)>>3);
    				c[22]=((p->tm_min )<<5)|((p->tm_sec)>>1);

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
	char input[100];
	int size=0;
	int i = 0,temp = 0,flag = 0;
	int j = 0;
	char name[100];
	char name_l[100];
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
		{
			temp = 0;
			flag = 0;
			int namei = 0;
			int flag2 = 0;
			int name2[100];
			char c ;
			while((c = getchar())!='\n')
			{
				if(c!= ' '&&flag2 == 0)
				{
					do{
						name[namei++]=c;
					}while((c = getchar())!=' '&&c!='\n');
					name[namei]='\0';
					namei = 0;
					flag2 = 1;
					if(c == '\n')
						break;
				}
				if(c!=" "&&flag2 == 1)
				{
					name2[namei++]=c;
				}
			}
			name2[namei]='\0';
			if(flag2 == 1){
				if(strcmp(name,"-a")!=0){
					for(i=0;i<strlen(name);i++)
					{
						if(name[i]=='/')
						{
							name_l[temp]='\0';
							temp=0;
							if(strcmp(name_l,"..")==0||strcmp(name_l,".")==0)
							{
								printf("输入有误");
								flag = 1;
								break;
							}
							if(fd_cd(name_l)<0)
							{
								flag = 1;
								break;
							}
							j++;
						}
						else
							name_l[temp++] = name[i];
					}
					if(fd_ls()<0)
					{
						flag = 1;
					}
					if(flag == 1)
					{
						flag = 0;
						for(i = 0;i<j;i++)
						{
							fd_cd("..");
						}
					}
				}
				else
				{
					if(namei!=0)
						strcpy(name,name2);
					for(i=0;i<strlen(name);i++)
					{
						if(name[i]=='/')
						{
							name_l[temp]='\0';
							temp=0;
							if(strcmp(name_l,"..")==0||strcmp(name_l,".")==0)
							{
								printf("输入有误");
								flag = 1;
								break;
							}
							if(fd_cd(name_l)<0)
							{
								flag = 1;
								break;
							}
							j++;
						}
						else
							name_l[temp++] = name[i];
					}
					if(fd_lsaa()<0)
					{
						flag = 1;
					}
					if(flag == 1)
					{
						flag = 0;
						for(i = 0;i<j;i++)
						{
							fd_cd("..");
						}
					}

				}
			}
			else
				fd_ls();
		}
		else if(strcmp(input, "cd") == 0)
		{
			temp = 0;
			flag = 0;
			scanf("%s", name);
			for(i=0;i<strlen(name);i++)
			{
				if(name[i]=='/')
				{
					name_l[temp]='\0';
					temp=0;
					if(strcmp(name_l,"..")==0||strcmp(name_l,".")==0)
					{
						printf("输入有误");
						flag = 1;
						break;
					}
					if(fd_cd(name_l)<0)
					{
						flag = 1;
						break;
					}
					j++;
				}
				else
					name_l[temp++] = name[i];
			}
			name_l[temp]='\0';
			if(fd_cd(name_l)<0)
			{
				flag = 1;
			}
			if(flag == 1)
			{
				flag = 0;
				for(i = 0;i<j;i++)
				{
					fd_cd("..");
				}
			}
		}
		else if(strcmp(input, "df") == 0)
		{
			temp = 0;
			flag = 0;
			scanf("%s", name);
			for(i=0;i<strlen(name);i++)
			{
				if(name[i]=='/')
				{
					name_l[temp]='\0';
					temp=0;
					if(strcmp(name_l,"..")==0||strcmp(name_l,".")==0)
					{
						printf("输入有误df");
						break;
					}
					if(fd_cd(name_l)<0)
					{
						break;
					}
					j++;
				}
				else
					name_l[temp++] = name[i];
			}
			name_l[temp]='\0';
			fd_df(name_l);
			for(i = 0;i<j;i++)
			{
				fd_cd("..");
			}
		}
		else if(strcmp(input, "cf") == 0)
		{
			temp = 0;
			flag = 0;
			scanf("%s",name);
			if(strcmp(name,"-w")!=0){
				scanf("%s", input);
				size = atoi(input);
				j = 0;
				temp = 0;
				for(i=0;i<strlen(name);i++)
				{
					if(name[i]=='/')
					{
						name_l[temp]='\0';
						temp=0;
						if(strcmp(name_l,"..")==0||strcmp(name_l,".")==0)
						{
							printf("输入有误");
							break;
						}
						if(fd_cd(name_l)<0)
						{
							break;
						}
						j++;
					}
					else
						name_l[temp++] = name[i];
				}
				name_l[temp]='\0';
				fd_cf(name_l,size);
				for(i = 0;i<j;i++)
				{
					fd_cd("..");
				}
			}
			else
			{
				scanf("%s", name);
				scanf("%s", input);
				size = strlen(input);
				j = 0;
				temp = 0;
				for(i=0;i<strlen(name);i++)
				{
					if(name[i]=='/')
					{
						name_l[temp]='\0';
						temp=0;
						if(strcmp(name_l,"..")==0||strcmp(name_l,".")==0)
						{
							printf("输入有误");
							break;
						}
						if(fd_cd(name_l)<0)
						{
							break;
						}
						j++;
					}
					else
						name_l[temp++] = name[i];
				}
				name_l[temp]='\0';
				fd_cfc(name_l,size,input);
				for(i = 0;i<j;i++)
				{
					fd_cd("..");
				}
			}
		}
		else if(strcmp(input, "mkdir") == 0)
		{
			temp = 0;
			flag = 0;
			scanf("%s", name);
			j = 0;
			temp = 0;
			for(i=0;i<strlen(name);i++)
			{
				if(name[i]=='/')
				{
					name_l[temp]='\0';
					temp=0;
					if(strcmp(name_l,"..")==0||strcmp(name_l,".")==0)
					{
						printf("输入有误");
						break;
					}
					if(fd_cd(name_l)<0)
					{
						break;
					}
					j++;
				}
				else
					name_l[temp++] = name[i];
			}
			name_l[temp]='\0';
			fd_mkdir(name_l);
			for(i = 0;i<j;i++)
			{
				fd_cd("..");
			}
		}
		else if(strcmp(input, "rm") == 0)
		{
			temp = 0;
			flag = 0;
			scanf("%s", name);
			if(strcmp(name,"-r")==0)
			{
				scanf("%s", name);
				j = 0;
				temp = 0;
				for(i=0;i<strlen(name);i++)
				{
					if(name[i]=='/')
					{
						name_l[temp]='\0';
						temp=0;
						if(strcmp(name_l,"..")==0||strcmp(name_l,".")==0)
						{
							printf("输入有误");
							break;
						}
						if(fd_cd(name_l)<0)
						{
							break;
						}
						j++;
					}
					else
						name_l[temp++] = name[i];
				}
				name_l[temp]='\0';
				fd_rmold(name_l);
				for(i = 0;i<j;i++)
				{
					fd_cd("..");
				}
			}
			else{
				j = 0;
				temp = 0;
				for(i=0;i<strlen(name);i++)
				{
					if(name[i]=='/')
					{
						name_l[temp]='\0';
						temp=0;
						if(strcmp(name_l,"..")==0||strcmp(name_l,".")==0)
						{
							printf("输入有误");
							break;
						}
						if(fd_cd(name_l)<0)
						{
							break;
						}
						j++;
					}
					else
						name_l[temp++] = name[i];
				}
				name_l[temp]='\0';
				fd_rm(name_l);
				for(i = 0;i<j;i++)
				{
					fd_cd("..");
				}
			}
		}
		else
			do_usage();
	}	

	return 0;
}



