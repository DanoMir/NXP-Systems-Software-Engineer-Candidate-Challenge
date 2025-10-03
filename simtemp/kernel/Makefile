# Makefile
# -----------------------------------------------------------
# Nombre del modulo (nombre que tendra el archivo .ko)
obj-m := nxp_simtemp.o

# Ruta al codigo fuente del Kernel
# Obtiene la ruta del codigo fuente de tu Kernel de Ubuntu
KERNEL_DIR := /lib/modules/$(shell uname -r)/build

# Directorio actual
PWD := $(shell pwd)

# Regla 'all':
# Invoca el sistema de build del Kernel (Kbuild)
all:
    make -C $(KERNEL_DIR) M=$(PWD) modules

# Regla 'clean':
# Elimina los archivos generados (.o, .ko, .mod.c, etc.)
clean:
    make -C $(KERNEL_DIR) M=$(PWD) clean

# Regla 'install' (Opcional, pero util para automatizar la instalacion)
install:
    make -C $(KERNEL_DIR) M=$(PWD) modules_install

.PHONY: all clean install