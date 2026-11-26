#include "web_debug.h"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

constexpr const char* kDashboardHtml = R"HTML(<!DOCTYPE html>
<html lang="cs">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
<title>WALL-E Live Debug</title>
<style>
  :root {
    --bg: #0f141b; --card: #18202c; --border: #2a3648;
    --text: #ecf2f8; --muted: #8ea0b8;
    --green: #22c55e; --red: #ef4444; --amber: #f59e0b;
    --purple: #a78bfa; --blue: #38bdf8;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { font-family: system-ui, -apple-system, sans-serif; background: var(--bg); color: var(--text); padding: 12px; max-width: 820px; margin: 0 auto; }
  .hdr { display: flex; flex-wrap: wrap; justify-content: space-between; align-items: center; gap: 8px; margin-bottom: 12px; }
  h1 { font-size: 1.15rem; color: var(--amber); letter-spacing: 0.5px; }
  .badges { display: flex; gap: 6px; flex-wrap: wrap; }
  .badge { padding: 4px 10px; border-radius: 6px; font-weight: 700; font-size: 0.8rem; background: #232e3f; color: var(--muted); }
  .b-ok { background: #14532d; color: var(--green); }
  .b-err { background: #450a0a; color: var(--red); }
  .b-cruise { background: #2e1065; color: var(--purple); }
  .b-blue { background: #0c4a6e; color: var(--blue); }
  .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(260px, 1fr)); gap: 10px; }
  .card { background: var(--card); border: 1px solid var(--border); border-radius: 10px; padding: 12px; }
  .card h2 { font-size: 0.85rem; color: var(--blue); text-transform: uppercase; margin-bottom: 8px; letter-spacing: 0.4px; }
  .bar-wrap { position: relative; height: 24px; background: #0c1016; border: 1px solid var(--border); border-radius: 5px; overflow: hidden; margin: 6px 0; }
  .bar-mid { position: absolute; left: 50%; top: 0; bottom: 0; width: 1px; background: var(--muted); }
  .bar-fill { position: absolute; top: 2px; bottom: 2px; transition: width 40ms linear, left 40ms linear; }
  .bar-lbl { position: absolute; inset: 0; display: flex; align-items: center; justify-content: center; font-family: monospace; font-size: 0.82rem; font-weight: 700; }
  .kv { display: flex; justify-content: space-between; font-family: monospace; font-size: 0.88rem; padding: 3px 0; border-bottom: 1px solid #202a38; }
  .kv:last-child { border-bottom: none; }
  .k { color: var(--muted); }
  .actions { display: flex; gap: 8px; margin-top: 12px; }
  button { flex: 1; padding: 12px; border: none; border-radius: 8px; font-weight: 700; font-size: 0.95rem; cursor: pointer; }
  .btn-estop { background: #991b1b; color: #fff; }
  .btn-estop:active { background: var(--red); }
  .btn-reset { background: #243042; color: var(--text); }
  .btn-reset:active { background: #334155; }
</style>
</head>
<body>
  <div class="hdr">
    <h1>WALL-E LIVE DEBUG</h1>
    <div class="badges">
      <span id="b-conn" class="badge">PŘIPOJUJI...</span>
      <span id="b-mode" class="badge">NONE</span>
      <span id="b-estop" class="badge b-ok">AKTIVNÍ</span>
      <span id="b-cruise" class="badge">TEMPOMAT OFF</span>
    </div>
  </div>

  <div class="grid">
    <div class="card">
      <h2>Motory (SW PWM 250 Hz)</h2>
      <div class="kv"><span class="k">Levý pás (Pin 19/21)</span><span id="v-lpwm">0 PWM</span></div>
      <div class="bar-wrap"><div class="bar-mid"></div><div id="bar-l" class="bar-fill"></div><div id="lbl-l" class="bar-lbl">0 %</div></div>
      <div class="kv"><span class="k">Pravý pás (Pin 7/11)</span><span id="v-rpwm">0 PWM</span></div>
      <div class="bar-wrap"><div class="bar-mid"></div><div id="bar-r" class="bar-fill"></div><div id="lbl-r" class="bar-lbl">0 %</div></div>
    </div>

    <div class="card">
      <h2>Enkodéry</h2>
      <div class="kv"><span class="k">Levý (Pin 23 / 29)</span><span id="v-encl">0</span></div>
      <div class="kv"><span class="k">Pravý (Pin 13 / 15)</span><span id="v-encr">0</span></div>
      <div class="kv"><span class="k">Tempomat cíl</span><span id="v-cruise">0 PWM</span></div>
      <div class="kv"><span class="k">Režim GPIO</span><span id="v-dry">—</span></div>
    </div>

    <div class="card">
      <h2>DualSense vstupy</h2>
      <div class="kv"><span class="k">R2 (vpřed) / L2 (vzad)</span><span id="v-trig">0.00 / 0.00</span></div>
      <div class="kv"><span class="k">Levá páčka (X / Y)</span><span id="v-ls">0.00 / 0.00</span></div>
      <div class="kv"><span class="k">Pravá páčka (X / Y)</span><span id="v-rs">0.00 / 0.00</span></div>
      <div class="kv"><span class="k">D-Pad / R1</span><span id="v-btns">—</span></div>
    </div>

    <div class="card">
      <h2>Superstrike vstupy</h2>
      <div class="kv"><span class="k">Levý raw ADC</span><span id="v-ssl">0</span></div>
      <div class="kv"><span class="k">Pravý raw ADC</span><span id="v-ssr">0</span></div>
      <div class="kv"><span class="k">Směr</span><span id="v-ssdir">+1</span></div>
    </div>
  </div>

  <div class="actions">
    <button class="btn-estop" onclick="post('/api/estop')">PŘEPNOUT E-STOP</button>
    <button class="btn-reset" onclick="post('/api/enc_reset')">VYNULOVAT ENKODÉRY</button>
  </div>

<script>
function setBar(barId, lblId, pwm) {
  const pct = (pwm / 255) * 100;
  const el = document.getElementById(barId);
  const w = Math.min(50, Math.abs(pct) / 2);
  if (pwm >= 0) {
    el.style.left = '50%'; el.style.width = w + '%'; el.style.background = '#22c55e';
  } else {
    el.style.left = (50 - w) + '%'; el.style.width = w + '%'; el.style.background = '#ef4444';
  }
  document.getElementById(lblId).textContent = (pct >= 0 ? '+' : '') + pct.toFixed(1) + ' % (' + pwm + ')';
}
async function post(url) { try { await fetch(url, {method:'POST'}); } catch(e) {} }
async function poll() {
  try {
    const r = await fetch('/api/state', {cache: 'no-store'});
    const s = await r.json();
    const bc = document.getElementById('b-conn');
    bc.textContent = 'ONLINE'; bc.className = 'badge b-ok';
    const bm = document.getElementById('b-mode');
    bm.textContent = s.mode; bm.className = 'badge ' + (s.mode === 'NONE' ? '' : 'b-blue');
    const be = document.getElementById('b-estop');
    be.textContent = s.controls_enabled ? 'AKTIVNÍ' : 'E-STOP';
    be.className = 'badge ' + (s.controls_enabled ? 'b-ok' : 'b-err');
    const bcr = document.getElementById('b-cruise');
    bcr.textContent = s.cruise_active ? ('TEMPOMAT ' + s.cruise_speed.toFixed(0) + ' PWM') : 'TEMPOMAT OFF';
    bcr.className = 'badge ' + (s.cruise_active ? 'b-cruise' : '');

    document.getElementById('v-lpwm').textContent = s.left_pwm + ' PWM';
    document.getElementById('v-rpwm').textContent = s.right_pwm + ' PWM';
    setBar('bar-l', 'lbl-l', s.left_pwm);
    setBar('bar-r', 'lbl-r', s.right_pwm);

    document.getElementById('v-encl').textContent = s.enc_left;
    document.getElementById('v-encr').textContent = s.enc_right;
    document.getElementById('v-cruise').textContent = s.cruise_active ? (s.cruise_speed.toFixed(1) + ' PWM (' + (s.cruise_speed/2.55).toFixed(0) + '%)') : 'OFF';
    document.getElementById('v-dry').textContent = s.dry_run ? 'DRY-RUN' : 'LIVE GPIO';

    document.getElementById('v-trig').textContent = s.r2.toFixed(2) + ' / ' + s.l2.toFixed(2);
    document.getElementById('v-ls').textContent = s.lx.toFixed(2) + ' / ' + s.ly.toFixed(2);
    document.getElementById('v-rs').textContent = s.rx.toFixed(2) + ' / ' + s.ry.toFixed(2);
    let b = [];
    if (s.dpad_up) b.push('▲'); if (s.dpad_down) b.push('▼');
    if (s.dpad_left) b.push('◀'); if (s.dpad_right) b.push('▶');
    if (s.r1) b.push('R1');
    document.getElementById('v-btns').textContent = b.length ? b.join(' ') : '—';

    document.getElementById('v-ssl').textContent = s.ss_left_raw;
    document.getElementById('v-ssr').textContent = s.ss_right_raw;
    document.getElementById('v-ssdir').textContent = s.ss_dir >= 0 ? 'VPŘED (+1)' : 'VZAD (-1)';
  } catch(e) {
    const bc = document.getElementById('b-conn');
    bc.textContent = 'OFFLINE'; bc.className = 'badge b-err';
  }
}
setInterval(poll, 50);
poll();
</script>
</body>
</html>
)HTML";

void sendAll(int fd, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (n <= 0) break;
        sent += static_cast<std::size_t>(n);
    }
}

}  // namespace

WebDebugServer::WebDebugServer(int listen_port,
                               std::function<void()> estop_cb,
                               std::function<void()> enc_reset_cb)
    : port(listen_port),
      on_estop_toggle(std::move(estop_cb)),
      on_enc_reset(std::move(enc_reset_cb)) {}

WebDebugServer::~WebDebugServer() {
    stop();
}

bool WebDebugServer::start() {
    stop();

    listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) return false;

    int opt = 1;
    ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<std::uint16_t>(port));

    if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 ||
        ::listen(listen_fd, 8) < 0) {
        std::cerr << "[WEB] Nelze spustit debug server na portu " << port << ".\n";
        ::close(listen_fd);
        listen_fd = -1;
        return false;
    }

    running.store(true);
    server_thread = std::thread(&WebDebugServer::serverLoop, this);
    std::cout << "[WEB] Live debug dashboard běží na portu " << port << " (0.0.0.0:" << port << ").\n";
    return true;
}

void WebDebugServer::stop() {
    running.store(false);
    if (server_thread.joinable()) {
        server_thread.join();
    }
    if (listen_fd >= 0) {
        ::close(listen_fd);
        listen_fd = -1;
    }
}

void WebDebugServer::updateTelemetry(const TelemetrySnapshot& next) {
    std::lock_guard<std::mutex> lock(state_mutex);
    snapshot = next;
}

std::string WebDebugServer::buildJson() const {
    TelemetrySnapshot s;
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        s = snapshot;
    }
    std::ostringstream os;
    os << std::fixed << std::setprecision(2);
    os << "{"
       << "\"mode\":\"" << s.mode << "\","
       << "\"dry_run\":" << (s.dry_run ? "true" : "false") << ","
       << "\"controls_enabled\":" << (s.controls_enabled ? "true" : "false") << ","
       << "\"cruise_active\":" << (s.cruise_active ? "true" : "false") << ","
       << "\"cruise_speed\":" << s.cruise_speed << ","
       << "\"left_pwm\":" << s.left_pwm << ","
       << "\"right_pwm\":" << s.right_pwm << ","
       << "\"enc_left\":" << s.enc_left << ","
       << "\"enc_right\":" << s.enc_right << ","
       << "\"lx\":" << s.lx << ","
       << "\"ly\":" << s.ly << ","
       << "\"rx\":" << s.rx << ","
       << "\"ry\":" << s.ry << ","
       << "\"l2\":" << s.l2 << ","
       << "\"r2\":" << s.r2 << ","
       << "\"dpad_up\":" << (s.dpad_up ? "true" : "false") << ","
       << "\"dpad_down\":" << (s.dpad_down ? "true" : "false") << ","
       << "\"dpad_left\":" << (s.dpad_left ? "true" : "false") << ","
       << "\"dpad_right\":" << (s.dpad_right ? "true" : "false") << ","
       << "\"r1\":" << (s.r1 ? "true" : "false") << ","
       << "\"ss_left_raw\":" << s.ss_left_raw << ","
       << "\"ss_right_raw\":" << s.ss_right_raw << ","
       << "\"ss_dir\":" << s.ss_dir
       << "}";
    return os.str();
}

void WebDebugServer::serverLoop() {
    struct pollfd pfd {};
    pfd.fd = listen_fd;
    pfd.events = POLLIN;

    while (running.load()) {
        const int ret = ::poll(&pfd, 1, 100);
        if (ret > 0 && (pfd.revents & POLLIN)) {
            const int client_fd = ::accept(listen_fd, nullptr, nullptr);
            if (client_fd >= 0) {
                handleClient(client_fd);
                ::close(client_fd);
            }
        }
    }
}

void WebDebugServer::handleClient(int client_fd) {
    struct timeval tv {0, 300000};  // 300 ms read timeout
    ::setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    char buf[1024]{};
    const ssize_t n = ::recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return;

    std::string req(buf, static_cast<std::size_t>(n));
    std::string method, path;
    std::istringstream iss(req);
    iss >> method >> path;

    std::string status = "200 OK";
    std::string content_type = "text/plain; charset=utf-8";
    std::string body = "OK";

    if (method == "GET" && (path == "/" || path == "/index.html")) {
        content_type = "text/html; charset=utf-8";
        body = kDashboardHtml;
    } else if (method == "GET" && path == "/api/state") {
        content_type = "application/json";
        body = buildJson();
    } else if (method == "POST" && path == "/api/estop") {
        if (on_estop_toggle) on_estop_toggle();
        content_type = "application/json";
        body = "{\"ok\":true}";
    } else if (method == "POST" && path == "/api/enc_reset") {
        if (on_enc_reset) on_enc_reset();
        content_type = "application/json";
        body = "{\"ok\":true}";
    } else {
        status = "404 Not Found";
        body = "Not Found";
    }

    std::ostringstream resp;
    resp << "HTTP/1.1 " << status << "\r\n"
         << "Content-Type: " << content_type << "\r\n"
         << "Content-Length: " << body.size() << "\r\n"
         << "Cache-Control: no-store\r\n"
         << "Connection: close\r\n\r\n"
         << body;
    sendAll(client_fd, resp.str());
}
