# dnsupdater
Aliyun dynamic domain name refresher,with getipd server side source code.

用于阿里云的动态域名解析完整方案，包含域名刷新客户端，getip服务器端（用Ｃ与ＰＨＰ两种语言进行了实现）源代码。
基本上无需任何修改，即可用于实际工作当中。

用于进行动态域名解析的实现<br/>

dnsupdate.php - 客户端程序<br/>
getip.php -用于获取公网IP的助手程序的php实现<br/>

getipd 目录下为 用于获取公网IP的助手程序 C实现。<br/>
PHP实现与C实现2选1即可，那个方便用那个。<br/>

C实现的编译：<br/>
Windows下用VC打开 .sln文件即可<br/>
Linux 下用如下命令编译：<br/>
gcc -O2 -o getipd getip.c 

可以参考这里：<br/>
https://blog.csdn.net/ababab12345/article/details/90579601

