const { app, BrowserWindow } = require("electron");
const path = require("path");
const isDev = !app.isPackaged; // ⬅️ Detect dev vs build mode

app.whenReady().then(() => {
  const mainWindow = new BrowserWindow({
    width: 1000,
    height: 680,
    minWidth: 810,
    minHeight: 600,
    show: false,
    maximizable: true,
    webPreferences: {
      sandbox: false,
      nodeIntegration: true,
      nodeIntegrationInWorker: true,
      webSecurity: false,
      contextIsolation: false,
    },
  });

  // 🔁 Load dev server or built index.html
  if (isDev) {
    mainWindow.loadURL("http://localhost:5173");
  } else {
    mainWindow.loadFile(path.join(__dirname, "..", "dist", "index.html"));
  }

  mainWindow.on("ready-to-show", () => {
    mainWindow.maximize();
    mainWindow.show();
  });
});
