# dnsupdater
Aliyun dynamic domain name refresher,with getipd server side source code.

用于支持公网IP地址为动态分配地址，域名固定，IP地址可以不固定，通过固定域名访问资源的解决方案。

用于阿里云的动态域名解析完整方案，包含域名刷新客户端，帮助获取公网IP的getipd服务器端（用Ｃ与ＰＨＰ两种语言进行了实现）源代码。
基本上无需任何修改，即可用于实际工作当中。

### 用于进行动态域名解析的实现<br/>

dnsupdate.php - 客户端程序,放在动态IP地址的机器上或者经由动态IP地址的路由器内部计算器上。执行：<br/>
php dnsupdate.php<br/>
如果要隐藏控制台，请运行 程序 <br/>startdnsupdater.bat<br/>
<b>注意：</b> 需要正确安装PHP解释器

getip.php -用于获取公网IP的助手程序的php实现，放在公网上其它任何一台服务器上，帮助反馈动态IP地址<br/>
<br/>
getipd 目录下为 用于获取公网IP的助手程序 C实现。编译后放在公网上一台服务器上。<br/>
PHP实现与C实现2选1即可，那个方便用那个。<br/>

## getipd程序C语言版本实现的编译：<br/>
<b>Windows下用VC打开 .sln文件即可</b><br/>
<br/>
<b>Linux 下用如下命令编译</b>：<br/>
gcc -O2 -o getipd getip.c 

getipd　在linux下可以作为守护进程运行，用命令<br/>
./getipd -d 

可以参考这里：<br/>
https://blog.csdn.net/ababab12345/article/details/90579601

