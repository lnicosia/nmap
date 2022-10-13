#ifndef NMAP_H
# define NMAP_H

#include "libft.h"
#include "set.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <limits.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <linux/if.h>
#include <pthread.h>
#include <linux/in.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <errno.h>

/* STATUS */
#define OPEN 0
#define CLOSED 1
#define FILTERED 2
#define DOWN 3
#define ERROR 4
#define UNKNOWN 5
#define TIMEOUT 6
#define UP 7
#define READY 8
#define PRINTED 9
#define SCANNING 10
#define INVALID 11

#define UPDATE 1
#define UPDATE_TARGET 2
#define ALREADY_UPDATED 3

/* Max ips to scan in one command */
#define MAX_IPS 15

/* Default ephemeral ports */
#define DEFAULT_EPHEMERAL_MIN 32768
#define DEFAULT_EPHEMERAL_MAX 60999

#define LOCK(input)		pthread_mutex_lock(&input->lock);
#define UNLOCK(input)	pthread_mutex_unlock(&input->lock);

struct s_scan {
	struct sockaddr_in	*saddr; /* sockaddr_in of source */
	struct sockaddr_in	*daddr; /* sockaddr_in of dest */
	char				*dhostname; /* found destination hostname */
	int					scantype; /* Type of scan */
	int					status; /* Current status [READY/SCANNING/OPEN/CLOSED/FILTERED] */
	uint16_t			sport; /* Source port */
	uint16_t			dport; /* Destination port */
	struct timeval		start_time; /* Scan start time */
	struct timeval		end_time; /* Scan end time */

	pthread_mutex_t		lock; /* Mutex */

	struct s_scan		*next; /* Next scan */
};

struct s_ip {
	struct sockaddr_in	*saddr; /* sockaddr_in of source */
	struct sockaddr_in	*daddr; /* sockaddr_in of dest */
	char				*dhostname; /* found ip hostnme */
	char				*destination; /* user input */
	int					status; /* [UP/DOWN/ERROR] */
	struct s_scan		*scans; /* list of ports to scan along with the type of scan */
	struct s_ip			*next; /* next ip */
};

typedef struct	s_data {
	/* Options related */
	unsigned long long	opt;
	t_set				set;
	t_ipset				*ipset;

	/* Scan list */
	struct s_ip			*ips;

	/* Threads related */
	pthread_t			*threads;
	uint8_t				nb_threads;
	int					created_threads;

	/* Ephemeral ports */
	uint16_t			port_min;
	uint16_t			port_max;

	/* Is program run as root */
	uint8_t				privilegied;

	/* Counters */
	int					ip_counter;
	int					port_counter;
}						t_data;

struct			tcp_options {
};

struct			ip_options {
};

struct			tcp_packet {
	struct iphdr		ip;
	struct tcphdr		tcp;
};

struct			icmp_packet {
	struct iphdr		ip;
	struct icmphdr		icmp;
	struct tcp_packet	data;
};

extern t_data	g_data;

/* print.c */
void	print_ip4_header(struct ip *header);
void	print_time(struct timeval start_time,
	struct timeval end_time);
void	print_scans(struct s_ip *ips);

/* syn_scan.c */
int		syn_scan(struct s_scan *to_scan);

/* tcp_scan.c */
int		tcp_scan(struct s_scan *to_scan);

/* addr_config.c */
int dconfig(char *destination, uint16_t port, struct sockaddr_in *daddr,
	char **hostname);
int		sconfig(char *destination, struct sockaddr_in *saddr);

/* checksum.c */
unsigned short tcp_checksum(struct iphdr *ip, struct tcphdr *tcp);
unsigned short checksum(const char *buf, unsigned int size);

/* parse_file.c */
void	free_ipset(t_ipset **ipset);
int		parse_file(char *path, t_ipset **head);

/* parse_option_line.c */
void print_usage(FILE* f);

/* nmap.c */
int		ft_nmap(char *path);

/* free_and_exit.c */
void	free_and_exit(int exit_val);

/* craft_packet.c */
void	craft_ip_packet(void *packet, struct sockaddr_in *saddr,
	struct sockaddr_in *daddr, uint8_t protocol, struct ip_options *options);
void	craft_tcp_packet(void *packet, struct sockaddr_in *saddr,
	struct sockaddr_in *daddr, uint8_t flags, struct tcp_options *options);

/* list.c */
int		update_scans(struct s_scan *scan, int status, uint16_t source_port);
void	push_ip(struct s_ip **head, struct s_ip *new);
void	push_ports(struct s_ip **input, t_set *set);
void	free_ips(struct s_ip **ip);

#endif
