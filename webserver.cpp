/*
 * webserver.cpp - Terminal web na porta 2021
 *
 * Serve uma pagina HTML com terminal xterm.js
 * Cria um pseudo-terminal (pty) que executa o programa gestao
 * e faz bridge via WebSocket simples (sem biblioteca externa)
 *
 * Compilar Linux:
 *   g++ -std=c++11 -O2 -o webserver webserver.cpp -lpthread
 *
 * Executar:
 *   ./webserver         (inicia na porta 2021)
 *   Abrir: http://localhost:2021
 *
 * NOTA Windows: usar WSL ou instalar o servidor no Linux
 */

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib,"ws2_32.lib")
#define CLOSE_SOCKET(s) closesocket(s)
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#ifdef __linux__
  #include <pty.h>
  #include <utmp.h>
#elif defined(__APPLE__)
  #include <util.h>
#endif
#define CLOSE_SOCKET(s) close(s)
typedef int SOCKET;
#define INVALID_SOCKET -1
#define SOCKET_ERROR   -1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>

#define PORT 2021

/* ================================================================
 * SHA-1 para handshake WebSocket
 * ================================================================ */
static void sha1(const unsigned char* data, size_t len, unsigned char* digest) {
    uint32_t h[5]={0x67452301,0xEFCDAB89,0x98BADCFE,0x10325476,0xC3D2E1F0};
    auto rot=[](uint32_t v,int n)->uint32_t{return(v<<n)|(v>>(32-n));};
    std::vector<unsigned char> msg(data,data+len);
    msg.push_back(0x80);
    while(msg.size()%64!=56) msg.push_back(0);
    uint64_t bit_len=(uint64_t)len*8;
    for(int i=7;i>=0;--i) msg.push_back((bit_len>>(i*8))&0xFF);
    for(size_t i=0;i<msg.size();i+=64){
        uint32_t w[80];
        for(int j=0;j<16;++j) w[j]=((uint32_t)msg[i+j*4]<<24)|((uint32_t)msg[i+j*4+1]<<16)|((uint32_t)msg[i+j*4+2]<<8)|(uint32_t)msg[i+j*4+3];
        for(int j=16;j<80;++j) w[j]=rot(w[j-3]^w[j-8]^w[j-14]^w[j-16],1);
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4];
        for(int j=0;j<80;++j){
            uint32_t f,k;
            if(j<20){f=(b&c)|(~b&d);k=0x5A827999;}
            else if(j<40){f=b^c^d;k=0x6ED9EBA1;}
            else if(j<60){f=(b&c)|(b&d)|(c&d);k=0x8F1BBCDC;}
            else{f=b^c^d;k=0xCA62C1D6;}
            uint32_t tmp=rot(a,5)+f+e+k+w[j];
            e=d;d=c;c=rot(b,30);b=a;a=tmp;
        }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;
    }
    for(int i=0;i<5;++i){digest[i*4]=(h[i]>>24)&0xFF;digest[i*4+1]=(h[i]>>16)&0xFF;digest[i*4+2]=(h[i]>>8)&0xFF;digest[i*4+3]=h[i]&0xFF;}
}

/* Base64 encode */
static std::string base64(const unsigned char* in, size_t len) {
    static const char t[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string r;
    for(size_t i=0;i<len;i+=3){
        uint32_t v=((uint32_t)in[i]<<16)|(i+1<len?(uint32_t)in[i+1]<<8:0)|(i+2<len?(uint32_t)in[i+2]:0);
        r+=t[(v>>18)&63]; r+=t[(v>>12)&63];
        r+=(i+1<len)?t[(v>>6)&63]:'=';
        r+=(i+2<len)?t[v&63]:'=';
    }
    return r;
}

/* ================================================================
 * Pagina HTML com xterm.js
 * ================================================================ */
static const std::string HTML_PAGE = R"HTMLEOF(<!DOCTYPE html>
<html lang="pt">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>TechFix - Sistema de Gestão</title>
<link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/xterm@5.3.0/css/xterm.min.css"/>
<style>
  * { margin:0; padding:0; box-sizing:border-box; }
  body {
    background:#0a0a0f;
    display:flex; flex-direction:column; height:100vh;
    font-family:'Segoe UI',sans-serif;
  }
  #topbar {
    background:#12121a;
    border-bottom:1px solid #1e3a5f;
    padding:10px 20px;
    display:flex; align-items:center; justify-content:space-between;
    flex-shrink:0;
  }
  #topbar .brand {
    color:#4fc3f7;
    font-size:16px; font-weight:700; letter-spacing:2px;
  }
  #topbar .brand span { color:#81d4fa; }
  #topbar .status {
    font-size:12px; color:#546e7a;
    display:flex; align-items:center; gap:8px;
  }
  #dot {
    width:8px; height:8px; border-radius:50%;
    background:#ef5350; transition:background .3s;
  }
  #dot.connected { background:#66bb6a; }
  #terminal-wrap {
    flex:1; padding:12px 16px; overflow:hidden;
    display:flex; flex-direction:column;
  }
  #terminal {
    flex:1;
    border:1px solid #1e3a5f;
    border-radius:6px;
    overflow:hidden;
  }
  .xterm { padding:8px; }
  .xterm-viewport { overflow-y:auto !important; }
  #bottombar {
    background:#12121a;
    border-top:1px solid #1e3a5f;
    padding:6px 20px;
    font-size:11px; color:#37474f;
    display:flex; gap:20px;
  }
</style>
</head>
<body>
<div id="topbar">
  <div class="brand">TECH<span>FIX</span> &nbsp;│&nbsp; Sistema de Gestão</div>
  <div class="status">
    <div id="dot"></div>
    <span id="status-txt">A ligar...</span>
  </div>
</div>
<div id="terminal-wrap">
  <div id="terminal"></div>
</div>
<div id="bottombar">
  <span>Porta 2021</span>
  <span>Terminal interativo</span>
  <span>UTF-8</span>
</div>
<script src="https://cdn.jsdelivr.net/npm/xterm@5.3.0/lib/xterm.min.js"></script>
<script src="https://cdn.jsdelivr.net/npm/xterm-addon-fit@0.8.0/lib/xterm-addon-fit.min.js"></script>
<script>
const term = new Terminal({
  cols: 80,
  rows: 26,
  fontFamily: "'Cascadia Code','Fira Code','Consolas',monospace",
  fontSize: 14,
  lineHeight: 1.2,
  theme: {
    background:    '#0d1117',
    foreground:    '#c9d1d9',
    cursor:        '#58a6ff',
    selectionBackground: '#264f78',
    black:   '#0d1117', red:     '#ff7b72',
    green:   '#3fb950', yellow:  '#d29922',
    blue:    '#58a6ff', magenta: '#bc8cff',
    cyan:    '#39c5cf', white:   '#b1bac4',
    brightBlack:   '#6e7681', brightRed:     '#ffa198',
    brightGreen:   '#56d364', brightYellow:  '#e3b341',
    brightBlue:    '#79c0ff', brightMagenta: '#d2a8ff',
    brightCyan:    '#56d4dd', brightWhite:   '#f0f6fc',
  },
  cursorBlink: true,
  allowTransparency: false,
  convertEol: false,
  scrollback: 1000,
});

const fitAddon = new FitAddon.FitAddon();
term.loadAddon(fitAddon);
term.open(document.getElementById('terminal'));
fitAddon.fit();

const dot  = document.getElementById('dot');
const stxt = document.getElementById('status-txt');
let ws;

function connect() {
  const proto = location.protocol === 'https:' ? 'wss' : 'ws';
  ws = new WebSocket(proto + '://' + location.host + '/ws');
  ws.binaryType = 'arraybuffer';

  ws.onopen = () => {
    dot.classList.add('connected');
    stxt.textContent = 'Ligado';
    /* Enviar tamanho do terminal */
    const msg = JSON.stringify({type:'resize', cols:term.cols, rows:term.rows});
    ws.send(msg);
  };

  ws.onmessage = (ev) => {
    if (typeof ev.data === 'string') {
      term.write(ev.data);
    } else {
      const arr = new Uint8Array(ev.data);
      term.write(arr);
    }
  };

  ws.onclose = () => {
    dot.classList.remove('connected');
    stxt.textContent = 'Desligado - A reconectar...';
    setTimeout(connect, 2000);
  };

  ws.onerror = () => {
    ws.close();
  };
}

/* Enviar input do utilizador */
term.onData(data => {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({type:'input', data:data}));
  }
});

/* Redimensionar */
window.addEventListener('resize', () => {
  fitAddon.fit();
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({type:'resize', cols:term.cols, rows:term.rows}));
  }
});

connect();
</script>
</body>
</html>
)HTMLEOF";

/* ================================================================
 * WebSocket frame parsing
 * ================================================================ */
static std::string wsHandshake(const std::string& key) {
    std::string k = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    unsigned char digest[20];
    sha1((const unsigned char*)k.c_str(), k.size(), digest);
    return base64(digest, 20);
}

static bool parseWSFrame(const std::vector<uint8_t>& buf, size_t& consumed,
                          bool& is_text, std::string& payload) {
    if(buf.size()<2) return false;
    bool fin    = (buf[0]&0x80)!=0;
    int  opcode = buf[0]&0x0F;
    bool masked = (buf[1]&0x80)!=0;
    uint64_t plen = buf[1]&0x7F;
    size_t hdr=2;
    if(plen==126){if(buf.size()<4)return false;plen=((uint64_t)buf[2]<<8)|buf[3];hdr=4;}
    else if(plen==127){if(buf.size()<10)return false;plen=0;for(int i=0;i<8;i++)plen=(plen<<8)|buf[2+i];hdr=10;}
    size_t total=hdr+(masked?4:0)+plen;
    if(buf.size()<total) return false;
    uint8_t mask[4]={0,0,0,0};
    if(masked){mask[0]=buf[hdr];mask[1]=buf[hdr+1];mask[2]=buf[hdr+2];mask[3]=buf[hdr+3];hdr+=4;}
    payload.resize(plen);
    for(uint64_t i=0;i<plen;++i) payload[i]=(char)(buf[hdr+i]^(masked?mask[i%4]:0));
    consumed=total;
    is_text=(opcode==1);
    if(opcode==8) return false; /* close */
    return true;
}

static std::vector<uint8_t> makeWSFrame(const std::string& data, bool text=true) {
    std::vector<uint8_t> f;
    f.push_back(text?0x81:0x82);
    size_t len=data.size();
    if(len<126){f.push_back((uint8_t)len);}
    else if(len<65536){f.push_back(126);f.push_back((len>>8)&0xFF);f.push_back(len&0xFF);}
    else{f.push_back(127);for(int i=7;i>=0;--i)f.push_back((len>>(i*8))&0xFF);}
    f.insert(f.end(),(uint8_t*)data.data(),(uint8_t*)data.data()+len);
    return f;
}

/* ================================================================
 * Estrutura de conexao WebSocket + PTY
 * ================================================================ */
struct Client {
    SOCKET sock;
    int    pty_master;  /* fd do pseudo-terminal mestre */
    pid_t  child_pid;
    bool   ws_connected;
    pthread_t thread_recv;  /* thread: socket->pty */
    pthread_t thread_send;  /* thread: pty->socket */
};

/* Mutex para envio (pode ser chamado de duas threads) */
static pthread_mutex_t send_mtx = PTHREAD_MUTEX_INITIALIZER;

static void sendWS(SOCKET sock, const std::string& data) {
    auto frame = makeWSFrame(data);
    pthread_mutex_lock(&send_mtx);
    send(sock, (const char*)frame.data(), (int)frame.size(), 0);
    pthread_mutex_unlock(&send_mtx);
}

/* ================================================================
 * Thread: ler PTY e enviar ao WebSocket
 * ================================================================ */
struct PtyToWS { SOCKET sock; int pty_fd; };

static void* ptyReader(void* arg) {
    PtyToWS* p=(PtyToWS*)arg;
    char buf[4096];
    while(true) {
        ssize_t n=read(p->pty_fd, buf, sizeof(buf));
        if(n<=0) break;
        sendWS(p->sock, std::string(buf,n));
    }
    delete p;
    return NULL;
}

/* ================================================================
 * Processar mensagens JSON do cliente
 * ================================================================ */
static void processClientMsg(const std::string& msg, int pty_fd) {
    /* formato: {"type":"input","data":"..."} ou {"type":"resize","cols":80,"rows":24} */
    /* parser minimo sem biblioteca */
    auto findVal=[&](const std::string& key) -> std::string {
        std::string k="\""+key+"\":";
        size_t p=msg.find(k);
        if(p==std::string::npos) return "";
        p+=k.size();
        while(p<msg.size()&&msg[p]==' ') ++p;
        if(p<msg.size()&&msg[p]=='"'){
            ++p; std::string r;
            while(p<msg.size()&&msg[p]!='"'){
                if(msg[p]=='\\'&&p+1<msg.size()){
                    char nx=msg[++p];
                    if(nx=='n') r+='\n';
                    else if(nx=='r') r+='\r';
                    else if(nx=='t') r+='\t';
                    else r+=nx;
                } else r+=msg[p];
                ++p;
            }
            return r;
        }
        /* numero */
        std::string r;
        while(p<msg.size()&&(isdigit(msg[p])||msg[p]=='-')) r+=msg[p++];
        return r;
    };

    std::string type=findVal("type");
    if(type=="input") {
        std::string data=findVal("data");
        if(!data.empty()) write(pty_fd, data.c_str(), data.size());
    } else if(type=="resize") {
        int cols=atoi(findVal("cols").c_str());
        int rows=atoi(findVal("rows").c_str());
        if(cols>0&&rows>0){
            struct winsize ws;
            ws.ws_col=(unsigned short)cols;
            ws.ws_row=(unsigned short)rows;
            ws.ws_xpixel=0; ws.ws_ypixel=0;
            ioctl(pty_fd, TIOCSWINSZ, &ws);
        }
    }
}

/* ================================================================
 * Tratar uma conexao HTTP/WebSocket
 * ================================================================ */
static void handleConnection(SOCKET client_fd) {
    /* Ler request HTTP */
    char buf[8192]={0};
    int n=recv(client_fd, buf, sizeof(buf)-1, 0);
    if(n<=0){ CLOSE_SOCKET(client_fd); return; }
    std::string req(buf, n);

    /* Verificar se e upgrade WebSocket */
    bool isWS = (req.find("Upgrade: websocket")!=std::string::npos ||
                 req.find("Upgrade: Websocket")!=std::string::npos);

    if(!isWS) {
        /* Servir HTML */
        std::string resp =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "Connection: close\r\n"
            "Content-Length: "+std::to_string(HTML_PAGE.size())+"\r\n"
            "\r\n" + HTML_PAGE;
        send(client_fd, resp.c_str(), (int)resp.size(), 0);
        CLOSE_SOCKET(client_fd);
        return;
    }

    /* Extrair Sec-WebSocket-Key */
    std::string ws_key;
    size_t kp=req.find("Sec-WebSocket-Key:");
    if(kp==std::string::npos){CLOSE_SOCKET(client_fd);return;}
    kp+=18; while(kp<req.size()&&req[kp]==' ')++kp;
    while(kp<req.size()&&req[kp]!='\r'&&req[kp]!='\n') ws_key+=req[kp++];
    /* trim */
    while(!ws_key.empty()&&(ws_key.back()=='\r'||ws_key.back()=='\n'||ws_key.back()==' '))
        ws_key.pop_back();

    /* Resposta de handshake */
    std::string accept=wsHandshake(ws_key);
    std::string hs =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: "+accept+"\r\n\r\n";
    send(client_fd, hs.c_str(), (int)hs.size(), 0);

    /* Criar pseudo-terminal e forkar processo gestao */
    int pty_master=-1, pty_slave=-1;
    struct winsize ws_size;
    ws_size.ws_col=80; ws_size.ws_row=26;
    ws_size.ws_xpixel=0; ws_size.ws_ypixel=0;

    if(openpty(&pty_master,&pty_slave,NULL,NULL,&ws_size)<0){
        sendWS(client_fd,"[Erro: openpty falhou]\r\n");
        CLOSE_SOCKET(client_fd);
        return;
    }

    pid_t pid=fork();
    if(pid==0){
        /* Processo filho: executar gestao */
        close(pty_master);
        setsid();
        ioctl(pty_slave, TIOCSCTTY, 0);
        dup2(pty_slave, STDIN_FILENO);
        dup2(pty_slave, STDOUT_FILENO);
        dup2(pty_slave, STDERR_FILENO);
        if(pty_slave>2) close(pty_slave);
        /* Definir TERM para suporte a ANSI */
        putenv((char*)"TERM=xterm-256color");
        putenv((char*)"COLORTERM=truecolor");
        execl("./gestao","gestao",NULL);
        /* Se execl falhar */
        write(STDOUT_FILENO,"[Erro: executavel 'gestao' nao encontrado]\r\n",44);
        exit(1);
    }

    close(pty_slave);

    if(pid<0){
        sendWS(client_fd,"[Erro: fork falhou]\r\n");
        close(pty_master);
        CLOSE_SOCKET(client_fd);
        return;
    }

    /* Thread para ler PTY e enviar ao WebSocket */
    PtyToWS* p2w=new PtyToWS;
    p2w->sock=client_fd; p2w->pty_fd=pty_master;
    pthread_t tid;
    pthread_create(&tid,NULL,ptyReader,p2w);
    pthread_detach(tid);

    /* Loop principal: ler WebSocket e escrever no PTY */
    std::vector<uint8_t> wsbuf;
    char rbuf[4096];
    while(true) {
        int rn=recv(client_fd,rbuf,sizeof(rbuf),0);
        if(rn<=0) break;
        wsbuf.insert(wsbuf.end(),(uint8_t*)rbuf,(uint8_t*)rbuf+rn);
        while(true){
            size_t consumed=0; bool is_text; std::string payload;
            if(!parseWSFrame(wsbuf,consumed,is_text,payload)) break;
            wsbuf.erase(wsbuf.begin(),wsbuf.begin()+consumed);
            processClientMsg(payload, pty_master);
        }
    }

    /* Limpar */
    kill(pid, SIGTERM);
    waitpid(pid, NULL, WNOHANG);
    close(pty_master);
    CLOSE_SOCKET(client_fd);
}

/* ================================================================
 * Thread por conexao
 * ================================================================ */
static void* clientThread(void* arg) {
    SOCKET fd=(SOCKET)(intptr_t)arg;
    handleConnection(fd);
    return NULL;
}

/* ================================================================
 * MAIN do servidor
 * ================================================================ */
int main() {
#ifdef _WIN32
    WSADATA wsa; WSAStartup(MAKEWORD(2,2),&wsa);
#else
    signal(SIGPIPE,SIG_IGN);
    signal(SIGCHLD,SIG_IGN);
#endif

    SOCKET srv=socket(AF_INET,SOCK_STREAM,0);
    if(srv==INVALID_SOCKET){perror("socket");return 1;}

    int opt=1;
    setsockopt(srv,SOL_SOCKET,SO_REUSEADDR,(const char*)&opt,sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=INADDR_ANY;
    addr.sin_port=htons(PORT);

    if(bind(srv,(struct sockaddr*)&addr,sizeof(addr))<0){
        perror("bind"); return 1;
    }
    if(listen(srv,16)<0){perror("listen");return 1;}

    std::cout << "\n";
    std::cout << "  ╔══════════════════════════════════╗\n";
    std::cout << "  ║   TECHFIX - Servidor Web         ║\n";
    std::cout << "  ╠══════════════════════════════════╣\n";
    std::cout << "  ║   Porta  : " << PORT << "                  ║\n";
    std::cout << "  ║   URL    : http://localhost:" << PORT << "  ║\n";
    std::cout << "  ║   Ctrl+C : Parar servidor        ║\n";
    std::cout << "  ╚══════════════════════════════════╝\n\n";

    while(true){
        struct sockaddr_in cli_addr;
        socklen_t cli_len=sizeof(cli_addr);
        SOCKET cli=accept(srv,(struct sockaddr*)&cli_addr,&cli_len);
        if(cli==INVALID_SOCKET) continue;
        pthread_t t;
        pthread_create(&t,NULL,clientThread,(void*)(intptr_t)cli);
        pthread_detach(t);
    }

    CLOSE_SOCKET(srv);
    return 0;
}
