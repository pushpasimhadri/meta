/****************************************************************************
 * FileName:    ModuleDiagnostic.c
 * Author:      Suresh Kalidasan
 * Date:        20141213
 * Description:
 *              To access any register mapped via I/O range or CPU Range.
 *              This can be cross compiled for any arch.
 *              Suresh.Kalidasan@aricent.com for more information.
 * Version:     V0.1 20141213
 *              -Initial version and has limitation.
 *              -Tested in kernel > 3.0 for kernel 2.6 < ioctl needs to be 
 *              -modified
 **************************************************************************/

/*Include Files*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/sched.h>

/*Module Information*/
MODULE_LICENSE("GPL");

/*Driver Name*/
static char drv_name[]="DiagModule";

/*Memory Range, not used in V0.1*/
#define    MEMORY_START        0x41010000
#define    MEMORY_SIZE         (0x410e0000 - MEMORY_START)
/*Driver major number and minor number*/
#define    DRV_MAJOR           501
#define    DRV_MINOR           0
#define    ENABLE_ERR_DEBUG    (0)
#define    DELAY_OPERATION 	   (0)
#define    FAIL                (-1)
#define    LOOPTEST            (0)

/*work Q struct*/
static DECLARE_WAIT_QUEUE_HEAD(WorkerThreadFn_WaitQueue);
rwlock_t WorkerThreadFn_Lock;
/*device structure*/
static dev_t g_devID;
static struct cdev DiagDevice;
/*Global variables*/
static int bNodeRegistered=0;
static DEFINE_SPINLOCK(DiagOpenLock);
static int DiagOpenCount=0;
static int bDeviceAdded=0;

/*Local function*/
static long  Diag_ioctl(struct file *f, unsigned int cmd, unsigned long arg);
ssize_t Diag_read(struct file* pFile,char __user * pUserData, size_t size, loff_t* pPos);
ssize_t Diag_write(struct file* pFile, const char __user* pUserData, size_t size, loff_t* pPos);
int Diag_release (struct inode *, struct file *);
int Diag_open(struct inode *, struct file *);
static int WorkerThreadFn(void* pVal);

/*Driver structure*/
static struct file_operations Diag_fops = {
	.owner =    THIS_MODULE,
	.read =     Diag_read,
	.write =    Diag_write,
	.unlocked_ioctl =    Diag_ioctl,
	.open =     Diag_open,
	.release = Diag_release,
};

/*local struture used*/
struct mem {
	unsigned int operation;
	unsigned int address;
	unsigned int value;
#if DELAY_OPERATION
	unsigned int delay_value;
#endif
};



/*
 * Read operation, 1byte.
 * Input:  Address to read.
 * Output: Read content upon success else FAIL.
 */
int inb_b(unsigned int temp_addr)
{
	//static unsigned int addr_alloc;
	static char* addr_alloc;

	//Remap for 1byte
	addr_alloc = ioremap(temp_addr,1);
	if(addr_alloc != NULL) {
#if ENABLE_ERR_DEBUG
		printk(KERN_ERR "Calculated V.A. %x \n",addr_alloc);
#endif
		//read 8bit of data
		temp_addr = ioread8(addr_alloc);
		/*for memory mapped IO operation only*/
		//temp_addr=readb(addr_alloc);
	} else {
		printk(KERN_ERR "Failed ioremap (inb)\n");
		return FAIL;
	}
	//give back the remmaped address
	iounmap(addr_alloc);
	return temp_addr;
}

/*
 * Read operation, 1Word(2Bytes for 32bit Machine).
 * Input:  Address to read.
 * Output: Read content upon success else FAIL.
 */
unsigned int inw_w(unsigned int temp_addr)
{
	//static unsigned int addr_alloc;
	static char* addr_alloc;

	//Remap for 1Word
	addr_alloc = ioremap(temp_addr,2);
	if(addr_alloc != NULL) {
#if ENABLE_ERR_DEBUG
		printk(KERN_ERR "Calculated V.A. %x \n",addr_alloc);
#endif
		//read 16bit of data
		temp_addr = ioread16(addr_alloc);
	} else {
		printk(KERN_ERR"Failed ioremap (inw)\n");
         return FAIL;
     }
     //Give back the read address
     iounmap(addr_alloc);
     return temp_addr;
}

/*
 * Read operation, 32bit(4Bytes for 32bit Machine).
 * Input:  Address to read.
 * Output: Read content upon success else FAIL.
 */
unsigned int inl_l(unsigned int temp_addr)
{
    //static unsigned int addr_alloc;
    static char* addr_alloc;

    //For Debug by Ragul M
    printk(KERN_INFO "Input register %x \n", temp_addr);

    //Remap for 4byte
    addr_alloc = ioremap(temp_addr,4);
    if(addr_alloc != NULL) {
#if ENABLE_ERR_DEBUG
		 printk(KERN_ERR "Calculated V.A. %x \n",addr_alloc);
#endif
        //read 32bit of data
         temp_addr = ioread32(addr_alloc);
    } else {
         printk(KERN_ERR"Failed ioremap (inl)\n");
         return FAIL;
    }

    //Give back the read address.
    iounmap(addr_alloc);
    return temp_addr;
}

/*
 * Write operation, 8bit(1Byte for 32bit Machine).
 * Input:  Address to write and its data.
 */
void outb_b(unsigned int value, unsigned int temp_addr)
{
    //static unsigned int addr_alloc;
    static char* addr_alloc;

    //remap for 1 byte
    addr_alloc = ioremap(temp_addr,1);

    if(addr_alloc != NULL) {
#if ENABLE_ERR_DEBUG
	printk(KERN_ERR "Calculated V.A. %x \n",addr_alloc);
#endif
        iowrite8(value, addr_alloc);
        /*for memory mapped IO operation*/
       //writeb(value,addr_alloc);
    } else {
       printk(KERN_ERR"Failed ioremap (outb)\n");
       return;
   }

        iounmap(addr_alloc);
}

/*
 * Write operation, 16bit(2Byte for 32bit Machine).
 * Input:  Address to write and its data.
 */
void outw_w(unsigned int value, unsigned int temp_addr)
{
    //static unsigned int addr_alloc;
    static char* addr_alloc;

    //remap the addr
    addr_alloc = ioremap(temp_addr,2);

    if(addr_alloc != NULL) {
#if ENABLE_ERR_DEBUG
        printk(KERN_ERR "Calculated V.A. %x \n",addr_alloc);
#endif
        //actual write of 16bit data
        iowrite16(value, addr_alloc);
    }else {
        printk(KERN_ERR"Failed ioremap (outw) \n");
        return;
    }
    //give back the address.
    iounmap(addr_alloc);
}

/*
 * Write operation, 32bit(4Byte for 32bit Machine).
 * Input:  Address to write and its data.
 */
void outl_l(unsigned int value, unsigned int temp_addr)
{

    //static unsigned int addr_alloc;
	static char* addr_alloc;

	addr_alloc = ioremap(temp_addr,4);

	if(addr_alloc != NULL) {
#if ENABLE_ERR_DEBUG
	    printk(KERN_ERR "Calculated V.A. %x \n",addr_alloc);
#endif
        //write 32bit data
        iowrite32(value, addr_alloc);
    } else {
        printk(KERN_ERR"Failed ioremap (outl) \n");
        return;
    }

    iounmap(addr_alloc);
}

/*
 * driver IOCTL call, this is called from user space
 * connected via node of major and minor numbers
 */
//#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
//int Diag_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
static long Diag_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{

     unsigned int a=0;
#if DELAY_OPERATION
     unsigned int temp_delay;
#endif
     struct mem tmp_mem;
     //tmp_mem = (struct mem *)arg;

    if(raw_copy_from_user(&tmp_mem,(void*) arg,sizeof(tmp_mem)))
    {
        return -EFAULT; //Segmentation fault.
    }
#if DELAY_OPERATION
	tmp_mem.delay_value = 0;
#endif

	switch(tmp_mem.operation) {

            case 13:
	        a = inb_b(tmp_mem.address);
		printk(KERN_INFO "Read Value from %x is %x \n", tmp_mem.address, a);
            break;
	    case 14:
	        a = inw_w(tmp_mem.address);
		printk(KERN_ERR "Read Value from %x is %x \n", tmp_mem.address, a);
	    break;
	    case 15:
		a = inl_l(tmp_mem.address);
		printk(KERN_INFO "Read Value from %x is %x \n", tmp_mem.address, a);
	    break;
	    case 23:
		outb_b(tmp_mem.value,tmp_mem.address);
		printk(KERN_ERR "Write Operation Completed \n");
	    break;
	    case 24:
         	outw_w(tmp_mem.value,tmp_mem.address);
		printk(KERN_ERR "Write Operation Completed \n");
	    break;
            case 25:
	  	outl_l(tmp_mem.value,tmp_mem.address);
		printk(KERN_ERR "Write Operation Completed \n");
	    break;
	    default:
		printk(KERN_ERR " Denied \n");
		return -1;
	}

#if DELAY_OPERATION
	for(temp_delay=1;temp_delay<=tmp_mem.delay_value;temp_delay++) {
		udelay(1000);
	}
#endif
return 0;
}



		/*
		 * driver read call, currently this is dummy, user can try this
		 */
ssize_t Diag_read(struct file* pFile,char __user * pUserData, size_t size, loff_t* pPos)
{
	ssize_t iRetVal=-EFAULT;
	if(!raw_copy_to_user(pUserData,&bDeviceAdded,sizeof(int)))
	{
		//TODO: Send something meaningful to the user
		iRetVal=sizeof(int);
	}
	else
	{
		printk(KERN_ERR "Diag:Unable to write to userspace\n");
	}

	return iRetVal;
}

/*
 * driver write operation, this is dummy now and user can test this
 */
ssize_t Diag_write(struct file* pFile, const char __user* pUserData, size_t size, loff_t* pPos)
{
	printk(KERN_INFO "Diag:Wrote %d\n",size);
	//TODO: Interpret & Process the data
	return size;
}

/*
 * driver release call, intialzied resources will be released.
 */
int Diag_release(struct inode* pInode,struct file* pFile)
{
	spin_lock(&DiagOpenLock);
	DiagOpenCount--;
	spin_unlock(&DiagOpenLock);
	return 0;
}

/*
 * driver open call.
 */
int Diag_open(struct inode* pInode,struct file* pFile)
{
	int iRetVal=0;
	spin_lock(&DiagOpenLock);

	printk(KERN_INFO "In driver open routine\n");
	if(!DiagOpenCount)
	{
		//global count variable
		DiagOpenCount++;
	}
	else
	{
		iRetVal=-EBUSY;
	}
	spin_unlock(&DiagOpenLock);

	return iRetVal;
}

/*
 * driver cleanup call.
 */
void Cleanup(void)
{
	//clean by reverse
	if(bDeviceAdded)
	{
		cdev_del(&DiagDevice);
		bDeviceAdded=0;
	}

	if(bNodeRegistered)
	{
		unregister_chrdev_region(g_devID,1);
		bNodeRegistered=0;
	}
}



/*
 * module init
 */
static int Diag_init(void)
{
	printk(KERN_ALERT "Diagnostic Operation Loaded...\n");

	//create device
	g_devID=MKDEV(DRV_MAJOR,DRV_MINOR);

	//register the created device
	if(register_chrdev_region(g_devID,1,drv_name))
	{
		printk(KERN_ERR "Unable to register device\n");
		return -1;

	}
	bNodeRegistered=1;

	//init device.
	cdev_init(&DiagDevice,&Diag_fops);

	//attach the driver
	if(cdev_add(&DiagDevice,g_devID,1))
	{
		printk(KERN_ERR "Unable to add device\n");
		Cleanup();
		return -1;
	}

	bDeviceAdded=1;

	/*
	 * this is only for user understanding, not used.
	 * this can be corrected
	 */ 
	//int iRetKT=kernel_thread(WorkerThreadFn,0,0);
	//printk(KERN_ERR "Kernel Thread iRetKT = %d \n",iRetKT);

	return 0;
}

/*
 * exit the driver
 */
static void Diag_exit(void)
{
	Cleanup();
	printk(KERN_ALERT "Diagnostic Operation Closed...\n");
}


/*
 * clean up this, i dont have udpated one. will share updated if i get
 * from my back up.
 */
static int WorkerThreadFn(void* pVal)
{
	unsigned int Counter=0;
	unsigned int CounterLoop=0;
	DECLARE_WAITQUEUE(wait, current);
	printk(KERN_ERR "WTF 1 \n");
	//	daemonize("WorkerThreadFn");
	printk(KERN_ERR "WTF 2 \n");
	//allow_signal(SIGKILL);
	printk(KERN_ERR "WTF 3 \n");
	//	add_wait_queue(&WorkerThreadFn_WaitQueue,&wait);
#if LOOPTEST
	while(1)
	{
		printk(KERN_ERR "WTF 4 \n");
		set_current_state(TASK_RUNNING);
		printk(KERN_ERR "WTF 5 \n");
		schedule();
		printk(KERN_ERR "WTF 6 \n");
		if(signal_pending(current)) break;
		printk(KERN_ERR "WTF 7 \n");
		read_lock(&WorkerThreadFn_Lock);
		printk(KERN_ERR "WTF 8 \n");
		if(Counter++ >= 3)
		{
			if(CounterLoop++ >= 2)
			{
#endif
				printk(KERN_ERR "Thread Counter = %d \n",Counter);
				//				mdelay(1000);
#if LOOPTEST
				CounterLoop=0;
			}
			Counter=0;
		}
		read_unlock(&WorkerThreadFn_Lock);
		printk(KERN_ERR "WTF 9 \n");
	}
#endif
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&WorkerThreadFn_WaitQueue, &wait);


	return 0;
}


/*Module entry and exit function*/
module_init(Diag_init);
module_exit(Diag_exit);
