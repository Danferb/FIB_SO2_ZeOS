/*
 * libc.c 
 */

#include <libc.h>

#include <types.h>

int errno;
int write (int fd, char *buffer, int size){
	int b;
	int f = fd;
	char * bf = buffer;
	int sz = size;
	b = 0;	
	__asm__ __volatile__ (
	"movl 	$4, %%eax;"
	"int	$0x80"
	:"=a"(b) /*output*/
	:"a"(b), "b"(f), "c"(bf), "d"(sz)/*input*/
	);
	if(b >= 0) return b;
	else{
		errno = -b;	
		return -1;
	}
}

void perror(){
	char buffer[256];
	itoa(errno, buffer);
	write(1, buffer, strlen(buffer));
}

int add(int par1, int par2){
	int res;
	res = 0;
	__asm__ (
	"push   %ebp;"
        "mov    %esp,%ebp;"
	"mov    0xc(%ebp),%eax;"
        "add    0x8(%ebp),%eax;"
	"movl	%eax, -4(%ebp);"
        "pop    %ebp;"
        "ret;");
	return res;
}

long inner (long n){
	int i;
	long suma;
	suma = 0;
	for (i=0; i<n; i++) suma = suma + i;
	return suma;
}

long outer (long n){
	int i;
	long acum;
	acum = 0;
	for (i = 0; i < n; i++) acum = acum + inner(i);
	return acum;
}

int gettime(){
	int time;
	time = 0;
	__asm__ (
	"movl	0x08(%ebp), %ebx;"
	"movl 	0x0c(%ebp), %ecx;"
	"movl 	0x10(%ebp), %edx;"
	"movl 	0x14(%ebp), %esi;"
	"movl 	0x18(%ebp), %edi;"
	"movl 	$10, %eax;"
	"int	$0x80;"
	"movl 	%eax, -4(%ebp);"
	);
	return time;
}
void itoa(int a, char *b)
{
  int i, i1;
  char c;
  
  if (a==0) { b[0]='0'; b[1]=0; return ;}
  
  i=0;
  while (a>0)
  {
    b[i]=(a%10)+'0';
    a=a/10;
    i++;
  }
  
  for (i1=0; i1<i/2; i1++)
  {
    c=b[i1];
    b[i1]=b[i-i1-1];
    b[i-i1-1]=c;
  }
  b[i]=0;
}

int strlen(char *a)
{
  int i;
  
  i=0;
  
  while (a[i]!=0) i++;
  
  return i;
}

int getpid(void){
	int pid = 0;
	__asm__ __volatile__ (
	"movl 	$20, %%eax;"
	"int	$0x80"
	:"=a"(pid) /*output*/
	:"a"(pid)/*input*/
	);
	return pid;
}

int fork(void){
	int pid = 0;
	__asm__ __volatile__(
	"movl $2, %%eax;"
	"int $0x80"
	:"=a"(pid)
	:"a"(pid)	
	);
	if(pid < 0){
		errno = -pid;
		return -1;	
	}
	errno = 0;
	return pid;
}

void exit(void){
	__asm__(
	"movl $1, %eax;"
	"int $0x80"	
	);
}

int yield(){
	int result;
	__asm__ __volatile__ ( 
		"int $0x80"
		:"=a" (result)
		:"a" (13)
	);
	return result;
}
int get_stats(int pid, struct stats *st){
	int result;
	__asm__ __volatile__ (
		"int $0x80"
		:"=a" (result)
		: "a" (35), "b" (pid), "c" (st)
	);
	if (result < 0){
		errno = -result;
		return -1;
	}
	errno = 0;
	return result;
}
int clone(void (*function) (void), void *stack){
	int result;
	__asm__ __volatile__ (
	"movl 	$19, %%eax;"
	"int	$0x80"
	:"=a"(result) /*output*/
	:"a"(result), "b"(function), "c"(stack)/*input*/
	);
	if (result < 0){
		errno = -result;
		return -1;	
	}
	errno = 0;
	return result; 
	
}
