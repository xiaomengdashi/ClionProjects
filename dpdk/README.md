DPDK ARP/NDP Handler
===================

这个项目提供了一个用于处理ARP（IPv4）和NDP（IPv6）请求的DPDK模块。

文件说明:
---------
- arp_handler.h: ARP/NDP处理模块的头文件
- arp_handler.c: ARP/NDP处理模块的实现
- libarp_handler.a: 编译后的静态库文件
- example.c: 使用ARP/NDP处理模块的示例程序
- example: 编译后的示例程序可执行文件
- CMakeLists.txt: CMake构建配置文件

编译:
-----
```bash
# 创建构建目录
mkdir build && cd build

# 配置项目
cmake ..

# 编译项目
make

# 或者一步完成
mkdir build && cd build && cmake .. && make
```

清理构建文件:
------------
```bash
# 在build目录中
make clean

# 或者删除整个build目录
cd .. && rm -rf build
```

使用方法:
--------
1. 确保系统已安装DPDK库
2. 编译模块和示例程序
3. 运行示例程序:
   ```bash
   sudo ./example -- -p 0x1
   ```
   其中 `-p 0x1` 指定要使用的端口掩码

功能:
-----
- 处理IPv4 ARP请求并生成ARP响应
- 处理IPv6邻居请求（NS）并生成邻居通告（NA）
- 支持DPDK高速数据包处理
- 可以集成到更大的DPDK应用程序中

注意事项:
--------
- 需要root权限运行DPDK应用程序
- 需要绑定网卡到DPDK驱动
- 示例程序假设使用端口0