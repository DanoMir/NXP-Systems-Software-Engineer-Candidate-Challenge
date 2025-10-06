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

#include <linux/fs.h>            //File operations
#include <linux/miscdevice.h>    //Misdevice
#include <linux/spinlock.h>      //Sincronization of spinlock                     
#include <linux/wait.h>          //Notification of wait queue
#include <linux/hrtimer.h>       //Timer for simulation
#include <linux/slab.h>          //kzalloc/kfree    
#include <linux/uaccess.h>       //Copy to user 

#include <linux/time.h>          //Timer for simulation
#include <linux/of.h>            //Device Tree        

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel Miranda");
MODULE_DESCRIPTION("NXP SimpTemp");
MODULE_VERSION("1.0");

#define RING_BUFFER_SIZE    32          //Size of buffer
#define SAMPLE_AVAILABLE    (1<<0)
#define TRESHOLD_CROSSED    (1<<1)




// /--------------File Operations Prototypes for "read" and "poll"---------------

// static int nxp_simtemp_open(struct inode *inode, struct file *file);         //Prototype of function performed once when user space opens the file
// static int nxp_simtemp_release(struct inode *inode, struct file *file);  //Prototype of function performed when user space calls to close() or when the process end.
// static ssize_t nxp_simtemp_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);      //Prototype of function performed when User Space calls to read().
// static __poll_t nxp_simtemp_poll(struct file *file, struct poll_table_struct *wait);     //Prototype of function performed when User Space calls to poll(), select() or epoll().

static int nxp_simtemp_remove(struct platform_device *pdev);
static int nxp_simtemp_probe(struct platform_device *pdev):

//-----------------  Transferred Data  --------------------//
struct simtemp_sample       //Structure Data Contract [Logic]: Defines transferred data between kernel and user space. Inside the kernel and performed by user space
{
    u64 timestamp_ns;       //Sample in nanoseconds: Calculates the accuracy of sampling and jitter from User Space
    s32 temp_mC;            //Sample in millidegrees: Represents values in float point without float point arithmetic
    u32 flags;              //Sample status: Notificates to applications if NXP_SAMPLE_AVAILABLE or NXP_THRESHOLD_CROSSED   

}
__attribute__((packed));    //Avoid memory padding. Recomended by NXP Challenge to keep the same size for Kernel/User

//----------------    Ring Buffer  ------------------------------------//
struct simtemp_ring_buffer  //Structure for Storage [Logic]: Defines the architecture that stores and manipulates the data of "simtemp_sample[]""
{
    struct simtemp_sample buffer[RING_BUFFER_SIZE]; //Structure Data Contract [Logic]:
    size_t head;                                    //Writing Index: Used by producer "hrtimer" that puts the next sample 
    size_t tail;                                    //Reading Index: Used by consumer "read" of User Space that takes the next sample.
    u32 count;                                      //Counter to determine the validate samples in the buffer: ((count == 0) or (count == RING_BUFFER_SIZE)). 

};


//-------------    Driver    ----------------------------------------
struct nxp_simtemp_dev      //Global Structure [Logic]: Contains the configuration values, functionalities and interfaces of Driver reside
{    
    struct miscdevice           mdev;       //Structure of Interface [Kernel]: provided by kernel to register the character device
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

//Funbction Prototypes
static bool simtemp_buffer_is_empty(struct nxp_simtemp_dev *dev);
static void simtemp_buffer_push(struct nxp_simtemp_dev *dev, const struct simtemp_sample *sample);
static bool simtemp_buffer_pop(struct nxp_simtemp_dev *dev, struct simtemp_sample *sample);

//Ring Buffer State (Verifies if ring buffer is empty, useful for read() and poll())

static bool simtemp_buffer_is_empty(struct nxp_simtemp_dev *dev)
{
    return (dev->rb.count == 0);

}

//Push Function for write a new sample called by hrtimer[kernel] (Producer)
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

// Pop: Read and remove the oldest sample (Called by read() function)
static bool simtemp_buffer_pop(struct nxp_simtemp_dev *dev, struct simtemp_sample *sample)
{
    if(simtemp_buffer_is_empty(dev))
    {
        return false;
    }

    //Copy the data and the queue moves forward
    *sample = dev->rb.buffer[dev->rb.tail];
    dev->rb.tail = (dev->rb.tail + 1) % RING_BUFFER_SIZE;
    dev->rb.count--;
}

// //---------File Operations Table: Functions map of driver-----------------

// //struct file_operations [kernel]: Data Type provides by kernel. Contains all the pointers to the function that the driver performed to interact with files system of Linux (Kernel)
// //nxp_simtemp_fops [logic]: Variable provides by implemented logic only visible inside this file (static). 

// static const struct file_operations nxp_simtemp_fops =
// {

//     .owner      =THIS_MODULE,           //Indicates to Kernel that the set of functions belongs to actual module (user module)
//     .open       =nxp_simtemp_open,      //Pointer to the function performed when User Space calls to open("/dev/simtemp", ...)
//     .release    =nxp_simtemp_release,   //Pointer to the function performed when User Space calls to close(fd). 
//     .read       =nxp_simtemp_read,      //Pointer to the function performed when User Space calls to read(fd, ...)
//     .poll       =nxp_simtemp_poll,      //Pointer to the function performed when User Space calls to poll() or epoll().

// };




// //--------Structure for Device Tree Matching------------------//
// static const struct of_device_id nxp_simtemp_dt_match[]=
// {
//     {   .compatible = "nxp,simtemp"},
//     {   /*Sentinel*/}
// }

// static int nxp_simtemp_open(struct inode *inode, struct file *file);         //Prototype of function performed once when user space opens the file
// {
//     struct nxp_simtemp_dev *nxp_dev = container_of(file->private_data, struct nxp_simtemp_dev, mdev);

//     file->private_data = nxp_dev; //

//     return 0;
// }

// static int nxp_simtemp_release(struct inode *inode, struct file *file);  //Prototype of function performed when user space calls to close() or when the process end.
// {
// //Aqui se liberara memoria si se hubiera asignado memoria especifica para este proceso de apertura

// return 0;
// }

// static ssize_t nxp_simtemp_read(static ssize_t nxp_simtemp_read(struct file *file, char __user *buf, size_t count, loff_t *ppos));      //Prototype of function performed when User Space calls to read().
// {
//     //Block Logic
//     //Ring Buffer
//     //copy_to_user

//     return 0; //Returns 0 bytes readed

// }

// static __poll_t nxp_simtemp_poll(struct file *file, struct poll_table_struct *wait);     //Prototype of function performed when User Space calls to poll(), select() or epoll().
// {
//     // poll_wait Logic
//     // Return of flags 

//     return 0; 


// }
//----------------Platform Driver Section------------------------------ */
//Simulation by Device Tree, platform is the HW simulated towards *pdev
static int nxp_simtemp_probe(struct platform_device *pdev)
{
    struct nxp_simtemp_dev *nxp_dev; 
    int ret; //[kernel] Error code

//Memory for nxp_simtemp_dev.
//Memory Allocation: Allocates and clean memory for structure
//Kzalloc[kernel] defines the memory

nxp_dev = devm_kzalloc(&pdev->dev, sizeof(*nxp_dev), GFP_KERNEL);    //Allocate memory for *nxp_dev
if (!nxp_dev)
{
    return -ENOMEM; //[Kernel] Without Memory
}

platform_set_drvdata(pdev, nxp_dev); // [Kernel] Saves the pointer "nxp_dev" to structure "platform device" [kernel]

//Inicialice el spinlock y la wait_queue.
//Primitives for spinlock and wait_queue

spin_lock_init(&nxp_dev->lock); //[Kernel] Initialize spinlock 
init_waitqueue_head(&nxp_dev->wq);  //[Kernel] Initialize waiting queue 

//Device Tree parsing Configuration (By default)
//This values will be overwritten by Device Tree Parsing Logic through (sampling-ms) to obtain specific values of HW

nxp_dev->sampling_ms = 100;
nxp_dev->threshold_mC = 45000;

// Initialization and startup of producer (Timer and Buffer)

// NOTA: Aqui iría la lógica para leer y sobrescribir estos valores con el Device Tree
//Initializes Ring Buffer Logic (*head, *tail and *count from Ring Buffer initializes to 0)
simtemp_buffer_init(&nxp_dev->rb); // [Logic] Ensure a clean initial state before start the timer

//Initializes Timer Logic from Producer( hrtimer and callback initializes to 0) 
simtemp_timer_setup(nxp_dev); // [Logic] Activates the data activation flow


//
//Registre la miscdevice (/dev/simtemp) Files System.
//Register of Character Device

nxp_dev->mdev.minor = MISC_DYNAMIC_MINOR; // [Kernel] Device Number. Asks to Linux for a lower available number of available device
nxp_dev->mdev.name = "simtemp"; //[Logic] File Name created in miscdevice (/dev/simtemp) Files System
nxp_dev->mdev.fops = &nxp_simtemp_fops; // [Logic] Structure Operations Table is assigned to "open()","read()" and "poll()" functions

ret = misc_register(&nxp_dev->mdev); //[Kernel] Final Register for miscdevice (/dev/simtemp) Files System

if (ret) //If register fails 
{
    dev_err(&pdev->dev, "Error registrando miscdevice\n"); // [Kernel] Critical cleaning stopping hrtimer() before leaving the function
    return ret;
}

//Initialize the producer Timer
// simtemp_timer_setup(nxp_dev); //SE LLAMA AL FINAL

//Successful Message after device registered

dev_info(&pdev->dev, "NXP SimTemp device registered at /dev/%s\n", nxp_dev->mdev.name ); //[kernel] Prints message in kernel log (dmesg)
return 0;

}



// }
//----------------Release function (reverse of probe)---------------------------
static int nxp_simtemp_remove(struct platform_device *pdev)
{
    struct nxp_simtemp_dev *nxp_dev = platform_get_drvdata(pdev); //

    // CRÍTICO para Clean Unload: Detener el timer y desregistrar todo
    hrtimer_cancel(&nxp_dev->timer); //[Kernel] Stops the timer if miscdevice fails to prevents an Kernel Panic
    misc_deregister(&nxp_dev->mdev); // [Kernel] Delete Character Device of system files durin the clean remove
    // La memoria se libera automaticamente gracias a devm_kzalloc

    //Memory liberated automated by devm_kzalloc

    dev_info(&pdev->dev,"NXP SimTemp device unregistered. \n"); //[Kernel] Attachs device name with message log
    return 0;

}



// //------------Final register of Driver------------------------//

// static struct platform_driver nxp_simtemp_driver=
// {
//     .probe              = nxp_simtemp_probe,        //Pointer to function that is performed by kernel when it finds a compatible device
//     .remove             = nxp_simtemp_remove,       //Pointer to function that is performed by kernel when the module is unloaded
//     .driver             =
//     {
//         .name           = "nxp_simtemp",            //Name of driver for file system of Kernel
//         .owner          = THIS_MODULE,
//         .of_match_table = nxp_simtemp_dt_match,     //Search in the Device Tree devices with the string (nxp, simtemp) if it is found, "probe" is activated
//     },
// };












static void __exit nxp_simptemp_exit(void)
{

    printk(KERN_INFO "NXP SimTemp: Modulo descargado. Adios.\n");

}

static int __init nxp_simptemp_init(void)
{
    printk(KERN_INFO "NXP SimTemp: Modulo cargado exitosamente. Entorno listo.\n");
    return 0;
}

//----------  Macros for developing modules in the Kernel---------------------
//Always are located at the end of the code
module_init(nxp_simptemp_init); // Macro for indicate to kernel what funtion mus be called when the modulo is load nxp_simtemp_init
//module_platform_driver(nxp_simtemp_driver);       //Macro for driver that evolves the platform driver with probe/evolve
module_exit(nxp_simptemp_exit); // Macro for indicate to kernel what funtion must be called to unload the module nxp_simtemp_exit