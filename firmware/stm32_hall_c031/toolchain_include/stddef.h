#ifndef STM32_HALL_TOOLCHAIN_STDDEF_H
#define STM32_HALL_TOOLCHAIN_STDDEF_H

typedef __PTRDIFF_TYPE__ ptrdiff_t;
typedef __SIZE_TYPE__ size_t;
#ifndef __cplusplus
typedef __WCHAR_TYPE__ wchar_t;
#endif

#undef NULL
#ifndef __cplusplus
#define NULL ((void *)0)
#else
#define NULL 0
#endif

#define offsetof(type, member) __builtin_offsetof(type, member)

#endif
