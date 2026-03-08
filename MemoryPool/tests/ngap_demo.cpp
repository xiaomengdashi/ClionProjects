#include "../core/IndexedMemoryPool.hpp"
#include <iostream>
#include <vector>
#include <cstring>
#include <iomanip>

// NGAP message size categories
const size_t NGAP_TINY = 64;
const size_t NGAP_SMALL = 256;
const size_t NGAP_MEDIUM = 1024;
const size_t NGAP_LARGE = 4096;
const size_t NGAP_HUGE = 16384;

// Basic NGAP TLV structure
struct NgapTLV {
    uint16_t type;     // Type field
    uint16_t length;   // Length field  
    uint8_t value[];   // Value field (variable length)
} __attribute__((packed));

// NGAP list structure (count + array)
template<typename T>
struct NgapList {
    uint32_t count;    // Number of elements
    T items[];         // Variable length array
} __attribute__((packed));

// Example NGAP IE types (simplified)
enum NgapIEType : uint16_t {
    IE_AMF_UE_NGAP_ID = 10,
    IE_RAN_UE_NGAP_ID = 85,
    IE_PDU_SESSION_RESOURCE_SETUP_LIST = 74,
    IE_UE_AGGREGATE_MAXIMUM_BIT_RATE = 110,
    IE_GUAMI = 32,
    IE_UE_SECURITY_CAPABILITIES = 103
};

// Example PDU Session Resource structure
struct PduSessionResource {
    uint8_t pdu_session_id;
    uint32_t qos_flow_id;
    uint16_t tunnel_endpoint_id;
    uint8_t ip_address[16];  // IPv6 address
} __attribute__((packed));

// NGAP Memory Pool specialized for NGAP messages
class NgapMemoryPool {
private:
    IndexedMemoryPool m_pool;
    size_t m_active_messages = 0;

public:
    explicit NgapMemoryPool(size_t total_size = 10 * 1024 * 1024) 
        : m_pool(total_size) {}

    std::pair<void*, size_t> allocate_message(size_t estimated_size) {
        void* ptr = m_pool.allocate(estimated_size);
        if (ptr) {
            ++m_active_messages;
        }
        return {ptr, estimated_size};
    }

    void deallocate_message(void* ptr) {
        if (ptr) {
            m_pool.deallocate(ptr);
            --m_active_messages;
        }
    }

    size_t get_active_messages() const { return m_active_messages; }
    PoolStatistics get_statistics() const { return m_pool.get_statistics(); }
};

// NGAP Message Builder
class NgapMessageBuilder {
private:
    NgapMemoryPool& m_pool;
    uint8_t* m_buffer;
    size_t m_buffer_size;
    size_t m_current_offset;
    bool m_valid;
    bool m_finalized = false;

    static size_t align_size(size_t size, size_t alignment = 8) {
        return (size + alignment - 1) & ~(alignment - 1);
    }

public:
    NgapMessageBuilder(NgapMemoryPool& pool, size_t buffer_size) 
        : m_pool(pool), m_current_offset(0) {
        auto [ptr, size] = m_pool.allocate_message(buffer_size);
        m_buffer = static_cast<uint8_t*>(ptr);
        m_buffer_size = size;
        m_valid = (ptr != nullptr);
    }
    
    ~NgapMessageBuilder() {
        if (m_buffer && !m_finalized) {
            m_pool.deallocate_message(m_buffer);
        }
    }

    // Add TLV Information Element
    NgapMessageBuilder& add_ie_tlv(NgapIEType type, const void* data, size_t data_size) {
        if (!m_valid) return *this;
        
        size_t tlv_size = sizeof(NgapTLV) + data_size;
        tlv_size = align_size(tlv_size);
        
        if (m_current_offset + tlv_size > m_buffer_size) {
            m_valid = false;
            return *this;
        }
        
        NgapTLV* tlv = reinterpret_cast<NgapTLV*>(m_buffer + m_current_offset);
        tlv->type = static_cast<uint16_t>(type);
        tlv->length = static_cast<uint16_t>(data_size);
        std::memcpy(tlv->value, data, data_size);
        
        m_current_offset += tlv_size;
        return *this;
    }

    // Add list of PDU Session Resources
    NgapMessageBuilder& add_pdu_session_list(const std::vector<PduSessionResource>& sessions) {
        if (!m_valid) return *this;
        
        size_t list_size = sizeof(NgapList<PduSessionResource>) + 
                          sessions.size() * sizeof(PduSessionResource);
        list_size = align_size(list_size);
        
        if (m_current_offset + list_size > m_buffer_size) {
            m_valid = false;
            return *this;
        }
        
        NgapList<PduSessionResource>* list = 
            reinterpret_cast<NgapList<PduSessionResource>*>(m_buffer + m_current_offset);
        list->count = static_cast<uint32_t>(sessions.size());
        
        for (size_t i = 0; i < sessions.size(); ++i) {
            list->items[i] = sessions[i];
        }
        
        m_current_offset += list_size;
        return *this;
    }

    // Add UE IDs
    NgapMessageBuilder& add_ue_ids(uint64_t amf_ue_id, uint64_t ran_ue_id) {
        return add_ie_tlv(IE_AMF_UE_NGAP_ID, &amf_ue_id, sizeof(amf_ue_id))
               .add_ie_tlv(IE_RAN_UE_NGAP_ID, &ran_ue_id, sizeof(ran_ue_id));
    }

    // Add GUAMI (Globally Unique AMF Identifier)
    NgapMessageBuilder& add_guami(const uint8_t guami[16]) {
        return add_ie_tlv(IE_GUAMI, guami, 16);
    }

    // Get current construction status
    bool is_valid() const { return m_valid; }
    size_t get_used_size() const { return m_current_offset; }
    size_t get_remaining_space() const { 
        return m_valid ? (m_buffer_size - m_current_offset) : 0; 
    }

    // Finalize message construction
    std::pair<void*, size_t> finalize() {
        if (!m_valid) {
            return {nullptr, 0};
        }
        
        m_finalized = true;
        return {m_buffer, m_current_offset};
    }
};

// Demo functions
void demo_initial_context_setup_request() {
    std::cout << "=== NGAP Initial Context Setup Request Demo ===\n";
    
    NgapMemoryPool pool;
    NgapMessageBuilder builder(pool, NGAP_LARGE);
    
    // UE identifiers
    uint64_t amf_ue_id = 0x123456789ABCDEF0;
    uint64_t ran_ue_id = 0x0FEDCBA987654321;
    
    // GUAMI
    uint8_t guami[16] = {0x46, 0x00, 0x07, 0x00, 0x00, 0x01, 0x02, 0x03,
                         0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B};
    
    // PDU Session Resources
    std::vector<PduSessionResource> pdu_sessions;
    for (int i = 0; i < 3; ++i) {
        PduSessionResource session{};
        session.pdu_session_id = static_cast<uint8_t>(i + 1);
        session.qos_flow_id = static_cast<uint32_t>(100 + i);
        session.tunnel_endpoint_id = static_cast<uint16_t>(5000 + i);
        // Fill IPv6 address
        for (int j = 0; j < 16; ++j) {
            session.ip_address[j] = static_cast<uint8_t>(0x20 + i + j);
        }
        pdu_sessions.push_back(session);
    }
    
    // Build message using fluent interface
    auto [msg_ptr, msg_size] = builder
        .add_ue_ids(amf_ue_id, ran_ue_id)
        .add_guami(guami)
        .add_pdu_session_list(pdu_sessions)
        .finalize();
    
    if (msg_ptr) {
        std::cout << "Message built successfully!\n";
        std::cout << "Message size: " << msg_size << " bytes\n";
        std::cout << "Used buffer: " << std::hex;
        uint8_t* bytes = static_cast<uint8_t*>(msg_ptr);
        for (size_t i = 0; i < std::min(msg_size, static_cast<size_t>(64)); ++i) {
            std::cout << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[i]) << " ";
            if ((i + 1) % 16 == 0) std::cout << "\n";
        }
        std::cout << std::dec << "\n\n";
        
        pool.deallocate_message(msg_ptr);
    } else {
        std::cout << "Failed to build message!\n\n";
    }
}

void demo_handover_request() {
    std::cout << "=== NGAP Handover Request Demo ===\n";
    
    NgapMemoryPool pool;
    NgapMessageBuilder builder(pool, NGAP_MEDIUM);
    
    // Simplified handover context
    uint64_t amf_ue_id = 0xFEDCBA9876543210;
    uint64_t source_ran_ue_id = 0x1111222233334444;
    uint8_t target_guami[16] = {0x46, 0x00, 0x08, 0x00, 0x00, 0x02, 0x03, 0x04,
                                0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C};
    
    // Single PDU session for handover
    std::vector<PduSessionResource> handover_sessions;
    PduSessionResource session{};
    session.pdu_session_id = 5;
    session.qos_flow_id = 200;
    session.tunnel_endpoint_id = 6000;
    for (int i = 0; i < 16; ++i) {
        session.ip_address[i] = static_cast<uint8_t>(0x30 + i);
    }
    handover_sessions.push_back(session);
    
    auto [msg_ptr, msg_size] = builder
        .add_ue_ids(amf_ue_id, source_ran_ue_id)
        .add_guami(target_guami)
        .add_pdu_session_list(handover_sessions)
        .finalize();
    
    if (msg_ptr) {
        std::cout << "Handover Request built successfully!\n";
        std::cout << "Message size: " << msg_size << " bytes\n\n";
        
        pool.deallocate_message(msg_ptr);
    } else {
        std::cout << "Failed to build Handover Request!\n\n";
    }
}

void demo_memory_efficiency() {
    std::cout << "=== Memory Pool Efficiency Demo ===\n";
    
    NgapMemoryPool pool;
    std::vector<void*> messages;
    
    // Simulate message processing workload
    for (int i = 0; i < 100; ++i) {
        size_t msg_size = NGAP_SMALL;
        if (i % 10 == 0) msg_size = NGAP_LARGE;  // 10% large messages
        if (i % 20 == 0) msg_size = NGAP_HUGE;   // 5% huge messages
        
        NgapMessageBuilder builder(pool, msg_size);
        
        // Add some dummy data
        uint64_t dummy_ids[2] = {static_cast<uint64_t>(i), static_cast<uint64_t>(i * 2)};
        uint8_t dummy_guami[16];
        for (int j = 0; j < 16; ++j) {
            dummy_guami[j] = static_cast<uint8_t>(i + j);
        }
        
        auto [msg_ptr, msg_len] = builder
            .add_ue_ids(dummy_ids[0], dummy_ids[1])
            .add_guami(dummy_guami)
            .finalize();
        
        if (msg_ptr) {
            messages.push_back(msg_ptr);
        }
    }
    
    auto stats = pool.get_statistics();
    std::cout << "Created " << messages.size() << " messages\n";
    std::cout << "Active messages: " << pool.get_active_messages() << "\n";
    std::cout << "Memory usage: " << stats.current_bytes_used << " bytes\n";
    std::cout << "Peak memory: " << stats.peak_bytes_used << " bytes\n";
    std::cout << "Fragmentation: " << std::fixed << std::setprecision(2) 
              << (stats.fragmentation_ratio * 100) << "%\n\n";
    
    // Clean up
    for (void* msg : messages) {
        pool.deallocate_message(msg);
    }
    
    auto final_stats = pool.get_statistics();
    std::cout << "After cleanup - Active messages: " << pool.get_active_messages() << "\n";
    std::cout << "Memory usage: " << final_stats.current_bytes_used << " bytes\n\n";
}

int main() {
    std::cout << "NGAP Message Memory Pool Demo\n";
    std::cout << "=============================\n\n";
    
    try {
        demo_initial_context_setup_request();
        demo_handover_request(); 
        demo_memory_efficiency();
        
        std::cout << "All NGAP demos completed successfully!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error during demo: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}