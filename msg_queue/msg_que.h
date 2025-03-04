//
// Created by Kolane Steve on 25-3-4.
//

#ifndef MSG_QUE_H
#define MSG_QUE_H

/***************************** Include Files *********************************/
#include <deque>
#include <mutex>
#include <semaphore.h>
#include <iostream>
#include <thread>
#include "string.h"
#include <unistd.h>


/**************************** Type Definitions *******************************/
using namespace std;

typedef struct {
    char dest_name[10];
    uint8_t type;
    uint8_t msg[256];
} elements;

class msg_que {
public:
  /********************************************
  * Func: 创建指令队列
  * @param : p_name-> 队列名称
  *          block -> 是否为阻塞式
  */
  msg_que(const char* p_name = nullptr, int block = 1);
  ~msg_que();

  /********************************************
  * Func: 获取指令队列的名字
  */
  const char* get_name() const {
      return m_name;
  }

  /********************************************
  * Func: 用于一对多线程消息接收,与本线程name匹配后认为
  *       该消息是发给自己的
  * @param : this_name  -> 接收线程的名字
  *          msg -> 将消息发送至*msg指向的elements
  * @return : ture->接收成功，false->为接收到消息
  */
  bool recv_msg(const char* this_name, elements* msg);

  /********************************************
  * Func: 用于一对多线程消息发送
  * @param : dest_name  -> 接收线程的名字
  *          msg        -> 指向要发送的消息
  */
  void send_msg(const char* dest_name, elements* msg);

  /********************************************
  * Func: 用于一对一线程消息接收
  * @param  : msg -> 将消息发送至*msg指向的elements;
  * @return : ture-> 接收成功，false->为接收到消息
  */
  bool recv_msg(elements* msg);

  /********************************************
  * Func: 用于一对一线程消息发送
  * @param : msg -> 发送*msg指向(elements)的消息
  */
  void send_msg(elements* msg);
private:
  deque<elements> m_queue;
  char* m_name;
  static sem_t m_sem;
  static mutex m_mutex;
  int m_block;
};



#endif //MSG_QUE_H
