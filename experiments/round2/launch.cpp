#include <chrono>
#include <cstdio>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
int main(int argc,char**argv){
 if(argc!=2)return 2;
 auto t=std::chrono::steady_clock::now();
 pid_t p=fork();if(!p){execl(argv[1],argv[1],nullptr);_exit(127);}
 int st; rusage r{};wait4(p,&st,0,&r);
 double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();
 unsigned long long peak=0;FILE*f=fopen("/sys/fs/cgroup/memory.peak","r");if(f){fscanf(f,"%llu",&peak);fclose(f);}
 printf("%.6f %llu %ld %.6f\n",ms,peak,r.ru_maxrss,r.ru_utime.tv_sec*1000.+r.ru_utime.tv_usec/1000.+r.ru_stime.tv_sec*1000.+r.ru_stime.tv_usec/1000.);
 return WIFEXITED(st)?WEXITSTATUS(st):128;
}
