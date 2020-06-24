#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/net.h>
#include <net/sock.h>
#include <asm/processor.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/mm.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <linux/version.h>

#ifndef VM_RESERVED
#define VM_RESERVED   (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#define DEFAULT_PORT 2325
#define BUF_SIZE 512
#define MAP_SIZE (PAGE_SIZE * 1)

// custom commands
#define SLAVE_PD 0x12345676
#define SLAVE_CREATESOCK 0x12345677
#define SLAVE_MMAP 0x12345678
#define SLAVE_EXIT 0x12345679

typedef struct socket *ksocket_t;

// exported kscoket functions
extern int kclose(ksocket_t socket);
extern int kconnect(ksocket_t socket, struct sockaddr *address, int address_len);
extern ssize_t krecv(ksocket_t socket, void *buffer, size_t length, int flags);
extern ksocket_t ksocket(int domain, int type, int protocol);

extern unsigned int inet_addr(char* ip);
extern char *inet_ntoa(struct in_addr *in);

// init and exit module
static int __init slave_init(void);
static void __exit slave_exit(void);

// methods for file operations
int slave_close(struct inode *inode, struct file *filp);
int slave_open(struct inode *inode, struct file *filp);
static long slave_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
ssize_t receive_f(struct file *filp, char *buffer, size_t count, loff_t *ppos);

// for memory mapping
static int memory_mapping(struct file *filp, struct vm_area_struct *vma);
void mmap_open(struct vm_area_struct *vma) {}
void mmap_close(struct vm_area_struct *vma) {}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,11,0)
static int mmap_fault(struct vm_area_struct *vma, struct vm_fault *vmf) {
    vmf->page = virt_to_page(vma->vm_private_data);
    get_page(vmf->page);
    return 0;
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5,3,0)
static vm_fault_t mmap_fault(struct vm_fault *vmf) {
    vmf->page = virt_to_page(vmf->vma->vm_private_data);
    get_page(vmf->page);
    return 0;
}
#else
static int mmap_fault(struct vm_fault *vmf) {
    vmf->page = virt_to_page(vmf->vma->vm_private_data);
    get_page(vmf->page);
    return 0;
}
#endif

struct dentry *file1; // debug file

static ksocket_t sockfd_cli; // socket for slave device to connect to master device
static struct sockaddr_in addr_svr; //address of the master server
static int addr_len;
static mm_segment_t old_fs;

// file operations
static struct file_operations slave_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = slave_ioctl,
    .open = slave_open,
    .read = receive_f, // method for reading the device
    .release = slave_close,
    .mmap = memory_mapping
};

//misc device info
static struct miscdevice slave_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "slave_device",
    .fops = &slave_fops
};

// for mmap
struct vm_operations_struct mmap_vm_ops = {
    .open = mmap_open,
    .close = mmap_close,
    .fault = mmap_fault
};

static int __init slave_init(void) {
    int ret;
    // mounted on /sys/kernel/debug by default
    file1 = debugfs_create_file("slave_debug", 0644, NULL, NULL, &slave_fops);

    // register the device
    if ((ret = misc_register(&slave_dev)) < 0) {
        printk(KERN_ERR "[slave] misc register failed\n");
        return ret;
    }

    printk(KERN_INFO "[slave] registered\n");

    return 0;
}

static void __exit slave_exit(void) {
    misc_deregister(&slave_dev);

    printk(KERN_INFO "[slave] exited\n");
    debugfs_remove(file1);
}


static long slave_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    long ret = 0;
    char *tmp, ip[20];
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    switch (cmd) {
        // create socket and connect to master device
        case SLAVE_CREATESOCK:
            // obtain ip of master device
            if (copy_from_user(ip, (char*)arg, sizeof(ip)))
                return -ENOMEM;

            memset(&addr_svr, 0, sizeof(addr_svr));
            addr_svr.sin_family = AF_INET;
            addr_svr.sin_addr.s_addr = inet_addr(ip);
            addr_svr.sin_port = htons(DEFAULT_PORT);
            addr_len = sizeof(struct sockaddr_in);

            sockfd_cli = ksocket(AF_INET, SOCK_STREAM, 0);
            printk("[slave] socket is created: 0x%p\n", sockfd_cli);
            if (sockfd_cli == NULL) {
                printk("[slave] socket failed\n");
                return -1;
            }
            if (kconnect(sockfd_cli, (struct sockaddr*)&addr_svr, addr_len) < 0) {
                printk("[slave] connect failed\n");
                return -1;
            }
            tmp = inet_ntoa(&addr_svr.sin_addr);
            printk("[slave] connected to %s:%d\n", tmp, ntohs(addr_svr.sin_port));
            kfree(tmp);
            ret = 0;
            break;
        case SLAVE_MMAP:
            ret = krecv(sockfd_cli, filp->private_data, arg, 0);
            break;
        case SLAVE_EXIT:
            if (kclose(sockfd_cli) == -1) {
                printk("[slave] kclose cli error\n");
                return -1;
            }
            ret = 0;
            break;
        case SLAVE_PD:
            pgd = pgd_offset(current->mm, arg);
            p4d = p4d_offset(pgd, arg);
            pud = pud_offset(p4d, arg);
            pmd = pmd_offset(pud, arg);
            pte = pte_offset_kernel(pmd , arg);
            printk("[slave] page descriptor: %lX\n", pte_val(*pte));
            ret = 0;
            break;
    }

    set_fs(old_fs);
    return ret;
}

int slave_open(struct inode *inode, struct file *filp) {
    filp->private_data = kmalloc(MAP_SIZE, GFP_KERNEL);
    return 0;
}

ssize_t receive_f(struct file *filp, char *buffer, size_t count, loff_t *ppos) {
    char msg[BUF_SIZE];
    size_t len;
    len = krecv(sockfd_cli, msg, count * sizeof(char), 0);
    if (copy_to_user(buffer, msg, len))
        return -ENOMEM;
    return len;
}

int slave_close(struct inode *inode, struct file *filp) {
    kfree(filp->private_data);
    return 0;
}

static int memory_mapping(struct file *filp, struct vm_area_struct *vma) {
    // vma->vm_start: starting virtual address
    vma->vm_pgoff = virt_to_phys(filp->private_data)>>PAGE_SHIFT;
    if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, vma->vm_end - vma->vm_start, vma->vm_page_prot))
        return -EIO;
    vma->vm_flags |= VM_RESERVED;
    vma->vm_private_data = filp->private_data;
    vma->vm_ops = &mmap_vm_ops;
    mmap_open(vma);
    return 0;
}

module_init(slave_init);
module_exit(slave_exit);
MODULE_LICENSE("GPL");
