/*
 * sched.c - initializes struct for task 0 anda task 1
 */

#include <sched.h>
#include <mm.h>
#include <io.h>
#include <stats.h>
#include <utils.h>
#include <hardware.h>
#include <types.h>

extern struct list_head freequeue;
extern struct list_head readyqueue;
int dir_pages_num_occuped[NR_TASKS+2];
extern TSS tss;
int ticsallowed;
int remaining_quantum = 0;

#define DEFAULT_QUANTUM 10
struct task_struct *list_head_to_task_struct(struct list_head *l)
{
  return list_entry( l, struct task_struct, list);
}
/**
 * Container for the Task array and 2 additional pages (the first and the last one)
 * to protect against out of bound accesses.
 */
union task_union protected_tasks[NR_TASKS+2]
  __attribute__((__section__(".data.task")));

union task_union *task = &protected_tasks[1]; /* == union task_union task[NR_TASKS] */

#if 0
struct task_struct *list_head_to_task_struct(struct list_head *l)
{
  return list_entry( l, struct task_struct, list);
}
#endif

extern struct list_head blocked;

void init_stats(struct stats *s){
	s->user_ticks = 0;
	s->system_ticks = 0;
	s->blocked_ticks = 0;
	s->ready_ticks = 0;
	s->elapsed_total_ticks = get_ticks();
	s->total_trans = 0;
	s->remaining_ticks = get_ticks();
}

/* get_DIR - Returns the Page Directory address for task 't' */
page_table_entry * get_DIR (struct task_struct *t) 
{
	return t->dir_pages_baseAddr;
}

/* get_PT - Returns the Page Table address for task 't' */
page_table_entry * get_PT (struct task_struct *t) 
{
	return (page_table_entry *)(((unsigned int)(t->dir_pages_baseAddr->bits.pbase_addr))<<12);
}

void update_stats(unsigned long *v, unsigned long *elapsed)
{
  unsigned long current_ticks;
  
  current_ticks=get_ticks();
  
  *v += current_ticks - *elapsed;
  
  *elapsed=current_ticks;
  
}

int allocate_DIR(struct task_struct *t) 
{
 	int pos;
	for(pos = 0; pos < NR_TASKS + 2; ++pos){
		if(posdir_pages_num_occuped[pos] == 0){
			dir_pages_num_occuped[pos]++;
			t->dir_pages_baseAddr = (page_table_entry*) &dir_pages[pos]; 
			return 1;
		}
	}
	return 1;
}

void cpu_idle(void)
{
	__asm__ __volatile__("sti": : :"memory");

	while(1)
	{
	;
	}
}

void init_idle (void)
{
	struct list_head *l = list_first(&freequeue);
  	list_del(l);
  	struct task_struct *c = list_head_to_task_struct(l);
  	union task_union *uc = (union task_union*)c;
        c->PID=0;
 	c->quantum=DEFAULT_QUANTUM;
  	init_stats(&c->st);
  	allocate_DIR(c);
  	uc->stack[KERNEL_STACK_SIZE-1]=(unsigned long)&cpu_idle;
  	uc->stack[KERNEL_STACK_SIZE-2]=0;
  	c->ebp_ret=(int)&(uc->stack[KERNEL_STACK_SIZE-2]);
 	idle_task=c;
}

void init_task1(void)
{
	struct list_head *l = list_first(&freequeue);
  	list_del(l);
  	struct task_struct *c = list_head_to_task_struct(l);
  	union task_union *uc = (union task_union*)c;
  	c->PID=1;
  	c->quantum=DEFAULT_QUANTUM;
  	c->state=ST_RUN;
 	remaining_quantum=c->quantum;
  	init_stats(&c->st);
  	allocate_DIR(c);
  	set_user_pages(c);
  	tss.esp0=(DWord)&(uc->stack[KERNEL_STACK_SIZE]);
  	set_cr3(c->dir_pages_baseAddr);
}

	
	



void init_sched(){
  	INIT_LIST_HEAD(&freequeue);
	int i;
  	for (i=0; i<NR_TASKS; i++){
    		task[i].task.PID=-1;
   		list_add_tail(&(task[i].task.list), &freequeue);
		dir_pages_num_occuped[i] = 0;
  	}
	INIT_LIST_HEAD( &readyqueue );
	dir_pages_num_occuped[i+1] = 0;
	dir_pages_num_occuped[i+2] = 0;
}

struct task_struct* current()
{
  int ret_value;
  
  __asm__ __volatile__(
  	"movl %%esp, %0"
	: "=g" (ret_value)
  );
  return (struct task_struct*)(ret_value&0xfffff000);
}

void update_sched_data_rr(){
	remaining_quantum--;
}
int needs_sched_rr(){
	if(remaining_quantum == 0 && !list_empty(&freequeue)) return 1;
	if(remaining_quantum == 0) remaining_quantum = get_quantum(current());
	return 0;
}
void update_process_state_rr(struct task_struct *t, struct list_head *dst_queue){
	  if (t->state!=ST_RUN) list_del(&(t->list));
 	  if (dst_queue!=NULL){
    		list_add_tail(&(t->list), dst_queue);
    		if (dst_queue!=&readyqueue) t->state=ST_BLOCKED;
   		else{
      			update_stats(&(t->st.system_ticks), &(t->st.elapsed_total_ticks));
      			t->state=ST_READY;
    		}
  	  }
  	  else t->state=ST_RUN;
}

void sched_next_rr(){
	struct list_head *e;
 	struct task_struct *t;
  	if (!list_empty(&readyqueue)) {
		e = list_first(&readyqueue);
    		list_del(e);
		t=list_head_to_task_struct(e);
 	 }
 	else t=idle_task;
	t->state=ST_RUN;
  	remaining_quantum=get_quantum(t);
	update_stats(&(current()->st.system_ticks), &(current()->st.elapsed_total_ticks));
  	update_stats(&(t->st.ready_ticks), &(t->st.elapsed_total_ticks));
  	t->st.total_trans++;
	task_switch((union task_union*)t);
}
void schedule(){
	update_sched_data_rr();
	if(needs_sched_rr()){
		update_process_state_rr(current(), &readyqueue);
		sched_next_rr();
	}

}

int get_quantum (struct task_struct *t){
	return t->quantum;
}

void set_quantum (struct task_struct *t, int new_quantum){
	t->quantum = new_quantum;
}
void force_task_switch(){
	update_process_state_rr(current(), &readyqueue);
	sched_next_rr();
}

