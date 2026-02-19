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

            el.innerHTML = `
                    <div class="fill" style="height:${vol}%"></div>
                    <div class="content">
                      <div class="app-icon"><i class="fa-${icon.type} fa-${icon.icon}"></i></div>
                      <div class="app-name">${app.name}</div>
                      <div class="vol-label">${vol}</div>
                    </div>`;

            el.addEventListener('pointerdown', (e) => {
                activePid = pid;
                el.setPointerCapture?.(e.pointerId);   // drag kaçmasın
                triggerUpdate(e);
            });
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
    container.innerHTML = '';

    [...data.devices]
        .sort((a, b) => a.name.localeCompare(b.name))
        .forEach(dev => {
            const btn = document.createElement('div');
            btn.className = `device-btn ${dev.default ? 'active' : ''}`;
            btn.innerText = dev.name;
            btn.addEventListener('click', () => {
                callRequest('audiodevices', { cmd: 'setDevice', index: dev.index });
                lockModuleForMSec('audiodevices', 3000);
                [...document.getElementsByClassName('device-btn')].forEach(el => el.classList.remove('active'));
                btn.classList.add('active');
            });
            container.appendChild(btn);
        });
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
        elValue.innerHTML = `${Math.round(data.battery)}%`;
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

function updateLauncherActions(data) {
    if (!data || !data.ok) {
        return;
    }

    const launcherZone1 = document.getElementById('launcher-zone-1');
    const launcherZone2 = document.getElementById('launcher-zone-2');
    const launcherZone3 = document.getElementById('launcher-zone-3');
    const launcherZone4 = document.getElementById('launcher-zone-4');
    const launcherZones = [null, launcherZone1, launcherZone2, launcherZone3, launcherZone4];
    launcherZones.forEach(zone => {
        if (zone) {
            zone.innerHTML = '';
        }
    });

    data.apps.forEach(action => {
        const btn = document.createElement('div');
        btn.className = `launcher-btn`;
        btn.addEventListener('click', () => callRequest('launcher', { cmd: 'launch', index: action.index }));
        const icon = getIconByObj(action);
        btn.innerHTML = `<i class="fa-4x fa-${icon.type} fa-${icon.icon}"></i>`;
        launcherZones[action.zone || 1].appendChild(btn);
    });
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

let ws;
const wsUrl = `wss://${window.location.hostname}:3004`;
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

function connect() {
    ws = new WebSocket(wsUrl);

    ws.onopen = () => {
        console.log('Connected to server');
        // ws.send(JSON.stringify({ cmd: "runAction", payload: { action: 1} }));
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
                updateLauncherActions(payload.modules.launcher);
            }
        }
    };
    ws.onclose = (e) => {
        console.log(`Socket closed. Reconnecting in ${reconnectInterval / 1000}s...`, e.reason);
        setTimeout(connect, reconnectInterval);
    };

    ws.onerror = (err) => {
        console.error('Socket encountered error: ', err.message, 'Closing socket');
        ws.close();
    };
}

setInterval(checkAgentStatus, 1000);

connect();

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