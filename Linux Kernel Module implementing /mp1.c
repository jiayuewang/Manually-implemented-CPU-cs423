#define LINUX

// Kernel
#include <linux/module.h>
#include <linux/kernel.h>
// Proc filesystem
#include <linux/proc_fs.h>
// Memory
#include <linux/slab.h>
// Kernel Flags
#include <linux/gfp.h>
// Filesystem I/O
#include <linux/fs.h>
#include <linux/file.h> 
#include <linux/buffer_head.h>
#include <asm/segment.h>
// User Access
#include <asm/uaccess.h>
// Timer Libraries
#include <linux/timer.h>
#include <linux/jiffies.h>
// WorkQueue
#include <linux/workqueue.h>
// String
#include <linux/string.h>
// System Calls
#include <linux/syscalls.h> 
#include <linux/fcntl.h>
// Spinlock
#include <linux/spinlock.h>
// Given Functions
#include "mp1_given.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group_ID");
MODULE_DESCRIPTION("CS-423 MP1");

#define DEBUG 0
#define FILENAME "status"
#define DIRECTORY "mp1"

// /proc/mp1/status variables
static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *proc_entry;

// PID Linux Kernel Linked List variables
static struct pid_list head;
static struct pid_list *tmp;
static struct list_head *pos, *q;
static int list_size = 0;

// Interrupt Variables
static struct timer_list my_timer;
static struct workqueue_struct *queue;
// Bypass circular declaration
static void queue_timer_updates(void);

// Semaphore Lock for /proc/mp1/status
static spinlock_t lock;

// Linux Kernel Linked List Struct
struct pid_list {
   int pid;
   int runtime;
   struct list_head list;
};

//Reads data in linked list and outputs it to the userspace
static ssize_t mp1_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos){
   if (DEBUG) printk(KERN_ALERT "SENDING PIDS");
	//If ppos > 0, then that means this is the second time being called and all data has been sent 
	//If list_size == 0, that means the list is empty and we should return immediately
	if((int)*ppos > 0 || !list_size)
		return 0;
	//Loops through each element in the list and stores the data into a string which is
	//  sent out to userspace
	spin_lock(&lock);
       	  list_for_each_safe(pos, q, &head.list){
	     char my_buf[64];
       	     tmp = list_entry(pos, struct pid_list, list);
	     sprintf(my_buf,"%d: %lu\n",tmp->pid,tmp->runtime);
	     if(DEBUG) printk(KERN_ALERT "%d: %lu\n", tmp->pid,tmp->runtime);
	     if (copy_to_user(buffer + *ppos, my_buf, strlen(my_buf)+1))
	         return -EFAULT;
	     *ppos += 64;
       	  }
	spin_unlock(&lock);
	//Makes sure that retval is not 0 and it is less than count so that buffer is sent back as intended
	    int retval = count - *ppos;

	    if (DEBUG) printk(KERN_ALERT "SENT PIDS");
	    return retval;
}

// Recieves PID from user and inits new pid_list struct
// Must return copied count or will infinite loop
// 
// copy_from_user reference: http://stackoverflow.com/questions/23433936/return-value-of-copy-from-user
static ssize_t mp1_write(struct file *file, const char __user *buffer, size_t count, loff_t *data){
   int to_copy;

   if (DEBUG) printk(KERN_ALERT "RECEIVING PID");
   // Initialize memory for new linked list object
	tmp = (struct pid_list *)kmalloc(sizeof(struct pid_list), GFP_KERNEL);
   tmp->runtime = 0;
   char pid_str[count];
   to_copy = copy_from_user(pid_str, buffer, count);
   sscanf(pid_str,"%d%*s",&(tmp->pid));
   // Add new object to list
   spin_lock(&lock);
   list_add_tail(&(tmp->list), &(head.list));
   list_size++;
   spin_unlock(&lock);
   if (DEBUG) printk(KERN_ALERT "RECEIVED PID: %d",tmp->pid);

   *data += count - to_copy;
   return count - to_copy;
}

static const struct file_operations mp1_file = {
   .owner = THIS_MODULE,
   .read = mp1_read,
   .write = mp1_write,
};

 // Sets 5 seconds timer interrupt
 static void set_timer(void) {
    int ret;
    setup_timer(&my_timer, queue_timer_updates, 0);
    ret = mod_timer(&my_timer, jiffies + msecs_to_jiffies(5000));
    if (ret && DEBUG) printk(KERN_ALERT "ERROR IN MOD TIMER\n");
    if (DEBUG) printk(KERN_ALERT "5 SECOND TO TIMER INTERRUPT");
 }

 // Interrupt Bottom Half
 // Updates Process Runtimes and Updates /proc/mp1/status file
 static void update_runtimes(void){
    int ret;
    unsigned long cpu_use;

    if (DEBUG) printk(KERN_ALERT "UPDATING RUNTIMES\n");
    spin_lock(&lock);
    // Updates timings in linked list
       // Loops through processes and updates timings
       list_for_each_safe(pos, q, &head.list){
          tmp = list_entry(pos, struct pid_list, list);
          ret = get_cpu_use(tmp->pid, &cpu_use);
          if (!ret) {
             tmp->runtime = cpu_use;
	  }else{
		//Deletes processes that are killed from linked list
       		list_del(pos);
       		kfree(tmp);
		list_size--;
	  }
       }
    spin_unlock(&lock);
    if (DEBUG) printk(KERN_ALERT "FINISHED UPDATING RUNTIMES\n");
 }

 // Interrupt Top Half
 // Queues work function to be done
 static void queue_timer_updates(void){
    struct work_struct *work = (struct work_struct *)kmalloc(sizeof(struct work_struct), GFP_KERNEL);
    if (work) {
       INIT_WORK( (struct work_struct *)work, update_runtimes);
       queue_work( queue, (struct work_struct *)work );
    }
    set_timer();
 }

// mp1_init - Called when module is loaded
int __init mp1_init(void)
{
   #ifdef DEBUG
   printk(KERN_ALERT "MP1 MODULE LOADING\n");
   #endif

   // Initialize spinlock
   spin_lock_init(&lock);

   // Creates /proc/mp1/status
   proc_dir = proc_mkdir(DIRECTORY, NULL);
   proc_entry = proc_create(FILENAME, 0666, proc_dir, &mp1_file);  
   if (DEBUG) printk(KERN_ALERT "CREATED /proc/mp1/status \n");

   // initializes pid_list head
   if (DEBUG) printk(KERN_ALERT "INITIALIZE pid_list head\n");
   INIT_LIST_HEAD(&head.list);

   // Initialize Workqueue
    queue = create_workqueue("runtime_updates");

   // Queue 5 second timer interrupt
    set_timer();

   if (DEBUG) printk(KERN_ALERT "MP1 MODULE LOADED\n");
   return 0;   
}

// mp1_exit - Called when module is unloaded
void __exit mp1_exit(void)
{
   #ifdef DEBUG
   printk(KERN_ALERT "MP1 MODULE UNLOADING\n");
   #endif

   // Frees Timer_List
    del_timer(&my_timer);

   // Frees work queue
    flush_workqueue(queue);
    destroy_workqueue(queue);
    if (DEBUG) printk(KERN_ALERT "DELETED WORKQUEUE\n");
   
   // Deletes /proc/mp1/status
   proc_remove(proc_entry);
   proc_remove(proc_dir);
   if (DEBUG) printk(KERN_ALERT "DELETED /proc/mp1/status\n");
   
   // Frees pid_list memory   
   list_for_each_safe(pos, q, &head.list){
       tmp = list_entry(pos, struct pid_list, list);
       list_del(pos);
       kfree(tmp);
   }
   if (DEBUG) printk(KERN_ALERT "DELETED pid_list struct\n");

   if (DEBUG) printk(KERN_ALERT "MP1 MODULE UNLOADED\n");
}

// Register init and exit funtions
module_init(mp1_init);
module_exit(mp1_exit);
