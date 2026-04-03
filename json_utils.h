#pragma once
/*
 * json_utils.h - Utilitários JSON para o sistema de gestão
 * Implementação leve de parse/write JSON baseada em std::string
 */

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <functional>
#include <ctime>

/* ============================================================
 * JsonValue - representa qualquer valor JSON
 * ============================================================ */
struct JsonValue;
using JsonObject = std::map<std::string, JsonValue>;
using JsonArray  = std::vector<JsonValue>;

struct JsonValue {
    enum class Type { Null, Bool, Int, Double, String, Array, Object } type;

    bool        b{false};
    long long   i{0};
    double      d{0.0};
    std::string s;
    JsonArray   arr;
    JsonObject  obj;

    /* Construtores */
    JsonValue() : type(Type::Null) {}
    explicit JsonValue(bool v)             : type(Type::Bool),   b(v) {}
    explicit JsonValue(long long v)        : type(Type::Int),    i(v) {}
    explicit JsonValue(int v)              : type(Type::Int),    i(v) {}
    explicit JsonValue(double v)           : type(Type::Double), d(v) {}
    explicit JsonValue(const std::string& v): type(Type::String), s(v) {}
    explicit JsonValue(const char* v)      : type(Type::String), s(v) {}
    explicit JsonValue(JsonArray v)        : type(Type::Array),  arr(std::move(v)) {}
    explicit JsonValue(JsonObject v)       : type(Type::Object), obj(std::move(v)) {}

    /* Acesso fácil por chave (para Object) */
    JsonValue& operator[](const std::string& key) {
        if (type == Type::Null) { type = Type::Object; }
        return obj[key];
    }
    const JsonValue& operator[](const std::string& key) const {
        static JsonValue null_val;
        auto it = obj.find(key);
        return (it != obj.end()) ? it->second : null_val;
    }
    bool has(const std::string& key) const {
        return type == Type::Object && obj.find(key) != obj.end();
    }

    /* Conversões */
    std::string asString() const {
        if (type == Type::String) return s;
        if (type == Type::Int)    return std::to_string(i);
        if (type == Type::Double) { std::ostringstream o; o<<d; return o.str(); }
        if (type == Type::Bool)   return b ? "true" : "false";
        if (type == Type::Null)   return "";
        return "";
    }
    long long asInt() const {
        if (type == Type::Int)    return i;
        if (type == Type::Double) return (long long)d;
        if (type == Type::String) try { return std::stoll(s); } catch(...){ return 0; }
        return 0;
    }
    double asDouble() const {
        if (type == Type::Double) return d;
        if (type == Type::Int)    return (double)i;
        if (type == Type::String) try { return std::stod(s); } catch(...){ return 0.0; }
        return 0.0;
    }
    bool asBool() const {
        if (type == Type::Bool) return b;
        if (type == Type::Int)  return i != 0;
        return false;
    }
    bool isNull() const { return type == Type::Null; }
    bool isString() const { return type == Type::String; }
    bool isInt() const { return type == Type::Int; }
    bool isDouble() const { return type == Type::Double; }
    bool isBool() const { return type == Type::Bool; }
    bool isArray() const { return type == Type::Array; }
    bool isObject() const { return type == Type::Object; }
};

/* ============================================================
 * Parser JSON
 * ============================================================ */
class JsonParser {
    const std::string& src;
    size_t pos{0};

    void skipWs() {
        while (pos < src.size() && (src[pos]==' '||src[pos]=='\t'||src[pos]=='\n'||src[pos]=='\r'))
            ++pos;
    }
    char peek() { return pos < src.size() ? src[pos] : '\0'; }
    char next() { return pos < src.size() ? src[pos++] : '\0'; }
    void expect(char c) {
        skipWs();
        if (next() != c) throw std::runtime_error(std::string("JSON: expected '")+c+"'");
    }

    std::string parseString() {
        expect('"');
        std::string r;
        while (pos < src.size()) {
            char c = next();
            if (c == '"') return r;
            if (c == '\\') {
                char e = next();
                switch(e) {
                    case '"': r+='"'; break; case '\\': r+='\\'; break;
                    case '/': r+='/'; break; case 'n': r+='\n'; break;
                    case 't': r+='\t'; break; case 'r': r+='\r'; break;
                    default: r+=e;
                }
            } else { r += c; }
        }
        throw std::runtime_error("JSON: unterminated string");
    }

    JsonValue parseNumber() {
        size_t start = pos;
        bool isFloat = false;
        if (peek()=='-') ++pos;
        while (pos<src.size() && std::isdigit(src[pos])) ++pos;
        if (pos<src.size() && src[pos]=='.') { isFloat=true; ++pos; while(pos<src.size()&&std::isdigit(src[pos]))++pos; }
        if (pos<src.size() && (src[pos]=='e'||src[pos]=='E')) {
            isFloat=true; ++pos;
            if (pos<src.size()&&(src[pos]=='+'||src[pos]=='-'))++pos;
            while(pos<src.size()&&std::isdigit(src[pos]))++pos;
        }
        std::string num = src.substr(start, pos-start);
        if (isFloat) return JsonValue(std::stod(num));
        return JsonValue((long long)std::stoll(num));
    }

    JsonValue parseArray() {
        expect('['); skipWs();
        JsonArray arr;
        if (peek()==']') { ++pos; return JsonValue(arr); }
        while (true) {
            arr.push_back(parse());
            skipWs();
            if (peek()==']') { ++pos; return JsonValue(arr); }
            expect(',');
        }
    }

    JsonValue parseObject() {
        expect('{'); skipWs();
        JsonObject obj;
        if (peek()=='}') { ++pos; return JsonValue(obj); }
        while (true) {
            skipWs();
            std::string key = parseString();
            expect(':');
            obj[key] = parse();
            skipWs();
            if (peek()=='}') { ++pos; return JsonValue(obj); }
            expect(',');
        }
    }

public:
    explicit JsonParser(const std::string& s) : src(s) {}

    JsonValue parse() {
        skipWs();
        char c = peek();
        if (c=='"') return JsonValue(parseString());
        if (c=='{') return parseObject();
        if (c=='[') return parseArray();
        if (c=='t') { pos+=4; return JsonValue(true); }
        if (c=='f') { pos+=5; return JsonValue(false); }
        if (c=='n') { pos+=4; return JsonValue(); }
        if (c=='-' || std::isdigit(c)) return parseNumber();
        throw std::runtime_error(std::string("JSON: unexpected char '")+c+"'");
    }
};

/* ============================================================
 * Serialização JSON
 * ============================================================ */
inline std::string jsonEscapeString(const std::string& s) {
    std::string r = "\"";
    for (char c : s) {
        if (c=='"')  r+="\\\"";
        else if (c=='\\') r+="\\\\";
        else if (c=='\n') r+="\\n";
        else if (c=='\t') r+="\\t";
        else if (c=='\r') r+="\\r";
        else r+=c;
    }
    r+="\""; return r;
}

inline std::string jsonSerialize(const JsonValue& v, int indent=0) {
    std::string pad(indent*2, ' ');
    std::string pad2((indent+1)*2, ' ');
    switch(v.type) {
        case JsonValue::Type::Null:   return "null";
        case JsonValue::Type::Bool:   return v.b ? "true" : "false";
        case JsonValue::Type::Int:    return std::to_string(v.i);
        case JsonValue::Type::Double: { std::ostringstream o; o<<v.d; return o.str(); }
        case JsonValue::Type::String: return jsonEscapeString(v.s);
        case JsonValue::Type::Array: {
            if (v.arr.empty()) return "[]";
            std::string r="[\n";
            for (size_t i=0; i<v.arr.size(); ++i) {
                r += pad2 + jsonSerialize(v.arr[i], indent+1);
                if (i+1<v.arr.size()) r+=",";
                r+="\n";
            }
            return r + pad + "]";
        }
        case JsonValue::Type::Object: {
            if (v.obj.empty()) return "{}";
            std::string r="{\n";
            size_t cnt=0;
            for (auto it=v.obj.begin(); it!=v.obj.end(); ++it) {
                r += pad2 + jsonEscapeString(it->first) + ": " + jsonSerialize(it->second, indent+1);
                if (++cnt < v.obj.size()) r+=",";
                r+="\n";
            }
            return r + pad + "}";
        }
    }
    return "null";
}

/* ============================================================
 * Funções de I/O de ficheiros
 * ============================================================ */
inline JsonValue jsonParseFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return JsonValue(JsonArray{}); // retorna array vazio se não existe
    std::ostringstream buf;
    buf << f.rdbuf();
    std::string content = buf.str();
    if (content.empty() || content.find_first_not_of(" \t\r\n")==std::string::npos)
        return JsonValue(JsonArray{});
    try {
        JsonParser p(content);
        return p.parse();
    } catch (const std::exception& e) {
        std::cerr << "Erro ao ler JSON " << path << ": " << e.what() << "\n";
        return JsonValue(JsonArray{});
    }
}

inline bool jsonSaveFile(const std::string& path, const JsonValue& v) {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << jsonSerialize(v, 0) << "\n";
    return true;
}

/* Gera ID único baseado em timestamp + contagem */
inline std::string generateId(const std::string& prefix) {
    static long long counter = 0;
    auto t = ::time(nullptr);
    return prefix + std::to_string(t) + "_" + std::to_string(++counter);
}
