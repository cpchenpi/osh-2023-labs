#include <stdio.h>
#include <sys/syscall.h>      /* Definition of SYS_* constants */
#include <unistd.h>

void check_syscall(char *buf, int buf_len){
    printf("Syscall begin, buf_len: %d\n", buf_len);
    long ans = syscall(548, buf, buf_len);
    if (ans == 0){
        printf("Syscall successed! buf: %s\n", buf);
    } else {
        printf("Syscall failed!\n");
    }
}

int main() {
    printf("Hello! PB21000039\n");
    char buf_OK[50], buf_FAIL[20];
    check_syscall(buf_OK, 50);
    check_syscall(buf_FAIL, 20);
    while (1) {}
}
