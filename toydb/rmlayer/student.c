#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include "student.h"

void Student_Free(StudentData *data)
{
    size_t i;
    for(i=0;i<data->count;i++) free(data->lines[i].bytes);
    free(data->lines);
    memset(data,0,sizeof(*data));
}
int Student_Load(const char *path, int skip_header, StudentData *data)
{
    FILE *input;
    char *line=NULL,*copy;
    size_t capacity=0,used=0,length;
    ssize_t read;
    StudentLine *grown;
    memset(data,0,sizeof(*data));
    input=fopen(path,"rb");
    if(!input) { perror(path); return -1; }
    while((read=getline(&line,&capacity,input))>=0) {
        data->source_lines++;
        length=(size_t)read;
        if(length && line[length-1]=='\n') length--;
        if(length && line[length-1]=='\r') length--;
        if(!length) { data->blank_lines++; continue; }
        if(skip_header) { skip_header=0; continue; }
        if(data->count==used) {
            if(used>SIZE_MAX/2/sizeof(StudentLine)) goto failure;
            used=used ? used*2 : 128;
            grown=realloc(data->lines,used*sizeof(StudentLine));
            if(!grown) goto failure;
            data->lines=grown;
        }
        copy=malloc(length);
        if(!copy) goto failure;
        memcpy(copy,line,length);
        data->lines[data->count].bytes=copy;
        data->lines[data->count++].length=length;
        if(length>data->maximum) data->maximum=length;
        data->payload+=length;
    }
    if(ferror(input)) goto failure;
    free(line);
    if(fclose(input)) { Student_Free(data); return -1; }
    return 0;
failure:
    fprintf(stderr,"Cannot read complete dataset: %s\n",path);
    free(line); fclose(input); Student_Free(data); return -1;
}
