# Makefile

/* This is a text file that contains the instructions and necessary rules to 
automatize the compilation and management in software projects.

This file indicates to "make" which source files must be compilated and which order.*/
# ------------------------------------------------------------------------------------------------

# Module name for .ko file
obj-m := nxp_simtemp.o


# Root to source code of kernel

# Obtains the root to source code of Ubuntu Kernel
KERNEL_DIR := /lib/modules/$(shell uname -r)/build

# Directorio actual
PWD := $(shell pwd)



# "all" invokes the build of kernel
all:
    make -C $(KERNEL_DIR) M=$(PWD) modules


#"clean" eliminate the unwanted files generated during the compilation.
clean:
    make -C $(KERNEL_DIR) M=$(PWD) clean


# "install" automates all the installation durinf the compilation
install:
    make -C $(KERNEL_DIR) M=$(PWD) modules_install

.PHONY: all clean install