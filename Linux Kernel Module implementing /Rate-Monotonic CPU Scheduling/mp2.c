#define LINUX

// Kernel
#include <linux/module.h>
#include <linux/kernel.h>
// Kernel Flags
#include <linux/gfp.h>
// Kernel Thread
#include <linux/sched.h>
#include <linux/kthread.h>
// System Calls
#include <linux/syscalls.h> 
#include <linux/fcntl.h>
// Proc filesystem
#include <linux/proc_fs.h>
// Filesystem I/O
#include <linux/fs.h>
#include <linux/file.h> 
#include <linux/buffer_head.h>
#include <asm/segment.h>
// Memory
#include <linux/slab.h>
// Slac Allocator
#include <linux/mm.h>
// Spinlock
#include <linux/spinlock.h>
// User Access
#include <asm/uaccess.h>
// Timer Libraries
#include <linux/timer.h>
#include <linux/jiffies.h>
// Type Libraries
#include <linux/types.h>
// String
#include <linux/string.h>

// Given Functions
#include "mp2_given.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group_ID");
MODULE_DESCRIPTION("CS-423 MP2");

// Global Constants
#define DEBUG 1
#define FILENAME "status"
#define DIRECTORY "mp2"
#define BUFSIZE 128

/**
 * Linux Kernel Linked List Struct
 *
 * task_timer timer interrupt
 * list linux kernel linked list
 * linuxtask linux kernel task struct
 * pid user application pid
 * task_state user application state
 *    0 : SLEEP
 *    1 : READY
 *    2 : RUNNING
 * next_period time till next period
 * period user application period
 * runtime user application runtime
**/
typedef struct mp2_task_struct {
   struct timer_list task_timer;
   struct list_head list;
   struct task_struct* linuxtask;
   
   unsigned int pid;
   unsigned int task_state;
   uint64_t next_period;
   unsigned long period;
   unsigned long runtime; 
} mp2_struct;

// /proc/mp2/status
static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *proc_entry;

// Linux Kernel Linked List
static mp2_struct head;
static mp2_struct *tmp;
static struct list_head *pos, *q;
static int list_size = 0;

// Slab Allocator
static struct kmem_cache * kcache;

// Semaphore Lock for /proc/mp2/status
static spinlock_t lock;

// Current Running Process
static mp2_struct * current_process;

// Admission Control Storage
static unsigned long utilization;

// Kernel Thread for Bottom Half
struct task_struct * bottom_half;
int bottom_half_running;

// Bypass circular declaration
static void queue_dispatch_thread(unsigned long data);

/**
 * Get Current Time in microseconds
 *
 * RETURN current time in microseconds
**/
static unsigned long get_time(void){
   struct timeval time;
   do_gettimeofday(&time);
   return time.tv_sec * 1000000L + time.tv_usec;
}

/**
 * Proc Filesystem
 * Register User Application into RMS
 *
 * PARAM user_message string from user
**/
static void proc_fs_register(char * user_message) {
  int pid;
  int tmp_utilization;
  unsigned long period, runtime;

  sscanf(user_message, "R, %d, %lu, %lu", &pid, &period, &runtime);

  // Check Admission Control Policy
  tmp_utilization = utilization + (runtime * 1000) / period;
  if (tmp_utilization > 693) {
     if (DEBUG) { printk(KERN_ALERT "OVER UTILIZATION %d", tmp_utilization); }
     return;
  }

  // Allocate new struct 
  tmp = (mp2_struct *)kmem_cache_alloc(kcache, GFP_KERNEL);
  tmp->task_state = 0;
  tmp->pid = pid;
  tmp->period = period;
  tmp->runtime = runtime;
  tmp->next_period = get_time() + tmp->period;
  tmp->linuxtask = find_task_by_pid(tmp->pid);
  setup_timer(&(tmp->task_timer), queue_dispatch_thread, (unsigned long) tmp);

  // Update list_struct
  utilization = tmp_utilization;
  list_add_tail(&(tmp->list), &(head.list));
  list_size++;

  if (DEBUG) { printk(KERN_ALERT "Registered %d", pid); }
}

/**
 * Proc Filesystem
 * Deregister User Application from RMS
 * Frees mp2_struct tied to User Application
 *
 * PARAM user_message string from user
**/
static void proc_fs_deregister(char * user_message) {
  int pid;

  sscanf(user_message, "D, %d", &pid);

  // Loop through looking for correct mp2_struct
  list_for_each_safe(pos, q, &head.list) {
    tmp = list_entry(pos, mp2_struct, list);

    // Update Admission Control Policy
    if(pid == tmp->pid) {
      if (DEBUG) printk(KERN_INFO "PROCESS: %d DEREGISTERING", pid);
      utilization -= (tmp->runtime * 1000) / tmp->period;
      if (DEBUG) printk(KERN_ALERT "NEW UTILIZATION %lu", utilization); 

      // Free Memory
      list_del(pos);
      list_size--;
      del_timer(&(tmp->task_timer));
      kmem_cache_free(kcache,tmp);
      tmp = NULL;
      current_process = NULL;

      // Start another process
      wake_up_process(bottom_half);
      if (DEBUG) printk(KERN_INFO "STARTED ANOTHER PROCESS");
      
      if (DEBUG) printk(KERN_ALERT "PROCESS: %d DEREGISTERED PROPERLY\n", pid);
      return;
    }
  }
}

/**
 * Proc Filesystem
 * User Application Yield
 *
 * PARAM user_message string from user
**/
static void proc_fs_yield(char * user_message) {
  int pid, ret;
  long time_to_next_period;
  mp2_struct * yield_process = NULL;

  sscanf(user_message, "Y, %d", &pid);

  // Loop through to find Yielding mp2_struct
  list_for_each_safe(pos, q, &head.list) {
    tmp = list_entry(pos, mp2_struct, list);

    if (DEBUG) printk(KERN_INFO "SEARCHING FOR MATCH");
    if (tmp->pid == pid){
      if (DEBUG) printk(KERN_INFO "FOUND MATCH");
      yield_process = tmp;
      break;
    }
  }

  // Ensure Yield Process found
  if (yield_process == NULL) { return; }
  if (DEBUG) printk(KERN_ALERT "PROCESS: %d YIELDING", pid);

  // Get Time to next period  
  time_to_next_period = yield_process->next_period - get_time();
  if (time_to_next_period < 0) {
    if (DEBUG) printk(KERN_ALERT "Runtime > Period"); 
    return;
  }

  // Set time
  if (DEBUG) printk(KERN_ALERT "STARTING TIMER");
  ret = mod_timer(&(yield_process->task_timer), jiffies + msecs_to_jiffies(time_to_next_period / 1000));

  // Sleep task
  yield_process->task_state = 0;
  if (DEBUG) printk(KERN_INFO "SETTING TASK STATE");
  set_task_state(yield_process->linuxtask, TASK_UNINTERRUPTIBLE);

  if (DEBUG) printk(KERN_ALERT "SCHEDULING NEW FUNCTION");
  wake_up_process(bottom_half);

  if (DEBUG) printk(KERN_ALERT "PROCESS: %d YIELDED PROPERLY\n", pid);
}

/**
 * Proc Filesystem
 * Loops through mp2_struct list printing all mp2_structs to user
 *
 * RETURN number of bytes left in buffer
**/
static ssize_t mp2_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos){
  int retval;
  unsigned long flags;
  if (DEBUG) printk(KERN_INFO "SENDING PIDS");
  
  //If ppos > 0, then that means this is the second time being called and all data has been sent 
  //If list_size == 0, that means the list is empty and we should return immediately
  if((int)*ppos > 0 || !list_size) { 
    return 0; 
  }

	// Loops through mp2_struct list
	spin_lock_irqsave(&lock, flags);
  list_for_each_safe(pos, q, &head.list){
    // Generate string
    char my_buf[BUFSIZE];
    tmp = list_entry(pos, mp2_struct, list);
    sprintf(my_buf,"%d: %lu, %lu\n",tmp->pid,tmp->period, tmp->runtime);

    // Copy to buffer
    if(DEBUG) printk(KERN_ALERT "SENDING %s\n", my_buf);
    if (copy_to_user(buffer + *ppos, my_buf, strlen(my_buf)+1)) {
      return -EFAULT;
    }

    // Update position
    *ppos += strlen(my_buf);
  }

  spin_unlock_irqrestore(&lock, flags);
  //Makes sure that retval is not 0 and it is less than count so that buffer is sent back as intended
  retval = count - *ppos;
  if (DEBUG) printk(KERN_INFO "SENT PIDS");
  return retval;
}

/**
 * Proc Filesystem
 * Recieves PID from user and inits new mp2_struct struct
 *
 * RETURN copied data count
**/
static ssize_t mp2_write(struct file *file, const char __user *buffer, size_t count, loff_t *data){
  int to_copy;
  unsigned long flags;
  char user_message[count];
  if (DEBUG) printk(KERN_INFO "RECEIVING PID");

  // Initialize memory for new linked list object
  to_copy = copy_from_user(user_message, buffer, count);

  // Proc FS systems
  spin_lock_irqsave(&lock, flags);
  if (user_message[0] == 'R'){
    proc_fs_register(user_message);
  }
  else if (user_message[0] == 'D') {	
    proc_fs_deregister(user_message);
  }
  else if (user_message[0] == 'Y') {
    proc_fs_yield(user_message);
  }
  spin_unlock_irqrestore(&lock, flags);
  if (DEBUG) printk(KERN_ALERT "UNLOCKING");
  if (DEBUG) printk(KERN_ALERT "PROCESS %c", user_message[0]);

  *data += count - to_copy;
  return count - to_copy;
}

/**
 * Proc Filesystem
 * Function Setup
**/
static const struct file_operations mp2_file = {
  .owner = THIS_MODULE,
  .read = mp2_read,
  .write = mp2_write,
};

/**
 * Interrupt Bottom Half
 * Runs on alternate kernel thread, exits when bottom_half_running flag is false
 * 
 * Finds the process with the shortest period and runs it
 * If there is a running process, sleep it unless it is past its period
 * Called whenever any process has its task_state changed
 *
 * PARAM data irrelevant, in place to avoid errors
**/
static int dispatch_thread(void * data){
  struct sched_param sparam;
  unsigned long flags;
  mp2_struct * shortest_period;
  if (DEBUG) { printk(KERN_ALERT "dispatch_thread started"); }

  while(bottom_half_running) {
    if (DEBUG) { printk(KERN_ALERT "INTERRUPT BOTTOM HALF"); }
    
    // Mutex Lock
    spin_lock_irqsave(&lock, flags);

    // Find Process with shortest period
    shortest_period = NULL;
    list_for_each_safe(pos, q, &head.list) {
      tmp = list_entry(pos, mp2_struct, list);
      if (tmp->task_state == 1 && (shortest_period == NULL || tmp->period < shortest_period->period))
        shortest_period = tmp;
    } 

    // If none are READY, return
    if (shortest_period == NULL) {
      spin_unlock_irqrestore(&lock, flags);
      goto SLEEP;
    }

    // Update Current Running Process
    if (current_process != NULL && current_process->task_state == 2) {
      if (current_process->period < shortest_period->period) {
        spin_unlock_irqrestore(&lock, flags);
        goto SLEEP;
      }
      else {
        // Sets to READY
        current_process->task_state = 1;
        sparam.sched_priority = 0;
        sched_setscheduler(current_process->linuxtask, SCHED_NORMAL, &sparam);
      }
    }
    
    // Wakes new process
    if (DEBUG) printk(KERN_ALERT "WAKING NEW PROCESS");
    current_process = shortest_period;
    current_process->task_state = 2;
    wake_up_process(current_process->linuxtask);
    sparam.sched_priority = 99;
    sched_setscheduler(current_process->linuxtask, SCHED_FIFO, &sparam);
    spin_unlock_irqrestore(&lock, flags);

    // Sleep, wait for next wake up
    SLEEP:
    set_task_state(bottom_half, TASK_INTERRUPTIBLE);
    schedule();
  }
  return 0;
}

/**
 * Interrupt Top Half
 * Timer Interrupt callback function
 * Sets process to READY and calls bottom half
 * 
 * PARAM data mp2_struct pointer address to appropriate mp2_struct
**/
static void queue_dispatch_thread(unsigned long data){
  mp2_struct * process;
  if (DEBUG) { printk(KERN_ALERT "queue_dispatch start"); }

  // Update Process Information
  process = (mp2_struct *) data;
  process->task_state = 1;
  process->next_period = get_time() + process->period;

  if (DEBUG) { printk(KERN_ALERT "WAKE BOTTOM HALF %d", process->pid); }
  wake_up_process(bottom_half);
}

/**
 * Module Constructor
 * Called when the module is loaded
 * Initializes all necessary variables and allocates necessary memory
**/
int __init mp2_init(void)
{
   #ifdef DEBUG
   printk(KERN_INFO "MP1 MODULE LOADING\n");
   #endif

   // Initialize spinlock
   spin_lock_init(&lock);
   printk(KERN_INFO "INITIALIZED SPINLOCK\n");

   // Creates /proc/mp2/status
   proc_dir = proc_mkdir(DIRECTORY, NULL);
   proc_entry = proc_create(FILENAME, 0666, proc_dir, &mp2_file);  
   if (DEBUG) printk(KERN_INFO "CREATED /proc/mp2/status \n");

   // initializes mp2_struct head
   INIT_LIST_HEAD(&head.list);
   if (DEBUG) printk(KERN_INFO "INITIALIZE mp2_struct head\n");

   // initializes the slab allocator
   if (DEBUG) printk(KERN_INFO "INITIALIZED slab allocator\n");
   kcache = kmem_cache_create("mp2_struct",sizeof(mp2_struct),0,0,NULL);

   // Create Bottom Half Kernel Thread
   bottom_half = kthread_create(&dispatch_thread, NULL, "bottomhalf");
   get_task_struct(bottom_half);
   bottom_half_running = 1;

   // Initialize running process pointer
   current_process = NULL;

   // Initialize Admission Control Variable
   utilization = 0;

   if (DEBUG) printk(KERN_ALERT "MP1 MODULE LOADED\n");
   return 0;   
}

/**
 * Module Destructor
 * Called when the module is unloaded
 * Destroys  all necessary variables and frees used memory
**/
void __exit mp2_exit(void)
{
   #ifdef DEBUG
   printk(KERN_ALERT "MP1 MODULE UNLOADING\n");
   #endif

   // Deletes /proc/mp2/status
   proc_remove(proc_entry);
   proc_remove(proc_dir);
   if (DEBUG) printk(KERN_INFO "DELETED /proc/mp2/status\n");

   // Frees mp2_struct memory   
   list_for_each_safe(pos, q, &head.list){
       tmp = list_entry(pos, mp2_struct, list);
       del_timer(&(tmp->task_timer));
       list_del(pos);
       kmem_cache_free(kcache,tmp);
   }
   if (DEBUG) printk(KERN_INFO "DELETED pid_list struct\n");

   // destroys the slab allocator
   kmem_cache_destroy(kcache);
   if (DEBUG) printk(KERN_INFO "DESTROYED slab allocator\n");

   // Exits Bottom Half Kernel Thread
   bottom_half_running = 0;
   kthread_stop(bottom_half);
   put_task_struct(bottom_half);

   if (DEBUG) printk(KERN_ALERT "MP1 MODULE UNLOADED\n");
}

// Register init and exit functions
module_init(mp2_init);
module_exit(mp2_exit);
