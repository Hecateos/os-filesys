filesys: filesys.c filesys.h disk_image
	cc -o filesys filesys.c filesys.h
disk_image:	
	dd if=/dev/zero of=disk_image bs=1M count=20
	mkfs.msdos disk_image
	sudo mount -o loop disk_image virtual_fs/ -o umask=0000
clean:
	rm filesys disk_image
	sudo umount virtual_fs/
