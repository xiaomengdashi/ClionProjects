# c消息队列实现原理 c++ 消息队列
### 1、概述
- 消息队列实现的基本原理，其实还是最基础的锁、和信号量以及deque，其中deque用于存放消息内容，锁、和信号量做线程间同步;
- 消息队列有两种通信机制，一对多，或一对一
  - 消息队列可满足一个线程发送消息（发送时需要表明发给哪个线程），多个线程接收消息（多个线程之间为抢占模式），如确认消息是发给自己的则做相应的处理;
  - 一对一时不需要线程有自己的名字
- 消息队列具备阻塞和非阻塞两种模式
### 2、实现
- elements 消息队列的最小元素
  - dest_name 接收线程的名字，用于一对多通信
  - type 消息类别，在协议解析时用于区分消息处理方法
  - msg 消息主体
- 构造函数 msg_que(const char *p_name = NULL,int block = 1)
  - p_name 消息队列的名字
  - block 阻塞或非阻塞模式
- void send_msg(const char *dest_name, elements *msg);
  - 用于一对多线程消息发送，dest_name指定接收线程的名字
- bool recv_msg(char *this_name, elements *msg);
  - 用于一对多线程消息接收,与本线程name匹配后确认该消息是发给自己的
- void send_msg(elements *msg);
  - 用于一对一线程消息发送，不必指定接收线程的名字
- bool recv_msg(elements *msg);
  - 用于一对一线程消息接收