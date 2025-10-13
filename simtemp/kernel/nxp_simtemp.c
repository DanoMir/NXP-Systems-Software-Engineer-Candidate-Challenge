/***************************************************************************
        Open Source License 2025 NXP Semiconductor Challenge Stage
****************************************************************************
* Title        : nxp_simptemp.c
* Description  : Source File of simtemp
*
* Environment  : C Language
*
* Responsible  : Daniel R Miranda [danielrmirandacortes@gmail.com]
*
* Guidelines   : 
*
*
* 
*
***************************************************************************
* * * * * * * * * * * * * * * * * * * * * * * * * * * *
* * * * * * * * * * * Header Files *   *   *   *   *   *
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <linux/fs.h>               //File operations
#include <linux/platform_device.h>  //Platform Device
#include <linux/miscdevice.h>       //Misdevice
#include <linux/spinlock.h>         //Sincronization of spinlock                     
#include <linux/wait.h>             //Notification of wait queue
#include <linux/hrtimer.h>          //Timer for simulation
#include <linux/slab.h>             //kzalloc/kfree    
#include <linux/uaccess.h>          //Copy to user 

#include <linux/time.h>             //Timer for simulation
#include <linux/of.h>               //Device Tree    

#include <linux/mod_devicetable.h>
#include <linux/device.h>
#include <linux/poll.h>
#include <linux/random.h>
#include <linux/time.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel Miranda");
MODULE_DESCRIPTION("NXP SimpTemp");
MODULE_VERSION("1.0");

#define RING_BUFFER_SIZE    32          //Size of buffer
#define SAMPLE_AVAILABLE    (1<<0)
#define TRESHOLD_CROSSED    (1<<1)




// /--------------File Operations Prototypes for "read" and "poll"---------------

static int nxp_simtemp_open(struct inode *inode, struct file *file);                                //Function Prototype performed once when user space opens the file
static int nxp_simtemp_release(struct inode *inode, struct file *file);                             //Function Prototype performed when user space calls to close() or when the process end.
static ssize_t nxp_simtemp_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);   //Function Prototype performed when User Space calls to read().  
static __poll_t nxp_simtemp_poll(struct file *file, struct poll_table_struct *wait);                //Function Prototype performed when User Space calls to poll(), select() or epoll().
static void nxp_simtemp_remove(struct platform_device *pdev);                                       //Function Prototype [Kernel] structure from "platform_device.h"
static int nxp_simtemp_probe(struct platform_device *pdev);  

//static void simtemp_timer_setup(struct nxp_simtemp_dev *dev);//Function Prototype [Kernel] structure from "platform_device.h"
enum hrtimer_restart simtemp_timer_callback(struct hrtimer *timer);
//static void __exit nxp_simtemp_exit(void);
//static int __init nxp_simtemp_init(void);

// // //----------------------Logic Prototypes fo Functions [Logic]------------------
// static bool simtemp_buffer_is_empty(struct nxp_simtemp_dev *dev);
// static void simtemp_buffer_push(struct nxp_simtemp_dev *dev, const struct simtemp_sample *sample);
//static bool simtemp_buffer_pop(struct nxp_simtemp_dev *dev, struct simtemp_sample *sample);
// static void simtemp_buffer_init(struct simtemp_ring_buffer *rb);


//--------------------------Data Structure---------------------------------------
//----------------- Data Structure: Transferred Data  --------------------//
struct simtemp_sample       // Data Structure Contract [Logic]: Defines transferred data between kernel and user space. Inside the kernel and performed by user space
{
    u64 timestamp_ns;       //Sample in nanoseconds: Calculates the accuracy of sampling and jitter from User Space
    s32 temp_mC;            //Sample in millidegrees: Represents values in float point without float point arithmetic
    u32 flags;              //Sample status: Notificates to applications if NXP_SAMPLE_AVAILABLE or NXP_THRESHOLD_CROSSED   

}
__attribute__((packed));    //Avoid memory padding. Recomended by NXP Challenge to keep the same size for Kernel/User

//---------------- Data Structure:  Ring Buffer  ------------------------------------//
struct simtemp_ring_buffer  //Structure for Storage [Logic]: Defines the architecture that stores and manipulates the data of "simtemp_sample[]""
{
    struct simtemp_sample buffer[RING_BUFFER_SIZE]; //Structure Data Contract [Logic]:
    size_t head;                                    //Writing Index: Used by producer "hrtimer" that puts the next sample 
    size_t tail;                                    //Reading Index: Used by consumer "read" of User Space that takes the next sample.
    u32 count;                                      //Counter to determine the validate samples in the buffer: ((count == 0) or (count == RING_BUFFER_SIZE)). 

};

//------------- Data Structure:  Driver (nxp_simtemp)   ----------------------------------------
struct nxp_simtemp_dev      //Global Structure [Logic]: Contains the configuration values, functionalities and interfaces of Driver reside
{    
    struct miscdevice           mdev;       //Structure of Interface [Kernel]: Miscellaneous Device to register the Character Device
    wait_queue_head_t           wq;         //Structure of sincronization [Kernel]: Used by "poll" function
    spinlock_t                  lock;       //Structure of concurrency [Kernel]: Protection of storage (shared resources) of interrupts and simultaneous access 

    struct hrtimer              timer;      //Structure of timer [Kernel]: Data Producer to initializes the High Resolution
    ktime_t                     period_ns;  //Structure of Time Type [Kernel]: Data Times with nanosecond precision

    struct simtemp_ring_buffer  rb;         //Structure of storage [Logic]: Circular buffer (Data storage)

    //Configuration of variables for sysfs to export information from Kernel Subsystems to space user
    s32                         threshold_mC;   //Temperature
    s32                         sampling_ms;    //Period

    //Configuration of variables for statistics
    u32                         alerts_count;   //Variable for diagnostic function as logic counter (stats_show) that indicates how many data crossed a critical treshold
    u32                         updates_count;  //Variable for diagnostic function as logic counter that indicates how many data was produced

};


//---------------Timer Callback (Data Generator) Producer------------------------------------------
// Telemetry System
//Timer Intializer function
// High Precision 'hrtimer' configutration.
static void simtemp_timer_setup(struct nxp_simtemp_dev *dev) // For nxp_simtemp_probe(). Here '*dev' pointer is created.
{
    //Period is defined by default (100ms)
    dev->period_ns = ms_to_ktime(dev->sampling_ms);

    //Timer is initialized.
    // CLOCK_MONOTONIC: Clock from [kernel] independently from changes.
    // HRTIMER_MODE_REL: Configuration to trigger periodically.
    hrtimer_init(&dev->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL); 

    //Assigns pointer to function every time timer is triggered
    //Timer start
    dev->timer.function = simtemp_timer_callback; //Data producer where the sample is generated and Ring Buffer is full. 

    //Kernel starts to perform Timer in time interval defined
    //Timer starts
    hrtimer_start(&dev->timer, dev->period_ns, HRTIMER_MODE_REL);

    
}




static struct platform_device *simtemp_pdev;

//extern struct platform_driver nxp_simtemp_driver;

//------------END DEBUG------------------

// //----------------------Logic Prototypes fo Functions [Logic]------------------
static bool simtemp_buffer_is_empty(struct nxp_simtemp_dev *dev);
static void simtemp_buffer_push(struct nxp_simtemp_dev *dev, const struct simtemp_sample *sample);
static bool simtemp_buffer_pop(struct nxp_simtemp_dev *dev, struct simtemp_sample *sample);
static void simtemp_buffer_init(struct simtemp_ring_buffer *rb);

//---Ring Buffer Logic Initialization-----------------
static void simtemp_buffer_init(struct simtemp_ring_buffer *rb)
{
    //Initializes Writing Pointer (Head)
    rb->head = 0;

    //Initializes Reading Pointer (Tail)
    rb->tail = 0;

    //Initializes Writing Pointer (Counter)
    rb->count = 0;

    //Maybe memset to clean buffer.
}



// ---------------------- (SimTemp Functions)  ---------------------------------------

//Logic Prototypes (SimTemp Function-Ring Buffer State): Verifies if ring buffer is empty, useful for read() and poll())
static bool simtemp_buffer_is_empty(struct nxp_simtemp_dev *dev)
{
    return (dev->rb.count == 0);

}

//Logic Driver Producer (SimTemp Function-Buffer Push): Push Function for write a new sample called by hrtimer[kernel] (Producer)
//simtemp_buffer_push is a function by hrtimer() callback.
//Writes a new sample in Ring Buffer even with overwrite.

static void simtemp_buffer_push(struct nxp_simtemp_dev *dev, const struct simtemp_sample *sample)
{
    //Algorithm of Overwrite Logic, If buffer is full, 
    if(dev->rb.count == RING_BUFFER_SIZE) //Overwrite
    {
        dev->rb.tail = (dev->rb.tail + 1) % RING_BUFFER_SIZE;
        dev->rb.count--;
        printk(KERN_WARNING "NXP SimTemp: Buffer overflow, discarded sample.\n");


    }
    //After overwriting, new data is writed and Tail and Head is updated.
    //Algorithm of sample writing through module (Wrap Around)
    dev->rb.buffer[dev->rb.head] = *sample;                 //sample value is copied to buffer array in actual position of writing pointer.
    dev->rb.head = (dev->rb.head + 1) % RING_BUFFER_SIZE;   //if head reaches to end of Ring Buffer, module makes the pointer returns to 0 index.
    dev->rb.count++;                                        //counter is incremented
}

//Logic Prototypes (SimTemp Function-Pop): Read and remove the oldest sample (Called by read() function)
//Consumer Central Routine in [Kernel] Driver
//Used only in nxp_simtemp_read() function while SpinLock is implemented, ensuring the atomicity.
static bool simtemp_buffer_pop(struct nxp_simtemp_dev *dev, struct simtemp_sample *sample)
{
    //Verifies if Buffer have data, if empty returns false.
    if(simtemp_buffer_is_empty(dev))
    {
        return false; //If nothing to read
        //Blocks the process through wait_event_interruptible process
    }

    //Copy the data and the queue moves forward
    *sample = dev->rb.buffer[dev->rb.tail];     
    
    //If Tail reaches the end of the array, returns to 0 index
    dev->rb.tail = (dev->rb.tail + 1) % RING_BUFFER_SIZE;   //Moves Tail one position forward through "wrap-around" with module(%)
    dev->rb.count--;                                        //Decreases the counter to indicate that sample buffer has been consumed.

    return true; //If reading was successful
}

//---------------Timer Callback (Data Generator) Producer------------------------------------------
//------------------Data Producer [Kernel] periodic and precise ------------------ 
// Activated each time when 'hrtimer' is triggered each 'sampling_ms'
// Interruption context (Softirq) soft interruption
enum hrtimer_restart simtemp_timer_callback(struct hrtimer *timer) //[kernel]
{
    // Obtains the memory address of 'nxp_simtemp_dev' through 'struct hrtimer *timer'
    struct nxp_simtemp_dev *dev = container_of(timer, struct nxp_simtemp_dev, timer); //Macro [kernel] to navigates in memory, obtains the memory address
    struct simtemp_sample   sample;     // access to timestamp_ns, temp_mC and flags
    unsigned long flags;                // variable flag
    s32 current_temp;
    u32 random_offset;

    //Data Generation

    get_random_bytes(&random_offset, sizeof(random_offset));
    current_temp = 45000 + (random_offset % 10000 ) - 5000;

    sample.timestamp_ns = ktime_get_real_ns();       //Generates a timestamp in nanoseconds
    sample.temp_mC = current_temp;   //jiffies is a [kernel] counter 
    sample.flags = SAMPLE_AVAILABLE;            //Sets bit 0 to indicate a sample available for Consumer (read()).          

    //---Start critical section--
    //Ensuring atomicity (critical)
    spin_lock_irqsave(&dev->lock, flags);   //Adquires 'Spinlock' and disable interruptions in CPU

    if(current_temp > dev->threshold_mC)
    {
        sample.flags |= TRESHOLD_CROSSED;
        dev->alerts_count++;

    }



    //Data Writing [Logic]. Writes the sample in Ring Buffer through overwritting
    simtemp_buffer_push(dev, &sample);  //If buffer is full moving the tail if necessary

     dev->updates_count++;       //Counter for Diagnostic Function

    //Liberates 'spin_unlock()' adquired by 'simtemp_call_back()' or 'read()' rutines.
    spin_unlock_irqrestore(&dev->lock, flags); //Restores the interruptions states.

    //[Kernel] Wake-up the processes (read/poll) that are slept in Wait Queue (wq)
    wake_up_interruptible(&dev->wq); //Notifies the existence of new data to User Space processes
   
    
    //--End critical section---


    //Timer reassemble.
    //Compensate latency (callback time) and programes the next trigger after (dev->period_ns)
    hrtimer_forward_now(timer,dev->period_ns); //Mantains the periodicity

    return HRTIMER_RESTART; //Data required by 'hrtimer' API [kernel] to timer comes back 
}

// --------------------    Platform Driver     ------------------------------------
// ------------      Final register of Driver------------------------//
//----------------        File Prototypes--------------------------------

//-----------Platform Driver: ----- Hardware --------------------
// //--------Structure for Device Tree (Matching)------------------//
static const struct of_device_id nxp_simtemp_dt_match[]=
{
    {   .compatible = "nxp,simtemp"},
    {   /*Sentinel*/}                   //Sentinel stops the search
};


//-----------Platform Driver: ----- Driver Starter --------------------
//Indicates to Platform Driver from kernel How to manipulates this data from Driver
static struct platform_driver nxp_simtemp_driver=
{
    .probe              = nxp_simtemp_probe,        //Pointer to function that is performed by kernel when it finds a compatible device (nxp, simtemp)
    .remove             = nxp_simtemp_remove,       //Pointer to function that is performed by kernel when the module is unloaded (rmmod)
    .driver             =
    {
        .name           = "nxp_simtemp",            //Name of driver for file system of Kernel
        .owner          = THIS_MODULE,              //Indicates to Kernel that the set of functions belongs to actual module (user module)
        .of_match_table = nxp_simtemp_dt_match,     //Search in the Device Tree devices with the string (nxp, simtemp) if it is found, "probe" is activated
    },
};

// //---------File Operations Table: Functions for Driver Map-----------------

//----------------------- Files Prototypes------------------------------
//------Platform Driver: Interface Contract API .   nxp_simtemp_fops     --------------------------------------------------------------------
//Pointers table to functions
// "struct file_operations" [kernel]: Data Type provides by kernel. Contains all the pointers to the function that the driver performed to interact with files system of Linux (Kernel)
// "file_operations"[kernel]: Standard Template definied by Linux for Character Devices (/dev)
// "nxp_simtemp_fops" [logic]: Variable provides by implemented logic only visible inside this file (static). 
static const struct file_operations nxp_simtemp_fops =
{

    .owner      =THIS_MODULE,           //Indicates to Kernel that the set of functions belongs to actual module (user module)
    .open       =nxp_simtemp_open,      //Pointer to the function performed when User Space calls to open("/dev/simtemp", ...)
    .release    =nxp_simtemp_release,   //Pointer to the function performed when User Space calls to close(fd). 
    .read       =nxp_simtemp_read,      //Pointer to the function performed when User Space calls to read(fd, ...)
    .poll       =nxp_simtemp_poll,      //Pointer to the function performed when User Space calls to poll() or epoll().

};

//------------------ Platform Device     Functions     -----------------------------------/
// ----------- Platform Device: Interface Functions -------------

//nxp_simtemp_open() [Logic]
//'struct inode *inode' (/dev/simtemp) 
//'struct file *file' are parameters [kernel] for Active Sesion Application-Driver
//"file" represents the specific instance during the opening
static int nxp_simtemp_open(struct inode *inode, struct file *file)         //Function [Logic] performed once when User Space opens the file /dev/simtemp
{
    //Recover the pointer to Global State struct(nxp_simtemp_dev). Useful for the following functions.
    //'container_of' [kernel] obtains the pointer (file->private_data) from 'nxp_simtemp_dev' through 'mdev'
    // Here is created '*nxp_dev' pointer
    struct nxp_simtemp_dev *nxp_dev = container_of(file->private_data, struct nxp_simtemp_dev, mdev); 

    file->private_data = nxp_dev; //Stores the Driver Pointer in field (private_data) of structure (file).

    return 0;
}

static int nxp_simtemp_release(struct inode *inode, struct file *file)  //Prototype of function performed when user space calls to close() or when the process end.
{

//Here memory is liberated if specific memory was assignated for this aperture process.

return 0;
}

//---------nxp_simtemp_read() [Logic]--------- Consumer function for access to producer (hrtimer and Ring Buffer) performed in Kernel
// *buf: Pointer(char*) to Destination Buffer for RAM memory of User Space reserves to receive the sensor.
//char __user: Critical Qualifier of [kernel] to indicates this pointer (char*) does not belongs to Kernel.
static ssize_t nxp_simtemp_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)      //Prototype of function performed when User Space calls to read().
{
    //[Logic] Recovers the pointer nxp_dev from open()
    struct nxp_simtemp_dev *dev = file->private_data; //Asigns the memrory direction revovered from (file->private_data) to dev variable
    struct simtemp_sample sample;       //Sensor sample
    ssize_t retval = 0;                 //    
    unsigned long flags;            //Saves interruptions states.
    
    //Ring Buffer [Logic] must be large enough
    if (count < sizeof(sample))
    {
        return -EINVAL; //Error -22 Invalid Argument [kernel]: Buffer too small for sample.
    }


    while (simtemp_buffer_is_empty(dev))
    {
        if(file->f_flags & O_NONBLOCK)
        {
            return -EAGAIN; 
        }

        if (wait_event_interruptible(dev->wq, !simtemp_buffer_is_empty(dev)))
        {
            return -ERESTARTSYS;
        }
    }

    //Atomic extraction. SpinLock is acquired
    //Interrupts are disabled.
    //hrtimer is locked to avoid to write in Ring Buffer while read() is reading 
    //Avoids Race Condition.
    spin_lock_irqsave(&dev->lock, flags);
    
    //Buffer is readed.
    //Calls to Ring Buffer [Logic] to extract the oldest data.
    if (simtemp_buffer_pop(dev, &sample))
    {
        retval = sizeof(sample);    //Variable adquires the exact data of sample.

           if ((sample.flags & TRESHOLD_CROSSED) && dev->alerts_count > 0) {
                dev->alerts_count--;
        }
    }
    else
    {
        
        retval = -EAGAIN; //Error -11 Try Again. [Kernel] Only if buffer is empty just before the lock.

    }

    // [Kernel] Liberates SpinLock.
    // hrtimer returns to normal execution.
    spin_unlock_irqrestore(&dev->lock, flags);       

    if (retval > 0)
    {
        //Buffer Transfer [kernel]; Copies the Sample to Memory Direction of User Space Memory
        //Only allows to write in the buffer *buf of __user type is ONLY this function. 
        if (copy_to_user(buf, &sample, sizeof(sample)))
        {
            // If copy fails...
            return -EFAULT; //-14 [Kernel] Bad address

        }


    }

    return retval; //Returns the number of bytes (sample) in binary form reaed from (sizeof(sample))

}


//---------nxp_simtemp_poll() [Logic]--------- Events Mechanism--------
// Register the process like sleeping until data is ready and reports immediately.
//struct file *file [kernel]: gives access to Driver State to know what resources to protect and which data to verify.
//struct poll_table_struct *wait [kernel]: Register Mechanism of Callback that register the sleeping process from User Space in queue (wq)
static __poll_t nxp_simtemp_poll(struct file *file, struct poll_table_struct *wait)     //Prototype of function performed when User Space calls to poll(), select() or epoll().
{
    struct nxp_simtemp_dev *dev = file->private_data; //Saves pointer of Global Structure
    __poll_t mask = 0;          //Maks for python
    unsigned long flags;

    // [kernel] Register this process in Wait Queue (wq)
    // Crucial for the process to activate wake_up_interruptible and to be awakened.
    // Add User Space information to Wait Queue (dev->wq) from Driver 
    //Producer (hrtimer) calls to wake_up_interruptible(&dev->wq), Kernel reviews poll_table and wakes-up the Python Process
    poll_wait(file, &dev->wq, wait);    // poll_wait Logic [kernel] from poll_table_struct


    //Check reading status (Disponible data)
    if(!simtemp_buffer_is_empty(dev))
    {
        //mask to python
        mask |= (EPOLLIN | POLLRDNORM); // Disponible Data. PollInput: File is ready for reading. PollReadNormal: Normal Lecture Flag (no urgent)
    }

    //Critical section starts that uses a temporal spinlock to access to shared variable 'alerts_count'
    //Verificates alert events (Threshold)
    
    spin_lock_irqsave(&dev->lock, flags);

    if(dev->alerts_count >0)        //Global Variable
    {
        mask |= EPOLLPRI; //PollPriority: Event in high priotity
    }
    
    spin_unlock_irqrestore(&dev->lock, flags);
    //Critical section ends

    
    // Return of event mask (0 if must be waiting (sleeping)) or (>0 if wakes-up and performs read() function)
    return mask; 


}

//----------- sysfs Section-----------------------------





//Reading Function: Show
//Attribute sampling_ms(RW)
//[SHOW] Reading of the period of actual sample (dev->sampling_ms)
static ssize_t sampling_ms_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct nxp_simtemp_dev *nxp_dev = dev_get_drvdata(dev);
    unsigned long flags;
    ssize_t ret;

    //--------Critical Section: Protects access to shared variable---------
    spin_lock_irqsave(&nxp_dev->lock, flags);
    ret = sprintf(buf, "%u\n", nxp_dev->sampling_ms); //Copy value to 'buf'
    spin_unlock_irqrestore(&nxp_dev->lock, flags);
    //-------------------End of critical section---------------

    return ret;
};

//Write Function: Store
//[STORE]: Writing of new period of sampling (Stops/Restarts hrtimer)
static ssize_t sampling_ms_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct nxp_simtemp_dev *nxp_dev = dev_get_drvdata(dev);
    unsigned long value;
    unsigned long flags;
    int ret;

    //Converts input(strings) to number
    ret = kstrtoul(buf, 10, &value);
    if (ret)
    {
        return ret;
    }

    if(value < 10)
    {
        return -EINVAL;
    }

    //--------Critical Section: Stops, Updates and Restarts the hrtimer---------
    spin_lock_irqsave(&nxp_dev->lock, flags);

    //Cancel the timer to update period without race conditions
    hrtimer_cancel(&nxp_dev->timer);

    //Updating the state variables.
    nxp_dev->sampling_ms = (s32)value;
    nxp_dev->period_ns = ms_to_ktime(nxp_dev->sampling_ms);

    //Restarts timer with new period.
    hrtimer_start(&nxp_dev->timer, nxp_dev->period_ns, HRTIMER_MODE_REL);

    spin_unlock_irqrestore(&nxp_dev->lock, flags);
    //-------------------End of critical section---------------

    return count; //Return number of bytes processed.
};

//Reading Function: threshold mC Show
//Attribute threshold_mC(RW): Updates one variable
//[SHOW] Reading of alert umbral
static ssize_t threshold_mC_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct nxp_simtemp_dev *nxp_dev = dev_get_drvdata(dev);
    unsigned long flags;
    ssize_t ret;

    //--------Critical Section: ---------
    spin_lock_irqsave(&nxp_dev->lock, flags);

    ret = sprintf(buf, "%d\n", nxp_dev->threshold_mC); 

    spin_unlock_irqrestore(&nxp_dev->lock, flags);
    //-------------------End of critical section---------------

    return ret; 
}


//Reading Function: threshold mC store
//Attribute threshold_mC(RW): Updates one variable
//[SHOW] Writing the new umbral of alert
static ssize_t threshold_mC_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct nxp_simtemp_dev *nxp_dev = dev_get_drvdata(dev);
    s32 value;
    unsigned long flags;
    int ret;

    //Coonverts string input to int 32 bits
    ret = kstrtos32(buf, 10, &value);
    if (ret)
    {
        return ret;
    }

     //--------Critical Section: ---------
    spin_lock_irqsave(&nxp_dev->lock, flags);

    nxp_dev->threshold_mC = value;

    spin_unlock_irqrestore(&nxp_dev->lock, flags);
    //-------------------End of critical section---------------

    wake_up_interruptible(&nxp_dev->wq);

    return count; 


}


//Reading Function: Stats Show
//Attribute stats(R/O): Shows the counters of diagnostic in existence: updates_count, alerts_count.
//[SHOW] Reading of statiticals (updates, alerts, last error)
static ssize_t stats_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct nxp_simtemp_dev *nxp_dev = dev_get_drvdata(dev);
    unsigned long flags;
    ssize_t ret;

    //--------Critical Section: ---------
    spin_lock_irqsave(&nxp_dev->lock, flags);

    //Formats the output like a legible string with all counters.
    ret = sprintf(buf, "updates = %u\nalerts = %u\nlast error = %d\n", nxp_dev->updates_count, nxp_dev->alerts_count, 0); 
    
    spin_unlock_irqrestore(&nxp_dev->lock, flags);

    
    //-------------------End of critical section--------------

    return ret; 
};

//Static definitions of attributes of sysfs
static DEVICE_ATTR_RW(sampling_ms);      //Read/Write
static DEVICE_ATTR_RW(threshold_mC);    //Read/Write
static DEVICE_ATTR_RO(stats);            //Read Only


//Attrubutes List to register in probe()
static struct attribute *nxp_simtemp_attrs[]=
{
        &dev_attr_sampling_ms.attr,
        &dev_attr_threshold_mC.attr,
        &dev_attr_stats.attr,
        NULL, 
        
};

//Attrbutes for resgistration in subsystem of devices (devices_create_file)
static const struct attribute_group nxp_simtemp_attr_group =
{
    .attrs = nxp_simtemp_attrs,

};



//------------------ Platform Device     Functions     -----------------------------------/
// ----------- Platform Device: Drive Functions (Lifecycle)----------------------------------------/

static int nxp_simtemp_probe(struct platform_device *pdev)
{
    printk(KERN_INFO "Debug 0 Initializes probe() function\n");
    struct nxp_simtemp_dev *nxp_dev; 
    int ret;

    struct device *dev = &pdev->dev; //se sustituye &pdev->dev por dev (Puntero Local)

    dev_info(dev,"Debug 1 Start\n");

    //Memory Allocation: Allocates and clean memory for structure nxp_simtemp_dev.
    nxp_dev = devm_kzalloc(dev, sizeof(*nxp_dev), GFP_KERNEL);    //Allocate
    if (!nxp_dev)
    {
        dev_err(dev, "Debug 2 Memory allocation failed\n");
        //kfree(nxp_dev);//Liberacion manual de memoria
        return -ENOMEM; //Without Memory
    }
    dev_info(dev,"Debug 3 Memoria allocated and valid\n");
    platform_set_drvdata(pdev, nxp_dev); //[kernel] Saves the pointer 

    dev_info(dev,"Debug 4 Driver Data Set\n");
    
    
    //Initializes primitives for spinlock and wait_queue.
    spin_lock_init(&nxp_dev->lock);     //Initialize spinlock [Kernel Function]
    init_waitqueue_head(&nxp_dev->wq);  //Initialize waiting queue [Kernel Function]

    dev_info(dev,"Debug 5 Primitives intialized\n");


    //---------------hrtimer implementation---------------

    //Initializes Ring Buffer
    simtemp_buffer_init(&nxp_dev->rb); //Buffer initialized

    //Producer start: hrtimer_init() and hrtimer_start() are initialized.
    //Initialize the producer Timer
    simtemp_timer_setup(nxp_dev);


    //Device Tree Configuration (By default)
    nxp_dev->sampling_ms = 100;
    nxp_dev->threshold_mC = 45000;


    //Register miscdevice (/dev/simtemp).
    //Register of Character Device
    nxp_dev->mdev.minor = MISC_DYNAMIC_MINOR; // Asks Linux for a lower available number
    nxp_dev->mdev.name = "simtemp"; //File Name in dev/simtemp
    nxp_dev->mdev.fops = &nxp_simtemp_fops; // Operations Table from nxp_simtemp_fops is assigned to Files System of Linux (/dev/simtemp)

    //Register the Device Driver and /dev/simtemp is created by kernel .
    ret = misc_register(&nxp_dev->mdev); 

    if (ret)
    {
        dev_err(dev, "Debug 6. Error registered miscdevice\n");
        //kfree(nxp_dev);//Liberacion manual de memoria
        return ret;
    }

    //Syfs Register
    ret = sysfs_create_group(&pdev->dev.kobj, &nxp_simtemp_attr_group);

    if (ret)
    {
        dev_err(dev, "Debug 7 Error registered sysfs group\n");
        misc_deregister(&nxp_dev->mdev);
        
        return ret;

    }
    dev_info(dev, "Debug 8 Device and Syfs registered successfully\n");
    //dev_info(dev, "NXP SimTemp device registered at /dev/%s\n", nxp_dev->mdev.name );
    return 0;

}

//----------------Platform Device: Release function (reverse of probe) (Lifecycle)---------------------------
static void nxp_simtemp_remove(struct platform_device *pdev) //[Kernel] structure from "platform_device.h"
{
    struct nxp_simtemp_dev *nxp_dev = platform_get_drvdata(pdev); //[Kernel] structure from "platform_device.h"

    //Clean Unload [kernel]: Stops timer and desregister all
    //hrtimer_cancel(&nxp_dev->timer); //[Kernel] Stops the timer if miscdevice fails to prevents an Kernel Panic
    //misc_deregister(&nxp_dev->mdev); // [Kernel] Delete Character Device of system files durin the clean remove
    //Memory automatically is liberated by devm_kzalloc

    

    if(nxp_dev)
    {
        
      
        
        // Producer is stopped (hrtimer initialized in probe function)
        hrtimer_cancel(&nxp_dev->timer); 


        //*-------Sysfs Secion */
        sysfs_remove_group(&pdev->dev.kobj, &nxp_simtemp_attr_group);

        
          // Unregistered Interface: 
        misc_deregister(&nxp_dev->mdev);

        

        dev_info(&pdev->dev,"NXP SimTemp device unregistered. \n");
    }
    
    
    

}



//---------------------First Functions for compilation ---------------------

//[logic] This function calls to platform_driver_register(&nxp_simtemp_driver)
static int __init simtemp_runtime_init(void)
{
   int ret;
    
    // 1. Register platform driver (for probe() is ready)
    ret = platform_driver_register(&nxp_simtemp_driver);
    if (ret) {
        printk(KERN_ERR "NXP SimTemp: Failed to register platform driver. Ret: %d\n", ret);
        return ret;
    }

    // 2. Memory allocation for virtual device.
    simtemp_pdev = platform_device_alloc("nxp_simtemp", PLATFORM_DEVID_NONE);
    if (!simtemp_pdev) {
        printk(KERN_ERR "NXP SimTemp: Failed to allocate platform device.\n");
        platform_driver_unregister(&nxp_simtemp_driver);
        return -ENOMEM;
    }
    
    // 3. Add virtual device (forces to call to nxp_simtemp_probe())
    ret = platform_device_add(simtemp_pdev);
    if (ret) {
        printk(KERN_ERR "NXP SimTemp: Failed to add virtual platform device. Ret: %d\n", ret);
        platform_driver_unregister(&nxp_simtemp_driver);
        platform_device_put(simtemp_pdev);
        return ret;
    }

    // If call to nxp_simtemp_probe() was successful:
    printk(KERN_INFO "NXP SimTemp: Virtual device and driver registered successfully.\n");
    return 0;

    

}


static void __exit simtemp_runtime_exit(void)
{
    //For Clean Unload. Cleaning in inverse order.
    platform_device_unregister(simtemp_pdev);          //
    platform_driver_unregister(&nxp_simtemp_driver);   //
    printk(KERN_INFO "NXP SimTemp: Modulo unloades. Bye.\n");

}



//-------------------Macros (always placed al the end of the code for Linux )-------------------------------
module_init(simtemp_runtime_init); // [Kernel] Macro for indicate to kernel what funtion mus be called when the modulo is load nxp_simtemp_init
module_exit(simtemp_runtime_exit); // [Kernel] Macro for indicate to kernel what funtion mus be called when the modulo is load nxp_simtemp_init