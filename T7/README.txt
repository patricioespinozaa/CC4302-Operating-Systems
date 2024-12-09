Este ejemplo es una adaptacion del tutorial incluido
(archivo "device drivers tutorial.pdf") y bajado de:
http://www.freesoftwaremagazine.com/articles/drivers_linux

---

Guia rapida:

*******************************************************************
Para insertar el modulo necesita que el .ko este en un disco local.
No puede cargar un modulo que vive en un disco ajeno a Linux.
*******************************************************************

Lo siguiente se debe realizar parados en
el directorio en donde se encuentra este README.txt

+ Compilacion (puede ser en modo usuario):
$ make
...
$ ls
... cena-filosofos.ko ...

+ Instalacion (en modo root)

# mknod /dev/cena-filosofos c 61 0
# chmod a+rw /dev/cena-filosofos
# insmod cena-filosofos.ko
# dmesg | tail
...
[...........] Inserting cena-filosofos module
#

+ Testing (en modo usuario preferentemente)

Ud. necesitara crear multiples shells independientes.  Luego
siga las instrucciones del enunciado de la tarea 7 de 2024-2

+ Desinstalar el modulo

# rmmod cena-filosofos.ko
#
