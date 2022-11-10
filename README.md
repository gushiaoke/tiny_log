## 一个只有头文件的日志库

日志写到%temp%\tiny_log目录下,文件名格式 

```
进程名-pid.log
```



依赖c++17,增加宏定义ENABLE_TINY_LOG

vs2019编译

功能:

- 支持日志级别

- 支持显示模块/源文件/行号/函数名
- exe和dll输出到同一个日志文件中
- 支持函数入口出口自动打印

```c++
#include <iostream>
#include "TinyLog.h"

int main()
{
    TINY_LOG_FUNC_RECORD();
    TINY_LOG_INFO(L"main");
    std::cout << "Hello World!\n";
}
```



```
[2022-11-10 19:41:37.380][TRACE][tiny_log!main(tiny_log.cpp:9)]enter
[2022-11-10 19:41:37.380][INFO][tiny_log!main(tiny_log.cpp:10)]main
[2022-11-10 19:41:37.380][TRACE][tiny_log!main(tiny_log.cpp:9)]leave
```

