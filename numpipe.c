
#include <linux/module.h> 		//Needed by all modules
#include <linux/init.h>			//Needed for macros
#include <linux/miscdevice.h>	//To handle miscellaneous devices
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <asm/uaccess.h>		//For put_user
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/kernel.h>	  	//Needed for KERN_INFO
#include <linux/fs.h>			//For file opr

#define DEVICE_NAME "numpipe"	//Dev name as it appears in /proc/devices:/dev/numpipe
#define BUFFER_SIZE 100			//Max length of the buffer to hold data 

#define DESC   " Kernel Module: Producer Consumer"
#define AUTHOR "Devina Dhuri <ddhuri1@binghamton.edu>"


//Passing Command Line Arguments to a Module
int pipe_size=5; 	
module_param(pipe_size,int,S_IRUGO);
MODULE_PARM_DESC(pipe_size, "The default pipe_size is 5 which can be replaced during installation");

// Declaring semaphores 
struct semaphore full;
struct semaphore empty;
struct semaphore mutex;

//static int Device_Open = 0;     //Is device open?  


//Functions
 //inode points to inode object and file is the pointer to file object.
 static int pipe_open(struct inode*,struct file *);
 //file is the pointer to file object, out is the buffer to hold data, size is length of buffer and off is its offset 
 static ssize_t pipe_write(struct file *file, const char *out, size_t size, loff_t *off);
 //file is the pointer to file object, out is the buffer to hold data, size is length of buffer and off is its offset 
 static ssize_t pipe_read(struct file *file, char *out, size_t size, loff_t *off);
  //inode points to inode object and file is the pointer to file object.
 static int pipe_close(struct inode*, struct file *);

//structure for queue
typedef struct pipe_queue
{
	int last;
	int first;	
	char ** buff;
}Queue;

Queue pipe;

//Structure for the File operation definition.
 struct file_operations pipe_fops = 
{
	.owner = THIS_MODULE,
	.release = pipe_close,	
	.open = pipe_open,
	.write = pipe_write,
	.read = pipe_read
	
};


// Declaring a Device Struct
static struct miscdevice pipe_device  = 
{
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEVICE_NAME,
	.fops = &pipe_fops
};


//To Register the miscellanous device driver.
static int __init numpipe_init(void)
{
	int ret,i;
	printk(KERN_INFO "NumPipe: Initializing the NumPipe KO with pipe_size=%d \n",pipe_size);
	//register the device
	ret = misc_register(&pipe_device);
	//check the return value error condition:
	if(ret<0)
	{	
		printk(KERN_ALERT "Registration FAILED!");
		return 0;
	}
	//The GFP_KERNEL flag specifies a normal kernel allocation. 
	pipe.buff = (char**)kmalloc(sizeof(char*) *pipe_size,GFP_KERNEL);
	if(pipe.buff == NULL)
	{
		//Error allocating memory in KERNEL
		printk(KERN_ALERT "ERROR: Kernel cannot allocate Memory!");
		//EFAULT is bad address error code
		return -EFAULT;
	}
	pipe.last=0;
	//initialization
	pipe.first=0;  

	for(i=0;i<pipe_size;i++)
	{
        pipe.buff[i] = (char*)kmalloc(sizeof(char)*BUFFER_SIZE,GFP_KERNEL);
		//memory allocation failure
		if(pipe.buff[i]==NULL)
		{
			//Error allocating memory in KERNEL
			printk(KERN_ALERT "ERROR: Kernel cannot allocate Memory!");
			return -EFAULT;
		}
    }
	//Controls access to critical section
	sema_init(&mutex,1);
	//Counts full buffer slots
	sema_init(&full,0);
	//Counts empty buffer slots
	sema_init(&empty,pipe_size);

	return 0;
}

//To de-registers the device driver.
static void __exit numpipe_exit(void) 
{
	int i;
	printk(KERN_INFO " Exiting from the NumPipe KO\n");
	//Free the allocated Memory in reveres order
	for(i=0;i<pipe_size;i++)
	{
        kfree(pipe.buff[i]);
    }
	//free memory
	kfree(pipe.buff);
	//int ret=misc_deregister(&pipe_device);
	misc_deregister(&pipe_device);
	//if (ret < 0)
	//	printk(KERN_ALERT "Error in unregistering device\n");
}

// Called when device is opened.  
static int pipe_open(struct inode *inode,struct file *file) 
{
	//if (Device_Open)
	//	return -EBUSY;
	//Device_Open++;
	printk(KERN_INFO "Device Opened\n");	
	return 0;
}
 

// Called when data to be written
static ssize_t pipe_write(struct file *file, const char *out, size_t size, loff_t * off)
{
	int status,ret_len;
	printk(KERN_INFO "Writing to the pipe...\n");
	
	//decrement the count of empty and the mutex
	if(down_interruptible(&empty) == 0 ) 
	{
		//if the device got interrupted the lock wont be applied 
		if(down_interruptible(&mutex)!=0)
		{
			printk(KERN_ALERT "Cannot acquire lock \n");
			return -EFAULT;
		} 
		//Entering the critical section
		else 
		{
			//check the last written location of buffer
			//if more than buffer size, reinitialize to start
			if(pipe.last >= pipe_size)
                	pipe.last=0;
			//set buff size to that of the writing bytes
			memset(pipe.buff[pipe.last],0,sizeof(char)*BUFFER_SIZE);	
			
			//copy from user to kernel area
			status = copy_from_user(pipe.buff[pipe.last],out,size);
			if(status==0) 
			{
				printk(KERN_INFO "Writing %s", pipe.buff[pipe.last]);
			}
			else
			{
				printk(KERN_ALERT "Error in writing!");
				return status;	
			}
			
			//Length of data written
			ret_len=strlen(pipe.buff[pipe.last]);
			//move to next node of queue
			pipe.last++;
			
			//leave critical region
			up(&mutex);
        }
		//increment counter of full slots
		up(&full);
	}
	else 
	{
		printk(KERN_ALERT "Cannot in acquire lock\n");
		return -EFAULT;
    }
	return ret_len;
}


//Called when data is to be read
static ssize_t pipe_read(struct file *file, char *out, size_t size, loff_t * off)
{
	int status,bytes_read;
	printk(KERN_INFO " Reading from the pipe\n");
	
	//decrement the count of full and the mutex
	if(down_interruptible(&full)==0) 
	{
		//Entering the critical section
		if(down_interruptible(&mutex)!=0)
		{
			printk(KERN_ALERT "Cannot acquire lock\n");
			return -EFAULT;	
		} 
		else 
		{
			//reinitialize buffer
			if(pipe.first >=pipe_size)
				pipe.first=0;
			
			//Calculate the length to be read
			bytes_read=strlen(pipe.buff[pipe.first]);
			//copy from kernel to user area
			status = copy_to_user(out, pipe.buff[pipe.first], bytes_read+1);
			//check copying status
			if(status ==0)
			{	
				printk(KERN_INFO "Reading: %s",pipe.buff[pipe.first]);
			}
			else
			{
				printk(KERN_ALERT " Error in reading!");	
				return -EFAULT;
			}
			pipe.first++;
			
			//Leave critical section
			up(&mutex);
		}
		//Increment count of empty slots
		up(&empty);
	} 
	else 
	{
		printk(KERN_ALERT "Cannot acquire lock\n");
		return -EFAULT;
	}
	return bytes_read;
}

//Called when device file is closed. 
static int pipe_close(struct inode * inode,struct file *file) 
{
	//Device_Open--;
	printk(KERN_INFO "Device Closed!\n");
	return 0;
}

//macros for init and exit
module_exit(numpipe_exit);
module_init(numpipe_init);

// What does this module do
MODULE_DESCRIPTION(DESC);	 
// Who wrote this module? 
MODULE_AUTHOR(AUTHOR);	
// License for the module
MODULE_LICENSE("GPL");

