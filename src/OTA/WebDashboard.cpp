#include <WiFi.h>
#include <WebServer.h>
#include <Arduino.h>

//* ************************************************************************
//* ************************ WEB DASHBOARD *******************************
//* ************************************************************************

WebServer server(80);

// Log buffer - stores last 500 messages
#define MAX_LOG_MESSAGES 500
#define MAX_MESSAGE_LENGTH 256

struct LogMessage {
  char message[MAX_MESSAGE_LENGTH];
  unsigned long timestamp;
};

LogMessage logBuffer[MAX_LOG_MESSAGES];
int logBufferIndex = 0;
int logBufferCount = 0;
bool bufferFull = false;

// Buffer for capturing serial output line by line
String currentLine = "";
bool dashboardInitialized = false;

// Function to add log message to buffer
void addLogToBuffer(const String& msg) {
  // Skip empty messages
  if (msg.length() == 0) return;
  
  // Truncate if too long
  String message = msg;
  message.trim(); // Remove leading/trailing whitespace
  if (message.length() == 0) return;
  
  if (message.length() > MAX_MESSAGE_LENGTH - 1) {
    message = message.substring(0, MAX_MESSAGE_LENGTH - 1);
  }
  
  // Store in buffer
  message.toCharArray(logBuffer[logBufferIndex].message, MAX_MESSAGE_LENGTH);
  logBuffer[logBufferIndex].timestamp = millis();
  
  logBufferIndex = (logBufferIndex + 1) % MAX_LOG_MESSAGES;
  if (!bufferFull && logBufferIndex == 0) {
    bufferFull = true;
  }
  if (!bufferFull) {
    logBufferCount++;
  } else {
    logBufferCount = MAX_LOG_MESSAGES;
  }
  
  // Logs are served via polling endpoint
}

// Public function to log messages (can be called from anywhere)
void logToDashboard(const String& message) {
  addLogToBuffer(message);
}

void logToDashboard(const char* message) {
  addLogToBuffer(String(message));
}

// Helper function to log to both Serial and dashboard
void serialPrintln(const String& message) {
  Serial.println(message);
  logToDashboard(message);
}

void serialPrintln(const char* message) {
  Serial.println(message);
  logToDashboard(message);
}

// API endpoint to get logs (polling)
void handleLogs() {
  String response = "[";
  int start = bufferFull ? logBufferIndex : 0;
  int count = logBufferCount;
  bool first = true;
  
  for (int i = 0; i < count; i++) {
    int idx = (start + i) % MAX_LOG_MESSAGES;
    if (!first) response += ",";
    first = false;
    response += "{\"time\":";
    response += String(logBuffer[idx].timestamp);
    response += ",\"msg\":\"";
    // Escape JSON special characters
    String msg = String(logBuffer[idx].message);
    msg.replace("\\", "\\\\");
    msg.replace("\"", "\\\"");
    msg.replace("\n", "\\n");
    msg.replace("\r", "\\r");
    response += msg;
    response += "\"}";
  }
  response += "]";
  
  server.send(200, "application/json", response);
}

// HTML page with embedded CSS/JS
const char* htmlPage = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Serial Log Dashboard</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Roboto', 'Oxygen', 'Ubuntu', 'Cantarell', 'Fira Sans', 'Droid Sans', 'Helvetica Neue', sans-serif;
            background: linear-gradient(135deg, #0f0c29 0%, #302b63 50%, #24243e 100%);
            min-height: 100vh;
            padding: 20px;
            -webkit-font-smoothing: antialiased;
            -moz-osx-font-smoothing: grayscale;
        }
        
        .container {
            max-width: 1400px;
            margin: 0 auto;
            background: rgba(255, 255, 255, 0.05);
            backdrop-filter: blur(10px);
            border-radius: 20px;
            border: 1px solid rgba(255, 255, 255, 0.1);
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);
            overflow: hidden;
            display: flex;
            flex-direction: column;
            height: calc(100vh - 40px);
        }
        
        .header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 24px 32px;
            display: flex;
            justify-content: space-between;
            align-items: center;
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
        }
        
        .header h1 {
            font-size: 28px;
            font-weight: 700;
            letter-spacing: -0.5px;
            text-shadow: 0 2px 4px rgba(0, 0, 0, 0.2);
        }
        
        .status {
            display: flex;
            align-items: center;
            gap: 12px;
            background: rgba(255, 255, 255, 0.15);
            padding: 8px 16px;
            border-radius: 20px;
            backdrop-filter: blur(10px);
        }
        
        .status-indicator {
            width: 10px;
            height: 10px;
            border-radius: 50%;
            background: #4ade80;
            box-shadow: 0 0 12px rgba(74, 222, 128, 0.8), inset 0 0 8px rgba(255, 255, 255, 0.3);
            animation: pulse 2s cubic-bezier(0.4, 0, 0.6, 1) infinite;
        }
        
        @keyframes pulse {
            0%, 100% { 
                opacity: 1;
                transform: scale(1);
            }
            50% { 
                opacity: 0.7;
                transform: scale(1.1);
            }
        }
        
        .status.disconnected .status-indicator {
            background: #ef4444;
            box-shadow: 0 0 12px rgba(239, 68, 68, 0.8), inset 0 0 8px rgba(255, 255, 255, 0.3);
        }
        
        .controls {
            padding: 20px 32px;
            background: rgba(255, 255, 255, 0.03);
            border-bottom: 1px solid rgba(255, 255, 255, 0.08);
            display: flex;
            gap: 12px;
            backdrop-filter: blur(5px);
        }
        
        button {
            padding: 10px 20px;
            border: none;
            border-radius: 10px;
            cursor: pointer;
            font-size: 14px;
            font-weight: 600;
            transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
            box-shadow: 0 2px 8px rgba(0, 0, 0, 0.15);
            letter-spacing: 0.3px;
            text-transform: uppercase;
            font-size: 12px;
        }
        
        button:hover {
            transform: translateY(-2px);
            box-shadow: 0 4px 12px rgba(0, 0, 0, 0.25);
        }
        
        button:active {
            transform: translateY(0);
            box-shadow: 0 2px 4px rgba(0, 0, 0, 0.2);
        }
        
        .btn-clear {
            background: linear-gradient(135deg, #ef4444 0%, #dc2626 100%);
            color: white;
        }
        
        .btn-clear:hover {
            background: linear-gradient(135deg, #dc2626 0%, #b91c1c 100%);
        }
        
        .btn-pause {
            background: linear-gradient(135deg, #f59e0b 0%, #d97706 100%);
            color: white;
        }
        
        .btn-pause:hover {
            background: linear-gradient(135deg, #d97706 0%, #b45309 100%);
        }
        
        .btn-resume {
            background: linear-gradient(135deg, #10b981 0%, #059669 100%);
            color: white;
        }
        
        .btn-resume:hover {
            background: linear-gradient(135deg, #059669 0%, #047857 100%);
        }
        
        .log-container {
            flex: 1;
            overflow-y: auto;
            padding: 24px;
            background: linear-gradient(180deg, #0a0a0f 0%, #1a1a2e 100%);
            font-family: 'SF Mono', 'Monaco', 'Inconsolata', 'Roboto Mono', 'Courier New', monospace;
            font-size: 13px;
            line-height: 1.8;
        }
        
        .log-entry {
            padding: 8px 12px;
            margin: 2px 0;
            color: #e4e4e7;
            border-left: 3px solid transparent;
            border-radius: 6px;
            word-wrap: break-word;
            transition: all 0.2s ease;
            background: rgba(255, 255, 255, 0.02);
        }
        
        .log-entry:hover {
            background: rgba(255, 255, 255, 0.06);
            border-left-color: rgba(255, 255, 255, 0.2);
            transform: translateX(2px);
        }
        
        .log-timestamp {
            color: #71717a;
            margin-right: 12px;
            font-size: 11px;
            font-weight: 500;
            opacity: 0.8;
        }
        
        .log-message {
            color: #e4e4e7;
        }
        
        .info-message {
            color: #60a5fa;
            border-left-color: #60a5fa;
        }
        
        .warning-message {
            color: #fbbf24;
            border-left-color: #fbbf24;
        }
        
        .error-message {
            color: #f87171;
            border-left-color: #f87171;
            background: rgba(248, 113, 113, 0.1);
        }
        
        .success-message {
            color: #34d399;
            border-left-color: #34d399;
        }
        
        ::-webkit-scrollbar {
            width: 10px;
        }
        
        ::-webkit-scrollbar-track {
            background: rgba(255, 255, 255, 0.05);
            border-radius: 10px;
        }
        
        ::-webkit-scrollbar-thumb {
            background: linear-gradient(180deg, #667eea 0%, #764ba2 100%);
            border-radius: 10px;
            border: 2px solid rgba(255, 255, 255, 0.05);
        }
        
        ::-webkit-scrollbar-thumb:hover {
            background: linear-gradient(180deg, #764ba2 0%, #667eea 100%);
        }
        
        .footer {
            padding: 16px 32px;
            background: rgba(255, 255, 255, 0.03);
            border-top: 1px solid rgba(255, 255, 255, 0.08);
            font-size: 13px;
            color: #a1a1aa;
            display: flex;
            justify-content: space-between;
            backdrop-filter: blur(5px);
            font-weight: 500;
        }
        
        .footer span:first-child {
            color: #e4e4e7;
        }
        
        #messageCount, #autoScrollStatus {
            color: #667eea;
            font-weight: 600;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>📊 ESP32 Serial Log Dashboard</h1>
            <div class="status" id="status">
                <div class="status-indicator"></div>
                <span id="statusText">Connected</span>
            </div>
        </div>
        
        <div class="controls">
            <button class="btn-clear" onclick="clearLogs()">Clear Logs</button>
            <button class="btn-pause" id="pauseBtn" onclick="togglePause()">Pause</button>
        </div>
        
        <div class="log-container" id="logContainer"></div>
        
        <div class="footer">
            <span>Total Messages: <span id="messageCount">0</span></span>
            <span>Auto-scroll: <span id="autoScrollStatus">ON</span></span>
        </div>
    </div>
    
    <script>
        let messageCount = 0;
        let isPaused = false;
        let autoScroll = true;
        const logContainer = document.getElementById('logContainer');
        const statusDiv = document.getElementById('status');
        const statusText = document.getElementById('statusText');
        const messageCountSpan = document.getElementById('messageCount');
        const autoScrollStatus = document.getElementById('autoScrollStatus');
        const pauseBtn = document.getElementById('pauseBtn');
        
        let lastLogCount = 0;
        let isConnected = false;
        
        function pollLogs() {
            fetch('/logs')
                .then(response => {
                    if (!response.ok) throw new Error('Network response was not ok');
                    isConnected = true;
                    statusDiv.classList.remove('disconnected');
                    statusText.textContent = 'Connected';
                    return response.json();
                })
                .then(logs => {
                    if (isPaused && lastLogCount > 0) return;
                    
                    // Only add new logs
                    if (logs.length > lastLogCount) {
                        for (let i = lastLogCount; i < logs.length; i++) {
                            addLogEntry(logs[i].time, logs[i].msg);
                        }
                        lastLogCount = logs.length;
                    }
                })
                .catch(error => {
                    isConnected = false;
                    statusDiv.classList.add('disconnected');
                    statusText.textContent = 'Disconnected';
                    console.error('Error fetching logs:', error);
                });
        }
        
        function clearLogs() {
            logContainer.innerHTML = '';
            messageCount = 0;
            messageCountSpan.textContent = '0';
            lastLogCount = 0; // Reset so we get all logs again
        }
        
        // Start polling
        setInterval(pollLogs, 250); // Poll every 250ms
        pollLogs(); // Initial poll
        
        function addLogEntry(timestamp, message) {
            messageCount++;
            messageCountSpan.textContent = messageCount;
            
            const entry = document.createElement('div');
            entry.className = 'log-entry';
            
            const timeStr = formatTime(timestamp);
            const timestampSpan = document.createElement('span');
            timestampSpan.className = 'log-timestamp';
            timestampSpan.textContent = timeStr;
            
            const messageSpan = document.createElement('span');
            messageSpan.className = 'log-message ' + getMessageClass(message);
            messageSpan.textContent = message;
            
            entry.appendChild(timestampSpan);
            entry.appendChild(messageSpan);
            logContainer.appendChild(entry);
            
            if (autoScroll && !isPaused) {
                logContainer.scrollTop = logContainer.scrollHeight;
            }
        }
        
        function formatTime(ms) {
            const seconds = Math.floor(ms / 1000);
            const mins = Math.floor(seconds / 60);
            const secs = seconds % 60;
            const hours = Math.floor(mins / 60);
            const finalMins = mins % 60;
            return `${String(hours).padStart(2, '0')}:${String(finalMins).padStart(2, '0')}:${String(secs).padStart(2, '0')}`;
        }
        
        function getMessageClass(message) {
            const msg = message.toLowerCase();
            if (msg.includes('error') || msg.includes('failed') || msg.includes('fail')) {
                return 'error-message';
            } else if (msg.includes('warning') || msg.includes('warn')) {
                return 'warning-message';
            } else if (msg.includes('success') || msg.includes('complete') || msg.includes('ready')) {
                return 'success-message';
            } else if (msg.includes('info') || msg.includes('===')) {
                return 'info-message';
            }
            return '';
        }
        
        function togglePause() {
            isPaused = !isPaused;
            if (isPaused) {
                pauseBtn.textContent = 'Resume';
                pauseBtn.className = 'btn-resume';
                autoScrollStatus.textContent = 'PAUSED';
            } else {
                pauseBtn.textContent = 'Pause';
                pauseBtn.className = 'btn-pause';
                autoScrollStatus.textContent = 'ON';
                logContainer.scrollTop = logContainer.scrollHeight;
            }
        }
        
        // Start polling on page load
        pollLogs();
    </script>
</body>
</html>
)HTML";

void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void setupWebDashboard() {
  // Setup web server
  server.on("/", handleRoot);
  server.on("/logs", handleLogs);
  server.begin();
}

void handleWebDashboard() {
  server.handleClient();
}

