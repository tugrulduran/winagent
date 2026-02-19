function formatTime(s) {
    if (!s || s <= 0) {
        return '00:00';
    }
    const hrs = Math.floor(s / 3600), mins = Math.floor((s % 3600) / 60), secs = Math.floor(s % 60);
    const ds = secs < 10 ? '0' + secs : secs;
    return hrs > 0 ? `${hrs}:${mins < 10 ? '0' + mins : mins}:${ds}` : `${mins}:${ds}`;
}

function toggleStopwatch() {
    const display = document.getElementById('stopwatch-value');
    if (!swIsRunning) {
        swIsRunning = true;
        swStartTime = Date.now() - swElapsedTime;
        swInterval = setInterval(() => {
            swElapsedTime = Date.now() - swStartTime;
            let ms = Math.floor((swElapsedTime % 1000) / 10), ts = Math.floor(swElapsedTime / 1000);
            let s = ts % 60, m = Math.floor((ts / 60) % 60), h = Math.floor(ts / 3600);
            const pad = (n) => String(n).padStart(2, '0');
            display.innerText = `${pad(h)}:${pad(m)}:${pad(s)}.${pad(ms)}`;
        }, 10);
        display.classList.add('active');
    }
    else {
        swIsRunning = false;
        clearInterval(swInterval);
    }
    document.getElementById('stopwatch-reset').style.visibility = 'visible';
}

function resetStopwatch() {
    swIsRunning = false;
    clearInterval(swInterval);
    swElapsedTime = 0;
    const display = document.getElementById('stopwatch-value');
    display.innerText = '00:00:00.00';
    display.classList.remove('active');
    document.getElementById('stopwatch-reset').style.visibility = 'hidden';
}

function toggleFullscreen() {
    if (!document.fullscreenElement) {
        document.documentElement.requestFullscreen();
    }
    else {
        document.exitFullscreen();
    }
}

async function toggleEnhancedWakeLock() {
    const btn = document.getElementById('btn-wakelock');
    const badge = document.getElementById('status-wakelock');
    if (!isLocked) {
        isLocked = true;
        btn.classList.add('active');
        badge.classList.add('active');
        badge.innerText = 'WAKE LOCK: ACTIVE';
        try {
            if ('wakeLock' in navigator) {
                wakeLockObj = await navigator.wakeLock.request('screen');
            }
        } catch (err) {
            console.error(err);
        }
    }
    else {
        isLocked = false;
        btn.classList.remove('active');
        badge.classList.remove('active');
        badge.innerText = 'WAKE LOCK: OFF';
        if (wakeLockObj) {
            await wakeLockObj.release();
            wakeLockObj = null;
        }
    }
}

function updateClock() {
    const timeValue = document.getElementById('main-clock');
    const dateValue = document.getElementById('main-calendar');
    const now = new Date();
    const days = ['SUNDAY', 'MONDAY', 'TUESDAY', 'WEDNESDAY', 'THURSDAY', 'FRIDAY', 'SATURDAY'];
    timeValue.innerText = now.toLocaleTimeString('tr-TR', { hour12: false });
    dateValue.innerText = `${now.toLocaleDateString('tr-TR')} ${days[now.getDay()]}`;
}

setInterval(updateClock, 1000);

if ('getBattery' in navigator) {
    navigator.getBattery().then(b => {
        updateClientBattery({ percent: Math.round(b.level * 100), charging: b.charging });
        b.addEventListener('chargingchange', () => {
            updateClientBattery({ percent: Math.round(b.level * 100), charging: b.charging });
        });
        b.addEventListener('levelchange', () => {
            updateClientBattery({ percent: Math.round(b.level * 100), charging: b.charging });
        });
    });
}
else {
    console.warn('getBattery() not supported in this browser.');
    document.getElementById('local-battery').style.display = 'none';
}

function updateClientBattery(data) {
    const elContainer = document.getElementById('local-battery');
    if (data.percent < 20) {
        elContainer.classList.add('critical');
    }
    else {
        elContainer.classList.remove('critical');
    }

    let levelIcon = 'battery-full';
    if (data.percent < 20) {
        levelIcon = 'battery-empty';
    }
    else if (data.percent < 40) {
        levelIcon = 'battery-quarter';
    }
    else if (data.percent < 60) {
        levelIcon = 'battery-half';
    }
    else if (data.percent < 80) {
        levelIcon = 'battery-three-quarters';
    }
    document.getElementById('level-icon').innerHTML = `<i class="fa-solid fa-rotate-270 fa-${levelIcon}"></i>`;

    const elCharging = document.getElementById('charging-icon');
    if (data.charging) {
        elCharging.style.display = 'block';
    }
    else {
        elCharging.style.display = 'none';
    }

    document.getElementById('local-battery-text').innerText = `${data.percent}%`;
}


function getIconById(id) {
    switch (id) {
        case 1:
            return { type: 'solid', icon: 'calculator' };
        case 2:
            return { type: 'regular', icon: 'clipboard' };
        case 3:
            return { type: 'brands', icon: 'chrome' };
        case 4:
            return { type: 'brands', icon: 'steam' };
        case 5:
            return { type: 'solid', icon: 'terminal' };
        default:
            return { type: 'solid', icon: 'question' };
    }
}

function getIconByObj(action) {
    if (action.icon) {
        if (action.icon.includes('fa-brands')) {
            return { type: 'brands', icon: action.icon.replace('fa-brands', '').trim().replace('fa-', '') };
        }
        else if (action.icon.includes('fa-regular')) {
            return { type: 'regular', icon: action.icon.replace('fa-regular', '').trim().replace('fa-', '') };
        }
        else if (action.icon.includes('fa-solid')) {
            return { type: 'solid', icon: action.icon.replace('fa-solid', '').trim().replace('fa-', '') };
        }
        else if (action.icon.trim().startsWith('fa-')) {
            return { type: 'solid', icon: action.icon.trim().replace('fa-', '') };
        }
        else if (action.icon.length > 0) {
            return { type: 'solid', icon: action.icon };
        }
        else {
            return { type: 'solid', icon: 'question' };
        }
    }
    else {
        return { type: 'solid', icon: 'question' };
    }
}

function getIconByProcessName(name) {
    switch (name) {
        case 'chrome.exe':
            return { type: 'brands', icon: 'chrome' };
        case 'steam.exe':
            return { type: 'brands', icon: 'steam' };
        default:
            return { type: 'solid', icon: 'question' };
    }
}

function getIconByAppTitle(title) {
    switch (title) {
        case 'MASTER VOLUME':
            return { type: 'solid', icon: 'volume-high' };
        case 'Google Chrome':
            return { type: 'brands', icon: 'chrome' };
        case 'Steam':
            return { type: 'brands', icon: 'steam' };
        case 'Battle.net':
            return { type: 'brands', icon: 'battle-net' };
        case 'Opera Internet Browser':
            return { type: 'brands', icon: 'opera' };
        default:
            return { type: 'solid', icon: 'question' };
    }
}

function getMediaSourceByTypeId(id) {
    switch (id) {
        case 0:
            return 'NO MEDIA';
        case 1:
            return 'GENERIC';
        case 2:
            return 'YOUTUBE';
        case 3:
            return 'TWITCH';
        case 4:
            return 'KICK';
        case 5:
            return 'MEDIA PLAYER';
        case 6:
            return 'YOUTUBE MUSIC';
        case 7:
            return 'SPOTIFY';
        default:
            return 'UNKNOWN';
    }
}


document.getElementById('stopwatch-value').addEventListener('pointerdown', toggleStopwatch);
document.getElementById('stopwatch-reset').addEventListener('pointerdown', resetStopwatch);
document.getElementById('btn-wakelock').addEventListener('pointerdown', toggleEnhancedWakeLock);
document.getElementById('btn-fullscreen').addEventListener('pointerdown', toggleFullscreen);
document.getElementById('btn-reload').addEventListener('pointerdown', () => location.reload());