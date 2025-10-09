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

//-----------cambios void por aca
//----Timer callback (Data Generator)
//Timer Intializer function
static void simtemp_timer_setup(struct nxp_simtemp_dev *dev)
{
    //Period is defined by default (100ms)
    dev->period_ns = ms_to_ktime(dev->sampling_ms);

    //hrtimer is initialized
    hrtimer_init(&dev->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL); 
    dev->timer.function = simtemp_timer_callback;

    //hrtimer starts
    hrtimer_start(&dev->timer, dev->period_ns, HRTIMER_MODE_REL);

    
}

//--------------inicio de mover enum 1
//---------------------------------------------aqui
//--------------fin de mover enum 1



//--------------*DEBUG*----------------

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
    rb->head = 0;

    //Initializes Writing Pointer (Counter)
    rb->count = 0;

    //Maybe memset to clean buffer.


}

//--------------cambios de void inicio


//--------------cambios de void fin


//-----------------fin de cambios void

// ---------------------- (SimTemp Functions)  ---------------------------------------

//Logic Prototypes (SimTemp Function-Ring Buffer State): Verifies if ring buffer is empty, useful for read() and poll())
static bool simtemp_buffer_is_empty(struct nxp_simtemp_dev *dev)
{
    return (dev->rb.count == 0);

}

//Logic Prototypes (SimTemp Function-Buffer Push): Push Function for write a new sample called by hrtimer[kernel] (Producer)
//simtemp_buffer_push is a function by hrtimer().

static void simtemp_buffer_push(struct nxp_simtemp_dev *dev, const struct simtemp_sample *sample)
{
    //Overwriting Logic, If buffer is full, 
    if(dev->rb.count == RING_BUFFER_SIZE) //Overwrite
    {
        dev->rb.tail = (dev->rb.tail + 1) % RING_BUFFER_SIZE;
        dev->rb.count--;
        printk(KERN_WARNING "NXP SimTemp: Buffer overflow, discarded sample.\n");


    }
    //Algorithm of sample writing through module (Wrap Around)
    dev->rb.buffer[dev->rb.head] = *sample;
    dev->rb.head = (dev->rb.head + 1) % RING_BUFFER_SIZE;
    dev->rb.count++; 
}

//Logic Prototypes (SimTemp Function-Pop): Read and remove the oldest sample (Called by read() function)
static bool simtemp_buffer_pop(struct nxp_simtemp_dev *dev, struct simtemp_sample *sample)
{
    if(simtemp_buffer_is_empty(dev))
    {
        return false; //If nothing to read
    }

    //Copy the data and the queue moves forward
    *sample = dev->rb.buffer[dev->rb.tail];
    dev->rb.tail = (dev->rb.tail + 1) % RING_BUFFER_SIZE;
    dev->rb.count--;

    return true; //If reading was successful
}

enum hrtimer_restart simtemp_timer_callback(struct hrtimer *timer)
{

    struct nxp_simtemp_dev *dev = container_of(timer, struct nxp_simtemp_dev, timer);
    struct simtemp_sample   sample;
    unsigned long flags;

    //Data Generation
    sample.timestamp_ns = ktime_get_ns();
    sample.temp_mC = 4000 + (jiffies % 5000);
    sample.flags = SAMPLE_AVAILABLE;

    //Ensuring atomicity
    spin_lock_irqsave(&dev->lock, flags);

    //Data logic simtemp_buffer_push is defined
    simtemp_buffer_push(dev, &sample);

    //Notifications

    wake_up_interruptible(&dev->wq);
    dev->updates_count++;

    spin_unlock_irqrestore(&dev->lock, flags);

    //Timer reassemble

    hrtimer_forward_now(timer,dev->period_ns);

    return HRTIMER_RESTART;
}

// ------------    Platform Driver     ------------------------------------
// //------------Final register of Driver------------------------//
//---------------- File Prototypes--------------------------------

//-----------Platform Driver: ----- Hardware --------------------
// //--------Structure for Device Tree Matching------------------//
static const struct of_device_id nxp_simtemp_dt_match[]=
{
    {   .compatible = "nxp,simtemp"},
    {   /*Sentinel*/}                   //Sentinel stops the search
};


//-----------Platform Driver: ----- Driver Starter --------------------
//Indicates to Platform Driver from kernel How to manipulates this data from Driver
static struct platform_driver nxp_simtemp_driver=

//extern struct platform_driver nxp_simtemp_driver=
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

// //---------File Operations Table: Functions map of driver-----------------

//**********  File Prototypes******************
// "struct file_operations" [kernel]: Data Type provides by kernel. Contains all the pointers to the function that the driver performed to interact with files system of Linux (Kernel)




//-----------------------------   Platform Driver: Interface Contract API .   nxp_simtemp_fops     --------------------------------------------------------------------
//Pointers table to functions
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
static int nxp_simtemp_open(struct inode *inode, struct file *file)         //Function performed once when User Space opens the file /dev/simtemp
{
    struct nxp_simtemp_dev *nxp_dev = container_of(file->private_data, struct nxp_simtemp_dev, mdev);

    file->private_data = nxp_dev; //

    return 0;
}

static int nxp_simtemp_release(struct inode *inode, struct file *file)  //Prototype of function performed when user space calls to close() or when the process end.
{

//Here memory is liberated if specific memory was assignated for this aperture process.

return 0;
}

static ssize_t nxp_simtemp_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)      //Prototype of function performed when User Space calls to read().
{
    //Block Logic
    //Ring Buffer
    //copy_to_user

    return 0; //Returns 0 bytes readed

}

static __poll_t nxp_simtemp_poll(struct file *file, struct poll_table_struct *wait)     //Prototype of function performed when User Space calls to poll(), select() or epoll().
{
    // poll_wait Logic
    // Return of flags 

    return 0; 


}






//------------------ Platform Device     Functions     -----------------------------------/
// ----------- Platform Device: Drive Functions (Lifecycle)----------------------------------------/

static int nxp_simtemp_probe(struct platform_device *pdev)
{
    printk(KERN_INFO "Debug 0 Initializes probe() function\n");
    struct nxp_simtemp_dev *nxp_dev; 
    int ret;

    struct device *dev = &pdev->dev; //se sustituye &pdev->dev por dev (Puntero Local)

    dev_info(dev,"Debug 1 Probar Inicio\n");

    //Memory Allocation: Allocates and clean memory for structure nxp_simtemp_dev.
    nxp_dev = devm_kzalloc(dev, sizeof(*nxp_dev), GFP_KERNEL);    //Allocate
    //nxp_dev = kzalloc(sizeof(*nxp_dev), GFP_KERNEL);    //Allocate
    if (!nxp_dev)
    {
        dev_err(dev, "Debug 2 Memory allocation failed\n");
        //kfree(nxp_dev);//Liberacion manual de memoria
        return -ENOMEM; //Without Memory
    }
    dev_info(dev,"Debug 3 Memoria allocated and valid\n");
    platform_set_drvdata(pdev, nxp_dev); //Guarda puntero

    dev_info(dev,"Debug 4 Driver Data Set\n");
    
    
    //Initializes primitives for spinlock and wait_queue.
    spin_lock_init(&nxp_dev->lock); //Initialize spinlock [Kernel Function]
    init_waitqueue_head(&nxp_dev->wq);  //Initialize waiting queue [Kernel Function]

    dev_info(dev,"Debug 5 Primitives intialized\n");


    //---------------hrtimer implementation---------------

    //Initializes Ring Buffer
    simtemp_buffer_init(&nxp_dev->rb); //Buffer initialized

    //Producer start
    //hrtimer_init() and hrtimer_start() are initialized
    simtemp_timer_setup(nxp_dev);



    //Device Tree parsing Configuration (By default)

    nxp_dev->sampling_ms = 100;
    nxp_dev->threshold_mC = 45000;


    //Register miscdevice (/dev/simtemp).
    //Register of Character Device

    nxp_dev->mdev.minor = MISC_DYNAMIC_MINOR; // Asks Linux for a lower available number
    nxp_dev->mdev.name = "simtemp"; //File Name in dev/simtemp
    nxp_dev->mdev.fops = &nxp_simtemp_fops; // Operations Table from nxp_simtemp_fops is assigned to Files System of Linux (/dev/simtemp)

    ret = misc_register(&nxp_dev->mdev); //Register the Device Driver and /dev/simtemp is created by kernel .

    if (ret)
    {
        dev_err(dev, "Debug 6. Error registrando miscdevice\n");
        //kfree(nxp_dev);//Liberacion manual de memoria
        return ret;
    }

    

    //Initialize the producer Timer
    //simtemp_timer_setup(nxp_dev); //to final

    dev_info(dev, "Debug 7\n");
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
        
        // 3. Desregistrar la Interfaz
        misc_deregister(&nxp_dev->mdev);
        // 2. Detener el Productor (¡CRÍTICO!)
        // Esta línea detiene el hrtimer inicializado en probe.
        hrtimer_cancel(&nxp_dev->timer); 
        
         

        dev_info(&pdev->dev,"NXP SimTemp device unregistered. \n");
    }
    
    //dev_info(&pdev->dev,"NXP SimTemp device unregistered. \n"); //[Kernel] Attachs device name with message log
    

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
    //for Clean Unload. Cleaning in inverse order.
    platform_device_unregister(simtemp_pdev);          //
    platform_driver_unregister(&nxp_simtemp_driver);   //
    printk(KERN_INFO "NXP SimTemp: Modulo unloades. Bye.\n");

}



//************* Macros (always placed al the end of the code for Linux )************************** */



module_init(simtemp_runtime_init); // [Kernel] Macro for indicate to kernel what funtion mus be called when the modulo is load nxp_simtemp_init
module_exit(simtemp_runtime_exit); // [Kernel] Macro for indicate to kernel what funtion mus be called when the modulo is load nxp_simtemp_init