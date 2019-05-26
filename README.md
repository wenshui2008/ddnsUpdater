# dnsupdater
Aliyun dynamic domain name refresher

用于进行动态域名解析的实现

dnsupdate.php - 客户端程序
getip.php -用于获取公网IP的助手程序的php实现

getipd 目录下为 用于获取公网IP的助手程序 C实现。
PHP实现与C实现2选1即可，那个方便用那个。

C实现的编译：
Windows下用VC打开 .sln文件即可
Linux 下用如下命令编译：
gcc -O2 -o getipd getip.c 

