const express = require('express');
const http = require('http');
const socketIo = require('socket.io');
const bodyParser = require('body-parser');
const path = require('path');

const app = express();
const server = http.createServer(app);
const io = socketIo(server);

const PORT = process.env.PORT || 3000;

app.use(bodyParser.json());
app.use(express.static(path.join(__dirname, 'public')));

// Store latest data from each worker
let workerData = {};

// API Endpoint for the ESP32 Surface Gateway to POST data
app.post('/api/telemetry', (req, res) => {
    const data = req.body;
    console.log('Received telemetry:', data);

    if (data.id) {
        // Add a timestamp
        data.timestamp = new Date().toLocaleTimeString();
        workerData[data.id] = data;

        // Broadcast to all connected web dashboards
        io.emit('worker_update', data);
        
        // Response to Gateway (could be used to send back commands)
        res.status(200).send("OK");
    } else {
        res.status(400).send("Invalid Data");
    }
});

// Endpoint to send commands back to the gateway (e.g. Trigger Alarm)
app.post('/api/command', (req, res) => {
    const { workerId, command } = req.body;
    console.log(`Sending command ${command} to ${workerId}`);
    
    // In a real LoRa mesh, we'd broadcast this.
    // For now, we emit it to the gateway via socket if it's connected, 
    // or wait for the gateway's next POST request response.
    io.emit('gateway_command', { workerId, command });
    res.status(200).json({ status: "Command Queued" });
});

app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

server.listen(PORT, () => {
    console.log(`Cloud Dashboard running on http://localhost:${PORT}`);
});
