# linux-6.0-fmsh
Upgrade the fmsh bsp sdk kernel from linux-4.14.55 to linux-6.0

客户需求：
复旦微 FMQL15 的最新 BSP SDK 包 FMQL-Linux-SDK-Prj-20240923, 自带的linux kernel 版本为 4.14.55,  客户目前其他项目使用的 kernel 版本为 6.x 较新的内核版本, 需要升级当前 SDK 中的内核版本。

移植工作：
1.	在保持目前 SDK 其他部分代码不变的情况下，只更新 kernel 版本， u-boot/buildroot/gcc 等不改动;
2.	更新 kernel 大致需要的改动部分，约100 个文件内容差异;
3.	需要新增改动约 116 个文件;
4.	保证内核能够加载启动的必要文件移植;
5.	Arch/arm  目录移植;
6.	Include 目录移植，解决编译出错问题;
7.	Driver/clk 目录移植;
8.	Driver/tty 目录移植;
9.	目前状况，内核6.0 移植以上必要文件后，可以在原有 Release SDK版本上加载启动：
![image](https://github.com/user-attachments/assets/1fe288ed-5a10-42a7-9421-0f9c87891610)
![image](https://github.com/user-attachments/assets/dfb14553-4749-4bf7-be74-38591a8e1d67)
10.	陆续将驱动部分移植并测试验证功能;
