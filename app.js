// CloudRover V1.0 Interactive Studio Engine & Cozmo-Style Emotion Core
document.addEventListener('DOMContentLoaded', () => {
  // Tab Switching
  const tabBtns = document.querySelectorAll('.tab-btn');
  const tabPanes = document.querySelectorAll('.tab-pane');

  tabBtns.forEach(btn => {
    btn.addEventListener('click', () => {
      const targetId = btn.getAttribute('data-tab');
      tabBtns.forEach(b => b.classList.remove('active'));
      tabPanes.forEach(p => p.classList.remove('active'));
      btn.classList.add('active');
      const targetPane = document.getElementById(targetId);
      if (targetPane) targetPane.classList.add('active');
    });
  });

  // Web Audio API Synthesizer (Simulating ESP32 DAC1 + PAM8403 Expressive Chimes)
  let audioCtx = null;
  function getAudioContext() {
    if (!audioCtx) {
      audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    }
    if (audioCtx.state === 'suspended') {
      audioCtx.resume();
    }
    return audioCtx;
  }

  function playTone(freq, duration, type = 'sine', rampTo = null, vol = 0.18) {
    try {
      const ctx = getAudioContext();
      const osc = ctx.createOscillator();
      const gain = ctx.createGain();
      osc.type = type;
      osc.frequency.setValueAtTime(freq, ctx.currentTime);
      if (rampTo) {
        osc.frequency.exponentialRampToValueAtTime(Math.max(10, rampTo), ctx.currentTime + duration);
      }
      gain.gain.setValueAtTime(vol, ctx.currentTime);
      gain.gain.exponentialRampToValueAtTime(0.0001, ctx.currentTime + duration);
      osc.connect(gain);
      gain.connect(ctx.destination);
      osc.start();
      osc.stop(ctx.currentTime + duration);
    } catch (e) {
      console.log('Audio error:', e);
    }
  }

  // Cozmo Robot Rich Acoustic Synthesis
  const Sfx = {
    normal() {
      playTone(523.25, 0.12, 'sine'); // C5
    },
    happy() {
      // Cozmo cheerful giggle arpeggio
      playTone(587.33, 0.08, 'sine'); // D5
      setTimeout(() => playTone(739.99, 0.08, 'sine'), 70); // F#5
      setTimeout(() => playTone(880.00, 0.09, 'sine'), 140); // A5
      setTimeout(() => playTone(1174.66, 0.18, 'triangle', 1300), 220); // D6 cheerful sweep
    },
    love() {
      // Warm romantic harp chords
      playTone(440.0, 0.14, 'sine');
      setTimeout(() => playTone(554.37, 0.14, 'sine'), 80);
      setTimeout(() => playTone(659.25, 0.16, 'sine'), 160);
      setTimeout(() => playTone(830.61, 0.28, 'sine'), 240);
      setTimeout(() => playTone(1108.73, 0.35, 'sine'), 320);
    },
    curious() {
      // "Huh?!" Rising quizzical question chirp
      playTone(320, 0.22, 'triangle', 680, 0.22);
    },
    shocked() {
      // Two-tone screeching alarm siren
      playTone(350, 0.12, 'sawtooth', 850, 0.25);
      setTimeout(() => playTone(850, 0.12, 'sawtooth', 350, 0.25), 130);
      setTimeout(() => playTone(350, 0.12, 'sawtooth', 850, 0.25), 260);
    },
    sad() {
      // Whining mournful drop
      playTone(520, 0.35, 'sine', 220, 0.2);
      setTimeout(() => playTone(390, 0.45, 'sine', 160, 0.16), 280);
    },
    angry() {
      // Grumpy low-frequency sawtooth buzzing growl
      playTone(130, 0.28, 'sawtooth', null, 0.25);
      setTimeout(() => playTone(110, 0.32, 'sawtooth', null, 0.25), 120);
    },
    sleepy() {
      // Gentle rhythmic breathing snore
      playTone(280, 0.5, 'sine', 380, 0.12);
      setTimeout(() => playTone(380, 0.6, 'sine', 240, 0.08), 500);
    },
    wink() {
      // Playful pop and wink chirp
      playTone(700, 0.06, 'sine');
      setTimeout(() => playTone(1200, 0.14, 'triangle'), 60);
    },
    excited() {
      // High-energy 8-bit victory level-up fanfare
      const notes = [523.25, 659.25, 783.99, 987.77, 1046.50, 1318.51];
      notes.forEach((freq, idx) => {
        setTimeout(() => playTone(freq, 0.1, 'square', null, 0.14), idx * 65);
      });
    },
    solar() {
      // Ascending daylight energy arpeggio
      playTone(523.25, 0.1, 'sine');
      setTimeout(() => playTone(659.25, 0.1, 'sine'), 110);
      setTimeout(() => playTone(783.99, 0.1, 'sine'), 220);
      setTimeout(() => playTone(1046.50, 0.12, 'sine'), 330);
      setTimeout(() => playTone(1318.51, 0.35, 'sine'), 440);
    }
  };

  // Cozmo-Style Face Elements
  const robotFace = document.getElementById('robotFace');
  const moodStatus = document.getElementById('moodStatus');
  let currentMood = 'normal';

  const MoodCatalog = {
    'normal': {
      text: 'STATUS: NORMAL IDLE (COZMO CURIOUS BLINK LOOP)',
      class: 'face-normal',
      sfx: Sfx.normal
    },
    'happy': {
      text: 'STATUS: HAPPY / GIGGLING (CLAP REACT & CELEBRATION)',
      class: 'face-happy',
      sfx: Sfx.happy
    },
    'love': {
      text: 'STATUS: IN LOVE / HEART EYES (AFFECTION & USER BONDING)',
      class: 'face-love',
      sfx: Sfx.love
    },
    'curious': {
      text: 'STATUS: CURIOUS / SKEPTICAL (ASYMMETRIC COZMO BROW)',
      class: 'face-curious',
      sfx: Sfx.curious
    },
    'shocked': {
      text: 'STATUS: SHOCKED (OBSTACLE < 150mm IMMEDIATE BRAKE!)',
      class: 'face-shocked',
      sfx: Sfx.shocked
    },
    'sad': {
      text: 'STATUS: POUTING / SAD (LOW BATTERY OR MISSION TIMEOUT)',
      class: 'face-sad',
      sfx: Sfx.sad
    },
    'angry': {
      text: 'STATUS: GRUMPY / ANGRY (MOTOR STALL DETECTED)',
      class: 'face-angry',
      sfx: Sfx.angry
    },
    'sleepy': {
      text: 'STATUS: SLEEPY / SNOOZING (LOW POWER STANDBY Zzz)',
      class: 'face-sleepy',
      sfx: Sfx.sleepy
    },
    'wink': {
      text: 'STATUS: PLAYFUL / WINKING (MISCHIEVOUS GREETING)',
      class: 'face-wink',
      sfx: Sfx.wink
    },
    'excited': {
      text: 'STATUS: EXCITED / VICTORY DANCE (GOAL REACHED!)',
      class: 'face-excited',
      sfx: Sfx.excited
    },
    'solar': {
      text: 'STATUS: SOLAR TRICKLE CHARGING (HARVESTING SUNLIGHT)',
      class: 'face-solar',
      sfx: Sfx.solar
    }
  };

  window.setMood = function(moodKey) {
    if (!robotFace) return;
    const mood = MoodCatalog[moodKey] || MoodCatalog['normal'];
    currentMood = moodKey;

    // Reset all face emotion classes
    robotFace.className = 'robot-face ' + mood.class;
    if (moodStatus) moodStatus.textContent = mood.text;

    // Trigger acoustic synthesis
    if (mood.sfx) mood.sfx();
  };

  // Click on Face to "Tickle" and make Cozmo Giggle!
  if (robotFace) {
    robotFace.addEventListener('click', () => {
      window.setMood('happy');
    });
  }

  // Automatic Natural Blinking Loop
  setInterval(() => {
    if (!robotFace) return;
    if (currentMood === 'normal' || currentMood === 'curious' || currentMood === 'solar') {
      robotFace.classList.add('face-blink');
      setTimeout(() => {
        robotFace.classList.remove('face-blink');
      }, 180);
    }
  }, 4200);

  // Micro-Saccade Glance (Subtle life-like look around)
  setInterval(() => {
    if (!robotFace) return;
    if (currentMood === 'normal' && !robotFace.classList.contains('face-blink')) {
      const glances = ['', 'glance-left', 'glance-right', ''];
      const pick = glances[Math.floor(Math.random() * glances.length)];
      robotFace.classList.remove('glance-left', 'glance-right');
      if (pick) {
        robotFace.classList.add(pick);
        setTimeout(() => {
          robotFace.classList.remove(pick);
        }, 1100);
      }
    }
  }, 7500);

  // Pinout Explorer Data
  const pinData = {
    'GPIO 5': { target: 'ST7735 TFT CS', bus: 'SPI Hardware', volt: '3.3V Logic', desc: 'Chip Select line for 1.8" TFT. Active LOW during frame rendering.' },
    'GPIO 4': { target: 'ST7735 TFT RST', bus: 'GPIO Output', volt: '3.3V Logic', desc: 'Hardware display reset line toggled low during driver init.' },
    'GPIO 2': { target: 'ST7735 TFT DC/A0', bus: 'GPIO Output', volt: '3.3V Logic', desc: 'Data / Command select line (LOW = command byte, HIGH = pixel data).' },
    'GPIO 23': { target: 'ST7735 TFT MOSI', bus: 'SPI Master Out', volt: '3.3V Logic', desc: 'High-speed SPI data line delivering eye animation bitmaps at 27MHz.' },
    'GPIO 18': { target: 'ST7735 TFT SCK', bus: 'SPI Clock', volt: '3.3V Logic', desc: 'SPI synchronizing clock signal driven by ESP32.' },
    'GPIO 21': { target: 'VL53L1X SDA', bus: 'I2C Bus (0x29)', volt: '3.3V Logic', desc: 'I2C Data line for laser distance telemetry.' },
    'GPIO 22': { target: 'VL53L1X SCL', bus: 'I2C Bus (0x29)', volt: '3.3V Logic', desc: 'I2C Clock line driven at 400kHz.' },
    'GPIO 16': { target: 'SIM7600E-H TXD', bus: 'UART2 RX Pin', volt: '3.3V Logic', desc: 'UART2 Receiver. Crossed to module TXD at 115200 baud for AT and SMS.' },
    'GPIO 17': { target: 'SIM7600E-H RXD', bus: 'UART2 TX Pin', volt: '3.3V Logic', desc: 'UART2 Transmitter. Crossed to module RXD at 115200 baud.' },
    'GPIO 12': { target: 'L298N IN1', bus: 'Motor Dir A', volt: '5V Tolerant', desc: 'Left Track Direction A. Boot-strap pin; keep disconnected during upload.' },
    'GPIO 13': { target: 'L298N IN2', bus: 'Motor Dir B', volt: '5V Tolerant', desc: 'Left Track Direction B.' },
    'GPIO 14': { target: 'L298N IN3', bus: 'Motor Dir A', volt: '5V Tolerant', desc: 'Right Track Direction A.' },
    'GPIO 27': { target: 'L298N IN4', bus: 'Motor Dir B', volt: '5V Tolerant', desc: 'Right Track Direction B.' },
    'GPIO 32': { target: 'L298N ENA', bus: 'PWM Speed', volt: '5V Tolerant', desc: 'Left Track PWM speed channel (0-255 duty cycle, jumpers removed).' },
    'GPIO 33': { target: 'L298N ENB', bus: 'PWM Speed', volt: '5V Tolerant', desc: 'Right Track PWM speed channel.' },
    'GPIO 34': { target: 'MAX9814 OUT', bus: 'ADC1 Channel 6', volt: 'Analog 0-3.3V', desc: 'Analog input-only pin reading mic output for acoustic clap detection.' },
    'GPIO 25': { target: 'Mixer Node', bus: 'DAC1 Analog Out', volt: '0-3.3V Waveform', desc: '8-bit DAC generating real-time sine melodies into passive audio mixer.' },
    'GPIO 35': { target: 'Solar Sense Divider', bus: 'ADC1 Channel 7', volt: 'Analog 0-3.0V', desc: 'Precision 2x100k divider reading solar panel voltage to trigger chime.' },
    'VIN': { target: '5.00V Power Bus', bus: 'Power In', volt: '5.00V DC', desc: 'Regulated 5.00V rail from LM2596/MT3608 powering NodeMCU internal LDO.' },
    'GND': { target: 'Common Star GND', bus: 'Ground Bus', volt: '0.00V Reference', desc: 'Central star ground point on perfboard eliminating audio hum.' }
  };

  const pinItems = document.querySelectorAll('.pin-item');
  const pinTitle = document.getElementById('pinTitle');
  const pinTarget = document.getElementById('pinTarget');
  const pinBus = document.getElementById('pinBus');
  const pinVolt = document.getElementById('pinVolt');
  const pinDesc = document.getElementById('pinDesc');

  pinItems.forEach(item => {
    item.addEventListener('click', () => {
      pinItems.forEach(i => i.classList.remove('active'));
      item.classList.add('active');
      const pinKey = item.getAttribute('data-pin');
      const data = pinData[pinKey];
      if (data) {
        pinTitle.textContent = pinKey;
        pinTarget.textContent = data.target;
        pinBus.textContent = data.bus;
        pinVolt.textContent = data.volt;
        pinDesc.textContent = data.desc;
        playTone(700, 0.05);
      }
    });
  });

  // Commissioning Progress Tracker
  const checks = document.querySelectorAll('.comm-check');
  const progressBar = document.getElementById('commProgress');
  const progressText = document.getElementById('commProgressText');

  function updateProgress() {
    if (!checks.length) return;
    let checkedCount = 0;
    checks.forEach(c => {
      if (c.checked) checkedCount++;
    });
    const pct = Math.round((checkedCount / checks.length) * 100);
    if (progressBar) progressBar.style.width = pct + '%';
    if (progressText) progressText.textContent = `${pct}% Complete (${checkedCount}/${checks.length} Tests Passed)`;
    if (pct === 100) {
      Sfx.excited();
    }
  }

  checks.forEach(c => c.addEventListener('change', updateProgress));

  // Search Filter for Fasteners & Electronics
  const searchInput = document.getElementById('componentSearch');
  if (searchInput) {
    searchInput.addEventListener('input', (e) => {
      const q = e.target.value.toLowerCase();
      document.querySelectorAll('.item-card').forEach(card => {
        const text = card.textContent.toLowerCase();
        if (text.includes(q)) {
          card.style.display = 'flex';
        } else {
          card.style.display = 'none';
        }
      });
    });
  }
});
