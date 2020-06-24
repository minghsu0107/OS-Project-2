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

// tells the memory management system not to attempt to swap out this VMA
#ifndef VM_RESERVED
#define VM_RESERVED   (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#define DEFAULT_PORT 2325
#define BUF_SIZE 512
#define MAP_SIZE (PAGE_SIZE * 1)

// custom commands
#define MASTER_PD 0x12345676
#define MASTER_CREATESOCK 0x12345677
#define MASTER_MMAP 0x12345678
#define MASTER_EXIT 0x12345679
#define BACKLOG 20

typedef struct socket *ksocket_t;

// exported kscoket functions
extern ksocket_t kaccept(ksocket_t socket, struct sockaddr *address, int *address_len);
extern int kbind(ksocket_t socket, struct sockaddr *address, int address_len);
extern int kclose(ksocket_t socket);
extern int klisten(ksocket_t socket, int backlog);
extern ssize_t ksend(ksocket_t socket, const void *buffer, size_t length, int flags);
extern ksocket_t ksocket(int domain, int type, int protocol);

extern char *inet_ntoa(struct in_addr *in);

// init and exit module
static int __init master_init(void);
static void __exit master_exit(void);

// methods for file operations
int master_close(struct inode *inode, struct file *filp);
int master_open(struct inode *inode, struct file *filp);
static long master_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static ssize_t send_f(struct file *filp, const char __user *buffer, size_t count, loff_t *ppos);

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

static ksocket_t sockfd_svr; // socket for master device
static ksocket_t sockfd_cli; // socket for slave device
static struct sockaddr_in addr_svr; // address for master device
static struct sockaddr_in addr_cli; // address for slave device
static int addr_len;
static mm_segment_t old_fs;

// file operations
static struct file_operations master_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = master_ioctl, // using custom ioctl
    .open = master_open, // method for opening the device
    .write = send_f, // method for writing to the device
    .release = master_close, // method when closing the device
    .mmap = memory_mapping // method for mapping device memory to program address space
};

// misc device info
static struct miscdevice master_dev = {
    .minor = MISC_DYNAMIC_MINOR, // automatic minor device number
    .name = "master_device",
    .fops = &master_fops
};

// for mmap
struct vm_operations_struct mmap_vm_ops = {
    .open = mmap_open,
    .close = mmap_close,
    .fault = mmap_fault
};

static int __init master_init(void) {
    int ret;
    // mounted on /sys/kernel/debug by default
    file1 = debugfs_create_file("debug_master", 0644, NULL, NULL, &master_fops);

    //register the device
    if ((ret = misc_register(&master_dev)) < 0) {
        printk(KERN_ERR "[master] misc register failed\n");
        return ret;
    }

    printk(KERN_INFO "[master] registered\n");

    // in order to use system calls like read and write in kernel
    old_fs = get_fs();
    set_fs(KERNEL_DS); // expand the memory limit to KERNEL_DS

    memset(&addr_cli, 0, sizeof(addr_cli));

    //initialize the master server
    sockfd_svr = sockfd_cli = NULL;
    memset(&addr_svr, 0, sizeof(addr_svr));
    addr_svr.sin_family = AF_INET;
    addr_svr.sin_addr.s_addr = INADDR_ANY;
    addr_svr.sin_port = htons(DEFAULT_PORT);
    addr_len = sizeof(struct sockaddr_in);

    sockfd_svr = ksocket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_svr == NULL) {
        printk("[master] socket failed\n");
        return -1;
    }
    printk("[master] socket is created: 0x%p\n", sockfd_svr);

    // bind the socket to the port on localhost
    if (kbind(sockfd_svr, (struct sockaddr *)&addr_svr, addr_len) < 0) {
        printk("[master] bind failed\n");
        return -1;
    }
    if (klisten(sockfd_svr, BACKLOG) < 0) {
        printk("[master] listen failed\n");
        return -1;
    }
    printk("[master] init succeeded\n");
    set_fs(old_fs);
    return 0;
}

static void __exit master_exit(void) {
    misc_deregister(&master_dev);
    printk("[master] misc deregister\n");

    if (kclose(sockfd_svr) == -1) {
        printk("[master] kclose error\n");
        return;
    }
    set_fs(old_fs);
    printk(KERN_INFO "[master] exited\n");
    debugfs_remove(file1);
}

static long master_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    long ret = 0;
    char *tmp;
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    switch (cmd) {
        // master device blocks until it accepts a connection from slave device
        // then master devices creates a socket for handling the connection
        case MASTER_CREATESOCK:
            sockfd_cli = kaccept(sockfd_svr, (struct sockaddr *)&addr_cli, &addr_len);
            if (sockfd_cli == NULL) {
                printk("[master] kaccept failed\n");
                return -1;
            }
            else {
                printk("[master] accept socket from client: 0x%p\n", sockfd_cli);
            }

            tmp = inet_ntoa(&addr_cli.sin_addr);
            printk("[master] new connection from %s:%d\n", tmp, ntohs(addr_cli.sin_port));
            kfree(tmp);
            ret = 0;
            break;
        case MASTER_MMAP:
            ret = ksend(sockfd_cli, filp->private_data, arg, 0);
            break;
        case MASTER_EXIT:
            if (kclose(sockfd_cli) == -1) {
                printk("[master] kclose cli error\n");
                return -1;
            }
            ret = 0;
            break;
        // virtual address to physical address conversion
        case MASTER_PD:
            pgd = pgd_offset(current->mm, arg);
            p4d = p4d_offset(pgd, arg);
            pud = pud_offset(p4d, arg);
            pmd = pmd_offset(pud, arg);
            pte = pte_offset_kernel(pmd , arg);
            printk("[master] page descriptor: %lX\n", pte_val(*pte));
            ret = 0;
            break;
    }

    set_fs(old_fs);
    return ret;
}

int master_open(struct inode *inode, struct file *filp) {
    filp->private_data = kmalloc(MAP_SIZE, GFP_KERNEL);
    return 0;
}

static ssize_t send_f(struct file *filp, const char __user *buffer, size_t count, loff_t *ppos) {
    char msg[BUF_SIZE];
    // return 0 on success
    if (copy_from_user(msg, buffer, count))
        return -ENOMEM;
    ksend(sockfd_cli, msg, count, 0);

    return count;
}

int master_close(struct inode *inode, struct file *filp) {
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

module_init(master_init);
module_exit(master_exit);
MODULE_LICENSE("GPL");
