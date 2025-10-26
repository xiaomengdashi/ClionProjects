# PFCP 消息编码问题分析

## 问题诊断

根据对现有代码和消息日志的分析，发现了以下PFCP消息编码问题，导致Wireshark无法正确解析：

---

## 一、核心问题分析

### 问题 1：PFCP报头Flags字段编码错误

**当前代码（错误）：**
```c
// smf_pfcp_client.c 第 161-165 行
hdr->flags = 0x00;  /* Version 1, no SEID initially */
if (with_seid) {
    hdr->flags |= 0x80;  /* Set S bit */
}
hdr->flags |= 0x20;  /* Set Version = 1 */
```

**问题：**
- 当设置 S bit (0x80) 并且 Version=1 (0x20) 时，flags = 0xA0 (10100000)
- 但根据3GPP TS 29.244规范，Version应该编码在Bit 6-5中，值为"01"
- 所以Version 1应该是：0x20 (00100000)
- 现在的实现把S bit和Version混在一起，导致 0xA0 这样的值

**正确的编码应该是：**
```
S=1, Version=1:  0x20 | 0x80 = 0xA0 ✓ (这个实际上是对的)
S=0, Version=1:  0x20         = 0x20 ✓
```

等等，这个看起来是对的。但让我检查实际发送的消息...

### 问题 2：发送消息中的Flags值分析

**实际发送的消息：**
```
Request Hex: 20 32 00 24 00 00 00 00 00 3c 00 05 00 c0 a8 01...
             ^^
             Flags = 0x20
```

**问题识别：**
- 消息发送的 Flags = 0x20 (00100000)
- 这表示：S bit = 0 (无SEID)，Version = 1 (00100000)
- 但消息中确实包含了SEID字段！（字节4-11: 00 00 00 00 00 3c 00 05）
- 这是矛盾的！S bit 应该是 1

**根本原因：**
在 `pfcp_build_session_establishment_request()` 函数中（第194行）：
```c
pfcp_build_header(ptr, PFCP_SESSION_EST_REQ, 100, 0, 0);
                                                    ^
                                                    with_seid = 0 (错误!)
```

传入的 `with_seid = 0`，所以flags没有设置S bit，但消息体中却包含了SEID。

---

### 问题 3：Message Length字段编码错误

**当前消息：**
```
Bytes 2-3: 00 24 (网络字节序) = 0x0024 = 36 字节
```

**问题：**
- 按照3GPP规范，Message Length = 消息总长度 - 8字节（PFCP Header前4字节）
- 但如果包含SEID（8字节），消息结构应该是：
  - Byte 0: Flags (1字节)
  - Byte 1: Message Type (1字节)
  - Byte 2-3: Message Length (2字节)
  - [Subtotal: 4字节] ← Message Length的计算从这里开始
  - Byte 4-11: SEID (8字节) ← 如果S=1
  - Byte 12-14: Sequence Number (3字节)
  - Byte 15: Reserved (1字节)
  - Byte 16+: Information Elements

**当前代码的问题：**
```c
msg_length += 8;  // 添加报头
// ...
return msg_length + 8;  // 再加8字节 (重复计算!)
```

返回值是 `44字节` (36+8)，但Message Length字段设置为 `36`。
这导致了不匹配。

---

### 问题 4：Sequence Number编码方式错误

**当前代码（第174行）：**
```c
*seq_ptr = htonl(get_next_seq_number() << 8);  /* 3 字节序列号 */
```

**问题分析：**
- 序列号是3字节，应该直接编码在Byte 12-14
- 但使用 `htonl()` 会转换为4字节网络字节序
- `<< 8` 左移8位会把3字节序列号移到Byte 1-3位置
- 这导致Byte 15 (Reserved) 被序列号占用

**例如序列号 = 0x000001：**
- 左移8位：0x000100
- htonl() 转换：0x00010000 (网络字节序)
- 存储结果：00 01 00 00
- 应该是：00 00 01 00

---

### 问题 5：Information Elements 结构编码错误

**Node ID IE (type 60)：**
```c
struct pfcp_node_id_ie {
    uint16_t type;           // type = 60
    uint16_t length;         // length = 5
    uint8_t node_id_type;    // 0 = IPv4
    uint32_t ipv4_addr;      // IPv4地址
} __attribute__((packed));  // 总长度: 2+2+1+4 = 9 字节
```

**问题：**
- Length字段应该是 `5` (不包括Type和Length本身)
- 但实际 IE 内容是：node_id_type(1字节) + ipv4_addr(4字节) = 5字节 ✓
- 结构体本身是9字节，所以总IE长度 = 9字节 ✓

但在消息中的实际布局：
```
00 3c 00 05          // Type=60, Length=5 (网络字节序)
00 c0 a8 01 1e       // Node ID Type=0, IPv4=192.168.1.30
```

这里有问题：`c0 a8 01 1e` 是网络字节序的 192.168.1.30，但C结构体在小端系统上可能会反向存储！

---

### 问题 6：Create FAR IE 编码不符合3GPP规范

**当前代码：**
```c
struct pfcp_create_far_ie {
    uint16_t type;              // Type = 70
    uint16_t length;            // Length
    uint32_t far_id;            // FAR ID
    uint8_t far_action;         // Action
    uint32_t destination;       // Destination
    uint32_t outer_ip_addr;     // Outer IP
    uint16_t outer_udp_port;    // Port
};  // 总长度: 2+2+4+1+4+4+2 = 19字节
```

**问题：**
根据3GPP TS 29.244，Create FAR IE 的结构应该是：
- FAR ID (4字节) - 必须
- FAR Action Rule (1字节) - 必须
- Forwarding Parameters - 可选（嵌套的IEs）
  - Destination Interface (IE type 42)
  - Outer Header Creation (IE type 81, 嵌套IE结构)
  - 等等...

现在的代码试图把所有字段扁平化，这是**根本错误**。Create FAR IE 需要包含嵌套的Information Elements，而不是简单的字段。

---

## 二、Wireshark 无法解析的根本原因

1. **S bit 与实际SEID不匹配** → Wireshark无法确定报头长度
2. **Message Length计算错误** → Wireshark无法准确找到消息边界
3. **Sequence Number编码错误** → 字段对齐混乱
4. **Information Elements 嵌套结构未实现** → Wireshark无法识别IE结构
5. **字节序处理不一致** → 某些字段反向存储

---

## 三、正确的PFCP消息结构

### 3.1 Session Establishment Request（Message Type 50）

```
Offset  Length  Field               Value
0       1       Flags               0x20 (S=0, V=1) 或 0xA0 (S=1, V=1)
1       1       Message Type        50 (0x32)
2       2       Message Length      (不含前4字节)
----- 如果 S=0，无SEID，直接到序列号 -----
4       3       Sequence Number     0xABCDEF → 编码为 AB CD EF xx
7       1       Reserved            0x00
----- 如果 S=1，包含SEID -----
4       8       SEID                0x0000000012345678
12      3       Sequence Number     0xABCDEF
15      1       Reserved            0x00
-----
16+     ...     Information Elements (TLV格式)
```

### 3.2 Information Element (IE) 结构 - TLV 格式

```
Offset  Length  Field
0       2       IE Type             (网络字节序)
2       2       IE Length           (不含Type和Length的长度，网络字节序)
4       N       IE Value            (长度由Length指定)

如果IE Value中包含嵌套IEs，则Value部分继续包含TLV结构
```

### 3.3 正确的节点ID IE（Type 60）

```
Offset  Length  Field
0       2       Type                60 (0x003C)
2       2       Length              5 (只有Value部分)
4       1       Node ID Type        0=IPv4, 1=IPv6, 2=FQDN
5       4       IPv4 Address        (网络字节序)
Total: 9 字节
```

---

## 四、修复步骤

### Step 1: 修复PFCP报头编码

```c
void pfcp_build_header(uint8_t *buffer, uint8_t msg_type,
                       uint16_t msg_length, uint64_t seid, int with_seid) {
    uint8_t flags = 0x20;  // Version=1, S=0初始化

    if (with_seid) {
        flags |= 0x80;  // Set S bit if SEID present
    }

    buffer[0] = flags;
    buffer[1] = msg_type;
    *(uint16_t *)&buffer[2] = htons(msg_length);

    if (with_seid) {
        *(uint64_t *)&buffer[4] = htobe64(seid);
        uint32_t seq = get_next_seq_number();
        buffer[12] = (seq >> 16) & 0xFF;
        buffer[13] = (seq >> 8) & 0xFF;
        buffer[14] = seq & 0xFF;
        buffer[15] = 0x00;  // Reserved
    } else {
        uint32_t seq = get_next_seq_number();
        buffer[4] = (seq >> 16) & 0xFF;
        buffer[5] = (seq >> 8) & 0xFF;
        buffer[6] = seq & 0xFF;
        buffer[7] = 0x00;  // Reserved
    }
}
```

### Step 2: 正确处理Message Length

```c
// Message Length = 整个消息长度 - 4字节报头
// 如果有SEID: 消息长度 = 4 + 8(SEID) + 4(Seq+Res) + IE_length
// 如果无SEID: 消息长度 = 4 + 4(Seq+Res) + IE_length
```

### Step 3: 使用正确的IE结构

```c
// 使用TLV格式，而不是扁平化结构
typedef struct {
    uint16_t type;
    uint16_t length;
    uint8_t value[];  // 变长数据
} pfcp_ie_t;
```

### Step 4: 手动编码IE而不是使用结构体

```c
void add_node_id_ie(uint8_t **ptr, const char *ip_str) {
    uint32_t ip = inet_addr(ip_str);

    // Type
    *(uint16_t *)*ptr = htons(60);
    *ptr += 2;

    // Length
    *(uint16_t *)*ptr = htons(5);  // 1(type) + 4(ipv4)
    *ptr += 2;

    // Node ID Type
    **ptr = 0;  // IPv4
    *ptr += 1;

    // IPv4 Address
    *(uint32_t *)*ptr = ip;  // 已经是网络字节序
    *ptr += 4;
}
```

---

## 五、测试验证

### 5.1 生成标准PFCP消息

使用以下Python脚本生成标准PFCP消息：

```python
import struct
import socket

def create_pfcp_ser(seid=None):
    # Flags
    flags = 0x20  # Version=1
    if seid is not None:
        flags |= 0x80  # S bit

    msg_type = 50  # Session Establishment Request

    # 构造消息体（IE部分）
    body = bytearray()

    # Node ID IE (Type 60)
    ie_type = struct.pack('>H', 60)
    ie_length = struct.pack('>H', 5)
    ie_value = b'\x00' + socket.inet_aton('192.168.1.30')  # IPv4
    body += ie_type + ie_length + ie_value

    # Message Length = 4(头后) + 8(SEID) + 1(Seq Hi) + 1(Seq Mid) + 1(Seq Lo) + 1(Res) + len(body)
    if seid is not None:
        msg_length = 8 + 4 + len(body)  # SEID + Seq+Res + Body
    else:
        msg_length = 4 + len(body)  # Seq+Res + Body

    # 报头
    header = struct.pack('>BBH', flags, msg_type, msg_length)

    # SEID（如果有）
    if seid is not None:
        header += struct.pack('>Q', seid)

    # 序列号 (3字节) + Reserved (1字节)
    seq = 0x000001
    header += struct.pack('>I', seq << 8)  # 左移8位，然后作为4字节

    return header + body
```

### 5.2 使用Wireshark验证

启动Wireshark并设置PFCP协议过滤器：
```
udp.port == 8805
```

应该能看到正确解析的PFCP消息。

---

## 六、完整的编码规范清单

- [ ] Flags 字段正确编码（S bit + Version）
- [ ] Message Type 正确设置（50=Req, 51=Rsp）
- [ ] Message Length 正确计算（不含前4字节）
- [ ] SEID 与 S bit 一致
- [ ] Sequence Number 占用3字节
- [ ] Reserved 字段为0
- [ ] Information Elements 使用TLV格式
- [ ] IE Type 和 IE Length 使用网络字节序
- [ ] IE Value 长度与 IE Length 一致
- [ ] 所有数字字段使用网络字节序（大端）

---

## 七、参考资源

- 3GPP TS 29.244 - Interface Specification between CP and UP
- PFCP Header：https://datatracker.ietf.org/doc/html/draft-ietf-tsvwg-tunnel-encaps-06
- Wireshark PFCP Dissector：https://github.com/wireshark/wireshark/blob/master/epan/dissectors/packet-pfcp.c
