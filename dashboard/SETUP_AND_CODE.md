# Cloud Dashboard — Setup & Code Guide

---

## What This Does

The Dashboard is a Node.js web application that:
- Receives live JSON telemetry from the Surface Gateway via HTTP POST.
- Displays a real-time tunnel map with worker positions.
- Shows health cards with sensor values per worker.
- Allows the operator to send evacuation commands back to the tunnels.

---

## Files

```
dashboard/
├── server.js          ← Node.js backend (HTTP + Socket.io)
└── public/
    └── index.html     ← Frontend live map dashboard
```

---

## How to Run

### Step 1 — Install Dependencies (First Time Only)

Open a terminal in the `dashboard/` folder and run:

```bash
npm install express socket.io body-parser
```

### Step 2 — Start the Server

```bash
node server.js
```

### Step 3 — Open the Dashboard

Open your browser and go to:

```
http://localhost:3000
```

---

## API Endpoints

| Endpoint | Method | Purpose |
|:---|:---|:---|
| `/api/telemetry` | POST | Gateway sends sensor JSON here |
| `/api/command` | POST | Operator sends "LEFT", "RIGHT", "EVAC" |
| `/` | GET | Serves the live HTML dashboard |

---

## Expose Online (Ngrok)

To make the dashboard accessible over the internet (for the Gateway):

```bash
# Install ngrok from ngrok.com/download
ngrok http 3000
```

Copy the URL (e.g., `https://abc123.ngrok-free.app`) and paste it as  
`CLOUD_SERVER_URL` in `surface_gateway.ino`.

---

## Live Data Format Expected

The dashboard parses JSON in this format from the Gateway:

```json
{
  "id":   "W-01",
  "zone": "Mine_Zone_2",
  "mq2":  512,
  "t":    28.5,
  "tag":  "a4f23c",
  "spike": false,
  "sos":  false,
  "fall": false
}
```

Each unique `id` creates a new Worker Card on the dashboard.
