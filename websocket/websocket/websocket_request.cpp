#include <cstdint>
#include <cstring>
#include "websocket_request.h"

Websocket_Request::Websocket_Request():
		fin_(),
		opcode_(),
		mask_(),
		masking_key_(),
		payload_length_(),
		payload_()
{
}

Websocket_Request::~Websocket_Request(){

}

int Websocket_Request::fetch_websocket_info(char *msg){
	int pos = 0;
	fetch_fin(msg, pos);
	fetch_opcode(msg, pos);
	fetch_mask(msg, pos);
	fetch_payload_length(msg, pos);
	fetch_masking_key(msg, pos);
	return fetch_payload(msg, pos);
}

void Websocket_Request::print(){
	DEBUG_LOG("WEBSOCKET PROTOCOL\n"
				"FIN: %d\n"
				"OPCODE: %d\n"
				"MASK: %d\n"
				"PAYLOADLEN: %d\n"
				"PAYLOAD: %s",
				fin_, opcode_, mask_, payload_length_, payload_);

	reset();
}

void Websocket_Request::reset(){
	fin_ = 0;
	opcode_ = 0;
	mask_ = 0;
	memset(masking_key_, 0, sizeof(masking_key_));
	payload_length_ = 0;
	memset(payload_, 0, sizeof(payload_));
}

int Websocket_Request::fetch_fin(char *msg, int &pos){
	fin_ = (unsigned char)msg[pos] >> 7;
	return 0;
}

int Websocket_Request::fetch_opcode(char *msg, int &pos){
	opcode_ = msg[pos] & 0x0f;
	pos++;
	return 0;
}

int Websocket_Request::fetch_mask(char *msg, int &pos){
	mask_ = (unsigned char)msg[pos] >> 7;
	return 0;
}

int Websocket_Request::fetch_masking_key(char *msg, int &pos){
	if(mask_ != 1)
		return 0;
	for(int i = 0; i < 4; i++)
		masking_key_[i] = msg[pos + i];
	pos += 4;
	return 0;
}

// 自定义 ntohll 函数
uint64_t ntohll(uint64_t value) {
    static const int num = 42;
    if (*reinterpret_cast<const char*>(&num) == num) {
        // 大端字节序，无需转换
        return value;
    } else {
        // 小端字节序，进行转换
        uint64_t swapped = ((value >> 56) & 0x00000000000000FF) |
                           ((value >> 40) & 0x000000000000FF00) |
                           ((value >> 24) & 0x0000000000FF0000) |
                           ((value >> 8)  & 0x00000000FF000000) |
                           ((value << 8)  & 0x000000FF00000000) |
                           ((value << 24) & 0x0000FF0000000000) |
                           ((value << 40) & 0x00FF000000000000) |
                           ((value << 56) & 0xFF00000000000000);
        return swapped;
    }
}

int Websocket_Request::fetch_payload_length(char *msg, int &pos){
    // 确保 msg 不为空指针
    if (msg == nullptr) {
        return -1;
    }
    // 先获取低 7 位作为初始的负载长度
    payload_length_ = static_cast<uint8_t>(msg[pos]) & 0x7f;
    pos++;

    if (payload_length_ == 126){
        uint16_t length = 0;
        memcpy(&length, msg + pos, 2);
        pos += 2;
        payload_length_ = ntohs(length);
    } else if (payload_length_ == 127){
        uint64_t length = 0;
        memcpy(&length, msg + pos, 8);
        pos += 8;
        payload_length_ = ntohll(length);
    }
    return 0;
}

int Websocket_Request::fetch_payload(char *msg, int &pos){
	memset(payload_, 0, sizeof(payload_));
	if(mask_ != 1){
		memcpy(payload_, msg + pos, payload_length_);
	}
	else {
		for(uint i = 0; i < payload_length_; i++){
			int j = i % 4;
			payload_[i] = msg[pos + i] ^ masking_key_[j];
		}
	}
	pos += payload_length_;
	return 0;
}
