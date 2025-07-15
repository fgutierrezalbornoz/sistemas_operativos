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


/* Declaration of cena-filosofos.c functions */
int filosofos_open(struct inode *inode, struct file *filp);
int filosofos_release(struct inode *inode, struct file *filp);
ssize_t filosofos_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);
ssize_t filosofos_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos);
void filosofos_exit(void);
int filosofos_init(void);

/* Structure that declares the usual file */
/* access functions */
struct file_operations filosofos_fops = {
  read: filosofos_read,
  write: filosofos_write,
  open: filosofos_open,
  release: filosofos_release
};

/* Declaration of the init and exit functions */
module_init(filosofos_init);
module_exit(filosofos_exit);

/*** El driver para lecturas sincronas *************************************/

#define TRUE 1
#define FALSE 0

/* Global variables of the driver */

int filosofos_major = 61;     /* Major number */

/* Buffer to store data */
#define MAX_SIZE 8192

static char *filosofos_buffer;
static ssize_t curr_size;
static int filosofos[5];//0:pensando; 1:comiendo; 2:esperando

/* El mutex y la condicion para cena-filosofos */
static KMutex mutex;
static KCondition cond;

int filosofos_init(void) {
  int rc;

  /* Registering device */
  rc = register_chrdev(filosofos_major, "cena-filosofos", &filosofos_fops);
  if (rc < 0) {
    printk(
      "<1>cena-filosofos: cannot obtain major number %d\n", filosofos_major);
    return rc;
  }
  curr_size= 0;
  m_init(&mutex);
  c_init(&cond);
  for(int i=0; i<5;i++){
    filosofos[i]=0;
  }

  /* Allocating filosofos_buffer */
  filosofos_buffer = kmalloc(MAX_SIZE, GFP_KERNEL);
  if (filosofos_buffer==NULL) {
    filosofos_exit();
    return -ENOMEM;
  }
  memset(filosofos_buffer, 0, MAX_SIZE);

  printk("<1>Inserting cena-filosofos module\n");
  return 0;
}

void filosofos_exit(void) {
  /* Freeing the major number */
  unregister_chrdev(filosofos_major, "cena-filosofos");

  /* Freeing buffer cena-filosofos */
  if (filosofos_buffer) {
    kfree(filosofos_buffer);
  }

  printk("<1>Removing cena-filosofos module\n");
}

int filosofos_open(struct inode *inode, struct file *filp) {
  int rc= 0;
  m_lock(&mutex);
  goto epilog;

epilog:
  m_unlock(&mutex);
  return rc;
}

int filosofos_release(struct inode *inode, struct file *filp) {
    m_lock(&mutex);  // Bloquear el mutex para asegurar la sincronización
    if (filp->f_mode & FMODE_WRITE) {
        // Verificar si el archivo tiene un filósofo asociado
        if (filp->private_data != NULL) {
            int *filosofo_status = (int *)(filp->private_data);  // Acceder a private_data
            int id = *filosofo_status;  // Obtener el ID del filósofo

            // Cambiar estado a "pensando" si estaba esperando
            if (filosofos[id] == 2) {  // Si el filósofo estaba esperando
                printk("<1>Filósofo %d estaba esperando y ahora pasa a pensando\n", id);
                filosofos[id] = 0;  // Cambiar a "pensando"
                c_broadcast(&cond);  // Notificar a otros filósofos que el filósofo pasó a pensando
            }

            // Liberar la memoria asociada con private_data
            kfree(filosofo_status);
            filp->private_data = NULL;  // Limpiar el puntero a private_data
        }
    }
    m_unlock(&mutex);  // Desbloquear el mutex
    return 0;
}


ssize_t filosofos_read(struct file *filp, char *buf, size_t count, loff_t *f_pos) {
    ssize_t rc;
    char estado[128]; // Buffer temporal para construir el estado
    int len = 0;      // Longitud acumulada de la cadena
    m_lock(&mutex);   // Bloqueo para sincronización
    for (int i = 0; i < 5; i++) {
        estado[len++] = i + '0';
        estado[len++] = ':';
        estado[len++] = ' ';

        // Agregar el estado correspondiente
        if (filosofos[i] == 0) {
            strcpy(&estado[len], "pensando");
            len += 9; // Longitud de "pensando"
        } else if (filosofos[i] == 1) {
            strcpy(&estado[len], "comiendo");
            len += 8; // Longitud de "comiendo"
        } else if (filosofos[i] == 2) {
            strcpy(&estado[len], "esperando");
            len += 9; // Longitud de "esperando"
        }

        // Agregar nueva línea
        estado[len++] = '\n';
    }
    m_unlock(&mutex); // Desbloqueo para sincronización

    // Si el puntero de posición está más allá del final, no hay nada que leer
    if (*f_pos >= len) {
        return 0; // Fin de archivo
    }

    // Ajustar la cantidad a leer si excede el tamaño disponible
    if (count > len - *f_pos) {
        count = len - *f_pos;
    }

    // Copiar los datos al espacio del usuario
    if (copy_to_user(buf, estado + *f_pos, count)) {
        return -EFAULT; // Error al copiar
    }

    *f_pos += count; // Actualizar la posición de lectura
    rc = count;      // Número de bytes leídos
    return rc;
}

ssize_t filosofos_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos) {
    ssize_t rc;
    loff_t last;
    char command[32];  // Buffer para almacenar el comando recibido

    m_lock(&mutex);

    last = *f_pos + count;
    if (last > MAX_SIZE) {
        count -= last - MAX_SIZE;
    }

    if (copy_from_user(command, buf, count) != 0) {
        rc = -EFAULT;
        goto epilog;
    }

    command[count] = '\0';  // Asegurar que la cadena esté terminada en nulo

    // Cuando se ingresa "comer id"
    if (strncmp(command, "comer", 5) == 0) {
        int id = -1;
        int i = 6;

        // Extraer el número después de "comer"
        while (i < count && command[i] == ' ') {
            i++;
        }
        if (i < count && command[i] >= '0' && command[i] <= '9') {
            id = 0;
            while (i < count && command[i] >= '0' && command[i] <= '9') {
                id = id * 10 + (command[i] - '0');
                i++;
            }
        }

        if (id >= 0 && id < 5) {
            printk("<1>Filósofo %d está intentando comer\n", id);

            int izquierdo = (id + 4) % 5;
            int derecho = (id + 1) % 5;

            while (filosofos[izquierdo] != 0 || filosofos[derecho] != 0) {
                printk("<1>Filósofo %d está esperando porque los vecinos están comiendo o esperando\n", id);
                
                if (filp->private_data == NULL) { //necesario para cuando se termina una espera por interrupción (control-C)
                    int *filosofo_status = kmalloc(sizeof(int), GFP_KERNEL);
                    if (!filosofo_status) {
                        rc = -ENOMEM;
                        goto epilog;
                    }
                    *filosofo_status = id;
                    filp->private_data = filosofo_status;
                    filosofos[id] = 2; // Cambiar a "esperando"
                }

                if (c_wait(&cond, &mutex)) {
                    if (filosofos[id] == 2) {
                        printk("<1>Filósofo %d fue interrumpido y ahora pasa a pensando\n", id);
                        filosofos[id] = 0; // Cambiar a "pensando"
                    }
                    filp->private_data = NULL;
                    rc = -EINTR;
                    goto epilog;
                }
                //si sale del wait es porque estaba esperando primero en la cola y un vecino dejó de comer
                if (filosofos[izquierdo] != 1 && filosofos[derecho] != 1) { 
                    break; // Salir del ciclo si los vecinos no están comiendo
                }
            }
            filosofos[id] = 1; // Cambiar a "comiendo"
            printk("<1>Filósofo %d ahora está comiendo\n", id);
            c_broadcast(&cond);
        } else {
            printk("<1>Error: Formato de comando inválido o filósofo fuera de rango\n");
            rc = -EINVAL;
            goto epilog;
        }
    } 
    //cuando se ingresa "pensar id"
    else if (strncmp(command, "pensar", 6) == 0) {//lectura del input para obtener el id
        int id = -1;
        int i = 7;

        // Extraer el número después de "pensar"
        while (i < count && command[i] == ' ') {
            i++;
        }
        if (i < count && command[i] >= '0' && command[i] <= '9') {
            id = 0;
            while (i < count && command[i] >= '0' && command[i] <= '9') {
                id = id * 10 + (command[i] - '0');
                i++;
            }
        }

        if (id >= 0 && id < 5) {
            printk("<1>Filósofo %d recibió el comando pensar\n", id);

            if (filosofos[id] == 1 || filosofos[id] == 2) {//si no está pensando
                filosofos[id] = 0; // Cambiar a "pensando"
                filp->private_data  =NULL;
                printk("<1>Filósofo %d ahora está pensando\n", id);
                c_broadcast(&cond); // Notificar a otros procesos
            } else {
                printk("<1>Filósofo %d ya estaba pensando\n", id);
            }
        } else {
            printk("<1>Error: Formato de comando inválido o filósofo fuera de rango\n");
            rc = -EINVAL;
            goto epilog;
        }
    } else {
        printk("<1>Error: Comando no reconocido\n");
        rc = -EINVAL;
        goto epilog;
    }

    *f_pos += count;
    rc = count;

epilog:
    m_unlock(&mutex);
    return rc;
}

