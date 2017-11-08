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

extern long int zeos_ticks;
extern struct list_head freequeue;
int globalPID = 1;
extern struct list_head readyqueue;

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

int sys_fork()
{
	if(list_empty(&freequeue)) return -102; /*102: EOEQ*/
	struct list_head * son = list_first( &freequeue);
	list_del(son); 
	struct task_struct * s = list_head_to_task_struct(son);
  	union task_union * son_union = (union task_union *) s;
	son_union->task.PID = ++globalPID;
	son_union->task.ebp_ret = current()->ebp_ret;
	union task_union * current_union = (union task_union *)current();
	for(int i = 0; i < KERNEL_STACK_SIZE; ++i){
		son_union->stack[i] = current_union->stack[i];
	}
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
	*(s->ebp_ret) = &ret_from_fork;/* set ret_from_fork on ebp*/
	*((s->ebp_ret) - 4) = 0;/*set 0 to get a return*/
	list_add(&s->list, &readyqueue);/*add to readyqueue*/
	s->st.user_ticks = 0;
	s->st.system_ticks = 0;
	s->st.blocked_ticks = 0;
	s->st.ready_ticks = 0;
	s->st.elapsed_total_ticks = 0;
	s->st.total_trans = 0;
	s->st.remaining_ticks = 0;
	s->state = 0;
	return son_union->task.PID;
}


void sys_exit(){
	for(int i = 255 + NUM_PAG_CODE; i < 255 + NUM_PAG_CODE + NUM_PAG_DATA; ++i){
		free_frame(get_frame(get_PT(current()), i));
	}
	for(int i = 0; i < 255 + NUM_PAG_CODE + NUM_PAG_DATA; ++i){
		del_ss_pag(get_PT(current()), i);
	}
	current()->PID = 0;
	free_user_pages(current());
	current()->state = 0;
	current()->quantum = 0;
	union task_union *del = (union task_union *) current();
	for(int i = 0; i < KERNEL_STACK_SIZE; ++i){
		del->stack[i] = -1;
	}
	globalPID--;
	schedule();
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

void sys_stats(){

}




