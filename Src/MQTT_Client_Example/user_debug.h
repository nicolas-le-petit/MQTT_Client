/*
@Date  : 8/11/2021
@Author: Tan Dung, Tran
@Brief : debug utils
 */


#ifndef _USER_DEBUG_
#define _USER_DEBUG_

#ifdef __cplusplus
extern "C" {
#endif

// #define __DEBUG__ 4

void user_debug_init(void);
void user_debug_print(int level, const char* module, int line, const char* fmt, ...);
void user_debug_print_error(const char* module, int line, int ret);
void user_debug_print_exact(const char* fmt, ...);

// test only
void __printf(const char *format, ...);

#if __DEBUG__ > 0
#ifndef __MODULE__
#error "__MODULE__ must be defined"
#endif
#endif

#if __DEBUG__ >= 1
#define ERR(...) do{ user_debug_print(1, __MODULE__, __LINE__, __VA_ARGS__); }while(0)
#define error(...) do{ user_debug_print(1, __MODULE__, __LINE__, __VA_ARGS__); }while(0)
#else
#define ERR(...) do{ }while(0)
#define error(...) do{ }while(0)
#endif

#if __DEBUG__ >= 2
#define WARN(...) do{ user_debug_print(2, __MODULE__, __LINE__, __VA_ARGS__); }while(0)
#define warn(...) do{ user_debug_print(2, __MODULE__, __LINE__, __VA_ARGS__); }while(0)
#else
#define WARN(...) do{ }while(0)
#define warn(...) do{ }while(0)
#endif

#if __DEBUG__ >= 3
#define INFO(...) do{ user_debug_print(3, __MODULE__, __LINE__, __VA_ARGS__); }while(0)
#define info(...) do{ user_debug_print(3, __MODULE__, __LINE__, __VA_ARGS__); }while(0)
#define LOG(...) do{ user_debug_print(3, __MODULE__, __LINE__, __VA_ARGS__); }while(0)
#define log(...) do{ user_debug_print(3, __MODULE__, __LINE__, __VA_ARGS__); }while(0)
#define CHECK(ret) do{ if(ret){ user_debug_print_error(__MODULE__, __LINE__, ret); } }while(0)
#define check(ret) do{ if(ret){ user_debug_print_error(__MODULE__, __LINE__, ret); } }while(0)
#else
#define INFO(...) do{ }while(0)
#define info(...) do{ }while(0)
#define LOG(...) do{ }while(0)
#define log(...) do{ }while(0)
#define CHECK(ret) do{ }while(0)
#define check(ret) do{ }while(0)
#endif

#if __DEBUG__ >= 4
#define DBG(...) do{ user_debug_print(4, __MODULE__, __LINE__, __VA_ARGS__); }while(0)
#define DBGX(...) do{ user_debug_print_exact(__VA_ARGS__); }while(0)
#define dbg(...) do{ user_debug_print(4, __MODULE__, __LINE__, __VA_ARGS__); }while(0)
#define dbgx(...) do{ user_debug_print_exact(__VA_ARGS__); }while(0)
#define debug(...) do{ user_debug_print(4, __MODULE__, __LINE__, __VA_ARGS__); }while(0)
#define debugx(...) do{ user_debug_print_exact(__VA_ARGS__); }while(0)
#define debug_strn(x, n) do {user_debug_print(4, __MODULE__, __LINE__, ""); \
							for (int i = 0; i < n; ++i) \
								if (x[i] != 0) debugx("%c", x[i]); \
								else break; \
						} while(0)
#else
#define DBG(...) do{ }while(0)
#define DBGX(...) do{ }while(0)
#define dbg(...) do{ }while(0)
#define dbgx(...) do{ }while(0)
#define debug(...) do{ }while(0)
#define debugx(...) do{ }while(0)
#define debug_strn(x, n)
#endif

#ifdef __cplusplus
}
#endif
 
#endif /* _USER_DEBUG_ */

