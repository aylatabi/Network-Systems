
#include <vmlinux.h>
#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>


#define TC_ACT_OK 0
#define ETH_P_IP 0x0800 /* Internet Protocol packet */
#define MAX_PAYLOAD_LENGTH 64

struct{
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, __u32);
} secret_key SEC(".maps");

struct{
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 2);
    __type(key, __u32);
    __type(value, __u32);
} data_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY); 
    __uint(max_entries, 1);          
    __type(key, int);
    __type(value, int);
} private_numbers_init SEC(".maps");

SEC("tc")
int private_numbers(struct __sk_buff *ctx)
{
    int key = 0; 
    int *flag;
    int init_done = 1;
    flag = bpf_map_lookup_elem(&private_numbers_init, &key);
    if (flag && *flag == 1) {
        return TC_ACT_OK; 
    }
    __u32 alice_key = bpf_htonl(0x0AE04C6E); //The ip of alice on my vm is 10.224.76.110 ->convert this to HEX 
    __u32 bob_key = bpf_htonl(0x0AE04CA7);

    __u32 alice_private_num = bpf_get_prandom_u32() % 7; 
    __u32 bob_private_num = bpf_get_prandom_u32() % 7;

    bpf_map_update_elem(&data_map, &alice_key, &alice_private_num, BPF_ANY);
    bpf_map_update_elem(&data_map, &bob_key, &bob_private_num, BPF_ANY);
    bpf_map_update_elem(&private_numbers_init, &key, &init_done, BPF_ANY);
    return TC_ACT_OK;
}

/// @tchook {"ifindex":2, "attach_point":"BPF_TC_INGRESS"}
/// @tcopts {"handle":1, "priority":1}
SEC("tc")
int tc_ingress(struct __sk_buff *ctx) //vm2 to vm1
{
    void *data_end = (void *)(__u64)ctx->data_end;
    void *data = (void *)(__u64)ctx->data;
    struct ethhdr *l2;
    struct iphdr *l3;
    struct udphdr *l4;
    __u8 *payload;

    if (ctx->protocol != bpf_htons(ETH_P_IP))
        return TC_ACT_OK;

    l2 = data;
    if ((void *)(l2 + 1) > data_end)
        return TC_ACT_OK;

    l3 = (struct iphdr *)(l2 + 1);
    if ((void *)(l3 + 1) > data_end)
        return TC_ACT_OK;

    l4 = (struct udphdr *)(l3 + 1);
    if ((void *)(l4 + 1) > data_end)
        return TC_ACT_OK;

    if (l3->protocol != IPPROTO_UDP)  //not equal to 17
        return TC_ACT_OK; 

    if (bpf_ntohs(l4->source) != 12345 && bpf_ntohs(l4->dest) != 12345)
        return TC_ACT_OK;

    int g = 5;
    const __u32 p = 23;
    __u32 alice_ip = bpf_htonl(0x0AE04C6E);
    __u32 bob_ip = bpf_htonl(0x0AE04CA7);
    __u32 *val;
    __u32 calc_secret_key = 1;
    int index = 0;

    payload = (__u8 *)(l4 + 1);
    unsigned long length = data_end - (void *)payload;
    const __u32 offset = sizeof(*l2) + sizeof(*l3) + sizeof(*l4);
    if (length < (MAX_PAYLOAD_LENGTH + 1))
    {
        __u8 payload_arr[MAX_PAYLOAD_LENGTH + 1];
        bpf_probe_read_kernel(payload_arr, length, payload);
        payload_arr[length] = 0;
        bpf_printk("Direction: Ingress, payload len: %lu, Message: %s", length, payload_arr);
        __u32 *shared_priv_key_exists = bpf_map_lookup_elem(&secret_key, &index);

        if (shared_priv_key_exists)
        {
           
            __u32 payload_length = length;
          
            __u8 key = (__u8)(*shared_priv_key_exists);
            
            for (__u32 i = 0; i < payload_length && i < MAX_PAYLOAD_LENGTH; i++) {
                payload_arr[i] ^= key; 
            }     
        
            for (__u32 i = 0; i < payload_length; i++) {
                if (bpf_skb_store_bytes(ctx, offset + i, &payload_arr[i], sizeof(__u8), 0) != 0) {
                    return TC_ACT_OK;
                }
            }
          
        }
        else
        {
            char received_ascii_val = payload_arr[0];
            __u32 shared_num = (__u32)received_ascii_val;

            bpf_printk("Partner Public Key %u", shared_num);    
            if (l3->saddr == alice_ip)
            {
                val = bpf_map_lookup_elem(&data_map, &bob_ip);
                
            }
            else if (l3->saddr == bob_ip)
            {
                val = bpf_map_lookup_elem(&data_map, &alice_ip);
            }
            else
            {
                return TC_ACT_OK;
            }

            if (val)
            {
                __u32 const private_num = *val;
                if (private_num > 100) {
                    return TC_ACT_OK; 
                }
                for (int i = 0; i < private_num; i++)
                {
                    calc_secret_key *= shared_num;
                }
                calc_secret_key = calc_secret_key % p;
                
            
                bpf_map_update_elem(&secret_key, &index, &calc_secret_key, BPF_ANY);
                bpf_printk("My private key %u, partner public key %u, share key: %u", private_num, shared_num, calc_secret_key);
            }
            else
            {
                return XDP_ABORTED;
            }
        }
        

    }
    return TC_ACT_OK;
}

/// @tchook {"ifindex":2, "attach_point":"BPF_TC_EGRESS"}
/// @tcopts {"handle":1, "priority":1}
SEC("tc")
int tc_egress(struct __sk_buff *ctx) //vm2 to vm1
{
    void *data_end = (void *)(__u64)ctx->data_end;
    void *data = (void *)(__u64)ctx->data;
    struct ethhdr *l2;
    struct iphdr *l3;
    struct udphdr *l4;
    __u8 *payload;

    if (ctx->protocol != bpf_htons(ETH_P_IP))
        return TC_ACT_OK;

    l2 = data;
    if ((void *)(l2 + 1) > data_end)
        return TC_ACT_OK;

    
    l3 = (struct iphdr *)(l2 + 1);
    if ((void *)(l3 + 1) > data_end)
        return TC_ACT_OK;

    l4 = (struct udphdr *)(l3 + 1);
    if ((void *)(l4 + 1) > data_end)
        return TC_ACT_OK;
        
    if (l3->protocol != IPPROTO_UDP)  //not equal to 17
        return TC_ACT_OK; 
    
    if ((bpf_ntohs(l4->source) != 12345) && (bpf_ntohs(l4->dest) != 12345))
        return TC_ACT_OK;

    payload = (__u8 *)(l4 + 1);
    char first_message[2] = " ";
    __u32 saddr;
    __u32 *value;
    int g = 5;
    int p = 23;
    __u32 result = 1;
    unsigned long length = data_end - (void *)payload;
    const __u32 offset = sizeof(*l2) + sizeof(*l3) + sizeof(*l4);
    if (length < (MAX_PAYLOAD_LENGTH + 1))
    {
        __u8 payload_arr[MAX_PAYLOAD_LENGTH + 1];
        bpf_probe_read_kernel(payload_arr, length, payload);
        payload_arr[length] = '\0';
        bpf_printk("Direction: Egress, payload len: %lu, Message: %s", length, payload_arr);
       
        if ((payload_arr[0] == ' ') && length == 6){
            

            saddr = l3->saddr;
            value = bpf_map_lookup_elem(&data_map, &saddr);
            if (value) {
                __u32 const private_num = *value;
                if (private_num > 100) {
                    return TC_ACT_OK;  
                }
                
                for (int i = 0; i < private_num; i++)
                {
                    result *= g;
                }

                result = result % p;
                
                bpf_printk("My public key: %u, my private key %u", result, private_num);
                
               
                if (bpf_skb_store_bytes(ctx, offset, &result, sizeof(__u32), 0) != 0) {
                    return TC_ACT_OK;
                }
                            
            } else {
                bpf_printk("Source IP %08x not found in map\n", bpf_ntohl(saddr));
            }
           
        }
        else
        {
            int index = 0;
            __u32 *shared_priv_key_exists = bpf_map_lookup_elem(&secret_key, &index);
            if (!shared_priv_key_exists) {
                return TC_ACT_OK; 
            }
        
            __u32 payload_length = length;
           
            __u8 key = (__u8)(*shared_priv_key_exists);
            
            for (__u32 i = 0; i < payload_length && i < MAX_PAYLOAD_LENGTH; i++) {
                payload_arr[i] ^= key; 
            }     
           
            for (__u32 i = 0; i < payload_length; i++) {
                if (bpf_skb_store_bytes(ctx, offset + i, &payload_arr[i], sizeof(__u8), 0) != 0) {
                   
                    return TC_ACT_OK;
                }
            }
        }
        
    }
  
    return TC_ACT_OK;
}

char __license[] SEC("license") = "GPL";