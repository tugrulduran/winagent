// ** Incoming data handlers

function updateCPU(data) {
    if (!data || !data.ok) {
        return;
    }
    const elContainer = document.getElementById('cpu-container');
    document.getElementById('cpu-bar').style.width = data.load + '%';
    document.getElementById('cpu-value').innerText = Math.round(data.load) + '%';

    if (data.load > 50) {
        elContainer.classList.add('critical');
    }
    else {
        elContainer.classList.remove('critical');
    }
}

function updateRAM(data) {
    if (!data || !data.ok) {
        return;
    }
    const used = data.total - data.available;
    const percent = Math.round(used / data.total * 100);
    const elContainer = document.getElementById('ram-container');
    document.getElementById('ram-bar').style.width = percent + '%';
    document.getElementById('ram-value').innerText = percent + '%';

    if (percent > 80) {
        elContainer.classList.add('critical');
    }
    else {
        elContainer.classList.remove('critical');
    }
}

function updateNet(data) {
    if (!data || !data.ok) {
        return;
    }
    const inKB = data.interfaces.map(iface => iface.rxSpeed).reduce((a, b) => a + b, 0);
    const outKB = data.interfaces.map(iface => iface.txSpeed).reduce((a, b) => a + b, 0);
    let inVal = Math.round(inKB) + ' KB/s';
    let outVal = Math.round(outKB) + ' KB/s';
    if (inKB > 1000) {
        const inMB = inKB / 1024;
        inVal = inMB.toFixed(1) + ' MB/s';
    }
    if (outKB > 1000) {
        const outMB = outKB / 1024;
        outVal = outMB.toFixed(1) + ' MB/s';
    }

    document.getElementById('net-dl-speed').innerText = inVal;
    document.getElementById('net-ul-speed').innerText = outVal;

    const bestInterface = data.interfaces.reduce((best, x) => (x.rxSpeed + x.txSpeed > best.rxSpeed + best.txSpeed ? x : best));
    if (!bestInterface) {
        return;
    }
    let netType = 'unknown';
    if (['wifi', 'wi-fi', 'wlan', 'wireless'].some(k => bestInterface.name.toLowerCase().includes(k))) {
        netType = 'Wi-Fi';
    }
    else if (['ethernet', 'eth', 'wired'].some(k => bestInterface.name.toLowerCase().includes(k))) {
        netType = 'Ethernet';
    }
    const elIcon = document.getElementById('net-interface-icon');
    const elName = document.getElementById('net-interface-text');
    if (netType === 'unknown') {
        elName.innerText = bestInterface.name;
    }
    else if ((inKB + outKB) > 10) {
        elName.innerText = netType;
        if (netType === 'Wi-Fi') {
            elIcon.innerHTML = `<i class="fa-solid fa-wifi"></i>`;
        }
        else if (netType === 'Ethernet') {
            elIcon.innerHTML = `<i class="fa-solid fa-network-wired"></i>`;
        }
        else {
            elIcon.innerHTML = `<i class="fa-solid fa-question"></i>`;
        }
    }
}

function updateAudioApps(data) {
    if (!data || !data.ok) {
        return;
    }
    // [...document.getElementsByClassName('strip')].forEach(el => el.remove());
    data.apps.forEach(app => {
        const pid = app.type === 0 ? 0xFFFFFFFF : parseInt(app.pid);
        let el = document.getElementById(`pid-${pid}`);
        const vol = Math.round(app.volume * 100);
        if (audioLocked === pid) {
            return;
        }
        if (!el) {
            el = document.createElement('div');
            el.dataset.type = app.type;
            el.id = `pid-${pid}`;
            el.classList.add('strip');
            if (app.muted) {
                el.classList.add('muted');
            }
            else {
                el.classList.remove('muted');
            }
            const icon = getIconByAppTitle(app.name);
            el.addEventListener('pointerdown', (e) => {
                activePid = pid;
                el.setPointerCapture?.(e.pointerId);   // drag kaçmasın
                triggerUpdate(e);
            });

            const fill = document.createElement('div');
            fill.className = 'fill';
            fill.style.height = vol + '%';
            el.appendChild(fill);
            const content = document.createElement('div');
            content.className = 'content';
            const appIcon = document.createElement('div');
            appIcon.className = 'app-icon';
            const appIconInner = document.createElement('i');
            appIconInner.className = `fa-${icon.type} fa-${icon.icon}`;
            appIcon.appendChild(appIconInner);
            const appName = document.createElement('div');
            appName.className = 'app-name';
            appName.innerText = app.name;
            const volLabel = document.createElement('div');
            volLabel.className = 'vol-label';
            volLabel.innerText = vol;
            content.appendChild(appIcon);
            content.appendChild(appName);
            content.appendChild(volLabel);
            el.appendChild(content);
            document.getElementById('mixer-container').appendChild(el);
        }
        else {
            if (activePid !== pid) {
                el.querySelector('.fill').style.height = vol + '%';
                el.querySelector('.vol-label').innerText = vol;
            }
            if (app.muted) {
                el.classList.add('muted');
            }
            else {
                el.classList.remove('muted');
            }
        }
    });

    const pids = data.apps.map(a => parseInt(a.pid));
    const mixerPids = [...document.getElementsByClassName('strip')].map(el => parseInt(el.id.split('-')[1]));
    mixerPids
        .filter(pid => !pids.includes(pid))
        .forEach(pid => {
            const el = document.getElementById(`pid-${pid}`);
            if (el && el.dataset.type === '0') {
                return;
            }
            if (el) {
                el.remove();
            }
        });
}

function updateAudioDevices(data) {
    if (!data || !data.ok) {
        return;
    }
    const container = document.getElementById('audio-devices-container');

    const deviceIds = data.devices.map(dev => dev.deviceId);
    const existingDeviceIds = [...document.getElementsByClassName('device-btn')].map(el => el.dataset.deviceId);

    // Clear non-existing devices one line
    existingDeviceIds.forEach(deviceId => {
        if (deviceIds.includes(deviceId)) {
            const el = document.querySelector(`#audio-devices-container .device-btn[data-device-id="${deviceId}"]`);
            if (!el) {
                container.removeChild(el);
            }
        }
    });

    const activeDevice = data.devices.find(dev => dev.default);

    // Add new devices
    [...data.devices]
        .sort((a, b) => a.name.localeCompare(b.name))
        .forEach(dev => {
            const deviceId = dev.deviceId;
            const el = document.querySelector(`#audio-devices-container .device-btn[data-device-id="${deviceId}"]`);
            if (!el) {
                const btn = document.createElement('div');
                btn.dataset.deviceId = deviceId;
                btn.className = `device-btn ${dev.default ? 'active' : ''}`;
                btn.innerText = dev.name;
                btn.addEventListener('click', () => {
                    callRequest('audiodevices', { cmd: 'setDevice', index: dev.index });
                    lockModuleForMSec('audiodevices', 3000);
                    [...document.getElementsByClassName('device-btn')].forEach(el => el.classList.remove('active'));
                    btn.classList.add('active');
                });
                container.appendChild(btn);
            }
            else {
                if (activeDevice.deviceId !== window.activeAudioDeviceId) {
                    if (activeDevice.deviceId === deviceId) {
                        el.classList.add('active');
                    }
                    else {
                        el.classList.remove('active');
                    }
                }
            }
        });

    if (activeDevice.deviceId !== window.activeAudioDeviceId) {
        window.activeAudioDeviceId = activeDevice.deviceId;
    }
}

function updateAudioAudeze(data) {
    const elContainer = document.getElementById('audeze-battery-container');
    const elValue = document.getElementById('audeze-text');
    if (data.ok) {
        elContainer.style.display = 'flex';
        if (data.battery <= 25) {
            elContainer.classList.add('critical');
        }
        else {
            elContainer.classList.remove('critical');
        }
        elValue.innerText = `${Math.round(data.battery)}%`;
    }
    else {
        elContainer.style.display = 'none';
    }
}

function updateMedia(data) {
    if (!data || data.source < 1) {
        return;
    }

    const hud = document.getElementById('media-hud'), fill = document.getElementById('media-progress-fill');
    if (!data || !data.title) {
        hud.style.display = 'none';
        return;
    }

    window.currentIsPlaying = data.isPlaying;
    window.currentTime = data.currentTime;
    window.totalTime = data.duration;
    source = getMediaSourceByTypeId(data.source);

    hud.style.display = 'flex';
    if (data.isPlaying) {
        hud.classList.remove('paused');
    }
    else {
        hud.classList.add('paused');
    }
    document.getElementById('media-title').innerText = data.title;
    document.getElementById('media-source').innerText = source;
    document.getElementById('media-status-text').innerText = window.currentIsPlaying ? 'PLAYING' : 'PAUSED';
    document.getElementById('media-time-current').innerText = formatTime(data.currentTime);
    document.getElementById('media-time-duration').innerText = formatTime(data.duration);
    fill.style.width = Math.min(100, (data.currentTime / data.duration) * 100) + '%';
}

async function ensureLauncherIcon(name, btnEl) {
    const cache = await caches.open('winagent-launcher-icons');
    const key = `/launcher/icon/${encodeURIComponent(name)}`;
    const hit = await cache.match(key);
    const now = new Date();

    if (!hit) {
        if (now - (window.iconResponses[name] || 0) > 5000) {
            window.iconResponses[name] = new Date();
            callRequest('launcher', { cmd: 'getIcon', name });
            setTimeout(() => window.launcherHash = null, 1000);
        }
        return false;
    }

    const dataUrl = await hit.text();
    btnEl.innerHTML = `<img class="launcher-icon" src="${dataUrl}" />`;

    return true;
}

function updateLauncherActions(data) {
    if (!data || !data.ok) {
        return;
    }

    const zones = [
        null,
        document.getElementById('launcher-zone-1'),
        document.getElementById('launcher-zone-2'),
        document.getElementById('launcher-zone-3'),
        document.getElementById('launcher-zone-4')
    ];

    zones.forEach(zone => {
        if (zone) {
            zone.innerHTML = '';
        }
    });

    data.apps.forEach(action => {
        const btn = document.createElement('div');
        btn.className = 'launcher-btn';
        btn.dataset.zone = action.zone || 1;
        btn.dataset.index = action.index;
        btn.addEventListener('click', () => callRequest('launcher', { cmd: 'launch', index: action.index }));
        zones[action.zone || 1].appendChild(btn);

        ensureLauncherIcon(action.name, btn).then(res => {
            if (!res) {
                btn.innerHTML = '<i class="fa-solid fa-3x fa-circle-play"></i>';
            }
        });
    });
}

async function onLauncherIconResponse(res) {
    if (!res || !res.ok || !res.name || !res.b64 || !res.mime) {
        console.error('Invalid launcher icon response:', res);
        return;
    }

    const dataUrl = `data:${res.mime};base64,${res.b64}`;
    const cache = await caches.open('winagent-launcher-icons');
    const key = `launcher/icon/${encodeURIComponent(res.name)}`;

    await cache.put(key, new Response(dataUrl, { headers: { 'Content-Type': 'text/plain' } }));
}

// ** Command handlers

function callRequest(module, payload) {
    console.log('CALL REQUEST:', module, payload);
    ws.send(JSON.stringify({ module, payload }));
}

function jumpMediaToTime(time) {
    callRequest('media', { cmd: 'jump', time });
    window.currentTime = time;
}

function jumpMediaToRelativeTime(time) {
    const newTime = window.currentTime + time;
    if (newTime < 0) {
        jumpMediaToTime(0);
    }
    else if (newTime > window.totalTime) {
        jumpMediaToTime(window.totalTime);
    }
    else {
        jumpMediaToTime(newTime);
    }
}


function checkAgentStatus() {
    const timeDiff = (Date.now() - lastDataReceived) / 1000;
    const badge = document.getElementById('status-live');
    if (timeDiff < 5) {
        badge.classList.remove('dead');
        badge.classList.add('active');
        badge.innerText = 'AGENT: LIVE';
    }
    else {
        badge.classList.remove('active');
        badge.classList.add('dead');
        badge.innerText = 'AGENT: DEAD';
    }
}

// ** Websocket connection
const WA_SECRET_STORAGE = 'winagent.secret';

function waNormalizeSecret(s) {
    return (s || '').replace(/\D/g, '').slice(0, 6);
}

function waGetSecret() {
    return waNormalizeSecret(localStorage.getItem(WA_SECRET_STORAGE));
}

function waShowAuth(msg) {
    const root = document.getElementById('wa-auth');
    const err = document.getElementById('wa-auth-err');
    const inp = document.getElementById('wa-auth-key');
    root.style.display = 'flex';
    err.textContent = msg || '';
    inp.value = '';
    setTimeout(() => inp.focus(), 0);
}

function waHideAuth() {
    document.getElementById('wa-auth').classList.add('hidden');
    document.getElementById('wa-auth-err').textContent = '';
}

function waBuildWsUrl(secret) {
    const proto = (location.protocol === 'https:') ? 'wss' : 'ws';
    const host = location.hostname;
    return `${proto}://${host}:3004/?key=${encodeURIComponent(secret)}`;
}

let ws;
// const wsUrl = `wss://${window.location.hostname}:3004`;
const reconnectInterval = 1000;

const lockedModules = {};

function lockModuleForMSec(module, msec) {
    lockedModules[module] = setTimeout(() => {
        delete lockedModules[module];
    }, msec);
}

function isModuleLocked(module) {
    return lockedModules[module] !== undefined;
}

let lastDataReceived = Date.now();

function dataReceived() {
    lastDataReceived = Date.now();
}

async function generateHash(obj) {
    const msgUint8 = new TextEncoder().encode(JSON.stringify(obj));
    const hashBuffer = await crypto.subtle.digest('SHA-256', msgUint8);
    const hashArray = Array.from(new Uint8Array(hashBuffer));
    return hashArray.map(b => b.toString(16).padStart(2, '0')).join('');
}

function connect() {
    const secret = waGetSecret();
    if (!secret || secret.length !== 6) {
        waShowAuth();
        return;
    }

    waHideAuth();

    const wsUrl = waBuildWsUrl(secret);
    ws = new WebSocket(wsUrl);

    ws.onopen = () => {
        console.log('Connected to server');
        document.getElementById('wa-auth').style.display = 'none';
        // ws.send(JSON.stringify({ cmd: "runAction", payload: { action: 1} }));
    };
    ws.onclose = (e) => {
        console.log(`Socket closed with code ${e.code}. Reconnecting in ${reconnectInterval / 1000}s...`, e.reason);
        if (e.code === 1008) {
            document.getElementById('wa-auth').style.display = 'flex';
            localStorage.removeItem(WA_SECRET_STORAGE);
            waShowAuth('Secret is wrong or has been changed. Re-enter the key.');
        }
        else {
            setTimeout(connect, reconnectInterval);
        }
    };

    ws.onerror = (err) => {
        console.error('Socket encountered error: ', err.message, 'Closing socket');
        ws.close();
    };
    ws.onmessage = (e) => {
        // console.log('RAW DATA:', e.data);
        dataReceived();
        const data = JSON.parse(e.data);
        const event = data.event;
        const payload = data.payload;
        if (event === 'update') {
            console.log('UPDATE EVENT:', payload);
            if ('basiccpu' in payload.modules) {
                updateCPU(payload.modules.basiccpu);
            }
            if ('basicmemory' in payload.modules) {
                updateRAM(payload.modules.basicmemory);
            }
            if ('basicnetwork' in payload.modules) {
                updateNet(payload.modules.basicnetwork);
            }
            if ('volumemixer' in payload.modules) {
                updateAudioApps(payload.modules.volumemixer);
            }
            if ('audiodevices' in payload.modules) {
                if (isModuleLocked('audiodevices')) {
                    return;
                }
                updateAudioDevices(payload.modules.audiodevices);
            }
            if ('audezemaxwell' in payload.modules) {
                updateAudioAudeze(payload.modules.audezemaxwell);
            }
            if ('media' in payload.modules) {
                updateMedia(payload.modules.media);
            }
            if ('launcher' in payload.modules) {
                generateHash(payload.modules.launcher).then(hash => {
                    if (hash !== window.launcherHash) {
                        updateLauncherActions(payload.modules.launcher);
                        window.launcherHash = hash;
                    }
                });
            }
        }
        else if (event === 'launcher_icon_update') {
            onLauncherIconResponse(data);
        }
    };
}

document.addEventListener('DOMContentLoaded', () => {
    const btn = document.getElementById('btn-wa-auth');
    const inp = document.getElementById('wa-auth-key');
    const err = document.getElementById('wa-auth-err');
    err.style.display = 'none';

    btn.addEventListener('click', () => {
        const s = waNormalizeSecret(inp.value);
        if (s.length !== 6) {
            err.innerText = 'Key is 6 digits long!';
            err.style.display = 'block';
            return;
        }
        localStorage.setItem(WA_SECRET_STORAGE, s);
        connect();
    });

    inp.addEventListener('keydown', (e) => {
        if (e.key === 'Enter') {
            btn.click();
        }
    });

    connect();
});

setInterval(checkAgentStatus, 1000);

function triggerUpdate(e) {
    const el = document.getElementById(`pid-${activePid}`);
    if (!el) {
        return;
    }
    const rect = el.getBoundingClientRect();
    const cy = (e.touches && e.touches.length) ? e.touches[0].clientY : e.clientY;
    if (cy == null) {
        return;
    }
    let val = Math.max(0, Math.min(100, Math.round(((rect.bottom - cy) / rect.height) * 100)));
    el.querySelector('.fill').style.height = val + '%';
    el.querySelector('.vol-label').innerText = val;

    audioLocked = activePid;
    setTimeout(() => {
        audioLocked = -1;
    }, 2000);

    callRequest('volumemixer', {
        cmd: el.dataset.type === '0' ? 'setMasterVolume' : 'setAppVolume',
        pid: activePid,
        volume: val / 100
    });
}

window.addEventListener('pointermove', (e) => {
    if (activePid !== null) {
        triggerUpdate(e);
    }
});
window.addEventListener('pointerup', () => {
    activePid = null;
});
window.addEventListener('pointercancel', () => {
    activePid = null;
});

// @formatter:off
document.getElementById('btn-media-prev')               .addEventListener('click', () => callRequest            ('media', { cmd: 'prev' }));
document.getElementById('btn-media-seek-back-fast')     .addEventListener('click', () => jumpMediaToRelativeTime(-30));
document.getElementById('btn-media-seek-back')          .addEventListener('click', () => jumpMediaToRelativeTime(-10));
document.getElementById('btn-media-play-pause')         .addEventListener('click', () => callRequest            ('media', { cmd: 'playpause' }));
document.getElementById('btn-media-seek-forward')       .addEventListener('click', () => jumpMediaToRelativeTime(10));
document.getElementById('btn-media-seek-forward-fast')  .addEventListener('click', () => jumpMediaToRelativeTime(30));
document.getElementById('btn-media-next')               .addEventListener('click', () => callRequest            ('media', { cmd: 'next' }));
// @formatter:on