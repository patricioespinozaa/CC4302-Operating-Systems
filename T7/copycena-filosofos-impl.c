/* Necessary includes for device drivers */
#include <linux/init.h>
/* #include <linux/config.h> */
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/fs.h> /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/types.h> /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h> /* O_ACCMODE */
#include <linux/uaccess.h> /* copy_from/to_user */

#include "kmutex.h"

MODULE_LICENSE("Dual BSD/GPL");

#define DEVICE_NAME "cena-filosofos"
#define NUM_FILOSOFOS 5

// Estados de los filósofos
enum estado { PENSANDO, ESPERANDO, COMIENDO };

// Estructura principal
struct cena_filosofos {
    enum estado estados[NUM_FILOSOFOS];
    kmutex_t mutex;
    kcond_t condiciones[NUM_FILOSOFOS];
    int prioridad[NUM_FILOSOFOS];
} cena;

// Prototipos
static ssize_t cena_write(struct file *file, const char __user *buf, size_t count, loff_t *pos);
static ssize_t cena_read(struct file *file, char __user *buf, size_t count, loff_t *pos);

// Operaciones del dispositivo
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .write = cena_write,
    .read = cena_read,
};

// Funciones auxiliares
void evaluar(int n) {
    int izq = (n + NUM_FILOSOFOS - 1) % NUM_FILOSOFOS;
    int der = (n + 1) % NUM_FILOSOFOS;
    if (cena.estados[n] == ESPERANDO &&
        cena.estados[izq] != COMIENDO &&
        cena.estados[der] != COMIENDO) {
        cena.estados[n] = COMIENDO;
        kcond_signal(&cena.condiciones[n]);
    }
}

void comer(int n) {
    kmutex_lock(&cena.mutex);
    cena.estados[n] = ESPERANDO;
    evaluar(n);
    while (cena.estados[n] != COMIENDO)
        kcond_wait(&cena.condiciones[n], &cena.mutex);
    kmutex_unlock(&cena.mutex);
}

void pensar(int n) {
    kmutex_lock(&cena.mutex);
    cena.estados[n] = PENSANDO;
    evaluar((n + NUM_FILOSOFOS - 1) % NUM_FILOSOFOS);
    evaluar((n + 1) % NUM_FILOSOFOS);
    kmutex_unlock(&cena.mutex);
}

// Funciones del dispositivo
static ssize_t cena_write(struct file *file, const char __user *buf, size_t count, loff_t *pos) {
    char comando[16];
    int n;

    if (count > 15)
        return -EINVAL;
    if (copy_from_user(comando, buf, count))
        return -EFAULT;

    comando[count] = '\0';
    if (sscanf(comando, "comer %d", &n) == 1) {
        comer(n);
    } else if (sscanf(comando, "pensar %d", &n) == 1) {
        pensar(n);
    } else {
        return -EINVAL;
    }

    return count;
}

static ssize_t cena_read(struct file *file, char __user *buf, size_t count, loff_t *pos) {
    char estado_str[128];
    int len = 0, i;

    kmutex_lock(&cena.mutex);
    for (i = 0; i < NUM_FILOSOFOS; i++) {
        len += snprintf(estado_str + len, sizeof(estado_str) - len,
                        "%d %s\n", i,
                        cena.estados[i] == PENSANDO ? "pensando" :
                        cena.estados[i] == ESPERANDO ? "esperando" : "comiendo");
    }
    kmutex_unlock(&cena.mutex);

    if (*pos >= len)
        return 0;

    if (count > len - *pos)
        count = len - *pos;

    if (copy_to_user(buf, estado_str + *pos, count))
        return -EFAULT;

    *pos += count;
    return count;
}

// Inicialización y limpieza
static int __init cena_init(void) {
    int i;

    if (register_chrdev(61, DEVICE_NAME, &fops)) {
        printk(KERN_ALERT "Error al registrar el dispositivo\n");
        return -EBUSY;
    }

    kmutex_init(&cena.mutex);
    for (i = 0; i < NUM_FILOSOFOS; i++) {
        cena.estados[i] = PENSANDO;
        kcond_init(&cena.condiciones[i]);
        cena.prioridad[i] = 0;
    }

    printk(KERN_INFO "Dispositivo /dev/cena-filosofos registrado\n");
    return 0;
}

static void __exit cena_exit(void) {
    unregister_chrdev(61, DEVICE_NAME);
    printk(KERN_INFO "Dispositivo /dev/cena-filosofos eliminado\n");
}

module_init(cena_init);
module_exit(cena_exit);