#ifndef TOYDB_STUDENT_H
#define TOYDB_STUDENT_H
#include <stddef.h>
typedef struct StudentLine { char *bytes; size_t length; } StudentLine;
typedef struct StudentData {
    StudentLine *lines;
    size_t count, maximum, source_lines, blank_lines;
    unsigned long long payload;
} StudentData;
int Student_Load(const char *path, int skip_header, StudentData *data);
void Student_Free(StudentData *data);
#endif
