#include <PZEM004Tv30.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#define PZEM_RX 16
#define PZEM_TX 17

PZEM004Tv30 pzem(Serial2, PZEM_RX, PZEM_TX);
AsyncWebServer server(80);

// Data settings
const int historySize = 30;  // Optimal for smoothness
float voltageHistory[historySize] = {220.0};
float currentHistory[historySize] = {0.5};
float powerHistory[historySize] = {100.0};
float energyHistory[historySize] = {0.1};
float frequencyHistory[historySize] = {50.0};
float pfHistory[historySize] = {0.9};
int dataIndex = 1;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Energy Monitor</title>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    :root {
      --voltage: #f1c40f;
      --current: #3498db;
      --power: #e74c3c;
      --energy: #2ecc71;
      --frequency: #9b59b6;
      --pf: #e67e22;
    }
    body {
      font-family: Arial, sans-serif;
      background: #f0f0f0;
      padding: 20px;
      margin: 0;
    }
    h1 {
      color: #2c3e50;
      text-align: center;
      margin-bottom: 30px;
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 15px;
      max-width: 1200px;
      margin: 0 auto;
    }
    .card {
      background: white;
      border-radius: 10px;
      padding: 20px;
      box-shadow: 0 2px 5px rgba(0,0,0,0.1);
      min-height: 180px;
    }
    .card-header {
      display: flex;
      align-items: center;
      margin-bottom: 15px;
    }
    .symbol {
      font-size: 32px;
      width: 50px;
      text-align: center;
      margin-right: 15px;
    }
    .voltage { color: var(--voltage); }
    .current { color: var(--current); }
    .power { color: var(--power); }
    .energy { color: var(--energy); }
    .frequency { color: var(--frequency); }
    .pf { color: var(--pf); }
    .value {
      font-size: 24px;
      color: #2c3e50;
    }
    .unit {
      font-size: 16px;
      color: #95a5a6;
      margin-left: 3px;
    }
    .label {
      color: #7f8c8d;
      font-size: 14px;
      font-weight: bold;
      margin-bottom: 5px;
    }
    .chart-container {
      height: 120px;
      width: 100%;
      margin-top: 10px;
    }
    canvas {
      width: 100%;
      height: 100%;
    }
    @media (max-width: 600px) {
      .grid {
        grid-template-columns: 1fr;
      }
    }
  </style>
</head>
<body>
  <h1>🔌 Energy Monitor</h1>
  
  <div class="grid">
    <!-- Row 1 -->
    <div class="card">
      <div class="card-header">
        <div class="symbol voltage">⚡</div>
        <div>
          <div class="label">VOLTAGE</div>
          <div><span class="value" id="voltage">220.0</span><span class="unit">V</span></div>
        </div>
      </div>
      <div class="chart-container">
        <canvas id="voltageChart"></canvas>
      </div>
    </div>

    <div class="card">
      <div class="card-header">
        <div class="symbol current">⇄</div>
        <div>
          <div class="label">CURRENT</div>
          <div><span class="value" id="current">0.50</span><span class="unit">A</span></div>
        </div>
      </div>
      <div class="chart-container">
        <canvas id="currentChart"></canvas>
      </div>
    </div>

    <!-- Row 2 -->
    <div class="card">
      <div class="card-header">
        <div class="symbol power">W</div>
        <div>
          <div class="label">POWER</div>
          <div><span class="value" id="power">100.0</span><span class="unit">W</span></div>
        </div>
      </div>
      <div class="chart-container">
        <canvas id="powerChart"></canvas>
      </div>
    </div>

    <div class="card">
      <div class="card-header">
        <div class="symbol energy">📈</div>
        <div>
          <div class="label">ENERGY</div>
          <div><span class="value" id="energy">0.100</span><span class="unit">kWh</span></div>
        </div>
      </div>
      <div class="chart-container">
        <canvas id="energyChart"></canvas>
      </div>
    </div>

    <!-- Row 3 -->
    <div class="card">
      <div class="card-header">
        <div class="symbol frequency">〰️</div>
        <div>
          <div class="label">FREQUENCY</div>
          <div><span class="value" id="frequency">50.0</span><span class="unit">Hz</span></div>
        </div>
      </div>
      <div class="chart-container">
        <canvas id="frequencyChart"></canvas>
      </div>
    </div>

    <div class="card">
      <div class="card-header">
        <div class="symbol pf">%</div>
        <div>
          <div class="label">POWER FACTOR</div>
          <div><span class="value" id="pf">0.90</span></div>
        </div>
      </div>
      <div class="chart-container">
        <canvas id="pfChart"></canvas>
      </div>
    </div>
  </div>

  <script>
    // Smooth Chart Implementation
    function createSmoothChart(canvasId, color) {
      const canvas = document.getElementById(canvasId);
      const ctx = canvas.getContext('2d');
      let data = Array(30).fill(0);
      
      return {
        update: function(newValue) {
          // Shift old data and add new
          data.shift();
          data.push(newValue);
          
          // Clear and redraw
          ctx.clearRect(0, 0, canvas.width, canvas.height);
          
          // Calculate dynamic scaling
          const max = Math.max(...data);
          const min = Math.min(...data);
          const range = Math.max(1, max - min); // Ensure no division by zero
          
          // Draw smooth line
          ctx.beginPath();
          ctx.strokeStyle = color;
          ctx.lineWidth = 2;
          ctx.lineJoin = 'round';
          
          for(let i = 0; i < data.length; i++) {
            const x = (i / (data.length - 1)) * canvas.width;
            const y = canvas.height - ((data[i] - min) / range) * canvas.height * 0.9;
            
            if(i === 0) {
              ctx.moveTo(x, y);
            } else {
              // Smooth quadratic curves
              const prevX = ((i-1) / (data.length - 1)) * canvas.width;
              const prevY = canvas.height - ((data[i-1] - min) / range) * canvas.height * 0.9;
              const cpx = (prevX + x) / 2;
              ctx.quadraticCurveTo(cpx, prevY, x, y);
            }
          }
          ctx.stroke();
        }
      };
    }

    // Initialize all charts
    const voltageChart = createSmoothChart('voltageChart', '#f1c40f');
    const currentChart = createSmoothChart('currentChart', '#3498db');
    const powerChart = createSmoothChart('powerChart', '#e74c3c');
    const energyChart = createSmoothChart('energyChart', '#2ecc71');
    const frequencyChart = createSmoothChart('frequencyChart', '#9b59b6');
    const pfChart = createSmoothChart('pfChart', '#e67e22');

    // Update all data
    function updateAllData() {
      fetch("/data")
        .then(response => response.json())
        .then(data => {
          // Update displayed values
          document.getElementById('voltage').textContent = data.voltage.toFixed(1);
          document.getElementById('current').textContent = data.current.toFixed(2);
          document.getElementById('power').textContent = data.power.toFixed(1);
          document.getElementById('energy').textContent = data.energy.toFixed(3);
          document.getElementById('frequency').textContent = data.frequency.toFixed(1);
          document.getElementById('pf').textContent = data.pf.toFixed(2);
          
          // Update charts smoothly
          voltageChart.update(data.voltage);
          currentChart.update(data.current);
          powerChart.update(data.power);
          energyChart.update(data.energy);
          frequencyChart.update(data.frequency);
          pfChart.update(data.pf);
        })
        .catch(err => console.log("Error:", err));
    }

    // Start updates (800ms refresh for smoothness)
    updateAllData();
    setInterval(updateAllData, 800);
  </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  
  // Start Access Point
  WiFi.softAP("EnergyMeter", "12345678");
  Serial.println();
  Serial.println("Access Point Started");
  Serial.println("SSID: EnergyMeter");
  Serial.println("Password: 12345678");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Initialize remaining history
  for(int i = dataIndex; i < historySize; i++) {
    voltageHistory[i] = 220.0;
    currentHistory[i] = 0.5;
    powerHistory[i] = 100.0;
    energyHistory[i] = 0.1;
    frequencyHistory[i] = 50.0;
    pfHistory[i] = 0.9;
  }

  // Start Web Server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
    // Get current readings
    float voltage = pzem.voltage();
    float current = pzem.current();
    float power = pzem.power();
    float energy = pzem.energy();
    float frequency = pzem.frequency();
    float pf = pzem.pf();

    // Update history
    if(!isnan(voltage)) {
      voltageHistory[dataIndex] = voltage;
      currentHistory[dataIndex] = current;
      powerHistory[dataIndex] = power;
      energyHistory[dataIndex] = energy;
      frequencyHistory[dataIndex] = frequency;
      pfHistory[dataIndex] = pf;
      
      dataIndex = (dataIndex + 1) % historySize;
    }

    // Prepare JSON response
    String json = "{";
    json += "\"voltage\":" + String(voltage) + ",";
    json += "\"current\":" + String(current) + ",";
    json += "\"power\":" + String(power) + ",";
    json += "\"energy\":" + String(energy) + ",";
    json += "\"frequency\":" + String(frequency) + ",";
    json += "\"pf\":" + String(pf);
    json += "}";
    
    request->send(200, "application/json", json);
  });

  server.begin();
}

void loop() {
  static unsigned long lastUpdate = 0;
  if(millis() - lastUpdate >= 800) {  // Match the 800ms web update rate
    lastUpdate = millis();
    // Sensors are read automatically when /data is accessed
  }
}
