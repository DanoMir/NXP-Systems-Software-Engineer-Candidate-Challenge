/***************************************************************************
        Open Source License 2025 NXP Semiconductor Challenge Stage
****************************************************************************
* Title        : nxp_simtemp.h
* Description  : Header File of nxp_simtemp.c
*
* Environment  : C Language
*
* Responsible  : Daniel R Miranda [danielrmirandacortes@gmail.com]
*
* Guidelines   : Linux Kernel Coding Style
*
*
* 
*
****************************************************************************
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
* * * * * * * * * * * Header Files *   *   *   *   *   *   *   *   *   *   *
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef _NXP_SIMTEMP_H_
#define _NXP_SIMTEMP_H_

#include <linux/module.h>           //Definitions, macros and structures for LKM (Loadable Kernel Module)
#include <linux/kernel.h>           //Macros and basic functions for interact with the Kernel
#include <linux/init.h>             //Macros for (LKMs) Linux Kernel Modules in its LifeCycle (Startup/Exit)
#include <linux/fs.h>               //File Operator for VFS Virtual File System
#include <linux/platform_device.h>  //Driver Platform Development: Structs for device and driver, state data and register of driver
#include <linux/miscdevice.h>       //Miscellaneous Character Devices in /dev/simtemp interface.
#include <linux/spinlock.h>         //Implementation, control and sincronization of spinlock in concurrency.                    
#include <linux/wait.h>             //Notification of wait queue: blocking, sleeping and wake-up
#include <linux/hrtimer.h>          //Timer for simulation of High Resolution
#include <linux/slab.h>             //Slab Allocator Functions and macros kzalloc/kfree    
#include <linux/uaccess.h>          //User Access. Functions and macros to transfer data between Kernel and User
#include <linux/time.h>             //Timer for simulation
#include <linux/of.h>               //Device Tree    
#include <linux/types.h>            //Defines Standar Data Types and Size for portabilitu between different architectures.    
#include <linux/mod_devicetable.h>  //Match Tables Compatibility with devices  
#include <linux/device.h>           //Generic Struct and Auxiliary Functions for Devices Subsystem amd Sysfs
#include <linux/poll.h>             //Polling interface for handling of I/O based in events.
#include <linux/random.h>           //Generation of random numbers RNG
#include <linux/time.h>             //Time measurement and timestamps

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel Miranda");
MODULE_DESCRIPTION("NXP SimpTemp");
MODULE_VERSION("1.0");

#define RING_BUFFER_SIZE    32          //Size of buffer
#define SAMPLE_AVAILABLE    (1<<0)      //Bit 0 for __u32 flags in struct simtemp_sample
#define TRESHOLD_CROSSED    (1<<1)      //Bit 1 for __u32 flags in struct simtemp_sample


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
    struct miscdevice           mdev;       //Structure of Interface [Kernel]: Miscellaneous Device to register the Character Device /dev/simtemp
    wait_queue_head_t           wq;         //Structure of sincronization [Kernel]: Used by "poll" function
    spinlock_t                  lock;       //Structure of concurrency [Kernel]: Protection of storage (shared resources) of interrupts and simultaneous access 

    struct hrtimer              timer;      //Structure of timer [Kernel]: Data Producer to initializes the High Resolution
    ktime_t                     period_ns;  //Structure of Time Type [Kernel]: Data Times with nanosecond precision

    struct simtemp_ring_buffer  rb;         //Structure of storage [Logic]: Circular buffer (Data storage)

    //Configuration of variables for sysfs to export information from Kernel Subsystems to space user
    s32                         threshold_mC;   //Temperature
    s32                         sampling_ms;    //Period

    //Configuration of variables for statistics
    u32                         alerts_count;   //Variable for Diagnostic functions as Logic Counter (stats_show) that indicates how many data crossed a critical treshold
    u32                         updates_count;  //Variable for Diagnostic functions as Logic Counter that indicates how many data was produced.

};

//--------------------  Function Prototypes  ------------------------------------------
//-----Function Prototypes: Driver Functions: Define the Life Cycle abd the Interface of Platform Driver------------------------

static struct platform_device *simtemp_pdev;

//---File Operations/Input-Output Functions----
static int nxp_simtemp_open(struct inode *inode, struct file *file);                                //Function Prototype performed once when user space opens the file
static int nxp_simtemp_release(struct inode *inode, struct file *file);                             //Function Prototype performed when user space calls to close() or when the process end.
static ssize_t nxp_simtemp_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);   //Function Prototype performed when User Space calls to read().  
static __poll_t nxp_simtemp_poll(struct file *file, struct poll_table_struct *wait);                //Function Prototype performed when User Space calls to poll(), select() or epoll().
//--- Driver Life Cycle Functions---
static void nxp_simtemp_remove(struct platform_device *pdev);                                       //Function Prototype [Kernel] structure from "platform_device.h"
static int nxp_simtemp_probe(struct platform_device *pdev);  

//-----Function Prototypes: Sysfs Control Interface Functions: Control Panel of Driver, interaction with configuration and diagnosis. 
//--- Reading Functions: -show  ---
static ssize_t sampling_ms_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t threshold_mC_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t stats_show(struct device *dev, struct device_attribute *attr, char *buf);
//--- Writing Functions: _store  ---
static ssize_t sampling_ms_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t threshold_mC_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t clear_alert_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);

//-----Function Prototypes: Module Lifecycle Functions: Entry and exit points for load and unload of Driver.
static int __init simtemp_runtime_init(void);
static void __exit simtemp_runtime_exit(void);

//----- Function Prototypes: Producer Control (Timer and Event Logic): Init, Mantain and operate the sampling ------------------
enum hrtimer_restart simtemp_timer_callback(struct hrtimer *timer);
static void simtemp_timer_setup(struct nxp_simtemp_dev *dev); //Este prototipo se declaro despues de la declaracion de la estructura.

//----- Function Prototypes: Ring Buffer functions (store management): Manage the Data structure used for the communication between producer and consumer.
static bool simtemp_buffer_is_empty(struct nxp_simtemp_dev *dev);
static void simtemp_buffer_push(struct nxp_simtemp_dev *dev, const struct simtemp_sample *sample);
static bool simtemp_buffer_pop(struct nxp_simtemp_dev *dev, struct simtemp_sample *sample);
static void simtemp_buffer_init(struct simtemp_ring_buffer *rb);


#endif // End of _NXP_SIMTEMP_H_