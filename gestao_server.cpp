/*
 * gestao_server.cpp - Servidor Central TechFix
 *
 * Porta 2022: protocolo TCP/JSON para clientes das lojas
 * Porta 2021: terminal web (xterm.js via WebSocket)
 *
 * Compilar:
 *   g++ -std=c++11 -O2 -o gestao_server gestao_server.cpp -lpthread
 *
 * Executar:
 *   ./gestao_server
 *   ./gestao_server --porta-lojas 2022 --porta-web 2021
 */

#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <csignal>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <termios.h>
#include <pty.h>
#include <utmp.h>
#include <unistd.h>

#include "json_utils.h"
#include "sha256.h"
#include "net_utils.h"
#include "protocolo.h"
#include "server_handlers.h"

static int g_porta_lojas = PROTO_PORT;   /* 2022 */
static int g_porta_web   = 2021;

/* ── Criar pastas de dados ────────────────────────────────── */
static void inicializarDiretorios() {
    mkdir(SRV_DATA, 0755);
    mkdir(SRV_DOCS, 0755);
}

/* ── Criar admin padrao se nao existir ────────────────────── */
static void inicializarAdmin() {
    JsonValue utils = jsonParseFile(F_UTIL);
    if (!utils.isArray()) utils = JsonValue(JsonArray{});
    if (!utils.arr.empty()) return;

    JsonObject admin;
    admin["id"]       = JsonValue(srvGenId("usr_"));
    admin["username"] = JsonValue(std::string("adm"));
    admin["password"] = JsonValue(hashPassword("admin123"));
    admin["nome"]     = JsonValue(std::string("Administrador"));
    admin["role"]     = JsonValue(std::string("admin"));
    admin["loja_id"]  = JsonValue(std::string(""));
    admin["estado"]   = JsonValue(std::string("ativo"));
    admin["criado_em"]= JsonValue(srvDataAtual());

    utils.arr.push_back(JsonValue(admin));
    jsonSaveFile(F_UTIL, utils);

    printf("  [!] Admin padrao criado: username=adm  password=admin123\n");
    printf("      Altere a password apos o primeiro login!\n\n");
}

/* ================================================================
 * Thread de cliente de loja (protocolo TCP/JSON)
 * ================================================================ */
struct ClientCtx {
    net_sock_t sock;
    char       ip[INET_ADDRSTRLEN];
};

static void* threadCliente(void* arg) {
    ClientCtx* ctx = (ClientCtx*)arg;
    net_sock_t sock = ctx->sock;
    char ip[INET_ADDRSTRLEN];
    strncpy(ip, ctx->ip, sizeof(ip));
    delete ctx;

    SrvSessao sessao;
    printf("  [+] Loja ligada: %s\n", ip);
    fflush(stdout);

    std::string line;
    while (netReadLine(sock, line, 30000)) {
        JsonValue req = netParseLine(line);
        if (req.isNull()) {
            netSendLine(sock, mkErr("JSON invalido"));
            continue;
        }

        std::string cmd = req["cmd"].asString();
        if (cmd.empty()) {
            netSendLine(sock, mkErr("Campo 'cmd' em falta"));
            continue;
        }

        if (cmd == CMD_LOGOUT) {
            printf("  [-] Logout: %s (%s)\n", sessao.username.c_str(), ip);
            fflush(stdout);
            break;
        }

        JsonValue resp = srvDispatch(cmd, req, sessao);
        netSendLine(sock, resp);
    }

    printf("  [-] Loja desligada: %s (%s)\n", sessao.username.empty() ? "?" : sessao.username.c_str(), ip);
    fflush(stdout);
    NET_CLOSE(sock);
    return NULL;
}

/* ================================================================
 * Servidor de lojas (porta 2022)
 * ================================================================ */
static void* threadServidorLojas(void* /*arg*/) {
    net_sock_t srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv == NET_INVALID) { perror("socket lojas"); return NULL; }

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#ifdef SO_REUSEPORT
    setsockopt(srv, SOL_SOCKET, SO_REUSEPORT, (const char*)&opt, sizeof(opt));
#endif

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(g_porta_lojas);

    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind lojas"); NET_CLOSE(srv); return NULL;
    }
    listen(srv, 32);
    printf("  Servidor de lojas  : 0.0.0.0:%d\n", g_porta_lojas);
    fflush(stdout);

    while (true) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        net_sock_t cli = accept(srv, (struct sockaddr*)&cli_addr, &cli_len);
        if (cli == NET_INVALID) continue;

        ClientCtx* ctx = new ClientCtx;
        ctx->sock = cli;
        inet_ntop(AF_INET, &cli_addr.sin_addr, ctx->ip, sizeof(ctx->ip));

        pthread_t t;
        pthread_create(&t, NULL, threadCliente, ctx);
        pthread_detach(t);
    }
    NET_CLOSE(srv);
    return NULL;
}

/* ================================================================
 * Servidor Web (porta 2021) - integrado do webserver.cpp
 * SHA-1 para WebSocket handshake
 * ================================================================ */
static void sha1ws(const unsigned char* data, size_t len, unsigned char* digest) {
    uint32_t h[5]={0x67452301,0xEFCDAB89,0x98BADCFE,0x10325476,0xC3D2E1F0};
    auto rot=[](uint32_t v,int n)->uint32_t{return(v<<n)|(v>>(32-n));};
    std::vector<unsigned char> msg(data,data+len);
    msg.push_back(0x80);
    while(msg.size()%64!=56) msg.push_back(0);
    uint64_t bl=(uint64_t)len*8;
    for(int i=7;i>=0;--i) msg.push_back((bl>>(i*8))&0xFF);
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
static std::string b64ws(const unsigned char* in, size_t len) {
    static const char t[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string r;
    for(size_t i=0;i<len;i+=3){
        uint32_t v=((uint32_t)in[i]<<16)|(i+1<len?(uint32_t)in[i+1]<<8:0)|(i+2<len?(uint32_t)in[i+2]:0);
        r+=t[(v>>18)&63];r+=t[(v>>12)&63];r+=(i+1<len)?t[(v>>6)&63]:'=';r+=(i+2<len)?t[v&63]:'=';
    }
    return r;
}
static std::string wsAccept(const std::string& key) {
    std::string k=key+"258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    unsigned char d[20]; sha1ws((const unsigned char*)k.c_str(),k.size(),d);
    return b64ws(d,20);
}
static std::vector<uint8_t> wsMakeFrame(const std::string& data) {
    std::vector<uint8_t> f; f.push_back(0x81);
    size_t len=data.size();
    if(len<126)f.push_back((uint8_t)len);
    else if(len<65536){f.push_back(126);f.push_back((len>>8)&0xFF);f.push_back(len&0xFF);}
    else{f.push_back(127);for(int i=7;i>=0;--i)f.push_back((len>>(i*8))&0xFF);}
    f.insert(f.end(),(uint8_t*)data.data(),(uint8_t*)data.data()+len);
    return f;
}
static bool wsParseFrame(const std::vector<uint8_t>& buf,size_t& consumed,std::string& payload){
    if(buf.size()<2)return false;
    int op=buf[0]&0x0F; if(op==8)return false;
    bool masked=(buf[1]&0x80)!=0;
    uint64_t plen=buf[1]&0x7F;
    size_t hdr=2;
    if(plen==126){if(buf.size()<4)return false;plen=((uint64_t)buf[2]<<8)|buf[3];hdr=4;}
    else if(plen==127){if(buf.size()<10)return false;plen=0;for(int i=0;i<8;i++)plen=(plen<<8)|buf[2+i];hdr=10;}
    size_t total=hdr+(masked?4:0)+plen;
    if(buf.size()<total)return false;
    uint8_t mask[4]={0,0,0,0};
    if(masked){mask[0]=buf[hdr];mask[1]=buf[hdr+1];mask[2]=buf[hdr+2];mask[3]=buf[hdr+3];hdr+=4;}
    payload.resize(plen);
    for(uint64_t i=0;i<plen;++i)payload[i]=(char)(buf[hdr+i]^(masked?mask[i%4]:0));
    consumed=total; return true;
}

static const std::string WEB_HTML = R"HTML(<!DOCTYPE html>
<html lang="pt"><head><meta charset="UTF-8"><title>TechFix — Gestao</title>
<link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/xterm@5.3.0/css/xterm.min.css"/>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#0d1117;display:flex;flex-direction:column;height:100vh;font-family:'Segoe UI',sans-serif}
#bar{background:#161b22;border-bottom:1px solid #21262d;padding:10px 20px;display:flex;align-items:center;justify-content:space-between;flex-shrink:0}
#bar .brand{color:#58a6ff;font-size:15px;font-weight:700;letter-spacing:2px}
#bar .brand span{color:#79c0ff}
#bar .st{font-size:12px;color:#484f58;display:flex;align-items:center;gap:8px}
#dot{width:8px;height:8px;border-radius:50%;background:#f85149;transition:background .3s}
#dot.on{background:#3fb950}
#wrap{flex:1;padding:10px 14px;overflow:hidden;display:flex;flex-direction:column}
#terminal{flex:1;border:1px solid #21262d;border-radius:6px;overflow:hidden}
.xterm{padding:6px}
#foot{background:#161b22;border-top:1px solid #21262d;padding:5px 20px;font-size:11px;color:#484f58;display:flex;gap:20px}
</style>
</head><body>
<div id="bar">
  <div class="brand">TECH<span>FIX</span> &nbsp;|&nbsp; Sistema de Gestao</div>
  <div class="st"><div id="dot"></div><span id="stxt">A ligar...</span></div>
</div>
<div id="wrap"><div id="terminal"></div></div>
<div id="foot"><span>Servidor Central</span><span>Porta )HTML" + std::to_string(2021) + R"HTML(</span><span>UTF-8</span></div>
<script src="https://cdn.jsdelivr.net/npm/xterm@5.3.0/lib/xterm.min.js"></script>
<script src="https://cdn.jsdelivr.net/npm/xterm-addon-fit@0.8.0/lib/xterm-addon-fit.min.js"></script>
<script>
const term=new Terminal({cols:80,rows:26,fontFamily:"'Cascadia Code','Fira Code',Consolas,monospace",fontSize:14,lineHeight:1.2,theme:{background:'#0d1117',foreground:'#c9d1d9',cursor:'#58a6ff',selectionBackground:'#264f78',black:'#0d1117',red:'#ff7b72',green:'#3fb950',yellow:'#d29922',blue:'#58a6ff',magenta:'#bc8cff',cyan:'#39c5cf',white:'#b1bac4',brightBlack:'#6e7681',brightRed:'#ffa198',brightGreen:'#56d364',brightYellow:'#e3b341',brightBlue:'#79c0ff',brightMagenta:'#d2a8ff',brightCyan:'#56d4dd',brightWhite:'#f0f6fc'},cursorBlink:true});
const fit=new FitAddon.FitAddon();
term.loadAddon(fit);term.open(document.getElementById('terminal'));fit.fit();
const dot=document.getElementById('dot'),stxt=document.getElementById('stxt');
let ws;
function connect(){
  ws=new WebSocket((location.protocol==='https:'?'wss':'ws')+'://'+location.host+'/ws');
  ws.binaryType='arraybuffer';
  ws.onopen=()=>{dot.classList.add('on');stxt.textContent='Ligado';ws.send(JSON.stringify({type:'resize',cols:term.cols,rows:term.rows}));};
  ws.onmessage=ev=>{if(typeof ev.data==='string')term.write(ev.data);else term.write(new Uint8Array(ev.data));};
  ws.onclose=()=>{dot.classList.remove('on');stxt.textContent='Desligado — A reconectar...';setTimeout(connect,2000);};
  ws.onerror=()=>ws.close();
}
term.onData(d=>{if(ws&&ws.readyState===1)ws.send(JSON.stringify({type:'input',data:d}));});
window.addEventListener('resize',()=>{fit.fit();if(ws&&ws.readyState===1)ws.send(JSON.stringify({type:'resize',cols:term.cols,rows:term.rows}));});
connect();
</script></body></html>)HTML";

static pthread_mutex_t g_ws_mtx = PTHREAD_MUTEX_INITIALIZER;
static void wsSend(net_sock_t sock, const std::string& data) {
    auto frame=wsMakeFrame(data);
    pthread_mutex_lock(&g_ws_mtx);
    send(sock,(const char*)frame.data(),(int)frame.size(),0);
    pthread_mutex_unlock(&g_ws_mtx);
}
struct PtyCtx { net_sock_t sock; int pty_fd; };
static void* ptyRead(void* arg) {
    PtyCtx* p=(PtyCtx*)arg; char buf[4096];
    while(true){ssize_t n=read(p->pty_fd,buf,sizeof(buf));if(n<=0)break;wsSend(p->sock,std::string(buf,n));}
    delete p; return NULL;
}
static void wsProcessMsg(const std::string& msg, int pty_fd) {
    auto fv=[&](const std::string& key)->std::string{
        std::string k="\""+key+"\":"; size_t p=msg.find(k);
        if(p==std::string::npos)return "";
        p+=k.size(); while(p<msg.size()&&msg[p]==' ')++p;
        if(p<msg.size()&&msg[p]=='"'){++p;std::string r;
            while(p<msg.size()&&msg[p]!='"'){if(msg[p]=='\\'&&p+1<msg.size()){char nx=msg[++p];if(nx=='n')r+='\n';else if(nx=='r')r+='\r';else if(nx=='t')r+='\t';else r+=nx;}else r+=msg[p];++p;}return r;}
        std::string r; while(p<msg.size()&&(isdigit(msg[p])||msg[p]=='-'))r+=msg[p++]; return r;
    };
    std::string type=fv("type");
    if(type=="input"){std::string d=fv("data");if(!d.empty()){(void)write(pty_fd,d.c_str(),d.size());}}
    else if(type=="resize"){int cols=atoi(fv("cols").c_str()),rows=atoi(fv("rows").c_str());
        if(cols>0&&rows>0){struct winsize ws;ws.ws_col=(unsigned short)cols;ws.ws_row=(unsigned short)rows;ws.ws_xpixel=0;ws.ws_ypixel=0;ioctl(pty_fd,TIOCSWINSZ,&ws);}}
}
static void handleWebConn(net_sock_t fd) {
    char buf[8192]={0}; int n=recv(fd,buf,sizeof(buf)-1,0);
    if(n<=0){NET_CLOSE(fd);return;}
    std::string req(buf,n);
    bool isWS=(req.find("Upgrade: websocket")!=std::string::npos||req.find("Upgrade: Websocket")!=std::string::npos);
    if(!isWS){
        std::string resp="HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\nConnection: close\r\nContent-Length: "+std::to_string(WEB_HTML.size())+"\r\n\r\n"+WEB_HTML;
        send(fd,resp.c_str(),(int)resp.size(),0);NET_CLOSE(fd);return;
    }
    std::string key; size_t kp=req.find("Sec-WebSocket-Key:");
    if(kp==std::string::npos){NET_CLOSE(fd);return;}
    kp+=18; while(kp<req.size()&&req[kp]==' ')++kp;
    while(kp<req.size()&&req[kp]!='\r'&&req[kp]!='\n')key+=req[kp++];
    while(!key.empty()&&(key.back()=='\r'||key.back()=='\n'||key.back()==' '))key.pop_back();
    std::string hs="HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: "+wsAccept(key)+"\r\n\r\n";
    send(fd,hs.c_str(),(int)hs.size(),0);

    int pty_m=-1,pty_s=-1;
    struct winsize ws_sz; ws_sz.ws_col=80;ws_sz.ws_row=26;ws_sz.ws_xpixel=0;ws_sz.ws_ypixel=0;
    if(openpty(&pty_m,&pty_s,NULL,NULL,&ws_sz)<0){wsSend(fd,"[Erro: openpty]\r\n");NET_CLOSE(fd);return;}
    pid_t pid=fork();
    if(pid==0){
        close(pty_m);setsid();ioctl(pty_s,TIOCSCTTY,0);
        dup2(pty_s,0);dup2(pty_s,1);dup2(pty_s,2);if(pty_s>2)close(pty_s);
        putenv((char*)"TERM=xterm-256color");putenv((char*)"COLORTERM=truecolor");
        execl("./gestao_loja","gestao_loja",NULL);
        (void)write(1,"[Erro: gestao_loja nao encontrado. Compile com: make]\r\n",55);exit(1);
    }
    close(pty_s);
    PtyCtx* pc=new PtyCtx; pc->sock=fd; pc->pty_fd=pty_m;
    pthread_t t; pthread_create(&t,NULL,ptyRead,pc); pthread_detach(t);

    std::vector<uint8_t> wsbuf; char rbuf[4096];
    while(true){
        int rn=recv(fd,rbuf,sizeof(rbuf),0);if(rn<=0)break;
        wsbuf.insert(wsbuf.end(),(uint8_t*)rbuf,(uint8_t*)rbuf+rn);
        while(true){size_t consumed=0;std::string payload;
            if(!wsParseFrame(wsbuf,consumed,payload))break;
            wsbuf.erase(wsbuf.begin(),wsbuf.begin()+consumed);
            wsProcessMsg(payload,pty_m);}
    }
    kill(pid,SIGTERM);waitpid(pid,NULL,WNOHANG);close(pty_m);NET_CLOSE(fd);
}
static void* threadWebConn(void* arg) {
    net_sock_t fd=(net_sock_t)(intptr_t)arg;
    handleWebConn(fd);return NULL;
}
static void* threadServidorWeb(void* /*arg*/) {
    net_sock_t srv=socket(AF_INET,SOCK_STREAM,0);
    if(srv==NET_INVALID){perror("socket web");return NULL;}
    int opt=1;
    setsockopt(srv,SOL_SOCKET,SO_REUSEADDR,(const char*)&opt,sizeof(opt));
#ifdef SO_REUSEPORT
    setsockopt(srv,SOL_SOCKET,SO_REUSEPORT,(const char*)&opt,sizeof(opt));
#endif
    struct sockaddr_in addr; memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET;addr.sin_addr.s_addr=INADDR_ANY;addr.sin_port=htons(g_porta_web);
    if(bind(srv,(struct sockaddr*)&addr,sizeof(addr))<0){perror("bind web");NET_CLOSE(srv);return NULL;}
    listen(srv,32);
    printf("  Terminal web       : http://0.0.0.0:%d\n", g_porta_web);
    fflush(stdout);
    while(true){
        struct sockaddr_in ca; socklen_t cl=sizeof(ca);
        net_sock_t cli=accept(srv,(struct sockaddr*)&ca,&cl);
        if(cli==NET_INVALID)continue;
        pthread_t t; pthread_create(&t,NULL,threadWebConn,(void*)(intptr_t)cli); pthread_detach(t);
    }
    NET_CLOSE(srv); return NULL;
}

/* ================================================================
 * MAIN
 * ================================================================ */
int main(int argc, char* argv[]) {
    /* Parsear argumentos */
    for(int i=1; i<argc; ++i) {
        if(strcmp(argv[i],"--porta-lojas")==0 && i+1<argc) g_porta_lojas=atoi(argv[++i]);
        if(strcmp(argv[i],"--porta-web")==0   && i+1<argc) g_porta_web  =atoi(argv[++i]);
    }

    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);
    netInit();

    inicializarDiretorios();
    inicializarAdmin();

    printf("\n");
    printf("  +==========================================+\n");
    printf("  |   TECHFIX — Servidor Central             |\n");
    printf("  +==========================================+\n");
    printf("  |                                          |\n");

    /* Iniciar servidor de lojas */
    pthread_t t_lojas;
    pthread_create(&t_lojas, NULL, threadServidorLojas, NULL);
    pthread_detach(t_lojas);

    /* Iniciar servidor web */
    pthread_t t_web;
    pthread_create(&t_web, NULL, threadServidorWeb, NULL);
    pthread_detach(t_web);

    sleep(1); /* dar tempo aos servidores para iniciar */

    /* Detectar IP local */
    int tmp_s=socket(AF_INET,SOCK_DGRAM,0);
    struct sockaddr_in dst; memset(&dst,0,sizeof(dst));
    dst.sin_family=AF_INET; dst.sin_port=htons(53); dst.sin_addr.s_addr=inet_addr("8.8.8.8");
    connect(tmp_s,(struct sockaddr*)&dst,sizeof(dst));
    struct sockaddr_in src; socklen_t slen=sizeof(src);
    getsockname(tmp_s,(struct sockaddr*)&src,&slen); close(tmp_s);
    char local_ip[INET_ADDRSTRLEN]; inet_ntop(AF_INET,&src.sin_addr,local_ip,sizeof(local_ip));

    printf("  |  IP local : %-28s|\n", local_ip);
    printf("  |  Lojas    : TCP port %-19d|\n", g_porta_lojas);
    printf("  |  Web      : http://%-22s|\n", (std::string(local_ip)+":"+std::to_string(g_porta_web)+"/").c_str());
    printf("  |                                          |\n");
    printf("  |  Ctrl+C   : Parar servidor               |\n");
    printf("  |                                          |\n");
    printf("  +==========================================+\n\n");
    printf("  Aguardando ligacoes...\n\n");
    fflush(stdout);

    /* Esperar para sempre (threads tratam das ligacoes) */
    while(true) sleep(3600);
    return 0;
}
