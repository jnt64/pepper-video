const electron = require('electron')

// Module to control application life.
const app = electron.app

// Module to create native browser window.
const BrowserWindow = electron.BrowserWindow

const path = require('path')
const url  = require('url')

// Synthetic Chromium Command Line Args. Register Player Plugins
app.commandLine.appendSwitch(
    'register-pepper-plugins', 
    path.join(__dirname, 'native/lib/mac/ppapi_sweet.1.so;application/x-sweet-1') + "," +
    path.join(__dirname, 'native/lib/mac/ppapi_sweet.2.so;application/x-sweet-2') + "," +
    path.join(__dirname, 'native/lib/mac/ppapi_sweet.3.so;application/x-sweet-3') + "," +
    path.join(__dirname, 'native/lib/mac/ppapi_sweet.4.so;application/x-sweet-4') + "," +
    path.join(__dirname, 'native/lib/mac/ppapi_sweet.5.so;application/x-sweet-5') + "," +
    path.join(__dirname, 'native/lib/mac/ppapi_sweet.6.so;application/x-sweet-6') + "," +
    path.join(__dirname, 'native/lib/mac/ppapi_sweet.7.so;application/x-sweet-7') + "," +
    path.join(__dirname, 'native/lib/mac/ppapi_sweet.8.so;application/x-sweet-8')
);

let mainWindow; // Global to avoid Garbage Collection.

function createWindow () {
  
    // Create the browser window.
    mainWindow = new BrowserWindow({
        'width': 320*2,
        'height': 240*2,
        'webPreferences': {
            'plugins': true // Allow our Video Player Plugin.
        }
    });

    // Load index.html.
    mainWindow.loadURL(url.format({
            pathname: path.join(__dirname, 'html/index.0.html'),
            protocol: 'file:',
            slashes: true
    }));

    // Open Chromium Developer Tools.
    mainWindow.webContents.openDevTools();

    // Window closed function.
    mainWindow.on('closed', function () {
        mainWindow = null // Delete the Reference. Windows can be stored in an array.
    })
}

app.on('ready', createWindow)

// Quit when all windows are closed.
app.on('window-all-closed', function () {
    if (process.platform !== 'darwin') {
        app.quit(); // Keep open until explicity quit on macOS.
    }
})

app.on('activate', function () {
    if (mainWindow === null) {
        createWindow();
    }
});
