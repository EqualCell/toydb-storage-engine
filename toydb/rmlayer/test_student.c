#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "student.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"student test line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
int main(void)
{
    StudentData data;
    FILE *file=fopen("lines.txt","wb");
    char *longline=malloc(10000);
    CHECK(file && longline);
    memset(longline,'x',10000);
    CHECK(fwrite("name;id\r\n\r\n",1,11,file)==11);
    CHECK(fwrite(longline,1,10000,file)==10000);
    CHECK(fwrite("\nlast;field",1,11,file)==11);
    CHECK(fclose(file)==0);
    CHECK(Student_Load("lines.txt",0,&data)==0);
    CHECK(data.count==3 && data.blank_lines==1 && data.maximum==10000);
    CHECK(data.lines[0].length==7 && !memcmp(data.lines[0].bytes,"name;id",7));
    CHECK(data.lines[1].length==10000 && !memcmp(data.lines[1].bytes,longline,10000));
    CHECK(data.lines[2].length==10 && !memcmp(data.lines[2].bytes,"last;field",10));
    Student_Free(&data);
    CHECK(Student_Load("lines.txt",1,&data)==0 && data.count==2 && data.payload==10010);
    Student_Free(&data); free(longline);
    puts("PASS: LF/CRLF, blank/header handling, 10000-byte line, final line without newline");
    return 0;
}
