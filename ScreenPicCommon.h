#ifndef ERR_HANDLE_H
#define ERR_HANDLE_H
#include <stdio.h>
#define RETURN_FALSE_IF_FAILED(condition,errinfo) \
if(condition)\
{\
    printf("Error:%s\n",errinfo);\
    return false;\
}
#define GOTO_CLEAR_IF_FAILED(condition,errinfo)\
if (condition)\
{\
    printf("Error:%s\n", errinfo); \
    goto clear; \
}
#endif // !ERR_HANDLE_H
