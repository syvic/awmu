/*
 * Copyright (c) 1997, 1998, 1999, 2004
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
 *	Jorge Gómez <syvic@sindormir.net>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#define VERSION "0.7"

/* Ansi codes for colors and screen redrawing */
#define RED "\e[31m"
#define GREEN "\e[32m"
#define BBLUE "\e[34m"
#define BLUE "\e[36m"
#define WHITE "\e[0m"

#define CLEAR "\e[2J"
#define INITT "\e[0;0H"
#define INITP "\e[2;0H"

#define MIN_QUALITY 19
#define MAX_QUALITY 120

#define WI_STRING		0x01
#define WI_BOOL			0x02
#define WI_WORDS		0x03
#define WI_HEXBYTES		0x04
#define WI_KEYSTRUCT		0x05
#define WI_SWORDS		0x06
#define WI_HEXWORDS		0x07


#include <machine/apm_bios.h>
#include <sys/file.h>

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/ethernet.h>
#include "net80211/ieee80211.h"
#include "net80211/ieee80211_ioctl.h"
#include <dev/wi/if_wavelan_ieee.h>
#include <dev/wi/if_wireg.h>

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <err.h>
#include <signal.h>


static int wi_getval(const char *, struct wi_req *);
static int wi_getvalmaybe(const char *, struct wi_req *);
static void wi_printstr(struct wi_req *);
static void wi_printwords(struct wi_req *);
static void wi_ptype_printwords(struct wi_req *);
static void wi_printbool(struct wi_req *);
static void wi_printhex(struct wi_req *);
static void wi_dumpinfo(const char *iface, const char *rrdfile);
static void wi_bar_printwords(struct wi_req *wreq, const char *rrd);
static void wi_dumpstats(const char *);
static void wi_printkeys(struct wi_req *);
static int  apm_getinfo(int fd, apm_info_t aip);
static int  apm_getinfo(int fd, apm_info_t aip);
static void wi_case_printwords(struct wi_req *wreq, int code,const char *rrd);


static int apm_getinfo(int fd, apm_info_t aip)
{
	if (ioctl(fd, APMIO_GETINFO, aip) == -1)
		return (-1);
	return 0;
}

static int apm()
{
	int fd;
	int t, h, m, s;
	struct apm_info info;
	fd = open("/dev/apm", O_RDONLY);
	printf ("\n%sVitals:%s\n",BBLUE,WHITE);

        apm_getinfo(fd, &info);	
	if (info.ai_batt_time == -1)
	{
		printf("%sRemaining battery time:\t\t%sUnavailable",GREEN,WHITE);
		return (-1);
	}
	t = info.ai_batt_time;
	s = t % 60;
	t /= 60;
	m = t % 60;
	t /= 60;
	h = t;
	printf("%sRemaining battery time:\t\t%s%d:%02d:%02d     ",GREEN,WHITE, h, m, s);
	return (info.ai_batt_time);
}

static int _wi_getval(const char *iface, struct wi_req *wreq)
{
	struct ifreq ifr;
	int s, retval;

	bzero((char *)&ifr, sizeof(ifr));

	strlcpy(ifr.ifr_name, iface, sizeof(ifr.ifr_name));
	ifr.ifr_data = (caddr_t)wreq;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1)
		err(1, "socket");
	retval = ioctl(s, SIOCGWAVELAN, &ifr);
	close(s);

	return (retval);
}

static int wi_getval(const char *iface, struct wi_req *wreq)
{
	if (_wi_getval(iface, wreq) == -1) 
	{
		if (errno != EINPROGRESS)
			err(1, "SIOCGWAVELAN");
		return (-1);
	}
	return (0);
}

static int wi_getvalmaybe(const char *iface, struct wi_req *wreq)
{
	if (_wi_getval(iface, wreq) == -1) 
	{
		if (errno != EINPROGRESS && errno != EINVAL)
			err(1, "SIOCGWAVELAN");
		return (-1);
	}
	return (0);
}

void
wi_printstr(struct wi_req *wreq)
{
	char *ptr;
	register unsigned int i;

	ptr = (char *)&wreq->wi_val[1];
	for (i = 0; i < wreq->wi_val[0]; i++) 
		if (ptr[i] == '\0')
			ptr[i] = ' ';
	ptr[i] = '\0';
	printf("%s", ptr);
	return;
}

static void
wi_printkeys(struct wi_req *wreq)
{
	int j, isprintable;
	register unsigned int i;
	struct wi_key		*k;
	struct wi_ltv_keys	*keys;
	char			*ptr;

	keys = (struct wi_ltv_keys *)wreq;

	for (i = 0; i < 4; i++) 
	{
		k = &keys->wi_keys[i];
		ptr = (char *)k->wi_keydat;
		isprintable = 1;
		for (j = 0; j < k->wi_keylen; j++) 
		{
			if (!isprint(ptr[j])) 
			{
				isprintable = 0;
				break;
			}
		}
		if (isprintable) 
		{
			ptr[j] = '\0';
			printf("%s", ptr);
		} 
		else 
		{
			printf("0x");
			for (j = 0; j < k->wi_keylen; j++) 
				printf("%02x", ptr[j] & 0xFF);
		}
	}
	return;
};

void wi_bar_printwords(struct wi_req *wreq, const char *rrd)
{
	int n;
	int puf;
	register unsigned int i;
	FILE *rrdfile;
	struct timeval tp;
	long unsigned int ut;

	gettimeofday(&tp,NULL);
	ut=tp.tv_sec;

	if (rrd!=NULL)
	{
		if ((rrdfile=fopen(rrd, "a+"))==NULL)
		{
			printf ("1111 Couldn't open %s.\nNot loging.\n", rrd);
			exit (-1);
		}
	}
	if (rrd!=NULL)
		fprintf (rrdfile, "%li:", ut);
	
	printf ("\n\t\t0        20        40        60        80        100       120");
	for (i = 0; i < wreq->wi_len - 1; i++)
	{
		switch(i) 
		{
			case 0:
				puf=wreq->wi_val[i];
				if (rrd!=NULL)
					fprintf (rrdfile, "%d:", puf);
				printf ("\nQuality(%d):\t",puf);
				break;
			
			case 1:
				puf=wreq->wi_val[i];
				if (rrd!=NULL)
					fprintf (rrdfile, "%d:", puf);
				printf ("\nSignal(%d):  \t",puf);
				break;
				
			case 2:	
				puf=wreq->wi_val[i];
				if (rrd!=NULL)
					fprintf (rrdfile, "%d:", puf);
				printf ("\nNoise(%d):\t",puf);
				break;
		}
		// Changes color bar depending on signal quality
		if (wreq->wi_val[i] > MIN_QUALITY)
			printf ("%s", GREEN);
		else
			printf ("%s", RED);
		for (n=0; n < (wreq->wi_val[i]/2) -1; n++)
			if (n < MAX_QUALITY/2 )
				printf ("=");
		printf (">");
		printf ("%s", WHITE);
		for (;n < MAX_QUALITY/2; n++)
			printf ("-");
	}
	if (rrd!=NULL)
		fclose (rrdfile);
	printf ("\n\n\e[34m802.11b:\e[32m");
	return;
}

void wi_case_printwords(struct wi_req *wreq, int code,const char *rrd)
{
	int j;
	int puf;
	register unsigned int i;
	FILE *rrdfile;

	if (rrd!=NULL)
		if ((rrdfile=fopen(rrd, "a+"))==NULL)
		{
			printf ("Couldn't open %s.\nNot loging.\n", rrd);
			exit (-1);
		}

	for (i = 0; i < wreq->wi_len - 1; i++)
	{
		if (code == 64961)
		{
			//Channel. Posible values: 1,2,3,4,5,6,7,8,9,10,11
			printf ("\e[30;47m");
			for (j=1; j <= 16; j++)
			{
				if (wreq->wi_val[i]==j)
					printf ("\e[31;47m%d \e[30;47m",j);
				else
					if (j==16)
						printf ("%d",j);
					else
						printf ("%d ",j);
			}
			printf ("\e[0;0m");
		}
		else
		{
			// FIX THIS UGLY CODE!
			printf ("\e[30;46m");
			puf=wreq->wi_val[i];
			switch(puf) {
				case 0:
				printf ("1 2 5.5 6 11 22");
				break;
				case 1:
				printf ("\e[31;46m1\e[30;46m 2 5.5 6 11 22");
				break;
				case 2:
				printf ("1 \e[31;46m2\e[30;46m 5.5 6 11 22");
				break;
				case 5:
				printf ("1 2 \e[31;46m5.5\e[30;46m 6 11 22");
				break;
				case 6:
				printf ("1 2 5.5 \e[31;46m6\e[30;46m 11 22");
				break;
				case 11:
				printf ("1 2 5.5 6 \e[31;46m11\e[30;46m 22");
				break;
				case 22:
				printf ("1 2 5.5 6 11 \e[31;46m22\e[30;46m");
				break;
				default:
				printf ("Unknown (%d)   ", wreq->wi_val[i]);
			}
			if (rrd!=NULL)
			{
				fprintf(rrdfile, "%d\n", puf);
				fclose(rrdfile);
			}
			printf ("\e[0;0m");
		}
	}
	return;
}

void wi_ptype_printwords(struct wi_req *wreq)
{
	register unsigned int i;
	for (i = 0; i < wreq->wi_len - 1; i++)
		switch(wreq->wi_val[i]) 
		{
			case 1:
			printf ("Managed");
			break;
			case 3:
			printf ("Ad-hoc ");
			break;
			case 6:
			printf ("Master ");
			break;
			default:
			printf ("Unknown");
		}
	return;
}

void wi_printwords(struct wi_req *wreq)
{
	register unsigned int i;
	for (i = 0; i < wreq->wi_len - 1; i++)
		printf("%d ", wreq->wi_val[i]);
	return;
}

void wi_printswords(struct wi_req *wreq)
{
	register unsigned int i;
	for (i = 0; i < wreq->wi_len - 1; i++)
		printf("%d ", ((int16_t *) wreq->wi_val)[i]);
	return;
}

void wi_printhexwords(struct wi_req *wreq)
{
	register unsigned int i;
	for (i = 0; i < wreq->wi_len - 1; i++)
		printf("%x ", wreq->wi_val[i]);
	return;
}

void wi_printbool(struct wi_req *wreq)
{
	if (wreq->wi_val[0])
		printf("Enabled ");
	else
		printf("Disabled");
	return;
}

void wi_printhex(struct wi_req *wreq)
{
	register unsigned int i;
	unsigned char		*c;

	c = (unsigned char *)&wreq->wi_val;

	for (i = 0; i < (wreq->wi_len - 1) * 2; i++) 
	{
		printf("%02x", c[i]);
		if (i < ((wreq->wi_len - 1) * 2) - 1)
			printf(":");
	}
	return;
}

struct wi_table {
	int			wi_code;
	int			wi_type;
	const char		*wi_str;
};

static struct wi_table wi_table[] = {
	{ WI_RID_COMMS_QUALITY, WI_WORDS, "\e[34mStats:\t" },
	{ WI_RID_NODENAME, WI_STRING, "Station name:\t\t\t" },
	{ WI_RID_CURRENT_SSID, WI_STRING, "Current netname (SSID):\t\t" },
	{ WI_RID_CURRENT_BSSID, WI_HEXBYTES, "Access point:\t\t\t" },
	{ WI_RID_CURRENT_CHAN, WI_WORDS, "Current channel:\t\t" },
	{ WI_RID_PROMISC, WI_BOOL, "Promiscuous mode:\t\t" },
	{ WI_RID_PORTTYPE, WI_WORDS, "Connection mode:\t\t"},
	{ WI_RID_MAC_NODE, WI_HEXBYTES, "MAC address:\t\t\t"},
	{ WI_RID_CUR_TX_RATE, WI_WORDS, "TX rate:\t\t\t"},
	{ 0, 0, NULL }
};

static struct wi_table wi_crypt_table[] = {
	{ WI_RID_ENCRYPTION, WI_BOOL, "\e[34mWEP:\e[32m\nWEP encryption:\t\t\t" },
	{ WI_RID_TX_CRYPT_KEY, WI_WORDS, "TX encryption key:\t\t" },
	{ WI_RID_DEFLT_CRYPT_KEYS, WI_KEYSTRUCT, "Encryption keys:\t\t" },
	{ 0, 0, NULL }
};

static void wi_dumpinfo(const char *iface, const char *rrd)
{
	struct 	wi_req		wreq;
	int			has_wep;
	register unsigned int i;
	struct wi_table		*w;
	int quality=64835;
	int ptype=64512;
	int channel=64961;
	int txrate=64836;
	bzero((char *)&wreq, sizeof(wreq));

	wreq.wi_len = WI_MAX_DATALEN;
	wreq.wi_type = WI_RID_WEP_AVAIL;

	if (wi_getval(iface, &wreq) == 0)
		has_wep = wreq.wi_val[0];
	else
		has_wep = 0;

	w = wi_table;

	for (i = 0; w[i].wi_type; i++) 
	{
		bzero((char *)&wreq, sizeof(wreq));

		wreq.wi_len = WI_MAX_DATALEN;
		wreq.wi_type = w[i].wi_code;

		if (wi_getvalmaybe(iface, &wreq) == -1)
			continue;
		printf("%s", w[i].wi_str);
		switch(w[i].wi_type) 
		{
			case WI_STRING:
				printf ("%s", WHITE);
				wi_printstr(&wreq);
				printf ("%s", GREEN);
				break;
			case WI_WORDS:
				printf ("%s", WHITE);
				if (wreq.wi_type==quality)
					wi_bar_printwords(&wreq,rrd);
				else if (wreq.wi_type==ptype)
					wi_ptype_printwords(&wreq);
				else if (wreq.wi_type==channel)
					wi_case_printwords(&wreq,wreq.wi_type,rrd);
				else if (wreq.wi_type==txrate)
					wi_case_printwords(&wreq,wreq.wi_type,rrd);
				else
					wi_printwords(&wreq);
				printf ("%s", GREEN);
				break;
			case WI_SWORDS:
				printf ("%s", WHITE);
				wi_printswords(&wreq);
				printf ("%s", GREEN);
				break;
			case WI_HEXWORDS:
				printf ("%s", WHITE);
				wi_printhexwords(&wreq);
				printf ("%s", GREEN);
				break;
			case WI_BOOL:
				printf ("%s", WHITE);
				wi_printbool(&wreq);
				printf ("%s", GREEN);
				break;
			case WI_HEXBYTES:
				printf ("%s", WHITE);
				wi_printhex(&wreq);
				printf ("%s", GREEN);
				break;
			default:
				break;
		}	
		printf("\n");
	}

	if (has_wep) 
	{
		w = wi_crypt_table;
		for (i = 0; w[i].wi_type; i++) 
		{
			bzero((char *)&wreq, sizeof(wreq));

			wreq.wi_len = WI_MAX_DATALEN;
			wreq.wi_type = w[i].wi_code;

			if (wi_getval(iface, &wreq) == -1)
			{
				printf ("%s", WHITE);
				continue;
			}
			printf("%s", w[i].wi_str);
			switch(w[i].wi_type) {
			case WI_STRING:
				printf ("%s", WHITE);
				wi_printstr(&wreq);
				printf ("%s", GREEN);
				break;
			case WI_WORDS:
				printf ("%s", WHITE);
				if (wreq.wi_type == WI_RID_TX_CRYPT_KEY)
					wreq.wi_val[0]++;
				wi_printwords(&wreq);
				printf ("%s", GREEN);
				break;
			case WI_BOOL:
				printf ("%s", WHITE);
				wi_printbool(&wreq);
				printf ("%s", GREEN);
				break;
			case WI_HEXBYTES:
				printf ("%s", WHITE);
				wi_printhex(&wreq);
				printf ("%s", GREEN);
				break;
			case WI_KEYSTRUCT:
				printf ("%s", WHITE);
				wi_printkeys(&wreq);
				printf ("%s", GREEN);
				break;
			default:
				break;
			}	
			printf("\n");
		}
	}
	return;
}

static void wi_dumpstats(const char *iface)
{
	struct wi_req		wreq;
	struct wi_counters	*c;

	bzero((char *)&wreq, sizeof(wreq));
	wreq.wi_len = WI_MAX_DATALEN;
	wreq.wi_type = WI_RID_IFACE_STATS;

	if (wi_getval(iface, &wreq) == -1)
		errx(1, "Cannot get interface stats");

	c = (struct wi_counters *)&wreq.wi_val;

	printf("\e[34mStatistics:\e[32m\n");
	printf("%sTX Kbs:%s\t%d\t", RED, WHITE, c->wi_tx_unicast_octets/1024);
	printf("%sTX frames:%s\t%d\t", RED, WHITE, c->wi_tx_unicast_frames);
	printf("%sRX Kbs:%s\t%d\t", GREEN, WHITE, c->wi_rx_unicast_octets/1024);
	printf("%sRX frames:%s\t%d\t", GREEN, WHITE, c->wi_rx_unicast_frames);
	return;
}

void quitproc()
{
	printf("%s %s\n", CLEAR, WHITE);
	exit(0); /* normal exit status */
}

int main(int argc, char *argv[])
{
	void    rt_xaddrs __P((caddr_t, caddr_t, struct rt_addrinfo *));
	typedef void af_status __P((int, struct rt_addrinfo *));

	const char		*iface = NULL; 
	int rrd=0;
	int conta=0;
	char *rrdfeed;
	
	/* Get the interface name */
	if (argc == 1)
	{
		iface = "wi0";
	}	

	if (argc > 1 && *argv[1] != '-') 
	{
		iface = argv[1];
		optind = 2; 
	} 
	if (argc == 4 && strcmp(argv[3],"-r"))
	{
		rrd=1;
		rrdfeed=argv[3];
	}
	
	signal(SIGINT, quitproc);
	
	optreset = 1;
	opterr = 1;
	printf ("%s %s", CLEAR, INITT);
	printf ("\t%sawmu %s:%s Another Wireless Monitoring Utility for FreeBSD", RED, VERSION, BLUE);
	while(1)
	{
		conta++;
		printf (INITP);
		if (rrd==0)
			wi_dumpinfo(iface,NULL);
		else 
		{
			if (conta == 1 || conta%10 == 1 )
				wi_dumpinfo(iface,rrdfeed);
			else
				wi_dumpinfo(iface,NULL);
		}	
		wi_dumpstats(iface);
		//apm();
		usleep (500000);
	}
}
