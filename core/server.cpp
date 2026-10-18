#include "server.hpp"

#include <sys/stat.h>

#include <cctype>
#include <cmath>
#include <fstream>

namespace zsv {

int32_t parseInt(const std::string& s) {
  if (s.empty()) throw NumberFormat("For input string: \"\"");
  size_t i = 0;
  bool neg = false;
  if (s[0] == '-' || s[0] == '+') { neg = s[0] == '-'; i = 1; if (s.size() == 1) throw NumberFormat("For input string: \"" + s + "\""); }
  int64_t v = 0;
  for (; i < s.size(); i++) {
    if (s[i] < '0' || s[i] > '9') throw NumberFormat("For input string: \"" + s + "\"");
    v = v * 10 + (s[i] - '0');
    if (v > 2147483648LL) throw NumberFormat("For input string: \"" + s + "\"");
  }
  if (neg) v = -v;
  if (v > INT32_MAX || v < INT32_MIN) throw NumberFormat("For input string: \"" + s + "\"");
  return (int32_t)v;
}

int64_t parseLong(const std::string& s) {
  if (s.empty()) throw NumberFormat("For input string: \"\"");
  size_t i = 0;
  bool neg = false;
  if (s[0] == '-' || s[0] == '+') { neg = s[0] == '-'; i = 1; if (s.size() == 1) throw NumberFormat("For input string: \"" + s + "\""); }
  unsigned long long v = 0;
  const unsigned long long lim = neg ? 9223372036854775808ULL : 9223372036854775807ULL;
  for (; i < s.size(); i++) {
    if (s[i] < '0' || s[i] > '9') throw NumberFormat("For input string: \"" + s + "\"");
    unsigned d = s[i] - '0';
    if (v > (lim - d) / 10) throw NumberFormat("For input string: \"" + s + "\"");
    v = v * 10 + d;
  }
  return neg ? (int64_t)(0 - v) : (int64_t)v;
}

std::vector<std::string> split(const std::string& s, char sep) {
  std::vector<std::string> r;
  size_t start = 0;
  for (size_t i = 0; i <= s.size(); i++) {
    if (i == s.size() || s[i] == sep) { r.push_back(s.substr(start, i - start)); start = i + 1; }
  }
  // Java String.split removes trailing empty strings (but "" -> [""])
  if (s.empty()) return {""};
  while (!r.empty() && r.back().empty()) r.pop_back();
  return r;
}

std::string replaceAll(std::string s, const std::string& from, const std::string& to) {
  if (from.empty()) return s;
  size_t p = 0;
  while ((p = s.find(from, p)) != std::string::npos) { s.replace(p, from.size(), to); p += to.size(); }
  return s;
}

std::string toUpperUS(std::string s) {
  for (char& c : s) c = (char)toupper((unsigned char)c);
  return s;
}

std::string str(const Value& v) { return v.javaToString(); }

size_t Request::read(uint8_t* out, size_t n) {
  size_t avail = body.size() - readPos;
  size_t k = std::min(n, avail);
  if (k) memcpy(out, body.data() + readPos, k);
  readPos += k;
  return k;
}

OfflineServer::OfflineServer(const Config& c) : cfg(c) {
  assets.root = cfg.assetsDir;
  assets.livedataRoot = cfg.livedataDir;
  livedataBaseUrl = cfg.livedataBaseUrl;
  mkdir(cfg.dataDir.c_str(), 0755);
  ziruDb = std::make_unique<zs::ZiruDb>(cfg.dataDir + "/ziru_save.db");
}

zs::Prefs& OfflineServer::sp(const std::string& name) {
  auto it = prefs.find(name);
  if (it != prefs.end()) return *it->second;
  auto p = std::make_unique<zs::Prefs>(cfg.dataDir + "/prefs_" + name + ".json");
  zs::Prefs& ref = *p;
  prefs[name] = std::move(p);
  return ref;
}

std::string OfflineServer::readAsset(const std::string& name) {
  std::string out;
  if (!assets.read(name, out)) throw IOEx(name);
  return out;
}

Response OfflineServer::newFixed(int status, const std::string& mime, Bytes body) {
  Response r;
  r.status = status;
  r.mime = mime;
  r.body = std::move(body);
  return r;
}

Response OfflineServer::newFixedText(int status, const std::string& text) {
  return newFixed(status, "text/plain", Bytes(text.begin(), text.end()));
}

void OfflineServer::logRequest(const std::string& a, const std::string& b) {
  if (cfg.logFile.empty()) return;
  std::ofstream f(cfg.logFile, std::ios::app);
  if (f) f << a << " | " << b << "\n";
}

void OfflineServer::startupAudit() {
  std::lock_guard<std::recursive_mutex> g(mu);
  isZiruLocalCommentaryInstalled();
  auditZiruCriticalAssetsV81();
  auditZiruWorldCupAssetsV86();
}

std::string OfflineServer::redirectUrl(const std::string& url) {
  // replaceFirst("https?://[^/]+", "https://127.0.0.1:8085")
  size_t p = std::string::npos;
  for (size_t i = 0; i + 7 <= url.size(); i++) {
    if (url.compare(i, 7, "http://") == 0 || (i + 8 <= url.size() && url.compare(i, 8, "https://") == 0)) { p = i; break; }
  }
  if (p == std::string::npos) return url;
  size_t hs = url.find("://", p) + 3;
  size_t he = hs;
  while (he < url.size() && url[he] != '/') he++;
  if (he == hs) return url;
  return url.substr(0, p) + "https://127.0.0.1:8085" + url.substr(he);
}

Response OfflineServer::serve(Request& r) {
  std::lock_guard<std::recursive_mutex> g(mu);
  try {
    return serveInner(r);
  } catch (const std::exception& e) {
    // NanoHTTPD would drop the connection or answer 500
    zs::loge("OFFLINE_SERVER", std::string("serve() threw: ") + e.what());
    return newFixedText(500, std::string("SERVER INTERNAL ERROR: ") + e.what());
  }
}

}  // namespace zsv
