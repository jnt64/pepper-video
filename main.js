const electron = require('electron')
// Module to control application life.
const app = electron.app
// Module to create native browser window.
const BrowserWindow = electron.BrowserWindow

const path = require('path')
const url = require('url')

// NACL (Pepper)
// require('./example.js');
// require('./common.js');

app.commandLine.appendSwitch('ppapi-flash-path', "/Users/james/Library/Application Support/Google/Chrome/PepperFlash/25.0.0.148/PepperFlashPlayer.plugin");
app.commandLine.appendSwitch('ppapi-flash-version', '25.0.0.148');

app.commandLine.appendSwitch('use-file-for-fake-video-capture', path.join(__dirname, 'basketball.mp4'));


app.commandLine.appendSwitch(
    'register-pepper-plugins', 
    path.join(__dirname, 'ppapi_hello.so;application/x-hello')  + "," + 
    path.join(__dirname, 'ppapi_gles2.so;application/x-gles2')  + "," + 
    path.join(__dirname, 'ppapi_video_decode.so;application/x-gles2-video') + "," +
    // path.join(__dirname, 'ppapi_sweet.so;application/x-sweet') + "," +
    path.join(__dirname, 'ppapi_sweet_decode.so;application/x-sweet_decode') + "," +
    path.join(__dirname, 'ppapi_media_stream_video.so;application/x-media_stream_video') + "," +
    path.join(__dirname, 'ppapi_sweet.1.so;application/x-sweet-1') + "," +
    path.join(__dirname, 'ppapi_sweet.2.so;application/x-sweet-2') + "," +
    path.join(__dirname, 'ppapi_sweet.3.so;application/x-sweet-3') + "," +
    path.join(__dirname, 'ppapi_sweet.4.so;application/x-sweet-4') + "," +
    path.join(__dirname, 'ppapi_sweet.5.so;application/x-sweet-5') + "," +
    path.join(__dirname, 'ppapi_sweet.6.so;application/x-sweet-6') + "," +
    path.join(__dirname, 'ppapi_sweet.7.so;application/x-sweet-7') + "," +
    path.join(__dirname, 'ppapi_sweet.8.so;application/x-sweet-8')
);
//app.commandLine.appendSwitch('register-pepper-plugins', path.join(__dirname, 'ppapi_gles2.so;application/x-gles2'))


// Keep a global reference of the window object, if you don't, the window will
// be closed automatically when the JavaScript object is garbage collected.
let mainWindow

function createWindow () {
  // Create the browser window.
  mainWindow = new BrowserWindow({
      'width': 1000,
      'height': 540*3,
      'webPreferences': {
        'plugins': true
      }
    });

  // and load the index.html of the app.
  mainWindow.loadURL(url.format({
    pathname: path.join(__dirname, 'index.html'),
    protocol: 'file:',
    slashes: true
  }))

  mainWindow.webContents.openDevTools();

  // Open the DevTools.
  // mainWindow.webContents.openDevTools()

  // Emitted when the window is closed.
  mainWindow.on('closed', function () {
    // Dereference the window object, usually you would store windows
    // in an array if your app supports multi windows, this is the time
    // when you should delete the corresponding element.
    mainWindow = null
  })
}

// This method will be called when Electron has finished
// initialization and is ready to create browser windows.
// Some APIs can only be used after this event occurs.
app.on('ready', createWindow)

// Quit when all windows are closed.
app.on('window-all-closed', function () {
  // On OS X it is common for applications and their menu bar
  // to stay active until the user quits explicitly with Cmd + Q
  if (process.platform !== 'darwin') {
    app.quit()
  }
})

app.on('activate', function () {
  // On OS X it's common to re-create a window in the app when the
  // dock icon is clicked and there are no other windows open.
  if (mainWindow === null) {
    createWindow()
  }
})

// In this file you can include the rest of your app's specific main process
// code. You can also put them in separate files and require them here.
