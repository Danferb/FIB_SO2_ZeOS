#include <libc.h>

char buff[24];
int pid;
extern int errno;

void perror();

int __attribute__ ((__section__(".text.main")))
  main(void)
{
    /* Next line, tries to move value 0 to CR3 register. This register is a privileged one, and so it will raise an exception */
      //__asm__ __volatile__ ("mov %0, %%cr3"::"r" (0) );

	char buff[5] = {'H','O','L','A','\n'};
	write(1,buff, strlen(buff));
	char cad[5] = {'R','Q','P','C','\n'};
	write(1,cad, strlen(cad));
	write(2,cad, strlen(cad));
	perror();
	while(1);
	return 0;
}



