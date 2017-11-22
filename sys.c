/*
 * sys.c - Syscalls implementation
 */
#include <devices.h>

#include <utils.h>

#include <io.h>

#include <mm.h>

#include <mm_address.h>

#include <sched.h>





#define LECTURA 0
#define ESCRIPTURA 1
typedef unsigned int Dword;
extern long int zeos_ticks;
extern struct list_head freequeue;
int global_PID = 1;
extern int remaining_quantum;
extern struct list_head readyqueue;
//void update_stats(unsigned long *v, unsigned long *elapsed);
int check_fd(int fd, int permissions)
{
  if (fd!=1) return -9; /*EBADF*/
  if (permissions!=ESCRIPTURA) return -13; /*EACCES*/
  return 0;
}

int sys_ni_syscall()
{
	return -38; /*ENOSYS*/
}
int ret_from_fork(){
	return 0;
}
int sys_getpid()
{
	return current()->PID;
}
void user_to_system(void)
{
  update_stats(&(current()->st.user_ticks), &(current()->st.elapsed_total_ticks));
}

void system_to_user(void)
{
  update_stats(&(current()->st.system_ticks), &(current()->st.elapsed_total_ticks));
}

int sys_clone(void (*function)(void), void *stack){
	
	if(list_empty(&freequeue)) return -102; /*102: EOEQ*/
	struct list_head * son = list_first( &freequeue);
	list_del(son); 
	struct task_struct * s = list_head_to_task_struct(son);
  	union task_union * son_union = (union task_union *) s;
	copy_data(current(), son_union, sizeof(union task_union));
	son_union->task.PID = ++global_PID;
	son_union->task.state =ST_READY;
	int register_ebp;
	__asm__ __volatile__(
		"movl %%ebp, %0"
		:"=g" (register_ebp)
		:
	);
	register_ebp = (register_ebp - (int)current()) + (int)(son_union);
	son_union->task.ebp_ret = register_ebp + sizeof(Dword);
	DWord temp_ebp=*(DWord*)register_ebp;
  	son_union->task.ebp_ret-=sizeof(DWord);
  	*(DWord*)(son_union->task.ebp_ret)=(DWord)&ret_from_fork;
  	son_union->task.ebp_ret-=sizeof(DWord);
  	*(DWord*)(son_union->task.ebp_ret)=temp_ebp;
  	init_stats(&(son_union->task.st));
	son_union->stack[KERNEL_STACK_SIZE-5] = (unsigned long)function;//COM
	son_union->stack[KERNEL_STACK_SIZE-2] = (unsigned long)stack; //COM
 	son_union->task.state=ST_READY;
  	list_add_tail(&(son_union->task.list), &readyqueue);
	return son_union->task.PID;
}

int sys_fork()
{
	if(list_empty(&freequeue)) return -102; /*102: EOEQ*/
	struct list_head * son = list_first( &freequeue);
	list_del(son); 
	struct task_struct * s = list_head_to_task_struct(son);
  	union task_union * son_union = (union task_union *) s;
	copy_data(current(), son_union, sizeof(union task_union));
	allocate_DIR(s);
	unsigned int a[NUM_PAG_DATA];
	for(int i = 0; i < NUM_PAG_DATA; ++i){
		a[i] = alloc_frame();
		if(a[i] == -1) return -103; /*EOFM*/
	}
	page_table_entry * PT = get_PT(s);
	for(int i = 0; i < 255; ++i){ /*System code and data*/
		set_ss_pag(PT, i, get_frame(get_PT(current()), i));	
	}
	for(int i = USER_FIRST_PAGE; i < USER_FIRST_PAGE + NUM_PAG_CODE; ++i){ /*User code(shared)*/
		set_ss_pag(PT, i, get_frame(get_PT(current()), i));
	}
	for(int i = 255 + NUM_PAG_CODE; i < 255 + NUM_PAG_CODE + NUM_PAG_DATA; ++i){/*PH frames for user data*/
		set_ss_pag(PT, i, a[i]);
	}
	for(int i =  0; i < NUM_PAG_DATA; ++i){/*Use 1 temporarl entry of father to copy user data*/
		set_ss_pag(get_PT(current()), 255+ NUM_PAG_CODE + NUM_PAG_DATA + 1, get_frame(PT, 255 + NUM_PAG_CODE + i));
		copy_data(L_USER_START+ NUM_PAG_CODE * PAGE_SIZE,L_USER_START+ NUM_PAG_CODE * PAGE_SIZE + NUM_PAG_CODE* PAGE_SIZE, PAGE_SIZE);
		del_ss_pag(get_PT(current()), 255+ NUM_PAG_CODE + NUM_PAG_DATA + 1);	
		set_cr3(get_DIR(current()));/*flush TLB*/
	}
	son_union->task.PID = ++global_PID;
	son_union->task.state =ST_READY;
	int register_ebp;
	__asm__ __volatile__(
		"movl %%ebp, %0"
		:"=g" (register_ebp)
		:
	);
	register_ebp = (register_ebp - (int)current()) + (int)(son_union);
	son_union->task.ebp_ret = register_ebp + sizeof(Dword);
	DWord temp_ebp=*(DWord*)register_ebp;
  	son_union->task.ebp_ret-=sizeof(DWord);
  	*(DWord*)(son_union->task.ebp_ret)=(DWord)&ret_from_fork;
  	son_union->task.ebp_ret-=sizeof(DWord);
  	*(DWord*)(son_union->task.ebp_ret)=temp_ebp;
  	init_stats(&(son_union->task.st));

 	son_union->task.state=ST_READY;
  	list_add_tail(&(son_union->task.list), &readyqueue);
	return son_union->task.PID;
}

int sys_yield()
{
  force_task_switch();
  return 0;
}

void sys_exit(){
	for(int i = 255 + NUM_PAG_CODE; i < 255 + NUM_PAG_CODE + NUM_PAG_DATA; ++i){
		free_frame(get_frame(get_PT(current()), i));
	}
	for(int i = 0; i < 255 + NUM_PAG_CODE + NUM_PAG_DATA; ++i){
		del_ss_pag(get_PT(current()), i);
	}
	list_add_tail(&(current()->list), &freequeue);
	current()->PID = -1;
	int pos;
	struct task_struct *t = current();
	pos = ((int)t-(int)task)/sizeof(union task_union);
	dir_pages_num_occuped[pos]--;
	global_PID--;
	sched_next_rr();
}

int sys_write (int fd, char *buffer, int size){
	int err;
	err = check_fd(fd, ESCRIPTURA);
	if(err != 0) return err;
	if(buffer == NULL) return err;
	if(size <= 0) return err;
	int checkcopy, i ;
	checkcopy = 0;
	char b[8];
	char *buff;
	buff = &b;
	/*checkcopy = copy_from_user(buffer, buff, size);
	if(checkcopy != 0) return err;
	err = sys_write_console(buff, size);
	*/
	i = 0;
	while(size > 0){
		if(size >= 8){
			checkcopy = copy_from_user(buffer + i, buff, 8);
			if(checkcopy != 0) return err;
			err = sys_write_console(buff, 8);
			i+=8;
			size-=8;
		}
		else{
			checkcopy = copy_from_user(buffer + i, buff, size);
			if(checkcopy != 0) return err;
			err = sys_write_console(buff, size);
			size = 0;	
		}
	}
	
	return err;
  
}

int sys_gettime(){
	return zeos_ticks;
}

int sys_get_stats(int pid, struct stats *st)
{
  int i;
  
  if (!access_ok(VERIFY_WRITE, st, sizeof(struct stats))) return -14; 
  
  if (pid<0) return -22;
  for (i=0; i<NR_TASKS; i++)
  {
    if (task[i].task.PID==pid)
    {
      task[i].task.st.remaining_ticks=remaining_quantum;
      copy_to_user(&(task[i].task.st), st, sizeof(struct stats));
      return 0;
    }
  }
  return -3; /*ESRCH */
}




