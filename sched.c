/*
 * sched.c - initializes struct for task 0 anda task 1
 */

#include <sched.h>
#include <mm.h>
#include <io.h>

extern struct list_head freequeue;
extern struct list_head readyqueue;
extern TSS tss;
int ticsallowed;

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


int allocate_DIR(struct task_struct *t) 
{
	int pos;

	pos = ((int)t-(int)task)/sizeof(union task_union);

	t->dir_pages_baseAddr = (page_table_entry*) &dir_pages[pos]; 

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
	struct list_head * e = list_first( &freequeue );
	list_del( e ); 
	struct task_struct * s = list_head_to_task_struct(e);
	(*s).PID = 0;
	allocate_DIR(s);
	union task_union * t = (union task_union *) s;
	(*t).stack[KERNEL_STACK_SIZE-4] = (unsigned long) &cpu_idle;
	(*t).stack[KERNEL_STACK_SIZE-8] = 0;
	(*s).ebp_ret = (unsigned int *) &((*t).stack[KERNEL_STACK_SIZE-8]); 
	idle_task = s;
	s->st.state = 0;
	s->st.user_ticks = 0;
	s->st.system_ticks = 0;
	s->st.blocked_ticks = 0;
	s->st.ready_ticks = 0;
	s->st.elapsed_total_ticks = 0;
	s->st.total_trans = 0;
	s->st.remaining_ticks = 0;
}

void init_task1(void)
{
	struct list_head * e = list_first( &freequeue );
	list_del(e);
	struct task_struct * s = list_head_to_task_struct(e);
	(*s).PID = 1;
	allocate_DIR(s);
	union task_union *t = (union task_union *) s;
	set_user_pages(s);
	tss.esp0 = (DWord) &(*t).stack[KERNEL_STACK_SIZE];
	set_cr3(s->dir_pages_baseAddr);
	s->st.user_ticks = 0;
	s->st.system_ticks = 0;
	s->st.blocked_ticks = 0;
	s->st.ready_ticks = 0;
	s->st.elapsed_total_ticks = 0;
	s->st.total_trans = 0;
	s->st.remaining_ticks = 0;
	s->state = 0;
}

	
	



void init_sched(){
	INIT_LIST_HEAD( &freequeue );
	INIT_LIST_HEAD( &readyqueue );
	for(int i = 0; i < NR_TASKS; ++i){
		list_add( &(task[i].task.list), &freequeue );
		
	}
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
	current()->quantum--;
}
int needs_sched_rr(){
	if(current()->quantum == 0 && !list_empty(&freequeue)) return 1;
	return 0;
}
void update_process_state_rr(struct task_struct *t, struct list_head *dst_queue){
	if(t->state == 0){
		list_del(t->list);
		if(t->state == 0){
			list_add(t->list, dst_queue);	
		}
		if(t->state == 1){
			list_add(t->list, NULL);		
		}
	}
}

void sched_next_rr(){
	union task_union *new;
	struct list_head * e = list_first(&readyqueue);
	list_del(e);
	struct task_struct * s = list_head_to_task_struct(e);
	new = (union task_union *) s;
	task_switch(new);
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

