#include "types.h"
#include "user.h"

char mem[4096 * 10];

int main() {
	printf(1, "accessing global memory\n");
	for(int i = 0; i < 4096 * 10; i++) {
		mem[i] = 'a';
	}
	char *x[30];
	printf(1, "accessing malloced memory\n");
	for(int i = 0; i < 20; i++) {
		x[i] = (char *)malloc(4096);
		x[i][0] = 'a';
	}
	printf(1, "accessing global memory\n");
	mem[4096 * 5] = 's';

	exit();
}
