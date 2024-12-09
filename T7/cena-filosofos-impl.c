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

// Funcioness
int cena_filosofos_open(struct inode *inode, struct file *filp);
int cena_filosofos_release(struct inode *inode, struct file *filp);
ssize_t cena_filosofos_read(struct file *filp, char __user *buff, size_t len, loff_t *off);
ssize_t cena_filosofos_write(struct file *filp, const char __user *buff, size_t len, loff_t *off);

int cena_filosofos_init(void);
void cena_filosofos_exit(void);

#define NUM_FILOSOFOS 5

typedef enum { PENSANDO, ESPERANDO, COMIENDO } estado_filosofo;

typedef struct {
    estado_filosofo estado;
    int id;
    KCondition cond;
    unsigned long tiempo_espera; // un filósofo no puede comer si un filósofo adyacente espera y llegó antes.
} filosofo;

static filosofo filosofos[NUM_FILOSOFOS];
static struct kmutex mesa_mutex;
unsigned long contador_tiempo = 0;

// Operaciones del dispositivo
struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = cena_filosofos_open,
    .release = cena_filosofos_release,
    .write = cena_filosofos_write,
    .read = cena_filosofos_read,
};

module_init(cena_filosofos_init);
module_exit(cena_filosofos_exit);

// Funciones auxiliares
void intentar_comer(int id) {
    int izq = (id + NUM_FILOSOFOS - 1) % NUM_FILOSOFOS;
    int der = (id + 1) % NUM_FILOSOFOS;

    if (filosofos[id].estado == ESPERANDO &&
        filosofos[izq].estado != COMIENDO && filosofos[der].estado != COMIENDO &&
        (filosofos[izq].estado != ESPERANDO || filosofos[id].tiempo_espera < filosofos[izq].tiempo_espera) &&
        (filosofos[der].estado != ESPERANDO || filosofos[id].tiempo_espera < filosofos[der].tiempo_espera)) {


        filosofos[id].estado = COMIENDO;
        printk(KERN_INFO "Filósofo %d comienza a comer\n", id);
        c_signal(&filosofos[id].cond); 
    }
}

int tomar_palillos(int id) {
    m_lock(&mesa_mutex);

    filosofos[id].tiempo_espera = contador_tiempo++;
    filosofos[id].estado = ESPERANDO;
    intentar_comer(id); // Intentar comer al cambiar a ESPERANDO

    while (filosofos[id].estado != COMIENDO) {
        if (c_wait(&filosofos[id].cond, &mesa_mutex) != 0) {
            // La espera fue interrumpida por una señal
            printk(KERN_INFO "Filósofo %d: espera interrumpida por una señal, volviendo a pensar\n", id);
            filosofos[id].estado = PENSANDO; // Restaurar estado
            m_unlock(&mesa_mutex);
            return -EINTR; // Notificar que la operación fue interrumpida
        }
    }

    m_unlock(&mesa_mutex);
    return 0; // Éxito
}

void dejar_palillos(int id) {

    m_lock(&mesa_mutex);

    int izq = (id + NUM_FILOSOFOS - 1) % NUM_FILOSOFOS;
    int der = (id + 1) % NUM_FILOSOFOS;
    
    filosofos[id].estado = PENSANDO;
    printk(KERN_INFO "Filósofo %d termina de comer y empieza a pensar\n", id);

    // Intentar despertar a los vecinos
    intentar_comer(izq);
    intentar_comer(der);

    m_unlock(&mesa_mutex);
}

// Funciones del módulo
int cena_filosofos_open(struct inode *inode, struct file *filp) {
    printk(KERN_INFO "Dispositivo cena_filosofos abierto\n");
    return 0;
}

int cena_filosofos_release(struct inode *inode, struct file *filp) {
    printk(KERN_INFO "Dispositivo cena_filosofos cerrado\n");
    return 0;
}

ssize_t cena_filosofos_read(struct file *filp, char __user *buff, size_t len, loff_t *off) {
    char estado[128];
    int offset = 0;
    int i;

    if (*off > 0) 
        return 0;

    m_lock(&mesa_mutex);
    for (i = 0; i < NUM_FILOSOFOS; i++) {
        offset += snprintf(estado + offset, sizeof(estado) - offset, "%d %s\n", i,
                           filosofos[i].estado == PENSANDO ? "pensando" :
                           filosofos[i].estado == ESPERANDO ? "esperando" : "comiendo");
    }
    m_unlock(&mesa_mutex);

    if (copy_to_user(buff, estado, offset)) {
        return -EFAULT;
    }

    *off += offset;
    return offset;
}

ssize_t cena_filosofos_write(struct file *filp, const char __user *buff, size_t len, loff_t *off) {
    char buffer[64];
    char accion[16];
    int id;

    if (len >= sizeof(buffer))
        return -EINVAL;

    if (copy_from_user(buffer, buff, len))
        return -EFAULT;

    buffer[len] = '\0';

    if (sscanf(buffer, "%15s %d", accion, &id) != 2 || id < 0 || id >= NUM_FILOSOFOS)
        return -EINVAL;

    if (strcmp(accion, "comer") == 0) {
        int result = tomar_palillos(id);
        if (result == -EINTR) {
            printk(KERN_INFO "Operación interrumpida por señal\n");
            return result; 
        }
    } else if (strcmp(accion, "pensar") == 0) {
        dejar_palillos(id);
    } else {
        return -EINVAL; 
    }

    return len;
}


int cena_filosofos_init(void) {
    int i;

    m_init(&mesa_mutex);

    for (i = 0; i < NUM_FILOSOFOS; i++) {
        filosofos[i].id = i;
        filosofos[i].estado = PENSANDO;
        c_init(&filosofos[i].cond);
    }

    if (register_chrdev(61, "cena_filosofos", &fops)) {
        printk(KERN_ALERT "Error al registrar el dispositivo\n");
        return -EIO;
    }

    printk(KERN_INFO "Driver de cena de filósofos cargado\n");
    return 0;
}

void cena_filosofos_exit(void) {
    unregister_chrdev(61, "cena_filosofos");
    printk(KERN_INFO "Driver de cena de filósofos descargado\n");
}
