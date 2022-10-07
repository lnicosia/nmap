#include "nmap.h"
#include "options.h"

static int send_syn(int sockfd,
	struct sockaddr_in *saddr, struct sockaddr_in *daddr)
{
	unsigned int len = 0;

	char packet[sizeof(struct iphdr)+sizeof(struct tcphdr)+len];
	struct iphdr *ip = (struct iphdr *)packet;
	struct tcphdr *tcp = (struct tcphdr *)(packet+sizeof(struct iphdr));

	ft_memset(packet, 0, sizeof(packet));

	/* Filling IP header */
	/* Version */
	ip->version = 4;
	/* Internet Header Length (how many 32-bit words are present in the header) */
	ip->ihl = sizeof(struct iphdr) / sizeof(uint32_t);
	/* Type of service */
	ip->tos = 0;
	/* Total length */
	ip->tot_len = htons(sizeof(packet));
	/* Identification (notes/ip.txt) */
	ip->id = 0;
	ip->frag_off = 0;
	/* TTL */
	ip->ttl = 64;
	/* Protocol (TCP) */
	ip->protocol = IPPROTO_TCP;
	/* Checksum */
	ip->check = 0; /* Calculated after TCP header */
	/* Source ip */
	memcpy(&ip->saddr, &saddr->sin_addr.s_addr, sizeof(ip->saddr));
	/* Dest ip */
	memcpy(&ip->daddr, &daddr->sin_addr.s_addr, sizeof(ip->daddr));

	/* Filling TCP header */
	/* Source port */
	memcpy(&tcp->source, &saddr->sin_port, sizeof(tcp->source));
	/* Destination port */
	memcpy(&tcp->dest, &daddr->sin_port, sizeof(tcp->dest));
	/* Seq num */
	tcp->seq = htons(0);
	/* Ack num */
	tcp->ack_seq = htons(0);
	/* Sizeof header / 4 */
	tcp->doff = sizeof(struct tcphdr) /  4;
	/* Flags */
	tcp->fin = 0;
	tcp->syn = 1;
	tcp->rst = 0;
	tcp->psh = 0;
	tcp->ack = 0;
	tcp->urg = 0;
	/* WTF is this */
	tcp->window = htons(64240);
	/* Checksum */
	tcp->check = 0; /* Calculated after headers */
	/* Indicates the urgent data, only if URG flag set */
	tcp->urg_ptr = 0;

	/* Checksums */
	tcp->check = tcp_checksum(ip, tcp);
	ip->check = checksum((const char*)packet, sizeof(packet));

	/* Sending handcrafted packet */
	if (g_data.opt & OPT_VERBOSE_INFO || g_data.opt & OPT_VERBOSE_DEBUG)
		fprintf(stderr, "[*] Ready to send SYN packet...\n");
	if (sendto(sockfd, packet, sizeof(packet), 0, (struct sockaddr *)daddr,
		sizeof(struct sockaddr)) < 0)
		return 1;

	/* Verbose prints */
	if (g_data.opt & OPT_VERBOSE_DEBUG)
		print_ip4_header((struct ip *)ip);
	if (g_data.opt & OPT_VERBOSE_INFO || g_data.opt & OPT_VERBOSE_DEBUG)
		fprintf(stderr, "[*] Sent SYN packet\n");

	return 0;
}

static int is_complete(struct s_scan *scan)
{
	while (scan) {
		//printf("[*] Scan %d, status: %d\n", scan->dport, scan->status);
		if (scan->status == READY)
			return 0;
		scan = scan->next;
	}

	return 1;
}

static int read_syn_ack(int sockfd, struct s_scan *scan)
{
	int ret;
	unsigned int len = sizeof(struct iphdr) + sizeof(struct tcphdr);
	char buffer[len];
	struct tcp_packet *packet;

	/* Receiving process */
	if (g_data.opt & OPT_VERBOSE_INFO || g_data.opt & OPT_VERBOSE_DEBUG)
		fprintf(stderr, "[*] Ready to receive...\n");
	ret = recv(sockfd, buffer, len, MSG_DONTWAIT);
	/* if (ret < 0) {
		if (g_data.opt & OPT_VERBOSE_INFO || g_data.opt & OPT_VERBOSE_DEBUG)
			fprintf(stderr, "[*] Packet timeout\n");
		return TIMEOUT;
	} */

	(void)ret;
	/* Invalid packet (packet too small) */
	/* if (ret < (int)sizeof(struct tcp_packet))
		return 0; */

	/* TODO: Packet error checking ? */
	packet = (struct tcp_packet *)buffer;

	if (g_data.opt & OPT_VERBOSE_DEBUG)
		print_ip4_header((struct ip *)&packet->ip);

	uint16_t to = packet->tcp.dest;
	struct s_scan *tmp = scan;

	// printf("[*] Received packet to: %d\n", ntohs(to));

	while (tmp) {
		if (tmp->saddr->sin_port == to) {
			// printf("[*] Updating scan: %d\n", tmp->sport);
			if (packet->tcp.rst)
				tmp->status = CLOSED;
			else if (packet->tcp.ack && packet->tcp.syn)
				tmp->status = OPEN;
			if (tmp == scan)
				return 1;
		}
		tmp = tmp->next;
	}
	return is_complete(scan);
}

int syn_scan(struct s_scan *scan)
{
	int sockfd;
	int one = 1;
	struct timeval timeout = {1, 533000};
	int ret;
	struct servent *s_service;
	char *service = "unknown";

	if (g_data.opt & OPT_VERBOSE_INFO || g_data.opt & OPT_VERBOSE_DEBUG)
		fprintf(stderr, "=========================================\n");

	/* Prepare ports */
	scan->saddr->sin_port = htons(scan->sport);
	scan->daddr->sin_port = htons(scan->dport);

	/* Verbose prints */
	if (g_data.opt & OPT_VERBOSE_INFO || g_data.opt & OPT_VERBOSE_DEBUG) {
		fprintf(stderr, "[*] Destination: %s (%s) on port: %d\n",
			scan->dhostname, inet_ntoa(scan->daddr->sin_addr),
			ntohs(scan->daddr->sin_port));
	}
	if (g_data.opt & OPT_VERBOSE_INFO || g_data.opt & OPT_VERBOSE_DEBUG) {
		fprintf(stderr, "[*] Source: %s on port: %d\n",
			inet_ntoa(scan->saddr->sin_addr), ntohs(scan->saddr->sin_port));
	}

	/* Socket creation */
	if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP)) < 0) {
		if (g_data.opt & OPT_VERBOSE_INFO || g_data.opt & OPT_VERBOSE_DEBUG)
			fprintf(stderr, "[*] Failed to create socket\n");
		scan->status = ERROR;
		return 1;
	}

	/* Set options */
	if ((setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one))) != 0) {
		if (g_data.opt & OPT_VERBOSE_INFO || g_data.opt & OPT_VERBOSE_DEBUG)
			fprintf(stderr, "[*] Failed to set header option\n");
		scan->status = ERROR;
		close(sockfd);
		return 1;
	}
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
		sizeof(timeout)) != 0)
	{
		if (g_data.opt & OPT_VERBOSE_INFO || g_data.opt & OPT_VERBOSE_DEBUG)
			fprintf(stderr, "[*] Failed to set timeout option\n");
		scan->status = ERROR;
		close(sockfd);
		return 1;
	}

	/* struct sockaddr_in bindaddr;
	ft_memcpy(&bindaddr, scan->saddr, sizeof(struct sockaddr_in));
	if (bind(sockfd, (struct sockaddr *)&bindaddr, sizeof(bindaddr)) != 0) {
		if (g_data.opt & OPT_VERBOSE_INFO || g_data.opt & OPT_VERBOSE_DEBUG)
			fprintf(stderr, "[*] Failed to bind port: %d\n", bindaddr.sin_port);
		scan->status = ERROR;
		close(sockfd);
		return 1;
	} */

	/* if ((connect(sockfd, (struct sockaddr *)scan->daddr, sizeof(struct sockaddr)) != 0)) {
		if (g_data.opt & OPT_VERBOSE_INFO || g_data.opt & OPT_VERBOSE_DEBUG)
			fprintf(stderr, "[*] Failed to connect to host\n");
		scan->status = ERROR;
		close(sockfd);
		return 1;
	} */

	/* Service detection */
	/* Network services database file /etc/services */
	if ((s_service = getservbyport(scan->daddr->sin_port, NULL)))
		service = s_service->s_name;
	scan->service = ft_strdup(service);

	/* Scan start time */
	if ((gettimeofday(&scan->start_time, NULL)) != 0) {
		scan->start_time.tv_sec = 0;
		scan->start_time.tv_usec = 0;
	}

	/* Scanning process */
	if (send_syn(sockfd, scan->saddr, scan->daddr) != 0)
		ret = ERROR;
	else {
		while (!(ret = read_syn_ack(sockfd, scan)));
	}

	/* Scan end time */
	if ((gettimeofday(&scan->end_time, NULL)) != 0) {
		scan->end_time.tv_sec = 0;
		scan->end_time.tv_usec = 0;
	}

	if (g_data.opt & OPT_VERBOSE_INFO || g_data.opt & OPT_VERBOSE_DEBUG)
		fprintf(stderr, "[*] Port status: %d\n", ret);

	if (g_data.opt & OPT_VERBOSE_INFO || g_data.opt & OPT_VERBOSE_DEBUG)
		fprintf(stderr, "=========================================\n");

	close(sockfd);
	return 0;
}
