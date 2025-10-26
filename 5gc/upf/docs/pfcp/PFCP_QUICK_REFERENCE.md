# PFCP 编码快速参考指南

## 标准 PFCP 报头格式 (RFC 8805 / 3GPP TS 29.244)

### 无 SEID 的报头 (8 字节)

```
Offset  Length  字段        值范围
------  ------  -----       --------
0       1       Flags       0x20 (S=0, V=1)
1       1       Message     0-255
                Type
2       2       Message     网络字节序
                Length
4       3       Sequence    0x000000-0xFFFFFF
                Number
7       1       Reserved    0x00
```

### 有 SEID 的报头 (16 字节)

```
Offset  Length  字段        值范围
------  ------  -----       --------
0       1       Flags       0xA0 (S=1, V=1)
1       1       Message     0-255
                Type
2       2       Message     网络字节序
                Length
4       8       SEID        64-bit 网络字节序
12      3       Sequence    0x000000-0xFFFFFF
                Number
15      1       Reserved    0x00
```

---

## PFCP 消息类型

```
Type    名称                                    方向
----    -----                                   ----
50      Session Establishment Request           SMF → UPF
51      Session Establishment Response          UPF → SMF
52      Session Modification Request            SMF → UPF
53      Session Modification Response           UPF → SMF
54      Session Deletion Request                SMF → UPF
55      Session Deletion Response               UPF → SMF
56      Session Report Request                  UPF → SMF
57      Session Report Response                 SMF → UPF
```

---

## 关键 Information Elements (IE) 类型

```
Type    名称                                    长度
----    -----                                   ----
5       Cause                                   1字节
13      Source Interface                        1字节+
42      Destination Interface                   1字节+
56      Create PDR                              可变
60      Node ID                                 5-?字节
70      Create FAR                              可变
81      Forwarding Parameters                   可变
87      Create QER                              可变
```

---

## Information Element (IE) 结构

### TLV 格式 (标准)

```
Offset  Length  字段            说明
------  ------  -----           ----
0       2       Type            网络字节序, 0-65535
2       2       Length          网络字节序, 不含Type和Length
4       N       Value           长度由Length指定
```

### 示例: Node ID IE (Type 60)

```
十六进制:  00 3c 00 05 00 c0 a8 01 1e
          ^^^^^ ^^^^^ ^^ ^^^^^^^^^^^^
          Type  Len   ID  IPv4地址

解析:
  Type = 0x003C = 60
  Length = 0x0005 = 5字节
  ID Type = 0x00 (IPv4)
  IPv4 = 0xC0A8011E = 192.168.1.30
```

---

## Flags 字段编码

### Bit 7: S (SEID Present Bit)

```
值  含义
--  ----
0   无SEID，报头8字节
1   有SEID，报头16字节
```

### Bits 6-5: Version

```
值  含义
--  ----
00  保留
01  PFCP版本1（当前标准）
10  保留
11  保留
```

### Bits 4-0: Reserved

```
必须为全0
```

### 标准 Flags 值

```
0x20 = 00100000 = S=0, Version=1 (无SEID)
0xA0 = 10100000 = S=1, Version=1 (有SEID)
```

---

## Message Length 计算规则

### 关键点

1. **Message Length** = 消息总长 - 4字节（前4字节固定报头）
2. **Message Length** 不包括 Flags、Message Type、Message Length 这3个字段
3. **Message Length** 包括：SEID（如果有）、Sequence Number、Reserved、所有 IE

### 计算示例

```
无SEID消息:
  固定报头 = 8字节 (Flags + Type + Length + Seq + Res)
  IE部分 = 9字节 (例如Node ID IE)

  Message Length = 8 - 4 + 9 = 13字节
  总消息 = 4 + 13 = 17字节

有SEID消息:
  固定报头 = 16字节 (Flags + Type + Length + SEID + Seq + Res)
  IE部分 = 9字节 (例如Node ID IE)

  Message Length = 16 - 4 + 9 = 21字节
  总消息 = 4 + 21 = 25字节
```

---

## 字节序处理

### 网络字节序 (Big Endian)

所有多字节字段必须使用网络字节序。

```c
// C语言字节序转换函数

htons(x)   // 16位主机→网络
ntohs(x)   // 16位网络→主机
htonl(x)   // 32位主机→网络
ntohl(x)   // 32位网络→主机
htobe64(x) // 64位主机→网络(大端)
be64toh(x) // 64位网络→主机
```

### 示例

```c
// 错误: 直接赋值
buffer[4] = seid;  // SEID是64位，不能直接赋值

// 正确: 使用字节序转换
uint64_t net_seid = htobe64(seid);
memcpy(&buffer[4], &net_seid, 8);

// 或逐字节赋值
buffer[4] = (seid >> 56) & 0xFF;
buffer[5] = (seid >> 48) & 0xFF;
// ... 继续 ...
buffer[11] = seid & 0xFF;
```

---

## Wireshark 验证清单

捕获 PFCP 消息后，在 Wireshark 中检查：

### 报头正确性

- [ ] Protocol 显示为 "PFCP"
- [ ] S bit 正确 (0 or 1)
- [ ] Version 显示为 1
- [ ] Message Type 正确
- [ ] Message Length 与实际一致
- [ ] SEID 存在状态与 S bit 匹配

### Information Elements

- [ ] IE Type 在有效范围内 (0-255)
- [ ] IE Length 不为0
- [ ] IE Value 长度与 IE Length 匹配
- [ ] 嵌套 IE 格式正确

### 无警告

- [ ] 无 "Malformed Packet"
- [ ] 无 "Reserved field not zero"
- [ ] 无 "Invalid message length"

---

## 常见错误

### 错误 1: Flags = 0x20 但消息包含 SEID

```
✗ 错误消息:
  20 32 00 24 10 00 00 00 00 00 00 01 ...
  Flags说 S=0，但后面有SEID！

✓ 正确:
  a0 32 00 15 10 00 00 00 00 00 00 01 ...
  Flags说 S=1，SEID存在，一致！
```

### 错误 2: Message Length 不匹配

```
✗ 错误:
  Message Length = 36
  但实际消息 = 44字节
  不匹配！

✓ 正确:
  Message Length = 21 (不含前4字节)
  总消息 = 4 + 21 = 25字节
  一致！
```

### 错误 3: Sequence Number 污染 Reserved 字段

```
✗ 错误 (使用htonl):
  00 01 00 00  (第4字节非零)

✓ 正确 (逐字节):
  00 00 01 00  (第4字节为0)
```

### 错误 4: IE Type 和 Length 不使用网络字节序

```
✗ 错误 (主机字节序):
  3c 00 05 00 ...  (Type=60错了)

✓ 正确 (网络字节序):
  00 3c 00 05 ...  (Type=60正确)
```

---

## C 代码模板

### 构造报头 (有 SEID)

```c
void build_pfcp_header_with_seid(uint8_t *buf, uint8_t msg_type,
                                 uint16_t msg_len, uint64_t seid,
                                 uint32_t seq_num) {
    // Flags: S=1, Version=1
    buf[0] = 0xA0;

    // Message Type
    buf[1] = msg_type;

    // Message Length (网络字节序)
    *(uint16_t *)&buf[2] = htons(msg_len);

    // SEID (网络字节序)
    *(uint64_t *)&buf[4] = htobe64(seid);

    // Sequence Number (3字节)
    buf[12] = (seq_num >> 16) & 0xFF;
    buf[13] = (seq_num >> 8) & 0xFF;
    buf[14] = seq_num & 0xFF;

    // Reserved
    buf[15] = 0x00;
}
```

### 添加 IE

```c
int add_ie(uint8_t *buf, uint16_t type,
           const uint8_t *value, uint16_t len) {
    // Type (网络字节序)
    *(uint16_t *)buf = htons(type);
    buf += 2;

    // Length (网络字节序)
    *(uint16_t *)buf = htons(len);
    buf += 2;

    // Value
    memcpy(buf, value, len);

    return 4 + len;  // 返回总长度
}
```

---

## 测试消息 (十六进制)

### 最小 Session Establishment Request (只有Node ID IE)

```
a0 32 00 15          Flags=0xA0, Type=50, Length=21
10 00 00 00          SEID (高32位)
00 00 00 01          SEID (低32位) = 0x1000000000000001
00 00 01 00          Seq=1, Reserved=0
00 3c 00 05          IE Type=60, Length=5
00 c0 a8 01 1e       IE Value (IPv4: 192.168.1.30)

总长度: 25字节
```

---

## 调试技巧

### 打印十六进制

```c
void print_hex(const uint8_t *data, int len) {
    for (int i = 0; i < len; i++) {
        printf("%02x ", data[i]);
        if ((i+1) % 16 == 0) printf("\n");
    }
    printf("\n");
}
```

### Wireshark 命令行

```bash
# 捕获到文件
tshark -i lo -d udp.port==8805,pfcp -w pfcp.pcap

# 读取文件
tshark -r pfcp.pcap -d udp.port==8805,pfcp

# 详细输出
tshark -r pfcp.pcap -d udp.port==8805,pfcp -V
```

---

## 参考链接

- 3GPP TS 29.244: https://www.3gpp.org/ftp/Specs/archive/29_series/29.244
- RFC 8805 (早期版本): https://tools.ietf.org/html/rfc8805
- Wireshark Wiki: https://wiki.wireshark.org/PFCP
