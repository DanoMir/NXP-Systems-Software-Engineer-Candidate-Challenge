/***************************************************************************
* Open Source License 2025 NXP Semiconductor Challenge Stage
****************************************************************************
* Title        : nxp_simptemp.c
*
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
***************************************************************************/
/* * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * Header Files *   *   *   *   *   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
//#include <linux/time.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel Miranda");
MODULE_DESCRIPTION("NXP SimpTemp");
MODULE_VERSION("1.0");

static int __int nxp_simptemp_init(void)
{
    prink(KERN_INFO "NXP SimTemp: Modulo cargado exitosamente. Entorno listo.\n");
    return 0;
}

static void __exit nxp_simtemp_exit(void)
{

    prink(KERN_INFO "NXP SimTemp: Modulo descargado. Adios.\n");

}

module_init(nxp_simptemp_init);
module_exit(nxp_simptemp_exit);