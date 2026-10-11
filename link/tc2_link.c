/* Port-owned TC2 C139/C422 bridge. No game RAM repairs or synthesized packets.
 * Register/RX-ring research: MAME (Angelo Salese, BSD-3-Clause), SailorSat
 * PR 13421 and pocketjazzy/MAME-TC2-Public @5ff99b9. See NOTICE.md.
 * Each TCP record is a BE byte length followed by the unchanged C422 words.
 * 0x8004 records carry an optional test-only frame barrier, never game data.
 */
#include "tc2_link.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define close closesocket
#define WAIT_ERR WSAGetLastError()
#define WOULD_BLOCK WSAEWOULDBLOCK
#define IN_PROGRESS WSAEWOULDBLOCK
typedef int socklen_t;
static SOCKET peer=INVALID_SOCKET;
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#define WAIT_ERR errno
#define WOULD_BLOCK EWOULDBLOCK
#define IN_PROGRESS EINPROGRESS
static int peer=-1;
#endif
extern uint8_t *tc2_vram_ptr(uint32_t address);
extern uint64_t tc2_cycles_now(void);
extern void tc2_irq_line(uint32_t bit, int on);
extern uint32_t rr_frame;
extern uint8_t tc2_ram[];
static int initialized, enabled, side, synchronize, finishing, phase; /* 1 listen, 2 connect, 3 connected */
static uint16_t *regfile, txptr, status=4;
static uint64_t tx_due, next_poll;
static uint8_t tx[8192], incoming[65536], outgoing[65536];
static size_t txlen, incoming_len, outgoing_len;
static uint32_t peer_frame;
static unsigned long sent, received, accepted, checksums;
static FILE *trace;
static void fail(const char *why) { fprintf(stderr,"[LINK] %s (socket error %d)\n",why,WAIT_ERR); exit(6); }
static int wait_socket(int writing, int seconds) {
    fd_set f; FD_ZERO(&f); FD_SET(peer,&f); struct timeval t={seconds,0};
    return select((int)peer+1,writing?NULL:&f,writing?&f:NULL,NULL,&t);
}
static void disconnected(const char *why) {
    fprintf(stderr,"[LINK] disconnected: %s; the cabinet ROM handles link loss\n",why);
    if((intptr_t)peer>=0) close(peer);
    peer=-1; phase=0; incoming_len=outgoing_len=0;
}
static void nonblocking(void) {
#ifdef _WIN32
    u_long nb=1; if(ioctlsocket(peer,FIONBIO,&nb)) fail("nonblocking socket failed");
#else
    if(fcntl(peer,F_SETFL,fcntl(peer,F_GETFL,0)|O_NONBLOCK)<0) fail("nonblocking socket failed");
#endif
}
static void connected(void) {
    nonblocking();
    int one=1; setsockopt(peer,IPPROTO_TCP,TCP_NODELAY,(const char *)&one,sizeof one);
#ifdef SO_NOSIGPIPE
    setsockopt(peer,SOL_SOCKET,SO_NOSIGPIPE,&one,sizeof one);
#endif
    phase=3; fprintf(stderr,"[LINK] connected side=%d sync=%d\n",side,synchronize);
}
static void flush_output(void) {
    /* Bounded nonblocking work on the emulation thread. Rendering never waits
     * for a slow or absent peer. Overflow models a lost cable, not stale replay. */
    if(phase!=3 || !outgoing_len) return;
#ifdef MSG_NOSIGNAL
    const int flags=MSG_NOSIGNAL;
#else
    const int flags=0;
#endif
    int n=(int)send(peer,(const char *)outgoing,(int)outgoing_len,flags);
    if(n>0) { memmove(outgoing,outgoing+n,outgoing_len-n); outgoing_len-=n; }
    else if(n<0 && WAIT_ERR!=WOULD_BLOCK) disconnected("send failed");
}
static void send_bytes(const uint8_t *p,size_t n) {
    if(phase!=3) return;
    if(n>sizeof outgoing-outgoing_len) { disconnected("TX queue overflow"); return; }
    memcpy(outgoing+outgoing_len,p,n); outgoing_len+=n; flush_output();
}
static void record(const char *kind,const uint8_t *p,size_t n) {
    if(!trace) return;
    fprintf(trace,"{\"event\":\"%s\",\"frame\":%u,\"bytes\":%zu,\"data\":\"",kind,rr_frame,n);
    for(size_t i=0;i<n;i++) fprintf(trace,"%02x",p[i]);
    fprintf(trace,"\"}\n"); fflush(trace);
}
static void init(void) {
    if(initialized) return;
    initialized=1;
    const char *config=getenv("TC2_LINK");
    if(!config || !*config || !strcmp(config,"0")) return;
    const char *s=getenv("TC2_LINK_SIDE");
    if(!s || (strcmp(s,"0") && strcmp(s,"1"))) fail("TC2_LINK_SIDE must be 0 (red) or 1 (blue)");
    side=atoi(s); synchronize=getenv("TC2_LINK_SYNC") && !strcmp(getenv("TC2_LINK_SYNC"),"1");
#ifdef _WIN32
    WSADATA w; if(WSAStartup(MAKEWORD(2,2),&w)) fail("WSAStartup failed");
#endif
    unsigned port=0; char host[64]; int used=0;
    int listener=sscanf(config,"listen:%u%n",&port,&used)==1 && config[used]==0;
    if(!listener && !(sscanf(config,"connect:%63[^:]:%u%n",host,&port,&used)==2 && config[used]==0)) fail("TC2_LINK must be listen:port or connect:IPv4:port");
    if(port<1 || port>65535) fail("invalid link port");
    peer=socket(AF_INET,SOCK_STREAM,0); if((intptr_t)peer<0) fail("socket failed");
    struct sockaddr_in addr; memset(&addr,0,sizeof addr); addr.sin_family=AF_INET; addr.sin_port=htons((uint16_t)port);
    nonblocking(); enabled=1;
    if(listener) {
        addr.sin_addr.s_addr=htonl(INADDR_ANY); int yes=1;
        setsockopt(peer,SOL_SOCKET,SO_REUSEADDR,(const char *)&yes,sizeof yes);
        if(bind(peer,(struct sockaddr *)&addr,sizeof addr) || listen(peer,1)) fail("listen failed");
        phase=1; fprintf(stderr,"[LINK] waiting on %u side=%d\n",port,side);
    } else {
        if(inet_pton(AF_INET,host,&addr.sin_addr)!=1) fail("invalid peer IPv4 address");
        if(connect(peer,(struct sockaddr *)&addr,sizeof addr)==0) connected();
        else if(WAIT_ERR==IN_PROGRESS) phase=2;
        else disconnected("connect failed; start the listening cabinet first");
    }
    const char *path=getenv("TC2_LINK_TRACE"); if(path) { trace=fopen(path,"w"); if(!trace) fail("cannot open trace"); }

}
int tc2_link_enabled(void) { init(); return enabled; }
int tc2_link_side(void) { init(); return enabled?side:0; }
static void irq(void) { tc2_irq_line(0x2000,1); }
static void receive_records(void) {
    size_t at=0;
    while(incoming_len-at>=2) {
        uint8_t *record_data=incoming+at;
        unsigned n=(record_data[0]<<8)|record_data[1];
        int control=n==0x8004; if(control)n=4;
        if(!n || n>8192 || n%2) { disconnected("invalid record length"); return; }
        if(incoming_len-at<n+2) break;
        if(control) {
            peer_frame=((uint32_t)record_data[2]<<24)|((uint32_t)record_data[3]<<16)|(record_data[4]<<8)|record_data[5];
        } else {
            if(!regfile || (status&2)) { at+=n+2; continue; } /* ROM must consume the prior RX notification. */
            uint8_t *ram=tc2_vram_ptr(0x06200000);
            for(unsigned i=0;i<n;i++) ram[0x2000+(((regfile[6]*2)+i)&0x1fff)]=record_data[i+2];
            regfile[6]=(regfile[6]+n/2)&0xfff;
            regfile[4]=(regfile[4]+n/2)&0xfff;
            status|=2; received++; record("rx",record_data+2,n); irq();
        }
        memmove(record_data,record_data+n+2,incoming_len-at-n-2); incoming_len-=n+2;
    }
}
static void network_poll(void) {
    if(phase==1) {
        if(wait_socket(0,0)<=0) return;
        intptr_t accepted_socket=accept(peer,NULL,NULL);
        if(accepted_socket<0) return;
        close(peer); peer=accepted_socket; connected();
    } else if(phase==2) {
        if(wait_socket(1,0)<=0) return;
        int error=0; socklen_t len=sizeof error;
        if(getsockopt(peer,SOL_SOCKET,SO_ERROR,(char *)&error,&len) || error) {
            disconnected("connect failed; start the listening cabinet first"); return;
        }
        connected();
    }
    if(phase!=3)return;
    flush_output();
    if(phase!=3)return;
    receive_records();
    if(phase!=3)return;
    /* One bounded receive per poll; the kernel supplies backpressure. */
    if(incoming_len==sizeof incoming) { disconnected("RX queue overflow"); return; }
    int n=(int)recv(peer,(char *)incoming+incoming_len,(int)(sizeof incoming-incoming_len),0);
    if(n>0) { incoming_len+=n; receive_records(); }
    else if(n==0) disconnected("peer closed");
    else if(WAIT_ERR!=WOULD_BLOCK) disconnected("receive failed");
}

void tc2_link_poll(void) {
    if(!tc2_link_enabled()) return;
    uint64_t now=tc2_cycles_now();
    if(txlen && now>=tx_due) {
        uint8_t header[2]={(uint8_t)(txlen>>8),(uint8_t)txlen}; send_bytes(header,2); send_bytes(tx,txlen);
        if(phase==3) { record("tx",tx,txlen); sent++; }
        txlen=0; regfile[5]=0; regfile[3]&=~1; status|=4; irq();
    }
    if(now<next_poll)return;
    next_poll=now+16934; network_poll();
}
uint16_t tc2_link_read(unsigned k,uint16_t *regs) {
    if(!tc2_link_enabled()) return regs[k];
    regfile=regs; tc2_link_poll();
    return k==0 ? status : regs[k];
}
void tc2_link_write(unsigned k,uint16_t v,uint16_t *regs) {
    regfile=regs;
    unsigned old=regs[k]; regs[k]=v;
    if(k==0) { status=v&6; tc2_irq_line(0x2000,0); }
    if(k==1) { tc2_irq_line(0x2000,v==0xfffb); }
    if(k==7) txptr=v;
    /* The ROM arms START=3, sets OFFSET/SIZE, clears prior IRQ flags and then
     * closes setup with START=2. DMA begins after that programming sequence. */
    if(k==3 && (old&1) && !(v&1) && regs[5]) {
        if(txlen) fail("TX programmed while busy");
        unsigned words=regs[5]; if(words>4096) fail("TX size exceeds RAM");
        uint8_t *ram=tc2_vram_ptr(0x06200000);
        txlen=words*2;
        for(size_t i=0;i<txlen;i++) tx[i]=ram[((txptr*2)+i)&0x3fff];
        txptr=(txptr+words)&0x1fff;
        status&=~4;
        tx_due=tc2_cycles_now()+169344; /* bounded asynchronous serializer, 1 ms */
    }
}
static uint32_t ram32(unsigned p) { return ((uint32_t)tc2_ram[p]<<24)|(tc2_ram[p+1]<<16)|(tc2_ram[p+2]<<8)|tc2_ram[p+3]; }
void tc2_link_observe(uint32_t pc) {
    if(pc==0x8000B8F0) accepted++;
    if(pc==0x8000BF38) checksums++;

}
void tc2_link_vblank(uint32_t frame) {
    if(tc2_link_enabled() && synchronize) {
        time_t connected_start=time(NULL);
        while(phase!=3) {
            network_poll();
            if(!phase || time(NULL)-connected_start>10) fail("test peer connection timeout");
            if(phase!=3) wait_socket(phase==2,1);
        }
        uint8_t token[6]={0x80,4,(uint8_t)(frame>>24),(uint8_t)(frame>>16),(uint8_t)(frame>>8),(uint8_t)frame};
        send_bytes(token,sizeof token);
        time_t start=time(NULL);
        while(peer_frame<frame-1) { network_poll(); if(time(NULL)-start>10) fail("frame barrier timeout"); if(peer_frame<frame-1) { if(phase!=3) fail("test peer disconnected"); wait_socket(0,1); } }
    }
    if((enabled || getenv("TC2_LINK_DIAG")) && frame%60==0) {
        fprintf(stderr,"[LINKSTATE] frame=%u side=%d tx=%lu rx=%lu accepted=%lu fresh=%u drift=%u mode=%u\n",frame,side,sent,received,accepted,tc2_ram[0x2f3502],(tc2_ram[0x2f3504]<<8)|tc2_ram[0x2f3505],ram32(0x2f3fd0));
    }
}
void tc2_link_report(void) {
    fprintf(stderr,"[LINK] summary tx=%lu rx=%lu accepted=%lu validators=%lu\n",sent,received,accepted,checksums);
    if(trace) fflush(trace);
    if(enabled && phase==3 && synchronize && !finishing) {
        finishing=1;
        const uint8_t done[6]={0x80,4,255,255,255,255}; send_bytes(done,6);
        time_t start=time(NULL);
        while(peer_frame!=UINT32_MAX) { network_poll(); if(time(NULL)-start>10) fail("finish timeout"); if(peer_frame!=UINT32_MAX) { if(phase!=3) fail("test peer disconnected"); wait_socket(0,1); } }
#ifdef _WIN32
        shutdown(peer,SD_SEND);
#else
        shutdown(peer,SHUT_WR);
#endif
        /* Drain the peer's half-close so queued bytes cannot cause TCP RST. */
        char discard[8192];
        while(wait_socket(0,2)>0) { int n=(int)recv(peer,discard,sizeof discard,0); if(n<=0)break; }
    }
}
