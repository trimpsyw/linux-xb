/* 
 * �����Ϊ��ѡ���Դ�����
 * ������İ�Ȩ(����Դ�뼰�����Ʒ����汾)��һ�й������С�
 * ����������ʹ�á������������
 * ��Ҳ�������κ���ʽ���κ�Ŀ��ʹ�ñ����(����Դ�뼰�����Ʒ����汾)���������κΰ�Ȩ���ơ�
 * =====================
 * ����: ������
 * ����: sunmingbao@126.com
 */

#include <stdio.h>
#include "common.h"
#include "debug.h"
#include "config_file.h"
#include "misc_utils.h"

t_fc_cfg  gt_fc_cfg;
t_pkt_cap_cfg  gt_pkt_cap_cfg;

t_stream    *g_apt_streams[MAX_STREAM_NUM];
int        nr_cur_stream;

const char *app_name = "pktsender";
const char version[4]={'1','0','0',0};

void *alloc_stream()
{
    t_stream *pt_stream = malloc(sizeof(t_stream));
    memset(pt_stream, 0, sizeof(t_stream));
    return pt_stream;
}

uint32_t  build_err_flags_v4(t_ether_packet *pt_eth, int len)
{
    uint32_t err_flags = 0;
    t_ip_hdr *iph = eth_data(pt_eth);

    if (len<MIN_PKT_LEN)
    {
            err_flags |= ERR_PKT_LEN;
            return err_flags; 
    }

    if (ip_pkt_len(iph)+14 > len)
    {
            err_flags |= ERR_PKT_LEN;
            return err_flags; 
    }

    if (ip_hdr_len(iph)<FIXED_IP_HDR_LEN || ip_hdr_len(iph)>ip_pkt_len(iph))
    {
            err_flags |= ERR_PKT_LEN;
            return err_flags; 
    }

   if (ip_checksum_wrong(iph))
    {
            err_flags |= ERR_IP_CHECKSUM;
            return err_flags; 
    }

    if (ip_pkt_is_frag(pt_eth))
    {
            return err_flags; 
    }

    if (iph->protocol==IPPROTO_TCP)
    {
        if (tcp_checksum_wrong(iph))
        {
            err_flags |= ERR_TCP_CHECKSUM;
            return err_flags; 
        }

    }
    else if (iph->protocol==IPPROTO_UDP)
    {
        t_udp_hdr *pt_udp_hdr = ip_data(iph);
        if (ntohs(pt_udp_hdr->len) != ip_data_len(iph))
        {
                err_flags |= ERR_PKT_LEN;
                return err_flags; 
        }

        if (udp_checksum_wrong(iph))
        {
            err_flags |= ERR_UDP_CHECKSUM;
            return err_flags; 
        }
    }
    else if (iph->protocol==IPPROTO_ICMP)
    {
        if (icmp_igmp_checksum_wrong(iph))
            err_flags |= ERR_ICMP_CHECKSUM;
    }
    else if (iph->protocol==IPPROTO_IGMP)
    {
        if (icmp_igmp_checksum_wrong(iph))
            err_flags |= ERR_IGMP_CHECKSUM;
    }

    return err_flags; 
}

uint32_t  build_err_flags_v6(t_ether_packet *pt_eth, int len)
{
    uint32_t err_flags = 0;
    t_ipv6_hdr *ip6h = eth_data(pt_eth);

    if (len<40)
    {
            err_flags |= ERR_PKT_LEN;
            return err_flags; 
    }

    if (ip6_pkt_len(ip6h)+14 > len)
    {
            err_flags |= ERR_PKT_LEN;
            return err_flags; 
    }


    if (ip_pkt_is_frag(pt_eth))
    {
            return err_flags; 
    }

    if (ip6h->nexthdr==IPPROTO_ICMPV6)
    {
        if (icmp_checksum_wrong6(ip6h))
        {
            err_flags |= ERR_ICMP_CHECKSUM;
            return err_flags; 
        }

    }

    else if (ip6h->nexthdr==IPPROTO_TCP)
    {
        if (tcp_checksum_wrong6(ip6h))
        {
            err_flags |= ERR_TCP_CHECKSUM;
            return err_flags; 
        }

    }
    else if (ip6h->nexthdr==IPPROTO_UDP)
    {
        t_udp_hdr *pt_udp_hdr = ip6_data(ip6h);
        if (ntohs(pt_udp_hdr->len) != ip6_data_len(ip6h))
        {
                err_flags |= ERR_PKT_LEN;
                return err_flags; 
        }

        if (udp_checksum_wrong6(ip6h))
        {
            err_flags |= ERR_UDP_CHECKSUM;
            return err_flags; 
        }
    }


    return err_flags; 
}

uint32_t  build_err_flags_arp(t_ether_packet *pt_eth, int len)
{
    uint32_t err_flags = 0;
    t_arp_hdr *pt_arp_hdr = eth_data(pt_eth);

    if (len<MIN_PKT_LEN)
    {
            err_flags |= ERR_PKT_LEN;
            return err_flags; 
    }
    
    if (len<(arp_pkt_len(pt_arp_hdr)+14))
    {
            err_flags |= ERR_PKT_LEN;
            return err_flags; 
    }
    
    return err_flags; 
}

uint32_t  build_err_flags(t_ether_packet *pt_eth, int len)
{
    uint32_t err_flags = 0;
    int type = eth_type(pt_eth);

    if (type==ETH_P_ARP)
        return build_err_flags_arp(pt_eth, len);

    if (type==ETH_P_IP)
        return build_err_flags_v4(pt_eth, len);
        
    if (type==ETH_P_IPV6)
        return build_err_flags_v6(pt_eth, len);
        
        return err_flags; 

}

void update_check_sum_v4(t_stream *pt_stream)
{
    t_ip_hdr *iph=eth_data(pt_stream->data);
    if (pt_stream->flags & CHECK_SUM_IP)
        ip_update_check(iph);

    if (ip_pkt_is_frag(&(pt_stream->eth_packet))) return;
    
    if (iph->protocol==IPPROTO_ICMP && (pt_stream->flags & CHECK_SUM_ICMP))
        icmp_igmp_update_check(iph);
    else if (iph->protocol==IPPROTO_IGMP && (pt_stream->flags & CHECK_SUM_IGMP))
        icmp_igmp_update_check(iph);
    else if (iph->protocol==IPPROTO_TCP && (pt_stream->flags & CHECK_SUM_TCP))
        tcp_update_check(iph);
    else if (iph->protocol==IPPROTO_UDP && (pt_stream->flags & CHECK_SUM_UDP))
        udp_update_check(iph);

}

void update_check_sum_v6(t_stream *pt_stream)
{
    t_ipv6_hdr *ip6h=eth_data(pt_stream->data);

    if (ip_pkt_is_frag(&(pt_stream->eth_packet))) return;

    if (ip6h->nexthdr==IPPROTO_TCP && (pt_stream->flags & CHECK_SUM_TCP))
        tcp_update_check6(ip6h);
    else if (ip6h->nexthdr==IPPROTO_UDP && (pt_stream->flags & CHECK_SUM_UDP))
        udp_update_check6(ip6h);
    else if (ip6h->nexthdr==IPPROTO_ICMPV6 && (pt_stream->flags & CHECK_SUM_ICMP))
        icmp_update_check6(ip6h);

}

void update_check_sum(t_stream *pt_stream)
{
    int type = eth_type(pt_stream->data);
    if (type==ETH_P_IP)
        update_check_sum_v4(pt_stream);
   else if (type==ETH_P_IPV6)
       update_check_sum_v6(pt_stream);

}


void update_len_v4(t_stream *pt_stream)
{
    t_ip_hdr *iph=eth_data(pt_stream->data);
    t_udp_hdr *udph=ip_data(iph);
    if (pt_stream->flags & IP_LEN)
        iph->tot_len = htons(pt_stream->len - eth_hdr_len(pt_stream->data));
    if (iph->protocol==IPPROTO_UDP && (pt_stream->flags & UDP_LEN))
        udph->len = htons(pt_stream->len - eth_hdr_len(pt_stream->data) - ip_hdr_len(iph));

}

void update_len_v6(t_stream *pt_stream)
{
    t_ipv6_hdr *ip6h =eth_data(pt_stream->data);
    t_udp_hdr *udph=ip6_data(ip6h);
    if (pt_stream->flags & IP_LEN)
        set_ip6_pkt_len(ip6h, pt_stream->len - eth_hdr_len(pt_stream->data));
    if (ip6h->nexthdr==IPPROTO_UDP && (pt_stream->flags & UDP_LEN))
        udph->len = htons(ip6_data_len(ip6h));

}

void update_len(t_stream *pt_stream)
{
    int type = eth_type(pt_stream->data);
    if (type==ETH_P_IP)
        update_len_v4(pt_stream);
    else if (type==ETH_P_IPV6)
        update_len_v6(pt_stream);
}

void append_err_text(char *info, uint32_t err_flags)
{
    if (err_flags==0) return;
    
    strcat(info, "(error:");
    if (err_flags&ERR_IP_CHECKSUM)
    {
        strcat(info, "ip check sum");
        goto exit;
    }

    if (err_flags&ERR_ICMP_CHECKSUM)
    {
        strcat(info, "icmp check sum");
        goto exit;
    }

    if (err_flags&ERR_IGMP_CHECKSUM)
    {
        strcat(info, "igmp check sum");
        goto exit;
    }

    if (err_flags&ERR_TCP_CHECKSUM)
    {
        strcat(info, "tcp check sum");
        goto exit;
    }

    if (err_flags&ERR_UDP_CHECKSUM)
    {
        strcat(info, "udp check sum");
        goto exit;
    }

    if (err_flags&ERR_PKT_LEN)
    {
        strcat(info, "packet length");
        goto exit;
    }
exit:
    strcat(info, ")");
    
}


void report_pkt_load_info(int not_loaded_all)
{
        printf("\n[packet load info]\n");
   
        printf("%s%d packets loaded\n", LINE_HDR, nr_cur_stream);


        if (not_loaded_all)
            printf("%s dump file has many packets, but we only load first %d packets\n"
            , LINE_HDR, MAX_STREAM_NUM);


}

const char * err_text(uint32_t err_flags)
{
    if (err_flags==0) return "";
    
    if (err_flags&ERR_IP_CHECKSUM)
    {
        return "ip check sum";
    }

    if (err_flags&ERR_ICMP_CHECKSUM)
    {
        return "icmp check sum";
    }

    if (err_flags&ERR_IGMP_CHECKSUM)
    {
        return "igmp check sum";
    }

    if (err_flags&ERR_TCP_CHECKSUM)
    {
        return "tcp check sum";
    }

    if (err_flags&ERR_UDP_CHECKSUM)
    {
        return "udp check sum";
    }

    if (err_flags&ERR_PKT_LEN)
    {
        return "packet length";
    }

    return " ";
    
}

int load_config_file(char *file_path, unsigned char *src_mac, unsigned char *dst_mac)
{
    FILE *file=fopen(file_path, "rb");
    int i, ret = 0;
    char version_tmp[4];
    int not_loaded_all = 0;

    fread(version_tmp, sizeof(version), 1, file);
    fread(&gt_fc_cfg, sizeof(gt_fc_cfg), 1, file);

    if (version_tmp[0] != version[0]+2)
    {
        DBG_PRINT_QUIT("cfg file version mismatch");
        DBG_PRINT_QUIT("please use source code in %c.x version of windows xb_ether_tester "
            "to rebuild this program", version[0]+2);
        ret = -1;
        goto EXIT;

    }

    fread(&gt_pkt_cap_cfg, PKT_CAP_CFG_FIX_LEN, 1, file);
    fread(gt_pkt_cap_cfg.filter_str_usr, gt_pkt_cap_cfg.filter_str_len, 1, file);
    gt_pkt_cap_cfg.filter_str_usr[gt_pkt_cap_cfg.filter_str_len]=0;

    fread(&nr_cur_stream, sizeof(nr_cur_stream), 1, file);
    if (nr_cur_stream > MAX_STREAM_NUM)
    {
        nr_cur_stream = MAX_STREAM_NUM;
        not_loaded_all = 1;
    }

    for(i=0;i<nr_cur_stream;i++)
    {
        g_apt_streams[i] = alloc_stream();
        fread(g_apt_streams[i], 1, STREAM_HDR_LEN, file);
        fread(g_apt_streams[i]->data, 1, g_apt_streams[i]->len, file);
        if (dst_mac)
            memcpy(g_apt_streams[i]->data, dst_mac, 6);
        if (src_mac)
            memcpy(g_apt_streams[i]->data+6, src_mac, 6);
        g_apt_streams[i]->err_flags = build_err_flags((void *)(g_apt_streams[i]->data), g_apt_streams[i]->len);
        if (g_apt_streams[i]->err_flags)
            printf("stream %s has errors: %s\n", g_apt_streams[i]->name, err_text(g_apt_streams[i]->err_flags));
    }

    report_pkt_load_info(not_loaded_all);


EXIT:
    fclose(file);
    return ret;
}

int load_bin_packet_file(char *file_path, unsigned char *src_mac, unsigned char *dst_mac)
{
    FILE *file=fopen(file_path, "rb");
    int ret = 0;
    int not_loaded_all = 0;

    nr_cur_stream = 1;
    g_apt_streams[0] = alloc_stream();
    ret=get_data_from_file(g_apt_streams[0]->data, file_path, 65535);
    if (dst_mac)
            memcpy(g_apt_streams[0]->data, dst_mac, 6);
    if (src_mac)
            memcpy(g_apt_streams[0]->data+6, src_mac, 6);

    g_apt_streams[0]->len = ret;
    g_apt_streams[0]->selected=1;
    g_apt_streams[0]->err_flags = build_err_flags((void *)(g_apt_streams[0]->data), g_apt_streams[0]->len);
    if (g_apt_streams[0]->err_flags)
            printf("input packet has errors: %s\n", err_text(g_apt_streams[0]->err_flags));
    report_pkt_load_info(not_loaded_all);

    fclose(file);
    ret=ret>0?0:ret;
    return ret;
}

int make_packet(const char *protocol, const unsigned char *src_mac, const unsigned char *dst_mac,  
                const char *src, const char *dst, 
                unsigned short sport, unsigned short dport,
                const char *payload, int len)
{
    int ret = 0;
    int not_loaded_all = 0;
    t_ether_packet *ether;
    t_ip_hdr *iph;
    t_tcp_hdr *tcph;
    t_udp_hdr *udph;
    t_icmp_hdr *icmph;
    char *payloadd;
    int len2;

    if(sport == 0) 
        sport = rand() % 1024 + 50000;
    if(dport == 0)
        dport = rand() % 1024 + 80;

    nr_cur_stream = 1;
    g_apt_streams[0] = alloc_stream();


    ether = (t_ether_packet *)g_apt_streams[0]->data;
    iph = (t_ip_hdr*)(ether + 1);

    memcpy(ether->dst, dst_mac, 6);
    memcpy(ether->src, src_mac, 6);
    ether->type = htons(0x0800);

    iph->ihl = 5;
    iph->version = 4;
    iph->tos = 0;
    //iph->tot_len = htons(sizeof(*iph) + sizeof(*tcph) + len);
    iph->id = 12345;
    iph->frag_off = htons(16384);
    iph->ttl = 64;
    //iph->protocol = 6;
    iph->check = 0;
    ip_str2n(&iph->daddr, dst);
    ip_str2n(&iph->saddr, src);
    //ip_update_check(iph);

    if(strcasecmp(protocol, "tcp") == 0) {
        tcph = (t_tcp_hdr*)(iph + 1);
        payloadd = tcph + 1;
        memcpy(payloadd, payload, len);
        ret = sizeof(*ether) + sizeof(*iph) + sizeof(*tcph) + len;
        iph->tot_len = htons(sizeof(*iph) + sizeof(*tcph) + len);
        iph->protocol = 6;
        ip_update_check(iph);
        
        tcph->source = htons(sport);
        tcph->dest = htons(dport);
        tcph->seq = 1;
        tcph->ack_seq = 2;
        tcph->doff = 5;
        tcph->fin = 0;
        tcph->syn = 1;
        tcph->rst = 0;
        tcph->psh = 0;
        tcph->ack = 0;
        tcph->urg = 0;
        tcph->window = htons(64240); 
        tcph->check = 0;
        tcph->urg_ptr = 0;
        tcp_update_check(iph);
    } else if(strcasecmp(protocol, "udp") == 0) {
        udph = (t_udp_hdr*)(iph + 1);
        payloadd = udph + 1;
        memcpy(payloadd, payload, len);
        ret = sizeof(*ether) + sizeof(*iph) + sizeof(*udph) + len;
        iph->tot_len = htons(sizeof(*iph) + sizeof(*udph) + len);
        iph->protocol = 17;
        ip_update_check(iph);

        udph->source = htons(sport);
        udph->dest = htons(dport);
        udph->len = htons(sizeof(*udph) + len);
        udph->check = 0;
        udp_update_check(iph);
    } else if(strcasecmp(protocol, "icmp") == 0) {
        icmph = (t_icmp_hdr*)(iph + 1);
        payloadd = icmph + 1;
        memcpy(payloadd, payload, len);
        ret = sizeof(*ether) + sizeof(*iph) + sizeof(*icmph) + len;
        iph->tot_len = htons(sizeof(*iph) + sizeof(*icmph) + len);
        iph->protocol = 1;
        ip_update_check(iph);

        icmph->type = 8;
        icmph->code = 0;
        icmph->un.echo.id = 1024;
        icmph->un.echo.sequence = 1280;
        icmp_igmp_update_check(iph);
    } else {
        printf("%s is not support!\n", protocol);
        return -1;
    }

    g_apt_streams[0]->len = ret;
    g_apt_streams[0]->selected=1;
    g_apt_streams[0]->err_flags = 0;

    g_apt_streams[0]->err_flags = build_err_flags((void *)(g_apt_streams[0]->data), g_apt_streams[0]->len);
    if (g_apt_streams[0]->err_flags)
            printf("input packet has errors: %s\n", err_text(g_apt_streams[0]->err_flags));

    ret=ret>0?0:ret;
    return ret;
}
