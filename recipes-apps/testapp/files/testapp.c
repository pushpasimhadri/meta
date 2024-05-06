/****************************************************************************
 * FileName:    ApplicationDiagnostic.c
 * Author:      Suresh Kalidasan
 * Date:        20141213
 * Description:
 *              To access any register mapped via I/O range or CPU Range.
 *              This can be cross compiled for any arch.
 *              Suresh.Kalidasan@aricent.com for more information.
 * Version:     V0.1 20141213
 *              -Initial version and has limitation.
 **************************************************************************/

/*Include files*/
#include <stdio.h>
#include <fcntl.h>
#include <asm/ioctl.h>

/*node path, make sure to run mknod before this*/
#define Driver_path "/dev/ModuleDiagnostic"
#define DELAY_OPERATION (0)

/*Interface struct between driver and application, user configurable*/
struct mem {
	unsigned int operation;
	unsigned int address;
	unsigned int value;
#if DELAY_OPERATION
	unsigned int delay_value;
#endif
};

/*C entry*/
int main(void)
{
	int hFile;
	int result,i;
	struct mem obj_mem;

	//vic_cvbs_interface stApplnDrvIf;

	hFile = open(Driver_path, O_WRONLY);

	if(hFile == -1)
	{
		printf("open failed for %s\n", Driver_path);
		return -1;
	}
	perror("Opening /home/suresh/diagnostic ");

	obj_mem.operation = 0;

	printf(" Select Operation Read/Write, enter 1->(Read) & 2->(write) \n");
	printf(" R/W Byte(8)->3, Word(16)->4, Long(32)->5 (100)->Quit \n");
	printf(" Valid Operation Code's 13, 14, 15, 23, 24, 25 & 100  \n");
	printf(" To give delay between next instruction add last value as delay code \n");

	while(obj_mem.operation != 100) {

		printf(" Pass Operation Code \n");
		scanf("%d", &obj_mem.operation);

		switch(obj_mem.operation) {

			case 13:
				printf(" 8 bit Read Selected \n");
				printf(" Pass Address \n");
				scanf("%x", &obj_mem.address);
				break;
			case 14:
				printf(" 16 bit Read Selected \n");
				printf(" Pass Address \n");
				scanf("%x", &obj_mem.address);
				break;
			case 15:
				printf(" 32 bit Read Selected \n");
				printf(" Pass Address \n");
				scanf("%x", &obj_mem.address);
				break;
			case 23:
				printf(" 8 bit Write Selected \n");
				printf(" Pass Address \n");
				scanf("%x", &obj_mem.address);
				printf(" Pass Value \n");
				scanf("%x", &obj_mem.value);
				break;
			case 24:
				printf(" 16 bit Write Selected \n");
				printf(" Pass Address \n");
				scanf("%x", &obj_mem.address);
				printf(" Pass Value \n");
				scanf("%x", &obj_mem.value);
				break;
			case 25:
				printf(" 32 bit Write Selected \n");
				printf(" Pass Address \n");
				scanf("%x", &obj_mem.address);
				printf(" Pass Value \n");
				scanf("%x", &obj_mem.value);
				break;
			default:
				printf(" Wrong entry, pls try example if you want to do \n");
				printf(" 16 bit word read, your input should be 14 \n");
				printf(" Here (1) for Read & (4) for Word \n");

		}
#if DELAY_OPERATION
		printf(" Enter delay value (in ms) default value - 0 \n");
		scanf("%x", &obj_mem.delay_value);
#endif
		result = ioctl(hFile,0,&obj_mem);
	} /*while*/
	close(hFile);

	printf(" Memory Access Operation Quit ! \n");

	printf(" Press Ctl + D to Quit \n");

	return 0;
}
