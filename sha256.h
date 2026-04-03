#pragma once
/*
 * sha256.h - Implementação SHA-256 em C++ puro
 * Sem dependências externas
 */

#include <string>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace sha256_impl {

static const uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

#define ROTR(x,n) (((x)>>(n))|((x)<<(32-(n))))
#define CH(x,y,z)  (((x)&(y))^(~(x)&(z)))
#define MAJ(x,y,z) (((x)&(y))^((x)&(z))^((y)&(z)))
#define SIG0(x) (ROTR(x,2)^ROTR(x,13)^ROTR(x,22))
#define SIG1(x) (ROTR(x,6)^ROTR(x,11)^ROTR(x,25))
#define sig0(x) (ROTR(x,7)^ROTR(x,18)^((x)>>3))
#define sig1(x) (ROTR(x,17)^ROTR(x,19)^((x)>>10))

inline std::string sha256(const std::string& msg) {
    // Estado inicial
    uint32_t h[8] = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };

    // Padding
    std::string data = msg;
    uint64_t bit_len = (uint64_t)msg.size() * 8;
    data += (char)0x80;
    while ((data.size() % 64) != 56) data += (char)0x00;
    for (int i=7; i>=0; --i) data += (char)((bit_len >> (i*8)) & 0xFF);

    // Processar blocos de 64 bytes
    for (size_t i=0; i<data.size(); i+=64) {
        uint32_t w[64];
        for (int j=0; j<16; ++j) {
            w[j] = ((uint8_t)data[i+j*4]<<24)|((uint8_t)data[i+j*4+1]<<16)|
                   ((uint8_t)data[i+j*4+2]<<8)|((uint8_t)data[i+j*4+3]);
        }
        for (int j=16; j<64; ++j)
            w[j] = sig1(w[j-2])+w[j-7]+sig0(w[j-15])+w[j-16];

        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for (int j=0; j<64; ++j) {
            uint32_t T1 = hh + SIG1(e) + CH(e,f,g) + K[j] + w[j];
            uint32_t T2 = SIG0(a) + MAJ(a,b,c);
            hh=g; g=f; f=e; e=d+T1; d=c; c=b; b=a; a=T1+T2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d;
        h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
    }

    std::ostringstream oss;
    for (int i=0; i<8; ++i) oss << std::hex << std::setw(8) << std::setfill('0') << h[i];
    return oss.str();
}

} // namespace

inline std::string hashPassword(const std::string& pw) {
    return sha256_impl::sha256(pw);
}
