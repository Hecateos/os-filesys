#ifndef FILESYS_H
#define FILESYS_H
#include<stddef.h>
#define DEVNAME "/home/yujianxun/data"                         
#define DIR_ENTRY_SIZE 32
#define SECTOR_SIZE 512
#define CLUSTER_SIZE 512*4                         
#define FAT_ONE_OFFSET 512                       
#define FAT_TWO_OFFSET 512+256*512                       
#define ROOTDIR_OFFSET 512+256*512+256*512                     
#define DATA_OFFSET 512+256*512+256*512+512*32        

           

/*ÊôÐÔÎ»ÑÚÂë*/
#define ATTR_READONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VLABEL 0x08
#define ATTR_SUBDIR 0x10
#define ATTR_ARCHIVE 0x20

/*Ê±¼äÑÚÂë 5£º6£º5 */
#define MASK_HOUR 0xf800 
#define MASK_MIN 0x07e0
#define MASK_SEC 0x001f

/*ÈÕÆÚÑÚÂë*/
#define MASK_YEAR 0xfe00
#define MASK_MONTH 0x01e0
#define MASK_DAY 0x001f

struct BootDescriptor_t{
	unsigned char Oem_name[9]; /*0x03-0x0a*/
	int BytesPerSector;        /*0x0b-0x0c*/
	int SectorsPerCluster;     /*0x0d*/
	int ReservedSectors;       /*0x0e-0x0f*/
	int FATs;                  /*0x10*/
	int RootDirEntries;        /*0x11-0x12*/
	int LogicSectors;          /*0x13-0x14*/
	int MediaType;             /*0x15*/
	int SectorsPerFAT;         /*0x16-0x17*/
	int SectorsPerTrack;       /*0x18-0x19*/
	int Heads;                 /*0x1a-0x1b*/
	int HiddenSectors;         /*0x1c-0x1d*/
};

struct Entry{
	unsigned char short_name[12];   /*×Ö½Ú0-10£¬11×Ö½ÚµÄ¶ÌÎÄ¼þÃû*/
	unsigned char long_name[27];    /*Î´Ê¹ÓÃ£¬26×Ö½ÚµÄ³¤ÎÄ¼þÃû*/
	unsigned short year,month,day;  /*22-23×Ö½Ú*/
	unsigned short hour,min,sec;    /*24-25×Ö½Ú*/
	unsigned short FirstCluster;    /*26-27×Ö½Ú*/
	unsigned int size;              /*28-31×Ö½Ú*/
	/*ÊôÐÔÖµ                        11×Ö½Ú
	*7  6  5  4  3  2  1  0
	*N  N  A  D  V  S  H  R         NÎ´Ê¹ÓÃ
	*/

	unsigned char readonly:1;
	unsigned char hidden:1;
	unsigned char system:1;
	unsigned char vlabel:1;
	unsigned char subdir:1;
	unsigned char archive:1;
};

int fd_ls();
int fd_cd(char *dir);
int fd_df(char *file_name);
int fd_cf(char *file_name,int size);

void findDate(unsigned short *year,
			  unsigned short *month,
			  unsigned short *day,
			  unsigned char info[2]);

void findTime(unsigned short *hour,
			  unsigned short *min,
			  unsigned short *sec,
			  unsigned char info[2]);
int ReadFat();
int WriteFat();
void ScanBootSector();
void ScanRootEntry();
int ScanEntry(char *entryname,struct Entry *pentry,int mode);
int GetEntry(struct Entry *entry);
void FileNameFormat(unsigned char *name);
unsigned short GetFatCluster(unsigned short prev);
void ClearFatCluster(unsigned short cluster);

int fd;
struct BootDescriptor_t bdptor;
struct Entry *curdir = NULL;
int dirno = 0;/*´ú±íÄ¿Â¼µÄ²ãÊý*/
struct Entry* fatherdir[10];

unsigned char fatbuf[512*250];  

#endif

