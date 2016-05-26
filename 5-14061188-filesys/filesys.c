#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<string.h>
#include<ctype.h>
#include "filesys.h"

//#define DEBUG
//#define LSFAT
//#define LSROOT
//#define LSDATA
#define RevByte(low,high) ((high)<<8|(low))
#define RevWord(lowest,lower,higher,highest) ((highest)<< 24|(higher)<<16|(lower)<<8|lowest) 

void a(int i)
{
    printf("anchor%d\n", i);
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
    if(read(fd,fatbuf,512 * 40)<0)
    {
        perror("read failed");
        return -1;
    }
    return 1;
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
    if(write(fd,fatbuf,512 * 40)<0)
    {
        perror("write failed");
        return -1;
    }
    if(lseek(fd,FAT_TWO_OFFSET,SEEK_SET)<0)
    {
        perror("lseek failed");
        return -1;
    }
    if((write(fd,fatbuf,512 * 40))<0)
    {
        perror("write failed");
        return -1;
    }
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
 * 功能：解析路径
 */
void ParseAbsolute(char** source)
{
    if ((*source)[0] == '/') {
        (*source)++;
    } else {
        if (curdir != NULL) {
            v_curdir = (struct Entry*)malloc(sizeof(struct Entry));
            memcpy(v_curdir, curdir, sizeof(struct Entry));
        }
    }
}

void ParsePath(char** source, char *dest)
{
    int i = 0;
    for (;(*source)[i] != '/' && (*source)[i] != '\0';dest[i] = toupper((*source)[i]), i++);
    dest[i] = '\0';
    if ((*source)[i] == '\0') {
        (*source) += i;
    } else {
        (*source) += i + 1;
    }
}

void v_free(int mode) {
    int i = 0;
    if (mode) {
        dirno = v_dirno;
        for (; i < v_dirno; i++) {
            if (v_fatherdir[i] != NULL) {
                if (fatherdir[i] == NULL) {
                    fatherdir[i] = (struct Entry*)malloc(sizeof(struct Entry));
                }
                memcpy(fatherdir[i], v_fatherdir[i], sizeof(struct Entry));
            } else {
                free(fatherdir[i]);
                fatherdir[i] = NULL;
            }
        }
        if (v_curdir != NULL && v_curdir->FirstCluster != 0) {
            if (curdir == NULL) {
                curdir = (struct Entry*)malloc(sizeof(struct Entry));
            }
            memcpy(curdir, v_curdir, sizeof(struct Entry));
        } else {
            free(curdir);
            curdir = NULL;
        }
    }
    free(v_curdir);
    v_curdir = NULL;
    for (i = 0; i < v_dirno; i++) {
        free(v_fatherdir[i]);
        v_fatherdir[i] = NULL;
    }
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
#ifdef DEBUG
    for (i = 0; i < 32; i++ ) printf("%x\t", buf[i]);
    printf("\n");
#endif
#ifdef LSFAT
    for (i = 0; i < 32; i++ ) printf("%x\t", buf[i]);
    printf("\n");
#endif
#ifdef LSROOT
    for (i = 0; i < 32; i++ ) printf("%x\t", buf[i]);
    printf("\n");
#endif
#ifdef LSDATA
    for (i = 0; i < 32; i++ ) printf("%x\t", buf[i]);
    printf("\n");
#endif
    count += ret;
    if(buf[0]==0xe5 || buf[0]== 0x00)
    {
        return -1*count;
    } else {
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
 *参数：entryname 类型：char
 ：pentry    类型：struct Entry*
 ：filetype      类型：int，filetype=1，为目录表项；filetype=0，为文件
 *返回值：偏移值大于0，则成功；-1，则失败
 *功能：搜索当前目录，查找文件或目录项
 */
int ScanEntry (char *entryname, struct Entry *pentry, int filetype)
{
    int ret,offset,i;
    int cluster_addr, cluster;
    char uppername[80];
    for(i=0;i< strlen(entryname);i++)
        uppername[i]= toupper(entryname[i]);
    uppername[i]= '\0';
    /*扫描根目录*/
    if(v_curdir == NULL || v_curdir->FirstCluster == 0)  
    {
        if (strcmp(entryname, ".") == 0) {
            if (filetype) return 1;
            else return -1;
        } 
        if((ret = lseek(fd,ROOTDIR_OFFSET,SEEK_SET))<0)
            perror ("lseek ROOTDIR_OFFSET failed");
        offset = ROOTDIR_OFFSET;
        while(offset<DATA_OFFSET)
        {
            ret = GetEntry(pentry);
            offset +=abs(ret);
            if(ret > 0 && pentry->subdir == filetype &&!strcmp((char*)pentry->short_name,uppername)) {
                return offset;
            }
        }
        return -1;
    }

    /*扫描子目录*/
    else
    {
        for (cluster = v_curdir->FirstCluster; cluster != 0xffff; cluster = GetFatCluster(cluster)) { 
            cluster_addr = DATA_OFFSET + (cluster - 2)*CLUSTER_SIZE;
            if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
                perror("lseek cluster_addr failed");
            offset= cluster_addr;
            while(offset<cluster_addr + CLUSTER_SIZE)
            {
                ret= GetEntry(pentry);
                offset += abs(ret);
                if(ret > 0 && pentry->subdir == filetype &&!strcmp((char*)pentry->short_name,uppername)) {
                    return offset;
                }
            }
        }
        return -1;
    }
}

int isEmpty(char *path) 
{
    int cluster, cluster_addr, ret, offset;
    unsigned char buf[DIR_ENTRY_SIZE];
    struct Entry *pentry = (struct Entry*)malloc(sizeof(struct Entry));
    struct Entry entry;
    ScanEntry(path, pentry, 1);
    for (cluster = pentry->FirstCluster; cluster < 0xfff7; cluster = GetFatCluster(cluster)) {
#ifdef DEBUG
        printf("clusterno:%x\n", cluster);
#endif
        cluster_addr = (cluster - 2) * CLUSTER_SIZE + DATA_OFFSET;
        if((ret= lseek(fd,cluster_addr,SEEK_SET))<0)
            perror("lseek cluster_addr failed"); 
        offset = cluster_addr;
        while(offset < cluster_addr + CLUSTER_SIZE)
        {
            GetEntry(&entry);
            if((ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
                perror("read entry failed");
            offset += abs(ret);
            if(entry.FirstCluster != 1 && entry.FirstCluster != pentry->FirstCluster && buf[0]!=0xe5&&buf[0]!=0x00) {
                return 0;
            }
        }
    }
    free(pentry);
    return 1;
}

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

/*
 *功能：显示当前目录的内容
 *返回值：1，成功；-1，失败
 */
int fd_ls(char* path)
{
    int ret, offset, cluster_addr, cluster;
    struct Entry entry;
    if ((ret = v_fd_cd_path(&path, 2)) < 0) {
        return -1;
    }
    if (ret == 1) {
        v_fd_cd_dir(path);
    }
    if(v_curdir==NULL || v_curdir->FirstCluster == 0)
        printf("Root_dir\n");
    else
        printf("%s_dir\n",v_curdir->short_name);
    printf("\tname\tdate\t\t time\t\tcluster\tsize\t\tattr\n");
    if(v_curdir==NULL || v_curdir->FirstCluster == 0)  /*显示根目录区*/
    {
        if(ret == 1) {
            /*将fd定位到根目录区的起始地址*/
            if((ret= lseek(fd,ROOTDIR_OFFSET,SEEK_SET))<0)
                perror("lseek ROOTDIR_OFFSET failed");
            offset = ROOTDIR_OFFSET;
            /*从根目录区开始遍历，直到数据区起始地址*/
            while(offset < (DATA_OFFSET))
            {
#ifdef DEBUG
                printf("read:%x\n", offset);
#endif
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
        } else if (ret == 0) {
            ScanEntry(path, &entry, 0);
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
    else /*显示子目录*/
    {
        if (ret == 1) {
            for (cluster = v_curdir->FirstCluster; cluster < 0xfff7; cluster = GetFatCluster(cluster)){ 
                cluster_addr = DATA_OFFSET + (cluster-2) * CLUSTER_SIZE ;
                if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
                    perror("lseek cluster_addr failed");
                offset = cluster_addr;
                while(offset < cluster_addr + CLUSTER_SIZE)
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
        } else if (ret == 0){
            ScanEntry(path, &entry, 0);
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
    free(path);
#ifdef LSFAT
    offset = FAT_ONE_OFFSET;
    if((ret = lseek(fd,offset,SEEK_SET))<0)
        perror("lseek cluster_addr failed");
    for (offset = FAT_ONE_OFFSET; offset < FAT_TWO_OFFSET;offset += abs(GetEntry(&entry))) {
        printf("offset:%x\n", offset);
    }
#endif
#ifdef LSROOT
   offset = ROOTDIR_OFFSET; 
   if((ret = lseek(fd,offset,SEEK_SET))<0)
       perror("lseek cluster_addr failed");
   for (offset = ROOTDIR_OFFSET; offset < DATA_OFFSET;offset += abs(GetEntry(&entry))) {
        printf("offset:%x\n", offset);
   }
#endif
#ifdef LSDATA
   offset = DATA_OFFSET;
   if((ret = lseek(fd,offset,SEEK_SET))<0)
       perror("lseek cluster_addr failed");
   for (offset = DATA_OFFSET; offset < DATA_OFFSET+0x2000;offset += abs(GetEntry(&entry))) {
       printf("offset:%x\n", offset);
   }
#endif
   return 1;
}

/*
 * checktail = 0：检查是否存在且为文件；1：检查是否存在且为目录；2：检查是否存在且为目录或文件；3：检查是否不存在
 */
int v_fd_cd_path(char** path, int checktail)
{
    int i = 0, res = 0;
    char dirname[12];
    struct Entry *pentry = (struct Entry*)malloc(sizeof(struct Entry));
    for (; i < dirno; i++) {
        if (fatherdir[i] != NULL) {
            if (v_fatherdir[i] == NULL) {
                v_fatherdir[i] = (struct Entry*)malloc(sizeof(struct Entry));
            }
            memcpy(v_fatherdir[i], fatherdir[i], sizeof(struct Entry));
        } else {
            free(v_fatherdir[i]);
            v_fatherdir[i] = NULL;
        }
    }
    v_dirno = i;
    ParseAbsolute(path);
    for (ParsePath(path, dirname); (*path)[0] != '\0';ParsePath(path, dirname)) {
        if (v_fd_cd_dir(dirname) < 0) {
            res = -1;
            break;
        }
    }
    *path = (char*)malloc(strlen(dirname) + 1);
    memcpy(*path, dirname, strlen(dirname) + 1);
    if (res >= 0) {
        int checkfile = ScanEntry(dirname, pentry, 0), checkdir = ScanEntry(dirname, pentry, 1);
        if (checktail == 0) {
            if (checkfile < 0) {
                res = -1;
            } else {
                res = 0;
            }
        }
        if (checktail == 1) {
            if (checkdir < 0) {
                res = -1;
            } else {
                res = 1;
            }
        } else if (checktail == 2) {
            if (checkfile < 0 && checkdir < 0) {
                res = -1;
            } else if (checkfile < 0){
                res = 1;
            } else {
                res = 0;
            }
        } else if (checktail == 3) {
            if (ScanEntry(dirname, pentry, 0) < 0 && ScanEntry(dirname, pentry, 1) < 0) {
                res = 2;
            } else {
                res = -1;
            }
        }
    } 
    if (res < 0) {
        printf("Invalid path.\n");
    }
    free(pentry);
    return res;
}

/*
 *参数：dir，类型：char
 *返回值：1，成功；-1，失败
 *功能：改变目录到父目录或子目录
 */
int v_fd_cd_dir(char *dir)
{
    struct Entry *pentry;
    int ret;
    if(!strcmp(dir,"."))
    {
        return 1;
    }
    if(!strcmp(dir,"..") && (v_curdir == NULL || v_curdir->FirstCluster == 0))
        return 1;
    /*返回上一级目录*/
    if(!strcmp(dir,"..") && v_curdir != NULL && v_curdir->FirstCluster != 0)
    {
        free(v_curdir);
        v_curdir = v_fatherdir[--v_dirno];
        return 1;
    }
    pentry = (struct Entry*)malloc(sizeof(struct Entry));
    ret = ScanEntry(dir, pentry, 1);
    if(ret < 0)
    {
        free(pentry);
        return -1;
    }
    v_fatherdir[v_dirno++] = v_curdir;
    v_curdir = pentry;
    return 1;
}

int fd_cd_path(char *path) 
{
    if (v_fd_cd_path(&path, 1) < 0) {
        return -1;
    }
    v_fd_cd_dir(path);
    free(path);
    return 1;
}

/*
 *参数：path，类型：char*
 *返回值：1，成功；-1，失败
 *功能;删除当前目录下的文件或目录
 */
int fd_df(char *path)
{
    struct Entry *pentry, entry;
    char *tmp;
    unsigned char c;
    unsigned short seed,next;
    int ret, cluster, offset, cluster_addr;
    if ((ret = v_fd_cd_path(&path, 2)) < 0) {
        return -1;
    }
    pentry = (struct Entry*)malloc(sizeof(struct Entry));
    /*扫描当前目录查找文件*/
    if (ret == 0) {
        ret = ScanEntry(path, pentry, 0);
        /*清除fat表项*/
        seed = pentry->FirstCluster;
        while((next = GetFatCluster(seed))<0xfff7)
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
        if(lseek(fd,ret-0x40,SEEK_SET)<0)
            perror("lseek fd_df failed");
        if(write(fd,&c,1)<0)
            perror("write failed");
        free(pentry);
        if(WriteFat()<0)
            exit(1);
        free(path);
        return 1;
    } else if (ret == 1 && isEmpty(path)){
        ret = ScanEntry(path, pentry, 1);
        seed = pentry->FirstCluster;
        while((next = GetFatCluster(seed))<0xfff7)
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
        if(lseek(fd,ret-0x40,SEEK_SET)<0)
            perror("lseek fd_df failed");
        if(write(fd,&c,1)<0)
            perror("write failed");
        free(pentry);
        if(WriteFat()<0)
            exit(1);
        free(path);
        return 1;
    } else {
        ScanEntry(path, pentry, 1);
        for (cluster = pentry->FirstCluster; cluster < 0xfff7; cluster = GetFatCluster(cluster)) { 
            cluster_addr = DATA_OFFSET + (cluster-2) * CLUSTER_SIZE;
            if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
                perror("lseek cluster_addr failed");
            offset = cluster_addr;
            while(offset < cluster_addr + CLUSTER_SIZE)
            {
                ret = GetEntry(&entry);
                offset += abs(ret);
                if(ret > 0)
                {
                    if (entry.subdir && !isEmpty(entry.short_name)) {
                        v_fd_cd_dir(path);
                        fd_df(entry.short_name);
                        v_fd_cd_dir("..");
                    } else {
                        fd_df(entry.short_name);
                    }
                }
            }
        }
        return 1;
    }
}

/*
 *参数：path，类型：char*，创建文件的路径
 *返回值：1，成功；-1，失败
 *功能：在当前目录下创建文件
 */
int fd_cf(char *path, char *content)
{
    struct Entry *pentry;
    int size,ret,i=0,cluster_addr,offset,cluster;
    unsigned short clusterno[100];
    unsigned char c[DIR_ENTRY_SIZE];
    int index,clustersize;
    unsigned char buf[DIR_ENTRY_SIZE];
    if (v_fd_cd_path(&path, 3) < 0) {
        return -1;
    }
    pentry = (struct Entry*)malloc(sizeof(struct Entry));
    if (content != NULL) {
        size = strlen(content) + 1;
    } else {
        size = 1;
    }
    clustersize = (size / (CLUSTER_SIZE));
    if(size % (CLUSTER_SIZE) != 0)
        clustersize ++;
    if (content == NULL) {
        size = 0;
    }
    /*查询fat表，找到空白簇，保存在clusterno[]中*/
    for(cluster=2;cluster<=10240;cluster++)
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
    if(v_curdir==NULL || v_curdir->FirstCluster == 0)  /*往根目录下写文件*/
    {
        if((ret= lseek(fd,ROOTDIR_OFFSET,SEEK_SET))<0)
            perror("lseek ROOTDIR_OFFSET failed");
        offset = ROOTDIR_OFFSET;
        while(offset < DATA_OFFSET)
        {
            if((ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
                perror("read entry failed");
            offset += abs(ret);
            if((buf[0]== 0xe5 || buf[0]== 0x00) && buf[11] != 0x0f) {
                if((ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
                    perror("read entry failed");
                offset += abs(ret);
                if ((buf[0] == 0xe5 || buf[0] == 0x00) && buf[11] != 0x0f) {
                    offset = offset-2 * abs(ret);
                    if(lseek(fd,offset,SEEK_SET)<0)
                        perror("lseek fd_cf failed");
                    for(;i<=10;i++)
                        c[i]=' ';
                    for(i=1;i<=strlen(path);i++)
                    {
                        c[i]=toupper(path[i]);
                    }
                    c[22] = 0x00;
                    c[23] = 0x00;
                    c[24] = 0x00;
                    c[25] = 0x00;
                    /*写第一簇的值*/
                    c[26] = (clusterno[0] &  0x00ff);
                    c[27] = ((clusterno[0] & 0xff00)>>8);
                    /*写文件的大小*/
                    c[28] = (size &  0x000000ff);
                    c[29] = ((size & 0x0000ff00)>>8);
                    c[30] = ((size& 0x00ff0000)>>16);
                    c[31] = ((size& 0xff000000)>>24);
                    c[0] = 0xe5;
                    c[11] = 0x0f;
#ifdef DEBUG
                    printf("write:%x\n", offset);
#endif
                    if(write(fd,&c,DIR_ENTRY_SIZE)<0)
                        perror("write failed");

                    c[0] = path[0];
                    if (content == NULL) {
                        c[11] = 0x10;
                    } else {
                        c[11] = 0x01;
                    }
                    if(lseek(fd,offset + 0x20,SEEK_SET)<0)
                        perror("lseek fd_cf failed");
#ifdef DEBUG
                    printf("write:%x\n", offset + 0x20);
#endif
                    if(write(fd,&c,DIR_ENTRY_SIZE)<0)
                        perror("write failed");
                    if(WriteFat()<0)
                        exit(1);
                    break;
                }
            }
        }
        for (i = 0; i < clustersize; i++) {
#ifdef DEBUG
            printf("DATAOFFSET:%x\n", DATA_OFFSET);
#endif
            cluster_addr = DATA_OFFSET + (clusterno[i]-2) * CLUSTER_SIZE ;
            if((ret= lseek(fd,cluster_addr,SEEK_SET))<0)
                perror("lseek cluster_addr failed");
            if (content != NULL) {
                if (i == clustersize - 1) {
                    write(fd, content, strlen(content) + 1);
                } else {
                    write(fd, content, CLUSTER_SIZE);
                    content += CLUSTER_SIZE;
                }
            } else {
                c[0] = '.';
                c[1] = '\0';
                write(fd, c, DIR_ENTRY_SIZE);
                c[1] = '.';
                c[2] = '\0';
                c[26] = 0x00;
                c[27] = 0x00;
                write(fd, c, DIR_ENTRY_SIZE);
            }
        }
    }
    else 
    {
        for (cluster = v_curdir->FirstCluster; cluster < 0xfff7; cluster = GetFatCluster(cluster)) { 
            cluster_addr = (cluster -2 )*CLUSTER_SIZE + DATA_OFFSET;
            if((ret= lseek(fd,cluster_addr,SEEK_SET))<0)
                perror("lseek cluster_addr failed");
            offset = cluster_addr;
            while(offset < cluster_addr + CLUSTER_SIZE)
            {
                if((ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
                    perror("read entry failed");
                offset += abs(ret);
                if ((buf[0]== 0xe5 || buf[0] == 0x00) && buf[11] != 0x0f)
                {
                    if((ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
                        perror("read entry failed");
                    offset += abs(ret);
                    if ((buf[0]== 0xe5 || buf[0] == 0x00) && buf[11] != 0x0f) {
                        offset = offset-2 * abs(ret);
                        if(lseek(fd,offset,SEEK_SET)<0)
                            perror("lseek fd_cf failed");
                        for(;i<=10;i++)
                            c[i]=' ';
                        for(i=1;i<=strlen(path);i++)
                        {
                            c[i]=toupper(path[i]);
                        }
                        c[22] = 0x00;
                        c[23] = 0x00;
                        c[24] = 0x00;
                        c[25] = 0x00;
                        /*写第一簇的值*/
                        c[26] = (clusterno[0] &  0x00ff);
                        c[27] = ((clusterno[0] & 0xff00)>>8);
                        /*写文件的大小*/
                        c[28] = (size &  0x000000ff);
                        c[29] = ((size & 0x0000ff00)>>8);
                        c[30] = ((size& 0x00ff0000)>>16);
                        c[31] = ((size& 0xff000000)>>24);
                        c[0] = 0xe5;
                        c[11] = 0x0f;
#ifdef DEBUG
                        printf("write:%x\n", offset);
#endif
                        if(write(fd,&c,DIR_ENTRY_SIZE)<0)
                            perror("write failed");

                        c[0] = path[0];
                        if (content == NULL) {
                            c[11] = 0x10;
                        } else {
                            c[11] = 0x01;
                        }
                        if(lseek(fd,offset + 0x20,SEEK_SET)<0)
                            perror("lseek fd_cf failed");
#ifdef DEBUG
                        printf("write:%x\n", offset + 0x20);
#endif
                        if(write(fd,&c,DIR_ENTRY_SIZE)<0)
                            perror("write failed");
                        if(WriteFat()<0)
                            exit(1);
                        break;
                    }
                }
            }
            for (i = 0; i < clustersize; i++) {
                cluster_addr = DATA_OFFSET + (clusterno[i]-2) * CLUSTER_SIZE ;
                if((ret= lseek(fd,cluster_addr,SEEK_SET))<0)
                    perror("lseek cluster_addr failed");
                if (content != NULL) {
                    if (i == clustersize - 1) {
                        write(fd, content, strlen(content) + 1);
                    } else {
                        write(fd, content, CLUSTER_SIZE);
                        content += CLUSTER_SIZE;
                    }
                } else {
                    c[0] = '.';
                    c[1] = '\0';
                    write(fd, c, DIR_ENTRY_SIZE);
                    c[1] = '.';
                    c[2] = '\0';
                    c[26] = 0x00;
                    c[27] = 0x00;
                    write(fd, c, DIR_ENTRY_SIZE);
                }
            }
        }
    }
    free(path);
    free(pentry);
    return 1;
}

void do_usage()
{
    printf("please input a command, including followings:\n\tls [path]\t\t\tlist files\n\tcd <path>\t\tchange direcotry\n\tcf <path> <content>\tcreate a file\n\tdf <path>\t\tdelete a file\n\tmkdir <path> \t\tcreate a directory\n\texit\t\t\texit this system\n");
}

int main()
{
    char input[10], c;
    int size=0;
    char buf[120], content[1024];
    char *path;
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
        else if (strcmp(input, "ls") == 0) {
            if((c = getchar()) == '\n') {
                path = (char*)malloc(2);
                memcpy(path, ".", 2);
                fd_ls(path);
            } else if (c == ' ') {
                scanf("%s", buf);
                path = (char*)malloc(strlen(buf) + 1);
                memcpy(path, buf, strlen(buf) + 1);
                fd_ls(path);
            }
            v_free(0);
            free(path);
        }
        else if(strcmp(input, "cd") == 0)
        {
            scanf("%s", buf);
            path = (char*)malloc(strlen(buf) + 1);
            memcpy(path, buf, strlen(buf) + 1);    
            if (fd_cd_path(path) > 0) {
                v_free(1);
            } else {
                v_free(0);
            }
            free(path);
        }
        else if(strcmp(input, "df") == 0)
        {
            scanf("%s", buf);
            path = (char*)malloc(strlen(buf) + 1);
            memcpy(path, buf, strlen(buf) + 1);    
            fd_df(path);
            v_free(0);
            free(path);
        }
        else if(strcmp(input, "cf") == 0)
        {
            scanf("%s%s", buf, content);
            path = (char*)malloc(strlen(buf) + 1);
            memcpy(path, buf, strlen(buf) + 1);
            fd_cf(path, content);
            v_free(0);
            free(path);
        }
        else if(strcmp(input, "mkdir") == 0)
        {
            scanf("%s", buf);
            path = (char*)malloc(strlen(buf) + 1);
            memcpy(path, buf, strlen(buf) + 1);
            fd_cf(path, NULL);
            v_free(0);
            free(path);
        }
            do_usage();
    }	
    return 0;
}



